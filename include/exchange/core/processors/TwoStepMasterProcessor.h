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

#pragma once

#include "../common/CoreWaitStrategy.h"
#include "DisruptorExceptionHandler.h"
#include "SimpleEventHandler.h"
#include "WaitSpinningHelper.h"
#include <exchange/core/utils/ProcessorMessageCounter.h>
#include <atomic>
#include <cstdint>
#include <disruptor/EventProcessor.h>
#include <disruptor/MultiProducerSequencer.h>
#include <disruptor/ProcessingSequenceBarrier.h>
#include <disruptor/RingBuffer.h>
#include <string>

namespace exchange {
namespace core {
namespace processors {

// Forward declaration
template <typename WaitStrategyT> class TwoStepSlaveProcessor;

/**
 * TwoStepMasterProcessor - two-step processor (master step)
 * Implements EventProcessor interface (matches Java version)
 */
template <typename WaitStrategyT>
class TwoStepMasterProcessor : public disruptor::EventProcessor {
public:
  TwoStepMasterProcessor(
      disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand,
                                         WaitStrategyT> *ringBuffer,
      disruptor::ProcessingSequenceBarrier<
          disruptor::MultiProducerSequencer<WaitStrategyT>, WaitStrategyT>
          *sequenceBarrier,
      SimpleEventHandler *eventHandler,
      DisruptorExceptionHandler<common::cmd::OrderCommand> *exceptionHandler,
      common::CoreWaitStrategy coreWaitStrategy, const std::string &name);

  // EventProcessor interface implementation
  disruptor::Sequence &getSequence() override;
  void halt() override;
  bool isRunning() override;
  void run() override;

  /**
   * Set slave processor
   */
  void SetSlaveProcessor(TwoStepSlaveProcessor<WaitStrategyT> *slaveProcessor);

private:
  static constexpr int32_t IDLE = 0;
  static constexpr int32_t HALTED = 1;
  static constexpr int32_t RUNNING = 2;
  static constexpr int32_t MASTER_SPIN_LIMIT = 5000;

  std::atomic<int32_t> running_;
  disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand, WaitStrategyT>
      *ringBuffer_;
  disruptor::ProcessingSequenceBarrier<
      disruptor::MultiProducerSequencer<WaitStrategyT>, WaitStrategyT>
      *sequenceBarrier_;
  WaitSpinningHelper<common::cmd::OrderCommand, WaitStrategyT>
      *waitSpinningHelper_;
  SimpleEventHandler *eventHandler_;
  DisruptorExceptionHandler<common::cmd::OrderCommand> *exceptionHandler_;
  std::string name_;
  utils::ProcessorType processorType_;
  int32_t processorId_;
  disruptor::Sequence sequence_; // Changed from pointer to value (matches Java)
  TwoStepSlaveProcessor<WaitStrategyT> *slaveProcessor_;

  void ProcessEvents();
  void PublishProgressAndTriggerSlaveProcessor(int64_t nextSequence);
};

} // namespace processors
} // namespace core
} // namespace exchange
