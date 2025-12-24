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
#include <iostream>
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
              ringBuffer, sequenceBarrier, MASTER_SPIN_LIMIT,
              coreWaitStrategy)),
      eventHandler_(eventHandler), exceptionHandler_(exceptionHandler),
      name_(name),
      sequence_(disruptor::Sequence::INITIAL_VALUE),
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
  std::cout << "[TwoStepMasterProcessor:" << name_ << "] run() called" << std::endl;
  if (running_.compare_exchange_strong(const_cast<int32_t &>(IDLE), RUNNING)) {
    std::cout << "[TwoStepMasterProcessor:" << name_ << "] Starting, clearing alert" << std::endl;
    auto *barrier = static_cast<disruptor::ProcessingSequenceBarrier<
        disruptor::MultiProducerSequencer<WaitStrategyT>, WaitStrategyT> *>(
        sequenceBarrier_);
    barrier->clearAlert();

    try {
      if (running_.load() == RUNNING) {
        ProcessEvents();
      }
    } catch (...) {
      std::cout << "[TwoStepMasterProcessor:" << name_ << "] Exception in ProcessEvents" << std::endl;
      // Handle exception
    }
    running_.store(IDLE);
    std::cout << "[TwoStepMasterProcessor:" << name_ << "] Exiting run()" << std::endl;
  } else {
    std::cout << "[TwoStepMasterProcessor:" << name_ << "] Already running, skipping" << std::endl;
  }
}

template <typename WaitStrategyT>
void TwoStepMasterProcessor<WaitStrategyT>::SetSlaveProcessor(
    TwoStepSlaveProcessor<WaitStrategyT> *slaveProcessor) {
  slaveProcessor_ = slaveProcessor;
}

template <typename WaitStrategyT>
void TwoStepMasterProcessor<WaitStrategyT>::ProcessEvents() {
  // Set thread name (simplified for C++)
  // std::thread::current_thread().set_name("Thread-" + name_);
  std::cout << "[TwoStepMasterProcessor:" << name_ << "] ProcessEvents() started" << std::endl;

  int64_t nextSequence = sequence_.get() + 1L;
  int64_t currentSequenceGroup = 0;
  std::cout << "[TwoStepMasterProcessor:" << name_ << "] Starting from sequence " << nextSequence << ", current sequence_=" << sequence_.get() << std::endl;

  // wait until slave processor has instructed to run
  while (slaveProcessor_ != nullptr && !slaveProcessor_->isRunning()) {
    std::cout << "[TwoStepMasterProcessor:" << name_ << "] Waiting for slave processor to start..." << std::endl;
    std::this_thread::yield();
  }
  std::cout << "[TwoStepMasterProcessor:" << name_ << "] Slave processor started, entering main loop" << std::endl;

  while (true) {
    common::cmd::OrderCommand *cmd = nullptr;
    try {
      auto *ringBuffer = static_cast<disruptor::MultiProducerRingBuffer<
          common::cmd::OrderCommand, WaitStrategyT> *>(ringBuffer_);

      // should spin and also check another barrier
      std::cout << "[TwoStepMasterProcessor:" << name_ << "] TryWaitFor(" << nextSequence << "), current sequence_=" << sequence_.get() << std::endl;
      int64_t availableSequence = waitSpinningHelper_->TryWaitFor(nextSequence);
      std::cout << "[TwoStepMasterProcessor:" << name_ << "] TryWaitFor returned " << availableSequence << ", nextSequence=" << nextSequence << std::endl;

      if (nextSequence <= availableSequence) {
        std::cout << "[TwoStepMasterProcessor:" << name_ << "] Condition check: nextSequence(" << nextSequence << ") <= availableSequence(" << availableSequence << ") = TRUE" << std::endl;
        std::cout << "[TwoStepMasterProcessor:" << name_ << "] Processing sequences " << nextSequence << " to " << availableSequence << std::endl;
        while (nextSequence <= availableSequence) {
          cmd = &ringBuffer->get(nextSequence);
          std::cout << "[TwoStepMasterProcessor:" << name_ << "] Processing seq=" << nextSequence << ", cmd=" << static_cast<int>(cmd->command) << ", group=" << cmd->eventsGroup << std::endl;

          // switch to next group - let slave processor start doing its handling
          // cycle
          if (cmd->eventsGroup != currentSequenceGroup) {
            if (slaveProcessor_ != nullptr) {
              slaveProcessor_->SetNextSequence(nextSequence);
            }
            currentSequenceGroup = cmd->eventsGroup;
          }

          bool forcedPublish = eventHandler_->OnEvent(nextSequence, cmd);
          std::cout << "[TwoStepMasterProcessor:" << name_ << "] OnEvent completed, forcedPublish=" << forcedPublish << std::endl;
          nextSequence++;

          if (forcedPublish) {
            sequence_.set(nextSequence - 1);
            waitSpinningHelper_->SignalAllWhenBlocking();
          }

          if (cmd->command == common::cmd::OrderCommandType::SHUTDOWN_SIGNAL) {
            // having all sequences aligned with the ringbuffer cursor is a
            // requirement for proper shutdown let following processors to catch
            // up
            if (slaveProcessor_ != nullptr) {
              slaveProcessor_->SetNextSequence(nextSequence);
            }
          }
        }
        sequence_.set(availableSequence);
        std::cout << "[TwoStepMasterProcessor:" << name_ << "] Updated sequence to " << availableSequence << ", sequence_.get()=" << sequence_.get() << std::endl;
        waitSpinningHelper_->SignalAllWhenBlocking();
      } else {
        std::cout << "[TwoStepMasterProcessor:" << name_ << "] No events available, nextSequence=" << nextSequence << ", availableSequence=" << availableSequence << std::endl;
      }
    } catch (const disruptor::AlertException &ex) {
      if (running_.load() != RUNNING) {
        break;
      }
    } catch (const std::exception &ex) {
      if (exceptionHandler_) {
        exceptionHandler_->HandleEventException(ex, nextSequence, cmd);
      }
      sequence_.set(nextSequence);
      waitSpinningHelper_->SignalAllWhenBlocking();
      nextSequence++;
    } catch (...) {
      if (exceptionHandler_) {
        exceptionHandler_->HandleEventException(
            std::runtime_error("Unknown exception"), nextSequence, cmd);
      }
      sequence_.set(nextSequence);
      waitSpinningHelper_->SignalAllWhenBlocking();
      nextSequence++;
    }
  }
}

// Explicit template instantiations
template class TwoStepMasterProcessor<disruptor::BlockingWaitStrategy>;
template class TwoStepMasterProcessor<disruptor::YieldingWaitStrategy>;
template class TwoStepMasterProcessor<disruptor::BusySpinWaitStrategy>;

} // namespace processors
} // namespace core
} // namespace exchange
