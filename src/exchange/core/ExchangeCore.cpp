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

#include <disruptor/BlockingWaitStrategy.h>
#include <disruptor/BusySpinWaitStrategy.h>
#include <disruptor/EventFactory.h>
#include <disruptor/EventHandler.h>
#include <disruptor/EventTranslator.h>
#include <disruptor/YieldingWaitStrategy.h>
#include <disruptor/dsl/Disruptor.h>
#include <disruptor/dsl/ProducerType.h>
#include <disruptor/dsl/ThreadFactory.h>
#include <exchange/core/ExchangeApi.h>
#include <exchange/core/ExchangeCore.h>
#include <exchange/core/common/CoreWaitStrategy.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <exchange/core/common/config/ExchangeConfiguration.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <exchange/core/common/config/SerializationConfiguration.h>
#include <exchange/core/processors/DisruptorExceptionHandler.h>
#include <exchange/core/processors/GroupingProcessor.h>
#include <exchange/core/processors/MatchingEngineRouter.h>
#include <exchange/core/processors/ResultsHandler.h>
#include <exchange/core/processors/RiskEngine.h>
#include <exchange/core/processors/SharedPool.h>
#include <exchange/core/processors/SimpleEventHandler.h>
#include <exchange/core/processors/TwoStepMasterProcessor.h>
#include <exchange/core/processors/TwoStepSlaveProcessor.h>
#include <exchange/core/processors/journaling/DummySerializationProcessor.h>
#include <exchange/core/processors/journaling/ISerializationProcessor.h>
#include <exchange/core/utils/AffinityThreadFactory.h>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace exchange {
namespace core {

// OrderCommand EventFactory
class OrderCommandEventFactory
    : public disruptor::EventFactory<common::cmd::OrderCommand> {
public:
  common::cmd::OrderCommand newInstance() override {
    return common::cmd::OrderCommand{};
  }
};

// Adapter from AffinityThreadFactory to disruptor::dsl::ThreadFactory
class ThreadFactoryAdapter : public disruptor::dsl::ThreadFactory {
public:
  explicit ThreadFactoryAdapter(utils::AffinityThreadFactory *affinityFactory)
      : affinityFactory_(affinityFactory) {}

