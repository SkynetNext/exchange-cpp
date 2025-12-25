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
#include <exchange/core/common/CoreWaitStrategy.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/processors/WaitSpinningHelper.h>
#include <exchange/core/utils/Logger.h>
#include <thread>

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

  int64_t spin = spinLimit_;
  int64_t availableSequence;

  // Matches Java: use getCursor() + spin, then blocking if needed
  int64_t initialCursor = sequenceBarrier_->getCursor();
  while ((availableSequence = sequenceBarrier_->getCursor()) < seq &&
         spin > 0) {
    if (spin < yieldLimit_ && spin > 1) {
      std::this_thread::yield();
    } else if (block_ && lock_ && processorNotifyCondition_) {
      // Matches Java: lock.lock(); try { ... processorNotifyCondition.await();
      // } finally { lock.unlock(); }
      std::unique_lock<std::mutex> uniqueLock(*lock_);
      try {
        sequenceBarrier_->checkAlert();
        // lock only if sequence barrier did not progressed since last check
        if (availableSequence == sequenceBarrier_->getCursor()) {
          processorNotifyCondition_->wait(uniqueLock);
        }
      } catch (...) {
        // uniqueLock will automatically unlock in destructor
        throw;
      }
      // uniqueLock automatically unlocks here
    }
    spin--;
  }

  // Use sequencer to get highest published sequence if available
  // Matches Java: return (availableSequence < seq) ? availableSequence :
  // sequencer.getHighestPublishedSequence(seq, availableSequence);
  if (availableSequence < seq) {
    return availableSequence;
  }

  if (sequencer_) {
    return sequencer_->getHighestPublishedSequence(seq, availableSequence);
  }
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
template class WaitSpinningHelper<common::cmd::OrderCommand,
                                  disruptor::BlockingWaitStrategy>;
template class WaitSpinningHelper<common::cmd::OrderCommand,
                                  disruptor::YieldingWaitStrategy>;
template class WaitSpinningHelper<common::cmd::OrderCommand,
                                  disruptor::BusySpinWaitStrategy>;

} // namespace processors
} // namespace core
} // namespace exchange
