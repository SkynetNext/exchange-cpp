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

#include <disruptor/EventProcessor.h>
#include <disruptor/MultiProducerSequencer.h>
#include <disruptor/ProcessingSequenceBarrier.h>
#include <disruptor/RingBuffer.h>
#include <atomic>
#include <cstdint>
#include <string>
#include "DisruptorExceptionHandler.h"
#include "SimpleEventHandler.h"
#include "WaitSpinningHelper.h"

namespace exchange::core::processors {

/**
 * TwoStepSlaveProcessor - two-step processor (slave step)
 * Implements EventProcessor interface (matches Java version)
 */
template <typename WaitStrategyT>
class TwoStepSlaveProcessor : public disruptor::EventProcessor {
 public:
    TwoStepSlaveProcessor(
        disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand, WaitStrategyT>* ringBuffer,
        disruptor::ProcessingSequenceBarrier<disruptor::MultiProducerSequencer<WaitStrategyT>,
                                             WaitStrategyT>* sequenceBarrier,
        SimpleEventHandler* eventHandler,
        DisruptorExceptionHandler<common::cmd::OrderCommand>* exceptionHandler,
        const std::string& name);

    // EventProcessor interface implementation
    disruptor::Sequence& getSequence() override;
    void halt() override;
    bool isRunning() override;
    void run() override;

    /**
     * Handling cycle (matches Java public handlingCycle method)
     * Called by master processor to trigger slave processing
     * In Java, handlingCycle uses member variable nextSequence (set in run()).
     * The parameter processUpToSequence is the upper bound for processing.
     */
    void HandlingCycle(int64_t processUpToSequence);

 private:
    static constexpr int32_t IDLE = 0;
    static constexpr int32_t HALTED = 1;
    static constexpr int32_t RUNNING = 2;

    std::atomic<int32_t> running_;
    disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand, WaitStrategyT>* ringBuffer_;
    disruptor::ProcessingSequenceBarrier<disruptor::MultiProducerSequencer<WaitStrategyT>,
                                         WaitStrategyT>* sequenceBarrier_;
    WaitSpinningHelper<common::cmd::OrderCommand, WaitStrategyT>* waitSpinningHelper_;
    SimpleEventHandler* eventHandler_;
    DisruptorExceptionHandler<common::cmd::OrderCommand>* exceptionHandler_;
    std::string name_;
    disruptor::Sequence sequence_;  // Changed from pointer to value (matches Java)
    int64_t nextSequence_;

    void ProcessEvents();
};

}  // namespace exchange::core::processors
