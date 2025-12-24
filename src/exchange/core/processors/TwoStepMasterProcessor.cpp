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
#include <mutex>
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

  {
    std::lock_guard<std::mutex> lock(processors::log_mutex);
    std::cout << "[TwoStepMasterProcessor:" << name_
              << "] ProcessEvents() "
                 "started, nextSequence="
              << nextSequence << std::endl;
  }

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

      {
        std::lock_guard<std::mutex> lock(processors::log_mutex);
        std::cout << "[TwoStepMasterProcessor:" << name_
                  << "] TryWaitFor returned availableSequence="
                  << availableSequence << ", nextSequence=" << nextSequence
                  << std::endl;
        std::cout.flush();
      }

      if (nextSequence <= availableSequence) {
        while (nextSequence <= availableSequence) {
          cmd = &ringBuffer->get(nextSequence);

          {
            std::lock_guard<std::mutex> lock(processors::log_mutex);
            std::cout << "[TwoStepMasterProcessor:" << name_
                      << "] Processing seq=" << nextSequence
                      << ", cmd=" << static_cast<int>(cmd->command)
                      << ", eventsGroup=" << cmd->eventsGroup
                      << ", currentSequenceGroup=" << currentSequenceGroup
                      << std::endl;
          }

          // switch to next group - let slave processor start doing its handling
          // cycle
          if (cmd->eventsGroup != currentSequenceGroup) {
            {
              std::lock_guard<std::mutex> lock(processors::log_mutex);
              std::cout << "[TwoStepMasterProcessor:" << name_
                        << "] Switching group, calling "
                           "PublishProgressAndTriggerSlaveProcessor("
                        << nextSequence << ")" << std::endl;
              std::cout.flush();
            }
            PublishProgressAndTriggerSlaveProcessor(nextSequence);
            {
              std::lock_guard<std::mutex> lock(processors::log_mutex);
              std::cout << "[TwoStepMasterProcessor:" << name_
                        << "] PublishProgressAndTriggerSlaveProcessor("
                        << nextSequence << ") returned, continuing..."
                        << std::endl;
              std::cout.flush();
            }
            currentSequenceGroup = cmd->eventsGroup;
          }

          {
            std::lock_guard<std::mutex> lock(processors::log_mutex);
            std::cout << "[TwoStepMasterProcessor:" << name_
                      << "] About to call eventHandler_->OnEvent("
                      << nextSequence << ", cmd)" << std::endl;
            std::cout.flush();
          }
          bool forcedPublish = false;
          try {
            forcedPublish = eventHandler_->OnEvent(nextSequence, cmd);
          } catch (const std::exception &ex) {
            {
              std::lock_guard<std::mutex> lock(processors::log_mutex);
              std::cout << "[TwoStepMasterProcessor:" << name_
                        << "] Exception in eventHandler_->OnEvent("
                        << nextSequence << "): " << ex.what() << std::endl;
              std::cout.flush();
            }
            throw;
          } catch (...) {
            {
              std::lock_guard<std::mutex> lock(processors::log_mutex);
              std::cout << "[TwoStepMasterProcessor:" << name_
                        << "] Unknown exception in eventHandler_->OnEvent("
                        << nextSequence << ")" << std::endl;
              std::cout.flush();
            }
            throw;
          }
          {
            std::lock_guard<std::mutex> lock(processors::log_mutex);
            std::cout << "[TwoStepMasterProcessor:" << name_
                      << "] eventHandler_->OnEvent(" << nextSequence
                      << ") returned, forcedPublish=" << forcedPublish
                      << std::endl;
            std::cout.flush();
          }
          nextSequence++;
          {
            std::lock_guard<std::mutex> lock(processors::log_mutex);
            std::cout << "[TwoStepMasterProcessor:" << name_
                      << "] nextSequence incremented to " << nextSequence
                      << ", checking loop condition..." << std::endl;
            std::cout.flush();
          }

          if (forcedPublish) {
            sequence_.set(nextSequence - 1);
            waitSpinningHelper_->SignalAllWhenBlocking();
          }

          if (cmd->command == common::cmd::OrderCommandType::SHUTDOWN_SIGNAL) {
            {
              std::lock_guard<std::mutex> lock(processors::log_mutex);
              std::cout << "[TwoStepMasterProcessor:" << name_
                        << "] SHUTDOWN_SIGNAL detected, calling "
                           "PublishProgressAndTriggerSlaveProcessor("
                        << nextSequence << ")" << std::endl;
            }
            // having all sequences aligned with the ringbuffer cursor is a
            // requirement for proper shutdown let following processors to catch
            // up
            PublishProgressAndTriggerSlaveProcessor(nextSequence);
          }
        }
        sequence_.set(availableSequence);
        waitSpinningHelper_->SignalAllWhenBlocking();
        {
          std::lock_guard<std::mutex> lock(processors::log_mutex);
          std::cout << "[TwoStepMasterProcessor:" << name_
                    << "] Finished processing batch, sequence set to "
                    << availableSequence << ", nextSequence=" << nextSequence
                    << ", continuing loop..." << std::endl;
          std::cout.flush();
        }
      } else {
        {
          std::lock_guard<std::mutex> lock(processors::log_mutex);
          std::cout << "[TwoStepMasterProcessor:" << name_ << "] nextSequence("
                    << nextSequence << ") > availableSequence("
                    << availableSequence << "), waiting..." << std::endl;
          std::cout.flush();
        }
      }
    } catch (const disruptor::AlertException &ex) {
      {
        std::lock_guard<std::mutex> lock(processors::log_mutex);
        std::cout << "[TwoStepMasterProcessor:" << name_
                  << "] AlertException caught, running_=" << running_.load()
                  << std::endl;
      }
      if (running_.load() != RUNNING) {
        {
          std::lock_guard<std::mutex> lock(processors::log_mutex);
          std::cout << "[TwoStepMasterProcessor:" << name_
                    << "] Exiting ProcessEvents() due to AlertException"
                    << std::endl;
        }
        break;
      }
    } catch (const std::exception &ex) {
      {
        std::lock_guard<std::mutex> lock(processors::log_mutex);
        std::cout << "[TwoStepMasterProcessor:" << name_
                  << "] std::exception caught in outer catch: " << ex.what()
                  << ", nextSequence=" << nextSequence << std::endl;
        std::cout.flush();
      }
      if (exceptionHandler_) {
        {
          std::lock_guard<std::mutex> lock(processors::log_mutex);
          std::cout << "[TwoStepMasterProcessor:" << name_
                    << "] Calling exceptionHandler_->HandleEventException"
                    << std::endl;
          std::cout.flush();
        }
        exceptionHandler_->HandleEventException(ex, nextSequence, cmd);
        {
          std::lock_guard<std::mutex> lock(processors::log_mutex);
          std::cout << "[TwoStepMasterProcessor:" << name_
                    << "] exceptionHandler_->HandleEventException returned"
                    << std::endl;
          std::cout.flush();
        }
      }
      {
        std::lock_guard<std::mutex> lock(processors::log_mutex);
        std::cout << "[TwoStepMasterProcessor:" << name_
                  << "] Setting sequence to " << nextSequence
                  << ", then incrementing to " << (nextSequence + 1)
                  << std::endl;
        std::cout.flush();
      }
      sequence_.set(nextSequence);
      waitSpinningHelper_->SignalAllWhenBlocking();
      nextSequence++;
      {
        std::lock_guard<std::mutex> lock(processors::log_mutex);
        std::cout << "[TwoStepMasterProcessor:" << name_
                  << "] Exception handled, nextSequence=" << nextSequence
                  << ", continuing loop..." << std::endl;
        std::cout.flush();
      }
    } catch (...) {
      {
        std::lock_guard<std::mutex> lock(processors::log_mutex);
        std::cout << "[TwoStepMasterProcessor:" << name_
                  << "] Unknown exception caught in outer catch, nextSequence="
                  << nextSequence << std::endl;
        std::cout.flush();
      }
      if (exceptionHandler_) {
        exceptionHandler_->HandleEventException(
            std::runtime_error("Unknown exception"), nextSequence, cmd);
      }
      sequence_.set(nextSequence);
      waitSpinningHelper_->SignalAllWhenBlocking();
      nextSequence++;
      {
        std::lock_guard<std::mutex> lock(processors::log_mutex);
        std::cout << "[TwoStepMasterProcessor:" << name_
                  << "] Unknown exception handled, nextSequence="
                  << nextSequence << ", continuing loop..." << std::endl;
        std::cout.flush();
      }
    }
  }
}

template <typename WaitStrategyT>
void TwoStepMasterProcessor<WaitStrategyT>::
    PublishProgressAndTriggerSlaveProcessor(int64_t nextSequence) {
  {
    std::lock_guard<std::mutex> lock(processors::log_mutex);
    std::cout << "[TwoStepMasterProcessor:" << name_
              << "] PublishProgressAndTriggerSlaveProcessor(" << nextSequence
              << "): setting sequence to " << (nextSequence - 1) << std::endl;
  }
  sequence_.set(nextSequence - 1);
  waitSpinningHelper_->SignalAllWhenBlocking();
  if (slaveProcessor_ != nullptr) {
    {
      std::lock_guard<std::mutex> lock(processors::log_mutex);
      std::cout << "[TwoStepMasterProcessor:" << name_
                << "] Calling slaveProcessor_->HandlingCycle(" << nextSequence
                << ")" << std::endl;
    }
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