  std::thread newThread(std::function<void()> r) override {
    if (affinityFactory_) {
      auto thread = affinityFactory_->NewThread(r);
      return std::move(*thread);
    } else {
      return std::thread(r);
    }
  }

private:
  utils::AffinityThreadFactory *affinityFactory_;
};

// Helper to get WaitStrategy based on CoreWaitStrategy
// Note: This is simplified - we always use BlockingWaitStrategy for now
// TODO: Support dynamic WaitStrategy selection
disruptor::BlockingWaitStrategy *
GetWaitStrategy(common::CoreWaitStrategy strategy) {
  static disruptor::BlockingWaitStrategy blocking;
  // For now, always return BlockingWaitStrategy
  // TODO: Support other strategies
  return &blocking;
}

// Shutdown signal translator
class ShutdownSignalTranslator
    : public disruptor::EventTranslator<common::cmd::OrderCommand> {
public:
  void translateTo(common::cmd::OrderCommand &cmd, int64_t seq) override {
    cmd.command = common::cmd::OrderCommandType::SHUTDOWN_SIGNAL;
    cmd.resultCode = common::cmd::CommandResultCode::NEW;
  }
};

// Simplified implementation using BlockingWaitStrategy
// TODO: Support dynamic WaitStrategy selection
ExchangeCore::ExchangeCore(
    ResultsConsumer resultsConsumer,
    const common::config::ExchangeConfiguration *exchangeConfiguration)
    : exchangeConfiguration_(exchangeConfiguration), started_(false),
      stopped_(false) {

  const auto &perfCfg = exchangeConfiguration_->performanceCfg;
  const auto &serializationCfg = exchangeConfiguration_->serializationCfg;

  const int ringBufferSize = perfCfg.ringBufferSize;
  const int matchingEnginesNum = perfCfg.matchingEnginesNum;
  const int riskEnginesNum = perfCfg.riskEnginesNum;

  // Create EventFactory
  auto eventFactory = std::make_shared<OrderCommandEventFactory>();

  // Create ThreadFactory adapter
  // TODO: Get actual ThreadFactory from perfCfg
  ThreadFactoryAdapter threadFactoryAdapter(nullptr);

  // Get WaitStrategy (using BlockingWaitStrategy for now)
  // TODO: Support dynamic WaitStrategy based on perfCfg.waitStrategy
  auto *waitStrategyPtr = GetWaitStrategy(perfCfg.waitStrategy);

  // Create Disruptor (MULTI producer)
  using DisruptorType =
      disruptor::dsl::Disruptor<common::cmd::OrderCommand,
                                disruptor::dsl::ProducerType::MULTI,
                                disruptor::BlockingWaitStrategy>;
  auto disruptor = std::make_unique<DisruptorType>(
      eventFactory, ringBufferSize, threadFactoryAdapter, *waitStrategyPtr);

  // Get RingBuffer
  auto *ringBuffer = &disruptor->getRingBuffer();
  ringBuffer_ = ringBuffer;
  disruptor_ = disruptor.release(); // Store as void* for now

  // Create ExchangeApi (using BlockingWaitStrategy for now)
  // TODO: Support dynamic WaitStrategy
  api_ = std::make_unique<ExchangeApi<disruptor::BlockingWaitStrategy>>(
      static_cast<disruptor::MultiProducerRingBuffer<
          common::cmd::OrderCommand, disruptor::BlockingWaitStrategy> *>(
          ringBuffer));

  // Create serialization processor
  // TODO: Use factory from serializationCfg
  auto *dummyProcessor =
      processors::journaling::DummySerializationProcessor::Instance();
  serializationProcessor_ =
      static_cast<processors::journaling::ISerializationProcessor *>(
          dummyProcessor);

  // Create SharedPool
  const int poolInitialSize = (matchingEnginesNum + riskEnginesNum) * 8;
  const int chainLength = EVENTS_POOLING ? 1024 : 1;
  auto sharedPool = std::make_unique<processors::SharedPool>(
      poolInitialSize * 4, poolInitialSize, chainLength);

  // Create exception handler
  // Note: DisruptorExceptionHandler needs to be adapted to
  // disruptor::ExceptionHandler For now, we'll use a simplified approach
  auto exceptionHandler = std::make_unique<
      processors::DisruptorExceptionHandler<common::cmd::OrderCommand>>(
      "main", [this, ringBuffer](const std::exception &ex, int64_t seq) {
        // TODO: Log exception
        // Publish shutdown signal
        static ShutdownSignalTranslator shutdownTranslator;
        ringBuffer->publishEvent(shutdownTranslator);
        // Shutdown disruptor
        auto *disruptor = static_cast<DisruptorType *>(disruptor_);
        if (disruptor) {
          disruptor->shutdown();
        }
      });

  // TODO: Properly adapt DisruptorExceptionHandler to
  // disruptor::ExceptionHandler For now, we'll skip setDefaultExceptionHandler
  // auto *disruptor = static_cast<DisruptorType *>(disruptor_);
  // disruptor->setDefaultExceptionHandler(*exceptionHandler);

  // Create MatchingEngineRouter instances
  std::vector<std::unique_ptr<processors::MatchingEngineRouter>>
      matchingEngines;
  matchingEngines.reserve(matchingEnginesNum);
  for (int32_t shardId = 0; shardId < matchingEnginesNum; shardId++) {
    auto matchingEngine = std::make_unique<processors::MatchingEngineRouter>(
        shardId, matchingEnginesNum, perfCfg.orderBookFactory, sharedPool.get(),
        exchangeConfiguration, serializationProcessor_, nullptr);
    matchingEngines.push_back(std::move(matchingEngine));
  }

  // Create RiskEngine instances
  std::vector<std::unique_ptr<processors::RiskEngine>> riskEngines;
  riskEngines.reserve(riskEnginesNum);
  for (int32_t shardId = 0; shardId < riskEnginesNum; shardId++) {
    auto riskEngine = std::make_unique<processors::RiskEngine>(
        shardId, riskEnginesNum, serializationProcessor_, sharedPool.get(),
        exchangeConfiguration);
    riskEngines.push_back(std::move(riskEngine));
  }

  // Create event handlers for matching engines
  // Note: EventHandler is abstract, so we need concrete implementations
  class MatchingEngineEventHandler
      : public disruptor::EventHandler<common::cmd::OrderCommand> {
  public:
    MatchingEngineEventHandler(processors::MatchingEngineRouter *matchingEngine)
        : matchingEngine_(matchingEngine) {}

    void onEvent(common::cmd::OrderCommand &cmd, int64_t sequence,
                 bool endOfBatch) override {
      matchingEngine_->ProcessOrder(sequence, &cmd);
    }

  private:
    processors::MatchingEngineRouter *matchingEngine_;
  };

  std::vector<std::unique_ptr<MatchingEngineEventHandler>>
      matchingEngineHandlers;
  matchingEngineHandlers.reserve(matchingEnginesNum);
  for (size_t i = 0; i < matchingEngines.size(); i++) {
    matchingEngineHandlers.push_back(
        std::make_unique<MatchingEngineEventHandler>(matchingEngines[i].get()));
  }

  // Configure Disruptor pipeline:
  // 1. GroupingProcessor (G)
  // 2. Risk Pre-hold (R1) + Matching Engine (ME) + [Journaling (J)]
  // 3. Risk Release (R2)
  // 4. ResultsHandler (E)

  // Configure Disruptor pipeline:
  // 1. GroupingProcessor (G)
  // 2. Risk Pre-hold (R1) + Matching Engine (ME) + [Journaling (J)]
  // 3. Risk Release (R2)
  // 4. ResultsHandler (E)

  // TODO: Implement full pipeline configuration using Disruptor DSL
  // The processors (GroupingProcessor, TwoStepMasterProcessor, etc.) need
  // proper RingBuffer and SequenceBarrier types that match the Disruptor
  // template parameters. This requires careful type matching.

  // For now, we create the basic structure. Full pipeline integration will
  // require:
  // - Proper type matching for RingBuffer and SequenceBarrier
  // - EventHandlerGroup API usage to chain processors
  // - Proper lifecycle management of all processors

  // Store processors for lifecycle management
  // TODO: Store all processors for proper cleanup
}

ExchangeCore::~ExchangeCore() {
  if (!stopped_) {
    Shutdown();
  }

  // Clean up disruptor
  if (disruptor_) {
    using DisruptorType =
        disruptor::dsl::Disruptor<common::cmd::OrderCommand,
                                  disruptor::dsl::ProducerType::MULTI,
                                  disruptor::BlockingWaitStrategy>;
    delete static_cast<DisruptorType *>(disruptor_);
    disruptor_ = nullptr;
  }
}

void ExchangeCore::Startup() {
  if (started_) {
    return;
  }

  // Start Disruptor
  if (disruptor_) {
    using DisruptorType =
        disruptor::dsl::Disruptor<common::cmd::OrderCommand,
                                  disruptor::dsl::ProducerType::MULTI,
                                  disruptor::BlockingWaitStrategy>;
    auto *disruptor = static_cast<DisruptorType *>(disruptor_);
    disruptor->start();
    started_ = true;

    // Replay journal if enabled
    if (serializationProcessor_ != nullptr) {
      serializationProcessor_->ReplayJournalFullAndThenEnableJouraling(
          &exchangeConfiguration_->initStateCfg,
          static_cast<void *>(api_.get()));
    }
  }
}

void ExchangeCore::Shutdown(int64_t timeoutMs) {
  if (stopped_) {
    return;
  }

  stopped_ = true;

  try {
    using DisruptorType =
        disruptor::dsl::Disruptor<common::cmd::OrderCommand,
                                  disruptor::dsl::ProducerType::MULTI,
                                  disruptor::BlockingWaitStrategy>;
    auto *disruptor = static_cast<DisruptorType *>(disruptor_);
    auto *ringBuffer = static_cast<disruptor::MultiProducerRingBuffer<
        common::cmd::OrderCommand, disruptor::BlockingWaitStrategy> *>(
        ringBuffer_);

    // Publish shutdown signal
    static ShutdownSignalTranslator shutdownTranslator;
    ringBuffer->publishEvent(shutdownTranslator);

    // Shutdown disruptor
    if (disruptor) {
      if (timeoutMs < 0) {
        disruptor->shutdown();
      } else {
        disruptor->shutdown(timeoutMs);
      }
    }
  } catch (const disruptor::TimeoutException &e) {
    throw std::runtime_error(
        "Could not stop disruptor gracefully. Not all events may be "
        "executed.");
  }
}

} // namespace core
} // namespace exchange
