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

#include "DisruptorExceptionHandler.h"
#include "SimpleEventHandler.h"
#include "WaitSpinningHelper.h"
#include <disruptor/MultiProducerSequencer.h>
#include <disruptor/ProcessingSequenceBarrier.h>
#include <disruptor/RingBuffer.h>
#include <atomic>
#include <cstdint>
#include <string>

namespace exchange {
namespace core {
namespace processors {

/**
 * TwoStepSlaveProcessor - two-step processor (slave step)
 * Implements EventProcessor interface
 */
template <typename WaitStrategyT> class TwoStepSlaveProcessor {
public:
  TwoStepSlaveProcessor(
      disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand,
                                         WaitStrategyT> *ringBuffer,
      disruptor::ProcessingSequenceBarrier<
          disruptor::MultiProducerSequencer<WaitStrategyT>, WaitStrategyT>
          *sequenceBarrier,
      SimpleEventHandler *eventHandler,
      DisruptorExceptionHandler<common::cmd::OrderCommand> *exceptionHandler,
      const std::string &name);

  /**
   * Get sequence
   */
  disruptor::Sequence *GetSequence();

  /**
   * Halt processor
   */
  void Halt();

  /**
   * Check if running
   */
  bool IsRunning() const;

  /**
   * Run processor (main loop)
   */
  void Run();

  /**
   * Set next sequence to process (called by master)
   */
  void SetNextSequence(int64_t nextSequence);

private:
  static constexpr int32_t IDLE = 0;
  static constexpr int32_t HALTED = 1;
  static constexpr int32_t RUNNING = 2;

  std::atomic<int32_t> running_;
  void *ringBuffer_;      // Type-erased RingBuffer pointer
  void *sequenceBarrier_; // Type-erased SequenceBarrier pointer
  WaitSpinningHelper<common::cmd::OrderCommand, WaitStrategyT> *waitSpinningHelper_;
  SimpleEventHandler *eventHandler_;
  DisruptorExceptionHandler<common::cmd::OrderCommand> *exceptionHandler_;
  std::string name_;
  disruptor::Sequence *sequence_;
  int64_t nextSequence_;

  void ProcessEvents();
};

} // namespace processors
} // namespace core
} // namespace exchange
