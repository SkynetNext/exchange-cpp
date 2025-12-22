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
    int32_t spinLimit, common::CoreWaitStrategy waitStrategy)
    : sequenceBarrier_(sequenceBarrier),
      sequencer_(ExtractSequencer(ringBuffer)), spinLimit_(spinLimit),
      yieldLimit_(ShouldYield(waitStrategy) ? spinLimit / 2 : 0),
      block_(ShouldBlock(waitStrategy)), blockingWaitStrategy_(nullptr),
      lock_(nullptr), processorNotifyCondition_(nullptr) {
  // TODO: Extract blocking wait strategy and lock if needed for blocking mode
  // For now, simplified implementation without reflection
}

template <typename T, typename WaitStrategyT>
int64_t WaitSpinningHelper<T, WaitStrategyT>::TryWaitFor(int64_t seq) {
  sequenceBarrier_->checkAlert();

  int64_t spin = spinLimit_;
  int64_t availableSequence;
  while ((availableSequence = sequenceBarrier_->getCursor()) < seq &&
         spin > 0) {
    if (spin < yieldLimit_ && spin > 1) {
      std::this_thread::yield();
    } else if (block_) {
      // TODO: Implement blocking wait with lock
      // For now, simplified - just yield
      std::this_thread::yield();
    }
    spin--;
  }

  // Use sequencer to get highest published sequence if available
  if (sequencer_ && availableSequence >= seq) {
    return sequencer_->getHighestPublishedSequence(seq, availableSequence);
  }

  return availableSequence;
}

template <typename T, typename WaitStrategyT>
void WaitSpinningHelper<T, WaitStrategyT>::SignalAllWhenBlocking() {
  if (block_ && blockingWaitStrategy_) {
    // TODO: Call signalAllWhenBlocking on blocking wait strategy
    // For now, simplified
  }
}

template <typename T, typename WaitStrategyT>
disruptor::MultiProducerSequencer<WaitStrategyT> *
WaitSpinningHelper<T, WaitStrategyT>::ExtractSequencer(
    disruptor::MultiProducerRingBuffer<T, WaitStrategyT> *ringBuffer) {
  // In C++, we can't use reflection like Java
  // We need to access the sequencer through RingBuffer's internal structure
  // Since RingBuffer's sequencer() returns a reference, we can get its address
  // However, this is not safe if the sequencer is stored in an optional or
  // unique_ptr For now, we'll need to modify RingBuffer to provide sequencer
  // access or use a friend class relationship
  // TODO: Add getSequencer() method to RingBuffer or use friend classes
  // For now, we'll try to get it from the ringBuffer's internal structure
  // This is a workaround - in production, we should add proper accessor methods
  return nullptr; // TODO: Implement proper sequencer extraction
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
