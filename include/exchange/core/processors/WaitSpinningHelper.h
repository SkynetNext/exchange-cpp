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

#pragma once

#include "../common/CoreWaitStrategy.h"
#include <condition_variable>
#include <cstdint>
#include <disruptor/BlockingWaitStrategy.h>
#include <disruptor/MultiProducerSequencer.h>
#include <disruptor/ProcessingSequenceBarrier.h>
#include <disruptor/RingBuffer.h>
#include <mutex>
#include <string>

namespace exchange {
namespace core {
namespace processors {

// Thread-safe logging mutex for all processors
extern std::mutex log_mutex;

} // namespace processors
} // namespace core
} // namespace exchange

namespace exchange {
namespace core {
namespace processors {

/**
 * WaitSpinningHelper - helper for spinning and waiting on sequence barriers
 */
template <typename T, typename WaitStrategyT> class WaitSpinningHelper {
public:
  WaitSpinningHelper(
      disruptor::MultiProducerRingBuffer<T, WaitStrategyT> *ringBuffer,
      disruptor::ProcessingSequenceBarrier<
          disruptor::MultiProducerSequencer<WaitStrategyT>, WaitStrategyT>
          *sequenceBarrier,
      int32_t spinLimit, common::CoreWaitStrategy waitStrategy,
      const std::string &name = "");

  /**
   * Try to wait for sequence, with spinning and potentially blocking
   */
  int64_t TryWaitFor(int64_t seq);

  /**
   * Signal all waiting threads when blocking
   */
  void SignalAllWhenBlocking();

private:
  disruptor::ProcessingSequenceBarrier<
      disruptor::MultiProducerSequencer<WaitStrategyT>, WaitStrategyT>
      *sequenceBarrier_;
  disruptor::MultiProducerSequencer<WaitStrategyT> *sequencer_;
  int32_t spinLimit_;
  int32_t yieldLimit_;
  bool block_;

  // For blocking mode - matches Java reflection access
  disruptor::BlockingWaitStrategy *blockingWaitStrategy_;
  std::mutex *lock_;
  std::condition_variable *processorNotifyCondition_;

  // Name for logging purposes
  std::string name_;
};

} // namespace processors
} // namespace core
} // namespace exchange
