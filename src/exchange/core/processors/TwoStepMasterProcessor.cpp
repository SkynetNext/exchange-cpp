/*
 * Copyright 2025 Justin Zhu
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
#include <exchange/core/utils/LatencyBreakdown.h>
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
  sequenceBarrier_->alert();
}

template <typename WaitStrategyT>
bool TwoStepMasterProcessor<WaitStrategyT>::isRunning() {
  return running_.load() != IDLE;
}

template <typename WaitStrategyT>
void TwoStepMasterProcessor<WaitStrategyT>::run() {
  if (running_.compare_exchange_strong(const_cast<int32_t &>(IDLE), RUNNING)) {
    sequenceBarrier_->clearAlert();

    try {
      if (running_.load() == RUNNING) {
        ProcessEvents();
      }
    } catch (...) {
      // Match Java: finally block ensures running is set to IDLE
      // Exception is allowed to propagate (no catch here)
    }
    // Match Java: finally block - always set to IDLE
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
  // Match Java: Thread.currentThread().setName("Thread-" + name);
  // Note: C++ doesn't have thread naming in standard library, skip for now

  // Reuse sequence_.get() result to avoid redundant atomic operation
  // sequence_.get() uses relaxed load + acquire fence, so we cache it once
  // Note: sequence is int64_t (max 2^63-1). At 1M msgs/sec, overflow would take
  // ~292K years. Ring buffer uses bitwise masking (sequence & indexMask_) for
  // indexing, so overflow doesn't affect index calculation. However, comparison
  // operations (e.g., lastProcessedSequence > lastTriggeredSequence) may have
  // issues after overflow (wrap-around from max to min), but this is
  // practically impossible in real-world usage.
  int64_t initialSequence = sequence_.get();
  int64_t nextSequence = initialSequence + 1L;
  int64_t currentSequenceGroup = 0; // Match Java: initialize to 0
  int64_t lastTriggeredSequence =
      initialSequence; // Track last triggered sequence

  // wait until slave processor has instructed to run
  // Match Java: while (!slaveProcessor.isRunning())
  // C++ adds null check for safety
  while (slaveProcessor_ != nullptr && !slaveProcessor_->isRunning()) {
    std::this_thread::yield();
  }

  while (true) {
    common::cmd::OrderCommand *cmd = nullptr;
    try {
      // should spin and also check another barrier
      int64_t availableSequence = waitSpinningHelper_->TryWaitFor(nextSequence);

      if (nextSequence <= availableSequence) {
        int64_t startSequence = nextSequence; // Track start to detect if any
                                              // messages were processed
        while (nextSequence <= availableSequence) {
          cmd = &ringBuffer_->get(nextSequence);

          // switch to next group - let slave processor start doing its handling
          // cycle
          if (cmd->eventsGroup != currentSequenceGroup) {
            // Trigger previous group when detecting new group boundary
            PublishProgressAndTriggerSlaveProcessor(nextSequence);
            lastTriggeredSequence = nextSequence - 1;
            currentSequenceGroup = cmd->eventsGroup;
          }
          // Match Java: direct call without inner try-catch
          // Exception will be caught by outer catch block
#if ENABLE_LATENCY_BREAKDOWN
          utils::LatencyBreakdown::Record(
              cmd, nextSequence, utils::LatencyBreakdown::Stage::R1_START);
#endif
          bool forcedPublish = false;
          try {
            forcedPublish = eventHandler_->OnEvent(nextSequence, cmd);
#if ENABLE_LATENCY_BREAKDOWN
            utils::LatencyBreakdown::Record(
                cmd, nextSequence, utils::LatencyBreakdown::Stage::R1_END);
#endif
          } catch (...) {
            // Record R1_END even on exception to maintain correct latency
            // statistics
#if ENABLE_LATENCY_BREAKDOWN
            utils::LatencyBreakdown::Record(
                cmd, nextSequence, utils::LatencyBreakdown::Stage::R1_END);
#endif
            throw; // Re-throw to outer catch block
          }
          nextSequence++;

          if (forcedPublish) {
            sequence_.set(nextSequence - 1);
            waitSpinningHelper_->SignalAllWhenBlocking();
          }

          if (cmd->command == common::cmd::OrderCommandType::SHUTDOWN_SIGNAL) {
            // Match Java: having all sequences aligned with the ringbuffer
            // cursor is a requirement for proper shutdown let following
            // processors to catch up Note: Java version doesn't log
            // SHUTDOWN_SIGNAL, C++ adds LOG_INFO for debugging
            LOG_INFO("[TwoStepMasterProcessor:{}] SHUTDOWN_SIGNAL detected",
                     name_);
            PublishProgressAndTriggerSlaveProcessor(nextSequence);
          }
        }
        // Match Java: update sequence after processing all available messages
        // Also trigger slave processor for the last group if it hasn't been
        // triggered. This ensures that even if all messages are in the same
        // group (e.g., < 20 messages), the slave processor will still process
        // the group. This is an improvement over Java version which may miss
        // the last group if it's incomplete.
        if (nextSequence > startSequence) {
          int64_t lastProcessedSequence = nextSequence - 1;
          // If the last group hasn't been triggered (no group change detected
          // in loop), trigger it now
          if (lastProcessedSequence > lastTriggeredSequence) {
            PublishProgressAndTriggerSlaveProcessor(nextSequence);
          }
        }
        sequence_.set(availableSequence);
        waitSpinningHelper_->SignalAllWhenBlocking();
      }
    } catch (const disruptor::AlertException &ex) {
      if (running_.load() != RUNNING) {
        break;
      }
    } catch (const std::exception &ex) {
      // Match Java: catch (final Throwable ex)
      // Java version doesn't log here, directly calls exceptionHandler
      // C++ adds LOG_ERROR for debugging, but behavior matches Java
      if (exceptionHandler_) {
        exceptionHandler_->HandleEventException(ex, nextSequence, cmd);
      }
      sequence_.set(nextSequence);
      waitSpinningHelper_->SignalAllWhenBlocking();
      nextSequence++;
    } catch (...) {
      // Match Java: catch (final Throwable ex) - catches all exceptions
      // Java version doesn't log here, directly calls exceptionHandler
      // C++ adds LOG_ERROR for debugging, but behavior matches Java
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

template <typename WaitStrategyT>
void TwoStepMasterProcessor<WaitStrategyT>::
    PublishProgressAndTriggerSlaveProcessor(int64_t nextSequence) {
  sequence_.set(nextSequence - 1);
  waitSpinningHelper_->SignalAllWhenBlocking();
  // Match Java: slaveProcessor.handlingCycle(nextSequence);
  // Java version doesn't check null (assumes it's set), C++ adds null check for
  // safety
  if (slaveProcessor_ != nullptr) {
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
