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
#include <exchange/core/processors/TwoStepSlaveProcessor.h>
#include <exchange/core/processors/WaitSpinningHelper.h>
#include <exchange/core/utils/Logger.h>

namespace exchange::core::processors {

template <typename WaitStrategyT>
TwoStepSlaveProcessor<WaitStrategyT>::TwoStepSlaveProcessor(
    disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand, WaitStrategyT>* ringBuffer,
    disruptor::ProcessingSequenceBarrier<disruptor::MultiProducerSequencer<WaitStrategyT>,
                                         WaitStrategyT>* sequenceBarrier,
    SimpleEventHandler* eventHandler,
    DisruptorExceptionHandler<common::cmd::OrderCommand>* exceptionHandler,
    const std::string& name)
    : running_(IDLE)
    , ringBuffer_(ringBuffer)
    , sequenceBarrier_(sequenceBarrier)
    , waitSpinningHelper_(new WaitSpinningHelper<common::cmd::OrderCommand, WaitStrategyT>(
          ringBuffer,
          sequenceBarrier,
          0,
          common::CoreWaitStrategy::SECOND_STEP_NO_WAIT,
          name))
    , eventHandler_(eventHandler)
    , exceptionHandler_(exceptionHandler)
    , name_(name)
    , sequence_(disruptor::Sequence::INITIAL_VALUE)
    , nextSequence_(-1) {}

template <typename WaitStrategyT>
disruptor::Sequence& TwoStepSlaveProcessor<WaitStrategyT>::getSequence() {
    return sequence_;
}

template <typename WaitStrategyT>
void TwoStepSlaveProcessor<WaitStrategyT>::halt() {
    running_.store(HALTED);
    sequenceBarrier_->alert();
}

template <typename WaitStrategyT>
bool TwoStepSlaveProcessor<WaitStrategyT>::isRunning() {
    return running_.load() != IDLE;
}

template <typename WaitStrategyT>
void TwoStepSlaveProcessor<WaitStrategyT>::run() {
    if (running_.compare_exchange_strong(const_cast<int32_t&>(IDLE), RUNNING)) {
        sequenceBarrier_->clearAlert();
    } else if (running_.load() == RUNNING) {
        throw std::runtime_error("Thread is already running (S)");
    }

    nextSequence_ = sequence_.get() + 1L;
}

template <typename WaitStrategyT>
void TwoStepSlaveProcessor<WaitStrategyT>::HandlingCycle(int64_t processUpToSequence) {
    while (true) {
        common::cmd::OrderCommand* event = nullptr;
        try {
            int64_t availableSequence = waitSpinningHelper_->TryWaitFor(nextSequence_);

            // process batch
            while (nextSequence_ <= availableSequence && nextSequence_ < processUpToSequence) {
                event = &ringBuffer_->get(nextSequence_);
                eventHandler_->OnEvent(nextSequence_, event);
                nextSequence_++;
            }

            // exit if finished processing entire group (up to specified sequence)
            if (nextSequence_ == processUpToSequence) {
                // Match Java: update sequence after processing all messages in the
                // group
                sequence_.set(processUpToSequence - 1);
                waitSpinningHelper_->SignalAllWhenBlocking();
                return;
            }

        } catch (const std::exception& ex) {
            if (exceptionHandler_) {
                exceptionHandler_->HandleEventException(ex, nextSequence_, event);
            }
            sequence_.set(nextSequence_);
            waitSpinningHelper_->SignalAllWhenBlocking();
            nextSequence_++;
        } catch (...) {
            if (exceptionHandler_) {
                exceptionHandler_->HandleEventException(
                    std::runtime_error("Unknown exception"), nextSequence_, event);
            }
            sequence_.set(nextSequence_);
            waitSpinningHelper_->SignalAllWhenBlocking();
            nextSequence_++;
        }
    }
}

template <typename WaitStrategyT>
void TwoStepSlaveProcessor<WaitStrategyT>::ProcessEvents() {
    // This method is not used in two-step processor pattern
    // The actual processing is done in HandlingCycle
}

// Explicit template instantiations
template class TwoStepSlaveProcessor<disruptor::BlockingWaitStrategy>;
template class TwoStepSlaveProcessor<disruptor::YieldingWaitStrategy>;
template class TwoStepSlaveProcessor<disruptor::BusySpinWaitStrategy>;

}  // namespace exchange::core::processors
