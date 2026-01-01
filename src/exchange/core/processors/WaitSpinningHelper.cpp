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

namespace exchange {
namespace core {
namespace processors {

template <typename T, typename WaitStrategyT>
WaitSpinningHelper<T, WaitStrategyT>::WaitSpinningHelper(
    disruptor::MultiProducerRingBuffer<T, WaitStrategyT> *ringBuffer,
    disruptor::ProcessingSequenceBarrier<
        disruptor::MultiProducerSequencer<WaitStrategyT>, WaitStrategyT>
        *sequenceBarrier,
    int32_t spinLimit, common::CoreWaitStrategy waitStrategy,
    const std::string &name)
    : sequenceBarrier_(sequenceBarrier),
      sequencer_(&ringBuffer->getSequencer()), spinLimit_(spinLimit),
      yieldLimit_(ShouldYield(waitStrategy) ? spinLimit / 2 : 0),
      block_(ShouldBlock(waitStrategy)), blockingWaitStrategy_(nullptr),
      lock_(nullptr), processorNotifyCondition_(nullptr), name_(name) {
  // Matches Java: extract blocking wait strategy, lock, and condition variable
  // if blocking mode
  if (block_) {
    // Extract waitStrategy from sequencer (matches Java:
    // ReflectionUtils.extractField(AbstractSequencer.class, sequencer,
    // "waitStrategy"))
    auto &waitStrategy = sequencer_->getWaitStrategy();

    // Check if WaitStrategyT is BlockingWaitStrategy
    if constexpr (std::is_same_v<WaitStrategyT,
                                 disruptor::BlockingWaitStrategy>) {
      blockingWaitStrategy_ =
          &const_cast<disruptor::BlockingWaitStrategy &>(waitStrategy);
      // Extract lock and condition variable (matches Java:
      // ReflectionUtils.extractField(...))
      lock_ = &blockingWaitStrategy_->getMutex();
      processorNotifyCondition_ =
          &blockingWaitStrategy_->getConditionVariable();
    }
  }
}

template <typename T, typename WaitStrategyT>
int64_t WaitSpinningHelper<T, WaitStrategyT>::TryWaitFor(int64_t seq) {
  sequenceBarrier_->checkAlert();

  // Match Java: long spin = spinLimit;
  int64_t spin = spinLimit_;
  // Match Java: long availableSequence;
  int64_t availableSequence;

  // Match Java: while ((availableSequence = sequenceBarrier.getCursor()) < seq
  // && spin > 0)
  while ((availableSequence = sequenceBarrier_->getCursor()) < seq &&
         spin > 0) {
    // Match Java: if (spin < yieldLimit && spin > 1) { Thread.yield(); }
    // Note: In C++, we use CPU pause instead of yield for better performance
    // in low-latency scenarios. Pause reduces power consumption and cache
    // contention without triggering context switch (unlike yield).
    if (spin < yieldLimit_ && spin > 1) {
      // Use CPU pause instead of yield for lower latency
      // This is a C++ optimization: pause is ~1-5ns vs yield which may trigger
      // context switch (~1-10µs)
#if defined(__x86_64__) || defined(_M_X64)
      _mm_pause();
#elif defined(__aarch64__)
      __asm__ __volatile__("yield" ::: "memory");
#endif
    }
    // Match Java: else if (block) { ... }
    else if (block_) {
      // Match Java: lock.lock();
      std::unique_lock<std::mutex> lock(*lock_);
      try {
        // Match Java: sequenceBarrier.checkAlert();
        sequenceBarrier_->checkAlert();
        // Match Java: if (availableSequence == sequenceBarrier.getCursor()) {
        //     processorNotifyCondition.await();
        // }
        if (availableSequence == sequenceBarrier_->getCursor()) {
          processorNotifyCondition_->wait(lock);
        }
      } catch (...) {
        // Match Java: finally block ensures lock.unlock()
        // std::unique_lock automatically unlocks in destructor
        throw;
      }
      // std::unique_lock automatically unlocks here
    }

    // Match Java: spin--;
    spin--;
  }

  // Match Java: return (availableSequence < seq)
  //     ? availableSequence
  //     : sequencer.getHighestPublishedSequence(seq, availableSequence);
  if (availableSequence < seq) {
    return availableSequence;
  } else {
    return sequencer_->getHighestPublishedSequence(seq, availableSequence);
  }
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
template class WaitSpinningHelper<common::cmd::OrderCommand,
                                  disruptor::BlockingWaitStrategy>;
template class WaitSpinningHelper<common::cmd::OrderCommand,
                                  disruptor::YieldingWaitStrategy>;
template class WaitSpinningHelper<common::cmd::OrderCommand,
                                  disruptor::BusySpinWaitStrategy>;

} // namespace processors
} // namespace core
} // namespace exchange
