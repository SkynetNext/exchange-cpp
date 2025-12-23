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

#include "ExchangeApi.h"
#include "common/cmd/OrderCommand.h"
#include "common/config/ExchangeConfiguration.h"
#include <cstdint>
#include <functional>
#include <memory>

// Forward declarations
// Note: Disruptor and RingBuffer are forward declared in other headers
// with different template parameters, so we don't redeclare them here
namespace disruptor::dsl {
enum class ProducerType;
}

namespace exchange {
namespace core {
// Forward declaration - full definition needed in .cpp
namespace processors {
class GroupingProcessor;
template <typename WaitStrategyT> class TwoStepMasterProcessor;
template <typename WaitStrategyT> class TwoStepSlaveProcessor;
class EventProcessorAdapter;
class SimpleEventHandler;
namespace journaling {
class ISerializationProcessor;
}
} // namespace processors

/**
 * ExchangeCore - main exchange core class
 * Builds configuration and starts disruptor pipeline
 */
class ExchangeCore {
public:
  using ResultsConsumer =
      std::function<void(common::cmd::OrderCommand *, int64_t)>;

  ExchangeCore(
      ResultsConsumer resultsConsumer,
      const common::config::ExchangeConfiguration *exchangeConfiguration);

  ~ExchangeCore();

  /**
   * Startup - start the disruptor and replay journal
   */
  void Startup();

  /**
   * Shutdown - stop the disruptor
   */
  void Shutdown(int64_t timeoutMs = -1);

  /**
   * Get ExchangeApi instance
   */
  ExchangeApi<disruptor::BlockingWaitStrategy> *GetApi() { return api_.get(); }

private:
  // Type-erased Disruptor pointer (actual type depends on WaitStrategy)
  // Using BlockingWaitStrategy as default
  void *disruptor_;
  void *ringBuffer_; // Type-erased RingBuffer pointer
  std::unique_ptr<ExchangeApi<disruptor::BlockingWaitStrategy>> api_;
  processors::journaling::ISerializationProcessor *serializationProcessor_;
  const common::config::ExchangeConfiguration *exchangeConfiguration_;

  // Store processors for lifecycle management
  std::vector<std::unique_ptr<processors::GroupingProcessor>>
      groupingProcessors_;
  std::vector<std::unique_ptr<
      processors::TwoStepMasterProcessor<disruptor::BlockingWaitStrategy>>>
      r1Processors_;
  std::vector<std::unique_ptr<
      processors::TwoStepSlaveProcessor<disruptor::BlockingWaitStrategy>>>
      r2Processors_;
  std::vector<std::unique_ptr<processors::EventProcessorAdapter>>
      processorAdapters_;
  std::vector<std::unique_ptr<processors::SimpleEventHandler>> riskHandlers_;

  bool started_ = false;
  bool stopped_ = false;

  static constexpr bool EVENTS_POOLING = false;
};

} // namespace core
} // namespace exchange
