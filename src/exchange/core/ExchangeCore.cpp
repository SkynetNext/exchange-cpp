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
#include <iostream>
#include <memory>
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


// Helper to get WaitStrategy based on CoreWaitStrategy
template <typename WaitStrategyT> WaitStrategyT *GetWaitStrategyInstance();

template <>
disruptor::BusySpinWaitStrategy *
GetWaitStrategyInstance<disruptor::BusySpinWaitStrategy>() {
  static disruptor::BusySpinWaitStrategy instance;
  return &instance;
}

template <>
disruptor::YieldingWaitStrategy *
GetWaitStrategyInstance<disruptor::YieldingWaitStrategy>() {
  static disruptor::YieldingWaitStrategy instance;
  return &instance;
}

template <>
disruptor::BlockingWaitStrategy *
GetWaitStrategyInstance<disruptor::BlockingWaitStrategy>() {
  static disruptor::BlockingWaitStrategy instance;
  return &instance;
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

// Internal implementation interface
struct ExchangeCore::IImpl {
  virtual ~IImpl() = default;
  virtual void Startup() = 0;
  virtual void Shutdown(int64_t timeoutMs) = 0;
  virtual IExchangeApi *GetApi() = 0;
};

// Template implementation
template <typename WaitStrategyT>
class ExchangeCoreImpl : public ExchangeCore::IImpl {
public:
  using RingBufferT =
      disruptor::MultiProducerRingBuffer<common::cmd::OrderCommand,
                                         WaitStrategyT>;
  using DisruptorT =
      disruptor::dsl::Disruptor<common::cmd::OrderCommand,
                                disruptor::dsl::ProducerType::MULTI,
                                WaitStrategyT>;

  ExchangeCoreImpl(
      ExchangeCore::ResultsConsumer resultsConsumer,
      const common::config::ExchangeConfiguration *exchangeConfiguration)
      : exchangeConfiguration_(exchangeConfiguration) {
    const auto &perfCfg = exchangeConfiguration_->performanceCfg;
    const auto &serializationCfg = exchangeConfiguration_->serializationCfg;

    const int ringBufferSize = perfCfg.ringBufferSize;
    const int matchingEnginesNum = perfCfg.matchingEnginesNum;
    const int riskEnginesNum = perfCfg.riskEnginesNum;

    // 1. Serialization Processor
    auto *dummyProcessor =
        processors::journaling::DummySerializationProcessor::Instance();
    serializationProcessor_ =
        static_cast<processors::journaling::ISerializationProcessor *>(
            dummyProcessor);

    // 2. Shared Pool
    const int poolInitialSize = (matchingEnginesNum + riskEnginesNum) * 8;
    const int chainLength = false ? 1024 : 1; // EVENTS_POOLING
    sharedPool_ = std::make_unique<processors::SharedPool>(
        poolInitialSize * 4, poolInitialSize, chainLength);

    // 3. Matching Engines
    matchingEngines_.reserve(matchingEnginesNum);
    for (int32_t shardId = 0; shardId < matchingEnginesNum; shardId++) {
      matchingEngines_.push_back(
          std::make_unique<processors::MatchingEngineRouter>(
              shardId, matchingEnginesNum, perfCfg.orderBookFactory,
              sharedPool_.get(), exchangeConfiguration, serializationProcessor_,
              nullptr));
    }

    // 4. Risk Engines
    riskEngines_.reserve(riskEnginesNum);
    for (int32_t shardId = 0; shardId < riskEnginesNum; shardId++) {
      riskEngines_.push_back(std::make_unique<processors::RiskEngine>(
          shardId, riskEnginesNum, serializationProcessor_, sharedPool_.get(),
          exchangeConfiguration));
    }

    // 5. Disruptor Setup
    auto eventFactory = std::make_shared<OrderCommandEventFactory>();
    auto *waitStrategyPtr = GetWaitStrategyInstance<WaitStrategyT>();

    // Java: new Disruptor<>(..., threadFactory, ...)
    // threadFactory comes from perfCfg.getThreadFactory()
    if (!perfCfg.threadFactory) {
      throw std::runtime_error("PerformanceConfiguration.threadFactory is null");
    }

    disruptor_ = std::make_unique<DisruptorT>(
        eventFactory, ringBufferSize, *perfCfg.threadFactory, *waitStrategyPtr);

    auto &ringBuffer = disruptor_->getRingBuffer();
    api_ = std::make_unique<ExchangeApi<WaitStrategyT>>(&ringBuffer);

    // 6. Exception Handler
    exceptionHandler_ = std::make_unique<
        processors::DisruptorExceptionHandler<common::cmd::OrderCommand>>(
        "main", [this, &ringBuffer](const std::exception &ex, int64_t seq) {
          static ShutdownSignalTranslator shutdownTranslator;
          ringBuffer.publishEvent(shutdownTranslator);
          disruptor_->shutdown();
        });

    // 7. Pipeline Construction

    // Stage 1: Grouping
    class GroupingProcessorFactory
        : public disruptor::dsl::EventProcessorFactory<
              common::cmd::OrderCommand, RingBufferT> {
    public:
      GroupingProcessorFactory(
          const common::config::PerformanceConfiguration *perfCfg,
          common::CoreWaitStrategy coreWaitStrategy,
          processors::SharedPool *sharedPool,
          std::vector<std::unique_ptr<processors::GroupingProcessor>>
              &processors)
          : perfCfg_(perfCfg), coreWaitStrategy_(coreWaitStrategy),
            sharedPool_(sharedPool), processors_(processors) {}

      disruptor::EventProcessor &
      createEventProcessor(RingBufferT &ringBuffer,
                           disruptor::Sequence *const *barrierSequences,
                           int count) override {
        auto barrier = ringBuffer.newBarrier(barrierSequences, count);
        auto processor = std::make_unique<processors::GroupingProcessor>(
            &ringBuffer, barrier.get(), perfCfg_, coreWaitStrategy_,
            sharedPool_);

        processors_.push_back(std::move(processor));
        return *processors_.back();
      }

    private:
      const common::config::PerformanceConfiguration *perfCfg_;
      common::CoreWaitStrategy coreWaitStrategy_;
      processors::SharedPool *sharedPool_;
      std::vector<std::unique_ptr<processors::GroupingProcessor>> &processors_;
    };

    auto groupingFactory = GroupingProcessorFactory(
        &perfCfg, perfCfg.waitStrategy, sharedPool_.get(), groupingProcessors_);
    auto afterGrouping = disruptor_->handleEventsWith(groupingFactory);

    // Stage 2: Journaling (Optional)
    disruptor::EventHandler<common::cmd::OrderCommand> *journalingHandler =
        nullptr;
    if (serializationCfg.enableJournaling) {
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

      auto jh =
          std::make_unique<JournalingEventHandler>(serializationProcessor_);
      journalingHandler = jh.get();
      afterGrouping.handleEventsWith(*jh);
      eventHandlers_.push_back(std::move(jh));
    }

    // Stage 3: Risk Pre-Process (R1)
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

    class R1ProcessorFactory : public disruptor::dsl::EventProcessorFactory<
                                   common::cmd::OrderCommand, RingBufferT> {
    public:
      R1ProcessorFactory(
          processors::SimpleEventHandler *eventHandler,
          processors::DisruptorExceptionHandler<common::cmd::OrderCommand>
              *exceptionHandler,
          common::CoreWaitStrategy coreWaitStrategy, const std::string &name,
          std::vector<void *> &r1Processors,
          std::vector<std::unique_ptr<
              processors::TwoStepMasterProcessor<WaitStrategyT>>>
              &r1ProcessorsOwned,
          std::vector<disruptor::EventProcessor *> &r1EventProcessors)
          : eventHandler_(eventHandler), exceptionHandler_(exceptionHandler),
            coreWaitStrategy_(coreWaitStrategy), name_(name),
            r1Processors_(r1Processors), r1ProcessorsOwned_(r1ProcessorsOwned),
            r1EventProcessors_(r1EventProcessors) {}

      disruptor::EventProcessor &
      createEventProcessor(RingBufferT &ringBuffer,
                           disruptor::Sequence *const *barrierSequences,
                           int count) override {
        auto barrier = ringBuffer.newBarrier(barrierSequences, count);
        auto processor =
            std::make_unique<processors::TwoStepMasterProcessor<WaitStrategyT>>(
                &ringBuffer, barrier.get(), eventHandler_, exceptionHandler_,
                coreWaitStrategy_, name_);

        r1Processors_.push_back(processor.get());
        r1ProcessorsOwned_.push_back(std::move(processor));
        r1EventProcessors_.push_back(r1ProcessorsOwned_.back().get());
        return *r1EventProcessors_.back();
      }

    private:
      processors::SimpleEventHandler *eventHandler_;
      processors::DisruptorExceptionHandler<common::cmd::OrderCommand>
          *exceptionHandler_;
      common::CoreWaitStrategy coreWaitStrategy_;
      std::string name_;
      std::vector<void *> &r1Processors_;
      std::vector<
          std::unique_ptr<processors::TwoStepMasterProcessor<WaitStrategyT>>>
          &r1ProcessorsOwned_;
      std::vector<disruptor::EventProcessor *> &r1EventProcessors_;
    };

    for (size_t i = 0; i < riskEngines_.size(); i++) {
      auto handler =
          std::make_unique<RiskPreProcessHandler>(riskEngines_[i].get());
      auto r1Factory = R1ProcessorFactory(
          handler.get(), exceptionHandler_.get(), perfCfg.waitStrategy,
          "R1_" + std::to_string(i), r1Processors_, r1ProcessorsOwned_,
          r1EventProcessors_);
      afterGrouping.handleEventsWith(r1Factory);
      riskHandlers_.push_back(std::move(handler));
    }

    // Stage 4: Matching Engines (after R1)
    class MatchingEngineEventHandler
        : public disruptor::EventHandler<common::cmd::OrderCommand> {
    public:
      MatchingEngineEventHandler(
          processors::MatchingEngineRouter *matchingEngine)
          : matchingEngine_(matchingEngine) {}

      void onEvent(common::cmd::OrderCommand &cmd, int64_t sequence,
                   bool endOfBatch) override {
        matchingEngine_->ProcessOrder(sequence, &cmd);
      }

    private:
      processors::MatchingEngineRouter *matchingEngine_;
    };

    // Create afterR1 group (wait for all R1 processors to complete)
    // Java: disruptor.after(procR1.toArray(new TwoStepMasterProcessor[0]))
    // Java version uses after(EventProcessor...) which directly gets sequences
    auto afterR1 = disruptor_->after(r1EventProcessors_.data(),
                                     static_cast<int>(r1EventProcessors_.size()));

    for (auto &me : matchingEngines_) {
      auto handler = std::make_unique<MatchingEngineEventHandler>(me.get());
      afterR1.handleEventsWith(*handler);
      matchingEngineHandlers_.push_back(std::move(handler));
    }

    // Stage 5: Risk Post-Process (R2)
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

    std::vector<disruptor::EventHandlerIdentity *> meIdentities;
    for (auto &h : matchingEngineHandlers_)
      meIdentities.push_back(h.get());
    auto afterME = disruptor_->after(meIdentities.data(), meIdentities.size());

    class R2ProcessorFactory : public disruptor::dsl::EventProcessorFactory<
                                   common::cmd::OrderCommand, RingBufferT> {
    public:
      R2ProcessorFactory(
          processors::SimpleEventHandler *eventHandler,
          processors::DisruptorExceptionHandler<common::cmd::OrderCommand>
              *exceptionHandler,
          const std::string &name, std::vector<void *> &r2Processors,
          std::vector<
              std::unique_ptr<processors::TwoStepSlaveProcessor<WaitStrategyT>>>
              &r2ProcessorsOwned)
          : eventHandler_(eventHandler), exceptionHandler_(exceptionHandler),
            name_(name), r2Processors_(r2Processors),
            r2ProcessorsOwned_(r2ProcessorsOwned) {}

      disruptor::EventProcessor &
      createEventProcessor(RingBufferT &ringBuffer,
                           disruptor::Sequence *const *barrierSequences,
                           int count) override {
        auto barrier = ringBuffer.newBarrier(barrierSequences, count);
        auto processor =
            std::make_unique<processors::TwoStepSlaveProcessor<WaitStrategyT>>(
                &ringBuffer, barrier.get(), eventHandler_, exceptionHandler_,
                name_);

        r2Processors_.push_back(processor.get());
        r2ProcessorsOwned_.push_back(std::move(processor));
        return *r2ProcessorsOwned_.back();
      }

    private:
      processors::SimpleEventHandler *eventHandler_;
      processors::DisruptorExceptionHandler<common::cmd::OrderCommand>
          *exceptionHandler_;
      std::string name_;
      std::vector<void *> &r2Processors_;
      std::vector<
          std::unique_ptr<processors::TwoStepSlaveProcessor<WaitStrategyT>>>
          &r2ProcessorsOwned_;
    };

    for (size_t i = 0; i < riskEngines_.size(); i++) {
      auto handler =
          std::make_unique<RiskPostProcessHandler>(riskEngines_[i].get());
      auto r2Factory = R2ProcessorFactory(
          handler.get(), exceptionHandler_.get(), "R2_" + std::to_string(i),
          r2Processors_, r2ProcessorsOwned_);
      afterME.handleEventsWith(r2Factory);
      riskHandlers_.push_back(std::move(handler));
    }

    // Stage 6: Results Handler
    // Java: mainHandlerGroup = enableJournaling
    //   ? disruptor.after(arraysAddHandler(matchingEngineHandlers, jh))
    //   : afterMatchingEngine;
    resultsHandler_ =
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

    // Create mainHandlerGroup: wait for ME + J (if journaling enabled)
    auto mainHandlerGroup = afterME;
    if (serializationCfg.enableJournaling && journalingHandler) {
      // Wait for both ME and J
      std::vector<disruptor::EventHandlerIdentity *> meAndJIdentities;
      for (auto &h : matchingEngineHandlers_)
        meAndJIdentities.push_back(h.get());
      meAndJIdentities.push_back(journalingHandler);
      mainHandlerGroup =
          disruptor_->after(meAndJIdentities.data(), meAndJIdentities.size());
    }

    auto resHandler = std::make_unique<ResultsEventHandler>(
        resultsHandler_.get(), api_.get());
    mainHandlerGroup.handleEventsWith(*resHandler);
    eventHandlers_.push_back(std::move(resHandler));

    // Final Stage: Link R1 and R2
    for (size_t i = 0;
         i < r1ProcessorsOwned_.size() && i < r2ProcessorsOwned_.size(); i++) {
      r1ProcessorsOwned_[i]->SetSlaveProcessor(r2ProcessorsOwned_[i].get());
    }
    std::cout << "[ExchangeCoreImpl] Constructor: all stages completed" << std::endl;
  }

  void Startup() override {
    std::cout << "[ExchangeCore] Startup: calling disruptor_->start()" << std::endl;
    disruptor_->start();
    std::cout << "[ExchangeCore] Startup: disruptor_->start() completed" << std::endl;
    if (serializationProcessor_) {
      serializationProcessor_->ReplayJournalFullAndThenEnableJouraling(
          &exchangeConfiguration_->initStateCfg, api_.get());
    }
    std::cout << "[ExchangeCore] Startup: completed" << std::endl;
  }

  void Shutdown(int64_t timeoutMs) override {
    static ShutdownSignalTranslator shutdownTranslator;
    disruptor_->getRingBuffer().publishEvent(shutdownTranslator);
    if (timeoutMs < 0)
      disruptor_->shutdown();
    else
      disruptor_->shutdown(timeoutMs);
  }

  IExchangeApi *GetApi() override {
    return api_.get();
  }

private:
  const common::config::ExchangeConfiguration *exchangeConfiguration_;
  std::unique_ptr<DisruptorT> disruptor_;
  std::unique_ptr<ExchangeApi<WaitStrategyT>> api_;
  std::unique_ptr<processors::SharedPool> sharedPool_;
  processors::journaling::ISerializationProcessor *serializationProcessor_;

  std::unique_ptr<
      processors::DisruptorExceptionHandler<common::cmd::OrderCommand>>
      exceptionHandler_;
  std::unique_ptr<processors::ResultsHandler> resultsHandler_;

  // Lifecycle management
  std::vector<std::unique_ptr<processors::MatchingEngineRouter>>
      matchingEngines_;
  std::vector<std::unique_ptr<processors::RiskEngine>> riskEngines_;
  std::vector<std::unique_ptr<processors::GroupingProcessor>>
      groupingProcessors_;
  std::vector<void *> r1Processors_;
  std::vector<void *> r2Processors_;
  std::vector<
      std::unique_ptr<processors::TwoStepMasterProcessor<WaitStrategyT>>>
      r1ProcessorsOwned_;
  std::vector<std::unique_ptr<processors::TwoStepSlaveProcessor<WaitStrategyT>>>
      r2ProcessorsOwned_;
  std::vector<disruptor::EventProcessor *> r1EventProcessors_;
  std::vector<std::unique_ptr<processors::SimpleEventHandler>> riskHandlers_;
  std::vector<
      std::unique_ptr<disruptor::EventHandler<common::cmd::OrderCommand>>>
      eventHandlers_;
  std::vector<
      std::unique_ptr<disruptor::EventHandler<common::cmd::OrderCommand>>>
      matchingEngineHandlers_;
};

ExchangeCore::ExchangeCore(
    ResultsConsumer resultsConsumer,
    const common::config::ExchangeConfiguration *exchangeConfiguration)
    : exchangeConfiguration_(exchangeConfiguration) {
  const auto &perfCfg = exchangeConfiguration_->performanceCfg;

  switch (perfCfg.waitStrategy) {
  case common::CoreWaitStrategy::BUSY_SPIN:
    impl_ = std::make_unique<ExchangeCoreImpl<disruptor::BusySpinWaitStrategy>>(
        resultsConsumer, exchangeConfiguration);
    break;
  case common::CoreWaitStrategy::YIELDING:
    impl_ = std::make_unique<ExchangeCoreImpl<disruptor::YieldingWaitStrategy>>(
        resultsConsumer, exchangeConfiguration);
    break;
  case common::CoreWaitStrategy::BLOCKING:
  default:
    impl_ = std::make_unique<ExchangeCoreImpl<disruptor::BlockingWaitStrategy>>(
        resultsConsumer, exchangeConfiguration);
    break;
  }
}

ExchangeCore::~ExchangeCore() = default;

void ExchangeCore::Startup() {
  if (impl_)
    impl_->Startup();
}

void ExchangeCore::Shutdown(int64_t timeoutMs) {
  if (impl_)
    impl_->Shutdown(timeoutMs);
}

IExchangeApi *ExchangeCore::GetApi() {
  IExchangeApi *api = impl_ ? impl_->GetApi() : nullptr;
  return api;
}

} // namespace core
} // namespace exchange
