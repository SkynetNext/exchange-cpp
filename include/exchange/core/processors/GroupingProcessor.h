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
#include "../common/config/PerformanceConfiguration.h"
#include "SharedPool.h"
#include <atomic>
#include <cstdint>
#include <disruptor/EventProcessor.h>
#include <disruptor/MultiProducerSequencer.h>
#include <disruptor/ProcessingSequenceBarrier.h>
#include <disruptor/RingBuffer.h>

namespace exchange {
namespace core {
namespace processors {

/**
 * GroupingProcessor - groups small orders and identifies cancel-replace
 * patterns Implements EventProcessor interface (matches Java version)
 */
class GroupingProcessor : public disruptor::EventProcessor {
public:
  template <typename WaitStrategyT>
  GroupingProcessor(
      disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand,
                                         WaitStrategyT> *ringBuffer,
      disruptor::ProcessingSequenceBarrier<
          disruptor::MultiProducerSequencer<WaitStrategyT>, WaitStrategyT>
          *sequenceBarrier,
      const common::config::PerformanceConfiguration *perfCfg,
      common::CoreWaitStrategy coreWaitStrategy, SharedPool *sharedPool);

  // EventProcessor interface implementation
  disruptor::Sequence &getSequence() override;
  void halt() override;
  bool isRunning() override;
  void run() override;

private:
  static constexpr int32_t IDLE = 0;
  static constexpr int32_t HALTED = 1;
  static constexpr int32_t RUNNING = 2;
  static constexpr int32_t GROUP_SPIN_LIMIT = 1000;
  static constexpr int64_t L2_PUBLISH_INTERVAL_NS = 10'000'000;

  std::atomic<int32_t> running_;
  void *ringBuffer_;         // Type-erased RingBuffer pointer
  void *sequenceBarrier_;    // Type-erased SequenceBarrier pointer
  void *waitSpinningHelper_; // Type-erased WaitSpinningHelper pointer
  disruptor::Sequence sequence_;  // Changed from pointer to value (matches Java)
  SharedPool *sharedPool_;
  int32_t msgsInGroupLimit_;
  int64_t maxGroupDurationNs_;

  void ProcessEvents();
};

} // namespace processors
} // namespace core
} // namespace exchange
