/*
 * Copyright 2019 Maksim Zheravin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <disruptor/AlertException.h>
#include <disruptor/BlockingWaitStrategy.h>
#include <disruptor/BusySpinWaitStrategy.h>
#include <disruptor/YieldingWaitStrategy.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <exchange/core/processors/TwoStepMasterProcessor.h>
#include <exchange/core/processors/TwoStepSlaveProcessor.h>
#include <exchange/core/processors/WaitSpinningHelper.h>
#include <exchange/core/utils/Logger.h>
#include <thread>

namespace exchange {
namespace core {
namespace processors {

template <typename WaitStrategyT>
TwoStepMasterProcessor<WaitStrategyT>::TwoStepMasterProcessor(
    disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand, WaitStrategyT>
        *ringBuffer,
    disruptor::ProcessingSequenceBarrier<
        disruptor::MultiProducerSequencer<WaitStrategyT>, WaitStrategyT>
        *sequenceBarrier,
    SimpleEventHandler *eventHandler,
    DisruptorExceptionHandler<common::cmd::OrderCommand> *exceptionHandler,
    common::CoreWaitStrategy coreWaitStrategy, const std::string &name)
    : running_(IDLE), ringBuffer_(ringBuffer),
      sequenceBarrier_(sequenceBarrier),
      waitSpinningHelper_(
          new WaitSpinningHelper<common::cmd::OrderCommand, WaitStrategyT>(
              ringBuffer, sequenceBarrier, MASTER_SPIN_LIMIT, coreWaitStrategy,
              name)),
      eventHandler_(eventHandler), exceptionHandler_(exceptionHandler),
      name_(name), sequence_(disruptor::Sequence::INITIAL_VALUE),
      slaveProcessor_(nullptr) {}

template <typename WaitStrategyT>
disruptor::Sequence &TwoStepMasterProcessor<WaitStrategyT>::getSequence() {
  return sequence_;
}

template <typename WaitStrategyT>
void TwoStepMasterProcessor<WaitStrategyT>::halt() {
  running_.store(HALTED);
  // Cast sequenceBarrier_ to call alert()
  auto *barrier = static_cast<disruptor::ProcessingSequenceBarrier<
      disruptor::MultiProducerSequencer<WaitStrategyT>, WaitStrategyT> *>(
      sequenceBarrier_);
  barrier->alert();
}

template <typename WaitStrategyT>
bool TwoStepMasterProcessor<WaitStrategyT>::isRunning() {
  return running_.load() != IDLE;
}

template <typename WaitStrategyT>
void TwoStepMasterProcessor<WaitStrategyT>::run() {
  if (running_.compare_exchange_strong(const_cast<int32_t &>(IDLE), RUNNING)) {
    auto *barrier = static_cast<disruptor::ProcessingSequenceBarrier<
        disruptor::MultiProducerSequencer<WaitStrategyT>, WaitStrategyT> *>(
        sequenceBarrier_);
    barrier->clearAlert();

    try {
      if (running_.load() == RUNNING) {
        ProcessEvents();
      }
    } catch (...) {
      // Handle exception
    }
    running_.store(IDLE);
  }
}

template <typename WaitStrategyT>
void TwoStepMasterProcessor<WaitStrategyT>::SetSlaveProcessor(
    TwoStepSlaveProcessor<WaitStrategyT> *slaveProcessor) {
  slaveProcessor_ = slaveProcessor;
}

template <typename WaitStrategyT>
void TwoStepMasterProcessor<WaitStrategyT>::ProcessEvents() {
  int64_t nextSequence = sequence_.get() + 1L;
  int64_t currentSequenceGroup = 0;

  // wait until slave processor has instructed to run
  while (slaveProcessor_ != nullptr && !slaveProcessor_->isRunning()) {
    std::this_thread::yield();
  }

  while (true) {
    common::cmd::OrderCommand *cmd = nullptr;
    try {
      auto *ringBuffer = static_cast<disruptor::MultiProducerRingBuffer<
          common::cmd::OrderCommand, WaitStrategyT> *>(ringBuffer_);

      // should spin and also check another barrier
      int64_t availableSequence = waitSpinningHelper_->TryWaitFor(nextSequence);

      if (nextSequence <= availableSequence) {
        while (nextSequence <= availableSequence) {
          cmd = &ringBuffer->get(nextSequence);

          // switch to next group - let slave processor start doing its handling
          // cycle
          if (cmd->eventsGroup != currentSequenceGroup) {
            PublishProgressAndTriggerSlaveProcessor(nextSequence);
            currentSequenceGroup = cmd->eventsGroup;
          }
          bool forcedPublish = false;
          try {
            forcedPublish = eventHandler_->OnEvent(nextSequence, cmd);
          } catch (const std::exception &ex) {
            LOG_ERROR("[TwoStepMasterProcessor:{}] Exception in "
                      "eventHandler_->OnEvent({}): {}",
                      name_, nextSequence, ex.what());
            throw;
          } catch (...) {
            LOG_ERROR("[TwoStepMasterProcessor:{}] Unknown exception in "
                      "eventHandler_->OnEvent({})",
                      name_, nextSequence);
            throw;
          }
          nextSequence++;

          if (forcedPublish) {
            sequence_.set(nextSequence - 1);
            waitSpinningHelper_->SignalAllWhenBlocking();
          }

          if (cmd->command == common::cmd::OrderCommandType::SHUTDOWN_SIGNAL) {
            LOG_DEBUG("[TwoStepMasterProcessor:{}] SHUTDOWN_SIGNAL detected, "
                      "calling PublishProgressAndTriggerSlaveProcessor({})",
                      name_, nextSequence);
            // having all sequences aligned with the ringbuffer cursor is a
            // requirement for proper shutdown let following processors to catch
            // up
            PublishProgressAndTriggerSlaveProcessor(nextSequence);
          }
        }
        sequence_.set(availableSequence);
        waitSpinningHelper_->SignalAllWhenBlocking();
      }
    } catch (const disruptor::AlertException &ex) {
      LOG_DEBUG(
          "[TwoStepMasterProcessor:{}] AlertException caught, running_={}",
          name_, running_.load());
      if (running_.load() != RUNNING) {
        LOG_DEBUG("[TwoStepMasterProcessor:{}] Exiting ProcessEvents() due to "
                  "AlertException",
                  name_);
        break;
      }
    } catch (const std::exception &ex) {
      LOG_ERROR("[TwoStepMasterProcessor:{}] std::exception caught in outer "
                "catch: {}, nextSequence={}",
                name_, ex.what(), nextSequence);
      if (exceptionHandler_) {
        LOG_DEBUG("[TwoStepMasterProcessor:{}] Calling "
                  "exceptionHandler_->HandleEventException",
                  name_);
        exceptionHandler_->HandleEventException(ex, nextSequence, cmd);
        LOG_DEBUG("[TwoStepMasterProcessor:{}] "
                  "exceptionHandler_->HandleEventException returned",
                  name_);
      }
      LOG_DEBUG("[TwoStepMasterProcessor:{}] Setting sequence to {}, then "
                "incrementing to {}",
                name_, nextSequence, nextSequence + 1);
      sequence_.set(nextSequence);
      waitSpinningHelper_->SignalAllWhenBlocking();
      nextSequence++;
      LOG_DEBUG("[TwoStepMasterProcessor:{}] Exception handled, "
                "nextSequence={}, continuing loop...",
                name_, nextSequence);
    } catch (...) {
      LOG_ERROR("[TwoStepMasterProcessor:{}] Unknown exception caught in outer "
                "catch, nextSequence={}",
                name_, nextSequence);
      if (exceptionHandler_) {
        exceptionHandler_->HandleEventException(
            std::runtime_error("Unknown exception"), nextSequence, cmd);
      }
      sequence_.set(nextSequence);
      waitSpinningHelper_->SignalAllWhenBlocking();
      nextSequence++;
      LOG_DEBUG("[TwoStepMasterProcessor:{}] Unknown exception handled, "
                "nextSequence={}, continuing loop...",
                name_, nextSequence);
    }
  }
}

template <typename WaitStrategyT>
void TwoStepMasterProcessor<WaitStrategyT>::
    PublishProgressAndTriggerSlaveProcessor(int64_t nextSequence) {
  LOG_DEBUG(
      "[TwoStepMasterProcessor:{}] "
      "PublishProgressAndTriggerSlaveProcessor({}): setting sequence to {}",
      name_, nextSequence, nextSequence - 1);
  sequence_.set(nextSequence - 1);
  waitSpinningHelper_->SignalAllWhenBlocking();
  if (slaveProcessor_ != nullptr) {
    LOG_DEBUG("[TwoStepMasterProcessor:{}] Calling "
              "slaveProcessor_->HandlingCycle({})",
              name_, nextSequence);
    slaveProcessor_->HandlingCycle(nextSequence);
  }
}

// Explicit template instantiations
template class TwoStepMasterProcessor<disruptor::BlockingWaitStrategy>;
template class TwoStepMasterProcessor<disruptor::YieldingWaitStrategy>;
template class TwoStepMasterProcessor<disruptor::BusySpinWaitStrategy>;

} // namespace processors
} // namespace core
} // namespace exchange
