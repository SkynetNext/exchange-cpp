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
#include <disruptor/dsl/EventProcessorFactory.h>
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
#include <exchange/core/processors/EventProcessorAdapter.h>
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

  using WaitStrategyT = disruptor::BlockingWaitStrategy;
  using RingBufferT =
      disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand,
                                         WaitStrategyT>;
  using DisruptorT = DisruptorType;

  auto *disruptorPtr = static_cast<DisruptorT *>(disruptor_);
  bool enableJournaling = serializationCfg.enableJournaling;

  // Create SimpleEventHandler wrappers for RiskEngine methods
  class RiskPreProcessHandler : public processors::SimpleEventHandler {
  public:
    explicit RiskPreProcessHandler(processors::RiskEngine *riskEngine)
        : riskEngine_(riskEngine) {}
    bool OnEvent(int64_t seq, common::cmd::OrderCommand *event) override {
      return riskEngine_->PreProcessCommand(seq, event);
    }

  private:
    processors::RiskEngine *riskEngine_;
  };

  class RiskPostProcessHandler : public processors::SimpleEventHandler {
  public:
    explicit RiskPostProcessHandler(processors::RiskEngine *riskEngine)
        : riskEngine_(riskEngine) {}
    bool OnEvent(int64_t seq, common::cmd::OrderCommand *event) override {
      riskEngine_->PostProcessCommand(seq, event);
      return false;
    }

  private:
    processors::RiskEngine *riskEngine_;
  };

  // Create EventProcessorFactory for GroupingProcessor
  class GroupingProcessorFactory
      : public disruptor::dsl::EventProcessorFactory<common::cmd::OrderCommand,
                                                     RingBufferT> {
  public:
    GroupingProcessorFactory(
        const common::config::PerformanceConfiguration *perfCfg,
        common::CoreWaitStrategy coreWaitStrategy,
        processors::SharedPool *sharedPool)
        : perfCfg_(perfCfg), coreWaitStrategy_(coreWaitStrategy),
          sharedPool_(sharedPool) {}

    disruptor::EventProcessor &
    createEventProcessor(RingBufferT &ringBuffer,
                         disruptor::Sequence *const *barrierSequences,
                         int count) override {
      auto barrier = ringBuffer.newBarrier(barrierSequences, count);
      auto *processor = new processors::GroupingProcessor(
          &ringBuffer, barrier.get(), perfCfg_, coreWaitStrategy_, sharedPool_);
      // Store barrier and processor for lifecycle management
      ownedBarriers_.push_back(barrier);
      ownedProcessors_.push_back(
          std::unique_ptr<processors::GroupingProcessor>(processor));
      // Create adapter
      auto *adapter = new processors::EventProcessorAdapter(
          processor->GetSequence(), [processor]() { processor->Run(); },
          [processor]() { processor->Halt(); },
          [processor]() { return processor->IsRunning(); });
      ownedAdapters_.push_back(
          std::unique_ptr<processors::EventProcessorAdapter>(adapter));
      return *adapter;
    }

  private:
    const common::config::PerformanceConfiguration *perfCfg_;
    common::CoreWaitStrategy coreWaitStrategy_;
    processors::SharedPool *sharedPool_;
    using BarrierType = std::shared_ptr<disruptor::ProcessingSequenceBarrier<
        disruptor::MultiProducerSequencer<WaitStrategyT>, WaitStrategyT>>;
    std::vector<BarrierType> ownedBarriers_;
    std::vector<std::unique_ptr<processors::GroupingProcessor>>
        ownedProcessors_;
    std::vector<std::unique_ptr<processors::EventProcessorAdapter>>
        ownedAdapters_;
  };

  // 1. Create GroupingProcessor (G)
  GroupingProcessorFactory groupingFactory(&perfCfg, perfCfg.waitStrategy,
                                           sharedPool.get());
  auto afterGrouping = disruptorPtr->handleEventsWith(groupingFactory);

  // 2. [journaling (J)] in parallel with risk hold (R1) + matching engine (ME)
  disruptor::EventHandler<common::cmd::OrderCommand> *journalHandler = nullptr;
  if (enableJournaling) {
    class JournalingEventHandler
        : public disruptor::EventHandler<common::cmd::OrderCommand> {
    public:
      explicit JournalingEventHandler(
          processors::journaling::ISerializationProcessor *processor)
          : processor_(processor) {}
      void onEvent(common::cmd::OrderCommand &cmd, int64_t sequence,
                   bool endOfBatch) override {
        processor_->WriteToJournal(&cmd, sequence, endOfBatch);
      }

    private:
      processors::journaling::ISerializationProcessor *processor_;
    };
    static JournalingEventHandler journalHandlerInstance(
        serializationProcessor_);
    journalHandler = &journalHandlerInstance;
    afterGrouping.handleEventsWith(*journalHandler);
  }

  // Create R1 processors (TwoStepMasterProcessor for each RiskEngine)
  std::vector<disruptor::EventProcessor *> procR1Processors;
  procR1Processors.reserve(riskEnginesNum);
  std::vector<std::unique_ptr<RiskPreProcessHandler>> r1Handlers;
  r1Handlers.reserve(riskEnginesNum);

  for (size_t i = 0; i < riskEngines.size(); i++) {
    // Create SimpleEventHandler for PreProcessCommand
    auto r1Handler =
        std::make_unique<RiskPreProcessHandler>(riskEngines[i].get());
    r1Handlers.push_back(std::move(r1Handler));

    // Create EventProcessorFactory for TwoStepMasterProcessor
    class R1ProcessorFactory : public disruptor::dsl::EventProcessorFactory<
                                   common::cmd::OrderCommand, RingBufferT> {
    public:
      R1ProcessorFactory(
          processors::SimpleEventHandler *eventHandler,
          processors::DisruptorExceptionHandler<common::cmd::OrderCommand>
              *exceptionHandler,
          common::CoreWaitStrategy coreWaitStrategy, const std::string &name,
          processors::TwoStepMasterProcessor<WaitStrategyT> **outProcessor)
          : eventHandler_(eventHandler), exceptionHandler_(exceptionHandler),
            coreWaitStrategy_(coreWaitStrategy), name_(name),
            outProcessor_(outProcessor) {}

      disruptor::EventProcessor &
      createEventProcessor(RingBufferT &ringBuffer,
                           disruptor::Sequence *const *barrierSequences,
                           int count) override {
        auto barrier = ringBuffer.newBarrier(barrierSequences, count);
        auto *processor = new processors::TwoStepMasterProcessor<WaitStrategyT>(
            &ringBuffer, barrier.get(), eventHandler_, exceptionHandler_,
            coreWaitStrategy_, name_);
        *outProcessor_ = processor;
        auto *adapter = new processors::EventProcessorAdapter(
            processor->GetSequence(), [processor]() { processor->Run(); },
            [processor]() { processor->Halt(); },
            [processor]() { return processor->IsRunning(); });
        return *adapter;
      }

    private:
      processors::SimpleEventHandler *eventHandler_;
      processors::DisruptorExceptionHandler<common::cmd::OrderCommand>
          *exceptionHandler_;
      common::CoreWaitStrategy coreWaitStrategy_;
      std::string name_;
      processors::TwoStepMasterProcessor<WaitStrategyT> **outProcessor_;
    };

    processors::TwoStepMasterProcessor<WaitStrategyT> *r1Processor = nullptr;
    auto r1Factory = std::make_unique<R1ProcessorFactory>(
        r1Handlers.back().get(), exceptionHandler.get(), perfCfg.waitStrategy,
        "R1_" + std::to_string(i), &r1Processor);
    afterGrouping.handleEventsWith(*r1Factory);
    if (r1Processor != nullptr) {
      procR1Processors.push_back(static_cast<disruptor::EventProcessor *>(
          new processors::EventProcessorAdapter(
              r1Processor->GetSequence(),
              [r1Processor]() { r1Processor->Run(); },
              [r1Processor]() { r1Processor->Halt(); },
              [r1Processor]() { return r1Processor->IsRunning(); })));
    }
  }

  // After R1, add Matching Engine handlers
  // TODO: Fix EventHandlerGroup::after() type matching
  // The after() method requires EventHandlerIdentity*, but EventProcessor* is
  // not EventHandlerIdentity. Need to check how to properly chain
  // EventProcessors. For now, we'll use handleEventsWith directly on
  // afterGrouping
  // disruptor::dsl::EventHandlerGroup<common::cmd::OrderCommand,
  //                                   disruptor::dsl::ProducerType::MULTI,
  //                                   WaitStrategyT>
  //     afterR1 = disruptorPtr->after(procR1Processors.data(),
  //                                  static_cast<int>(procR1Processors.size()));
  // afterR1.handleEventsWith(*matchingEngineHandlers[0]);
  // for (size_t i = 1; i < matchingEngineHandlers.size(); i++) {
  //   afterR1.handleEventsWith(*matchingEngineHandlers[i]);
  // }

  // For now, add matching engine handlers directly to afterGrouping
  // This is a simplified version - full pipeline will require proper
  // EventHandlerGroup chaining
  afterGrouping.handleEventsWith(*matchingEngineHandlers[0]);
  for (size_t i = 1; i < matchingEngineHandlers.size(); i++) {
    afterGrouping.handleEventsWith(*matchingEngineHandlers[i]);
  }

  // 3. Risk Release (R2) after Matching Engine
  // Convert matchingEngineHandlers to EventHandlerIdentity array
  std::vector<disruptor::EventHandlerIdentity *> matchingEngineIdentities;
  matchingEngineIdentities.reserve(matchingEngineHandlers.size());
  for (auto &handler : matchingEngineHandlers) {
    matchingEngineIdentities.push_back(
        static_cast<disruptor::EventHandlerIdentity *>(handler.get()));
  }
  auto afterMatchingEngine =
      disruptorPtr->after(matchingEngineIdentities.data(),
                          static_cast<int>(matchingEngineIdentities.size()));

  std::vector<processors::TwoStepSlaveProcessor<WaitStrategyT> *> procR2;
  procR2.reserve(riskEnginesNum);
  std::vector<std::unique_ptr<RiskPostProcessHandler>> r2Handlers;
  r2Handlers.reserve(riskEnginesNum);

  for (size_t i = 0; i < riskEngines.size(); i++) {
    auto r2Handler =
        std::make_unique<RiskPostProcessHandler>(riskEngines[i].get());
    r2Handlers.push_back(std::move(r2Handler));

    class R2ProcessorFactory : public disruptor::dsl::EventProcessorFactory<
                                   common::cmd::OrderCommand, RingBufferT> {
    public:
      R2ProcessorFactory(
          processors::SimpleEventHandler *eventHandler,
          processors::DisruptorExceptionHandler<common::cmd::OrderCommand>
              *exceptionHandler,
          const std::string &name,
          processors::TwoStepSlaveProcessor<WaitStrategyT> **outProcessor)
          : eventHandler_(eventHandler), exceptionHandler_(exceptionHandler),
            name_(name), outProcessor_(outProcessor) {}

      disruptor::EventProcessor &
      createEventProcessor(RingBufferT &ringBuffer,
                           disruptor::Sequence *const *barrierSequences,
                           int count) override {
        auto barrier = ringBuffer.newBarrier(barrierSequences, count);
        auto *processor = new processors::TwoStepSlaveProcessor<WaitStrategyT>(
            &ringBuffer, barrier.get(), eventHandler_, exceptionHandler_,
            name_);
        *outProcessor_ = processor;
        auto *adapter = new processors::EventProcessorAdapter(
            processor->GetSequence(), [processor]() { processor->Run(); },
            [processor]() { processor->Halt(); },
            [processor]() { return processor->IsRunning(); });
        return *adapter;
      }

    private:
      processors::SimpleEventHandler *eventHandler_;
      processors::DisruptorExceptionHandler<common::cmd::OrderCommand>
          *exceptionHandler_;
      std::string name_;
      processors::TwoStepSlaveProcessor<WaitStrategyT> **outProcessor_;
    };

    processors::TwoStepSlaveProcessor<WaitStrategyT> *r2Processor = nullptr;
    auto r2Factory = std::make_unique<R2ProcessorFactory>(
        r2Handlers.back().get(), exceptionHandler.get(),
        "R2_" + std::to_string(i), &r2Processor);
    afterMatchingEngine.handleEventsWith(*r2Factory);
    if (r2Processor != nullptr) {
      procR2.push_back(r2Processor);
    }
  }

  // 4. ResultsHandler (E) after Matching Engine + [Journaling]
  // TODO: Implement full pipeline chain
  // For now, create ResultsHandler but don't add to pipeline yet
  auto resultsHandler =
      std::make_unique<processors::ResultsHandler>(resultsConsumer);
  class ResultsEventHandler
      : public disruptor::EventHandler<common::cmd::OrderCommand> {
  public:
    ResultsEventHandler(processors::ResultsHandler *handler,
                        ExchangeApi<WaitStrategyT> *api)
        : handler_(handler), api_(api) {}
    void onEvent(common::cmd::OrderCommand &cmd, int64_t sequence,
                 bool endOfBatch) override {
      handler_->OnEvent(&cmd, sequence, endOfBatch);
      api_->ProcessResult(sequence, &cmd);
    }

  private:
    processors::ResultsHandler *handler_;
    ExchangeApi<WaitStrategyT> *api_;
  };
  // TODO: Add ResultsEventHandler to pipeline after fixing EventHandlerGroup
  // chain static ResultsEventHandler resultsEventHandler(resultsHandler.get(),
  //                                                api_.get());
  // mainHandlerGroup.handleEventsWith(resultsEventHandler);

  // Attach slave processors to master processors
  // Store actual R1 processors for setSlaveProcessor
  std::vector<processors::TwoStepMasterProcessor<WaitStrategyT> *>
      actualR1Processors;
  actualR1Processors.reserve(riskEnginesNum);
  // TODO: Extract actual processors from procR1Processors adapters
  for (size_t i = 0; i < actualR1Processors.size() && i < procR2.size(); i++) {
    // Find the corresponding R1 processor
    // Note: procR1Processors contains adapters, we need to find the actual
    // processor For now, we'll need to store the actual processors separately
    // TODO: Store actual processors for setSlaveProcessor
  }

  // Store processors for lifecycle management
  // Note: Processors are owned by factories and adapters, which are owned by
  // Disruptor
  // We need to store references for cleanup if needed
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
