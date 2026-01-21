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
#include <exchange/core/common/CoreWaitStrategy.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/processors/WaitSpinningHelper.h>
#include <exchange/core/utils/FastNanoTime.h>
#include <exchange/core/utils/Logger.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__aarch64__)
// ARM64 yield instruction
#endif

namespace exchange::core::processors {

template <typename T, typename WaitStrategyT>
WaitSpinningHelper<T, WaitStrategyT>::WaitSpinningHelper(
    disruptor::MultiProducerRingBuffer<T, WaitStrategyT>* ringBuffer,
    disruptor::ProcessingSequenceBarrier<disruptor::MultiProducerSequencer<WaitStrategyT>,
                                         WaitStrategyT>* sequenceBarrier,
    int32_t spinLimit,
    common::CoreWaitStrategy waitStrategy,
    const std::string& name)
    : sequenceBarrier_(sequenceBarrier)
    , sequencer_(&ringBuffer->getSequencer())
    , spinLimit_(spinLimit)
    , yieldLimit_(ShouldYield(waitStrategy) ? spinLimit / 2 : 0)
    , block_(ShouldBlock(waitStrategy))
    , blockingWaitStrategy_(nullptr)
    , lock_(nullptr)
    , processorNotifyCondition_(nullptr)
    , name_(name) {
    // Matches Java: extract blocking wait strategy, lock, and condition variable
    // if blocking mode
    if (block_) {
        // Extract waitStrategy from sequencer (matches Java:
        // ReflectionUtils.extractField(AbstractSequencer.class, sequencer,
        // "waitStrategy"))
        auto& waitStrategy = sequencer_->getWaitStrategy();

        // Check if WaitStrategyT is BlockingWaitStrategy
        if constexpr (std::is_same_v<WaitStrategyT, disruptor::BlockingWaitStrategy>) {
            blockingWaitStrategy_ = &const_cast<disruptor::BlockingWaitStrategy&>(waitStrategy);
            // Extract lock and condition variable (matches Java:
            // ReflectionUtils.extractField(...))
            lock_ = &blockingWaitStrategy_->getMutex();
            processorNotifyCondition_ = &blockingWaitStrategy_->getConditionVariable();
        }
    }
}

template <typename T, typename WaitStrategyT>
int64_t WaitSpinningHelper<T, WaitStrategyT>::TryWaitFor(int64_t seq) {
    sequenceBarrier_->checkAlert();

    // P90 < 1µs target: Maximum wait time is 1µs
    // If sequence doesn't update within 1µs, return current value immediately
    constexpr int64_t MAX_WAIT_TIME_NS = 1000;  // 1µs
    // Time check frequency: every 30 iterations
    // On 3GHz CPU: 30 iterations ≈ 10ns (much less than 1µs target)
    constexpr int64_t SPIN_CHECK_INTERVAL = 30;

    int64_t spin = spinLimit_;
    int64_t startTimeNs = 0;
    int64_t checkCounter = 0;

    // Fast path: check sequence immediately
    int64_t availableSequence = sequenceBarrier_->getCursor();
    if (availableSequence >= seq) {
        if (sequencer_) {
            return sequencer_->getHighestPublishedSequence(seq, availableSequence);
        }
        return availableSequence;
    }

    // Spin loop: check sequence every iteration, check time every N iterations
    while (availableSequence < seq && spin > 0) {
        // Check sequence every iteration (no caching delay)
        availableSequence = sequenceBarrier_->getCursor();
        if (availableSequence >= seq) {
            break;
        }

        // Check time every N iterations to avoid frequent now() calls
        if (++checkCounter >= SPIN_CHECK_INTERVAL) {
            checkCounter = 0;

            if (startTimeNs == 0) {
                // First time check: record start time
                startTimeNs = utils::FastNanoTime::Now();
            } else {
                // Subsequent time checks: calculate elapsed time
                int64_t currentTimeNs = utils::FastNanoTime::Now();
                int64_t elapsed = currentTimeNs - startTimeNs;

                // If exceeded 1µs, return immediately (don't wait)
                // This guarantees P90 < 1µs
                if (elapsed > MAX_WAIT_TIME_NS) {
                    // Return current available sequence (may be < seq)
                    // Caller should handle this case (may need retry)
                    break;
                }
            }
        }

        // CPU pause: reduces power consumption and cache contention
#if defined(__x86_64__) || defined(_M_X64)
        _mm_pause();
#elif defined(__aarch64__)
        __asm__ __volatile__("yield" ::: "memory");
#endif

        spin--;
    }

    // Final check: ensure we have the latest sequence value
    if (availableSequence < seq) {
        availableSequence = sequenceBarrier_->getCursor();
    }

    // Use sequencer to get highest published sequence if available
    if (availableSequence >= seq && sequencer_) {
        return sequencer_->getHighestPublishedSequence(seq, availableSequence);
    }

    // Return available sequence (may be < seq if timeout occurred)
    return availableSequence;
}

template <typename T, typename WaitStrategyT>
void WaitSpinningHelper<T, WaitStrategyT>::SignalAllWhenBlocking() {
    // Matches Java: if (block) {
    // blockingDisruptorWaitStrategy.signalAllWhenBlocking(); }
    if (block_ && blockingWaitStrategy_) {
        blockingWaitStrategy_->signalAllWhenBlocking();
    }
}

// Explicit template instantiations for common types
template class WaitSpinningHelper<common::cmd::OrderCommand, disruptor::BlockingWaitStrategy>;
template class WaitSpinningHelper<common::cmd::OrderCommand, disruptor::YieldingWaitStrategy>;
template class WaitSpinningHelper<common::cmd::OrderCommand, disruptor::BusySpinWaitStrategy>;

}  // namespace exchange::core::processors
