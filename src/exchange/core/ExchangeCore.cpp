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

#include <atomic>
#include <disruptor/BlockingWaitStrategy.h>
#include <disruptor/BusySpinWaitStrategy.h>
#include <disruptor/EventFactory.h>
#include <disruptor/EventHandler.h>
#include <disruptor/EventTranslator.h>
#include <disruptor/TimeoutException.h>
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
#include <exchange/core/orderbook/OrderBookEventsHelper.h>
#include <exchange/core/processors/DisruptorExceptionHandler.h>
#include <exchange/core/processors/GroupingProcessor.h>
#include <exchange/core/processors/MatchingEngineRouter.h>
#include <exchange/core/processors/ResultsHandler.h>
#include <exchange/core/processors/RiskEngine.h>
#include <exchange/core/processors/SharedPool.h>
#include <exchange/core/processors/SimpleEventHandler.h>
#include <exchange/core/processors/TwoStepMasterProcessor.h>
#include <exchange/core/processors/TwoStepSlaveProcessor.h>
#include <exchange/core/processors/WaitSpinningHelper.h>
#include <exchange/core/processors/journaling/DummySerializationProcessor.h>
#include <exchange/core/processors/journaling/ISerializationProcessor.h>
#include <exchange/core/utils/FastNanoTime.h>
#include <exchange/core/utils/Logger.h>
#include <latch>
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
      : exchangeConfiguration_(exchangeConfiguration), started_(false),
        stopped_(false), processorStartupLatch_(nullptr) {
    LOG_DEBUG("Building exchange core from configuration: "
              "ExchangeConfiguration{{...}}");
    const auto &perfCfg = exchangeConfiguration_->performanceCfg;
    const auto &serializationCfg = exchangeConfiguration_->serializationCfg;

    const int ringBufferSize = perfCfg.ringBufferSize;
    const int matchingEnginesNum = perfCfg.matchingEnginesNum;
    const int riskEnginesNum = perfCfg.riskEnginesNum;

    // 1. Serialization Processor
    // Match Java: use serializationProcessorFactory from config
    if (serializationCfg.serializationProcessorFactory) {
      serializationProcessor_ = serializationCfg.serializationProcessorFactory(
          exchangeConfiguration_);
    } else {
      // Fallback to dummy processor if factory not set
      auto *dummyProcessor =
          processors::journaling::DummySerializationProcessor::Instance();
      serializationProcessor_ =
          static_cast<processors::journaling::ISerializationProcessor *>(
              dummyProcessor);
    }

    // 2. Shared Pool
    const int poolInitialSize = (matchingEnginesNum + riskEnginesNum) * 8;
    // Use OrderBookEventsHelper::EVENTS_POOLING to determine chain length
    constexpr bool EVENTS_POOLING =
        orderbook::OrderBookEventsHelper::EVENTS_POOLING;
    const int chainLength = EVENTS_POOLING ? 1024 : 1;
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
      throw std::runtime_error(
          "PerformanceConfiguration.threadFactory is null");
    }

    // Use original ThreadFactory - Disruptor now supports latch directly
    disruptor_ = std::make_unique<DisruptorT>(
        eventFactory, ringBufferSize, *perfCfg.threadFactory, *waitStrategyPtr);

    auto &ringBuffer = disruptor_->getRingBuffer();
    api_ = std::make_unique<ExchangeApi<WaitStrategyT>>(&ringBuffer);

    // 6. Exception Handler
    // Match Java behavior: publish SHUTDOWN_SIGNAL and call shutdown()
    // Now that halt() matches Java (doesn't join threads), this won't deadlock
    exceptionHandler_ = std::make_unique<
        processors::DisruptorExceptionHandler<common::cmd::OrderCommand>>(
        "main", [this, &ringBuffer](const std::exception &ex, int64_t seq) {
          LOG_ERROR("[ExceptionHandler] Handling exception: {}, seq={}",
                    ex.what(), seq);
          LOG_INFO("[ExceptionHandler] Publishing SHUTDOWN_SIGNAL");
          static ShutdownSignalTranslator shutdownTranslator;
          ringBuffer.publishEvent(shutdownTranslator);
          LOG_INFO("[ExceptionHandler] SHUTDOWN_SIGNAL published, calling "
                   "disruptor_->shutdown()");
          disruptor_->shutdown();
          LOG_INFO("[ExceptionHandler] disruptor_->shutdown() completed");
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
          std::vector<
              std::shared_ptr<processors::GroupingProcessor<WaitStrategyT>>>
              &processors,
          std::vector<BarrierPtr> &ownedBarriers)
          : perfCfg_(perfCfg), coreWaitStrategy_(coreWaitStrategy),
            sharedPool_(sharedPool), processors_(processors),
            ownedBarriers_(ownedBarriers) {}

      std::shared_ptr<disruptor::EventProcessor>
      createEventProcessor(RingBufferT &ringBuffer,
                           disruptor::Sequence *const *barrierSequences,
                           int count) override {
        auto barrier = ringBuffer.newBarrier(barrierSequences, count);
        // CRITICAL: Save barrier to ensure it outlives the processor
        // The barrier contains FixedSequenceGroup which holds Sequence*
        // pointers. These pointers must remain valid for the lifetime of the
        // processor. In Java, GC manages object lifetime. In C++, we must
        // explicitly manage ownership.
        ownedBarriers_.push_back(barrier);
        auto processor =
            std::make_shared<processors::GroupingProcessor<WaitStrategyT>>(
                &ringBuffer, barrier.get(), perfCfg_, coreWaitStrategy_,
                sharedPool_);

        processors_.push_back(processor);
        return processor;
      }

    private:
      const common::config::PerformanceConfiguration *perfCfg_;
      common::CoreWaitStrategy coreWaitStrategy_;
      processors::SharedPool *sharedPool_;
      std::vector<std::shared_ptr<processors::GroupingProcessor<WaitStrategyT>>>
          &processors_;
      std::vector<BarrierPtr> &ownedBarriers_;
    };

    auto groupingFactory = GroupingProcessorFactory(
        &perfCfg, perfCfg.waitStrategy, sharedPool_.get(), groupingProcessors_,
        ownedBarriers_);
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
          std::vector<std::shared_ptr<
              processors::TwoStepMasterProcessor<WaitStrategyT>>>
              &r1ProcessorsOwned,
          std::vector<disruptor::EventProcessor *> &r1EventProcessors,
          std::vector<BarrierPtr> &ownedBarriers)
          : eventHandler_(eventHandler), exceptionHandler_(exceptionHandler),
            coreWaitStrategy_(coreWaitStrategy), name_(name),
            r1Processors_(r1Processors), r1ProcessorsOwned_(r1ProcessorsOwned),
            r1EventProcessors_(r1EventProcessors),
            ownedBarriers_(ownedBarriers) {}

      std::shared_ptr<disruptor::EventProcessor>
      createEventProcessor(RingBufferT &ringBuffer,
                           disruptor::Sequence *const *barrierSequences,
                           int count) override {
        auto barrier = ringBuffer.newBarrier(barrierSequences, count);
        // CRITICAL: Save barrier to ensure it outlives the processor
        // The barrier contains FixedSequenceGroup which holds Sequence*
        // pointers. These pointers must remain valid for the lifetime of the
        // processor.
        ownedBarriers_.push_back(barrier);
        auto processor =
            std::make_shared<processors::TwoStepMasterProcessor<WaitStrategyT>>(
                &ringBuffer, barrier.get(), eventHandler_, exceptionHandler_,
                coreWaitStrategy_, name_);

        r1Processors_.push_back(processor.get());
        r1ProcessorsOwned_.push_back(processor);
        r1EventProcessors_.push_back(processor.get());

        return processor;
      }

    private:
      processors::SimpleEventHandler *eventHandler_;
      processors::DisruptorExceptionHandler<common::cmd::OrderCommand>
          *exceptionHandler_;
      common::CoreWaitStrategy coreWaitStrategy_;
      std::string name_;
      std::vector<void *> &r1Processors_;
      std::vector<
          std::shared_ptr<processors::TwoStepMasterProcessor<WaitStrategyT>>>
          &r1ProcessorsOwned_;
      std::vector<disruptor::EventProcessor *> &r1EventProcessors_;
      std::vector<BarrierPtr> &ownedBarriers_;
    };

    for (size_t i = 0; i < riskEngines_.size(); i++) {
      auto handler =
          std::make_unique<RiskPreProcessHandler>(riskEngines_[i].get());
      auto r1Factory = R1ProcessorFactory(
          handler.get(), exceptionHandler_.get(), perfCfg.waitStrategy,
          "R1_" + std::to_string(i), r1Processors_, r1ProcessorsOwned_,
          r1EventProcessors_, ownedBarriers_);
      afterGrouping.handleEventsWith(r1Factory);
      riskHandlers_.push_back(std::move(handler));
    }

    // Stage 4: Matching Engines (after R1)
    class MatchingEngineEventHandler
        : public disruptor::EventHandler<common::cmd::OrderCommand> {
    public:
      MatchingEngineEventHandler(
          processors::MatchingEngineRouter *matchingEngine, int32_t shardId)
          : matchingEngine_(matchingEngine), shardId_(shardId) {}

      void onEvent(common::cmd::OrderCommand &cmd, int64_t sequence,
                   bool endOfBatch) override {
        matchingEngine_->ProcessOrder(sequence, &cmd);
      }

    private:
      processors::MatchingEngineRouter *matchingEngine_;
      int32_t shardId_;
    };

    // Create afterR1 group (wait for all R1 processors to complete)
    // Java: disruptor.after(procR1.toArray(new TwoStepMasterProcessor[0]))
    // Java version uses after(EventProcessor...) which directly gets sequences
    // Use the EventProcessor* overload of after() - convert EventProcessor** to
    // EventProcessor *const *
    auto afterR1 =
        disruptor_->after(const_cast<disruptor::EventProcessor *const *>(
                              r1EventProcessors_.data()),
                          static_cast<int>(r1EventProcessors_.size()));

    // Create all MatchingEngine handlers first (matches Java:
    // matchingEngineHandlers array) Java: final EventHandler<OrderCommand>[]
    // matchingEngineHandlers = ...
    for (size_t i = 0; i < matchingEngines_.size(); i++) {
      auto handler = std::make_unique<MatchingEngineEventHandler>(
          matchingEngines_[i].get(), static_cast<int32_t>(i));
      matchingEngineHandlers_.push_back(std::move(handler));
    }

    // Register all MatchingEngine handlers at once (matches Java:
    // handleEventsWith(matchingEngineHandlers)) Java:
    // disruptor.after(procR1.toArray(...)).handleEventsWith(matchingEngineHandlers)
    if (!matchingEngineHandlers_.empty()) {
      // Use a helper lambda to call handleEventsWith with all handlers
      // Since C++ variadic templates require compile-time parameter count,
      // we handle common cases (1-4 handlers) explicitly
      auto registerHandlers = [&afterR1](auto &handlers) {
        if (handlers.size() == 1) {
          afterR1.handleEventsWith(*handlers[0].get());
        } else if (handlers.size() == 2) {
          afterR1.handleEventsWith(*handlers[0].get(), *handlers[1].get());
        } else if (handlers.size() == 3) {
          afterR1.handleEventsWith(*handlers[0].get(), *handlers[1].get(),
                                   *handlers[2].get());
        } else if (handlers.size() >= 4) {
          // For 4+ handlers, register first 4 at once, then rest individually
          afterR1.handleEventsWith(*handlers[0].get(), *handlers[1].get(),
                                   *handlers[2].get(), *handlers[3].get());
          for (size_t i = 4; i < handlers.size(); i++) {
            afterR1.handleEventsWith(*handlers[i].get());
          }
        }
      };

      registerHandlers(matchingEngineHandlers_);
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
    for (auto &h : matchingEngineHandlers_) {
      meIdentities.push_back(h.get());
    }
    auto afterME = disruptor_->after(meIdentities.data(), meIdentities.size());
    // Debug: Log MatchingEngine sequence values after creating afterME
    // This is not in hot path, only executed once during initialization
    for (size_t i = 0; i < meIdentities.size(); i++) {
      try {
        int64_t seqValue = disruptor_->getSequenceValueFor(*meIdentities[i]);
        // Sequence value logged only if needed for debugging
      } catch (const std::exception &e) {
        // Sequence not available - expected in some configurations
      }
    }

    class R2ProcessorFactory : public disruptor::dsl::EventProcessorFactory<
                                   common::cmd::OrderCommand, RingBufferT> {
    public:
      R2ProcessorFactory(
          processors::SimpleEventHandler *eventHandler,
          processors::DisruptorExceptionHandler<common::cmd::OrderCommand>
              *exceptionHandler,
          const std::string &name, std::vector<void *> &r2Processors,
          std::vector<
              std::shared_ptr<processors::TwoStepSlaveProcessor<WaitStrategyT>>>
              &r2ProcessorsOwned,
          std::vector<BarrierPtr> &ownedBarriers)
          : eventHandler_(eventHandler), exceptionHandler_(exceptionHandler),
            name_(name), r2Processors_(r2Processors),
            r2ProcessorsOwned_(r2ProcessorsOwned),
            ownedBarriers_(ownedBarriers) {}

      std::shared_ptr<disruptor::EventProcessor>
      createEventProcessor(RingBufferT &ringBuffer,
                           disruptor::Sequence *const *barrierSequences,
                           int count) override {
        // Processor creation details logged only if needed for debugging
        auto barrier = ringBuffer.newBarrier(barrierSequences, count);
        // CRITICAL: Save barrier to ensure it outlives the processor
        // The barrier contains FixedSequenceGroup which holds Sequence*
        // pointers. These pointers must remain valid for the lifetime of the
        // processor.
        ownedBarriers_.push_back(barrier);
        auto processor =
            std::make_shared<processors::TwoStepSlaveProcessor<WaitStrategyT>>(
                &ringBuffer, barrier.get(), eventHandler_, exceptionHandler_,
                name_);

        r2Processors_.push_back(processor.get());
        r2ProcessorsOwned_.push_back(processor);
        return processor;
      }

    private:
      processors::SimpleEventHandler *eventHandler_;
      processors::DisruptorExceptionHandler<common::cmd::OrderCommand>
          *exceptionHandler_;
      std::string name_;
      std::vector<void *> &r2Processors_;
      std::vector<
          std::shared_ptr<processors::TwoStepSlaveProcessor<WaitStrategyT>>>
          &r2ProcessorsOwned_;
      std::vector<BarrierPtr> &ownedBarriers_;
    };

    for (size_t i = 0; i < riskEngines_.size(); i++) {
      auto handler =
          std::make_unique<RiskPostProcessHandler>(riskEngines_[i].get());
      auto r2Factory = R2ProcessorFactory(
          handler.get(), exceptionHandler_.get(), "R2_" + std::to_string(i),
          r2Processors_, r2ProcessorsOwned_, ownedBarriers_);
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

    // Stage 6: Results Handler
    // Java: mainHandlerGroup = enableJournaling
    //   ? disruptor.after(arraysAddHandler(matchingEngineHandlers, jh))
    //   : afterMatchingEngine;
    // Match Java exactly: mainHandlerGroup depends on ME (or ME+J), same as R2.
    // Both R2 and ResultsHandler run in parallel after ME (or ME+J) completes.
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

    // Create latch for startup synchronization (C++20 std::latch)
    // Get processor count from Disruptor (all processors are registered by now)
    // This is more robust than hard-coding the count
    const int totalProcessors = disruptor_->getProcessorCount();
    processorStartupLatch_ = std::make_unique<std::latch>(totalProcessors);
  }

  void Startup() override {
    // Match Java: check if already started
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true)) {
      return; // Already started
    }

    LOG_DEBUG("Starting disruptor...");
    // Start disruptor - threads will be created asynchronously
    // Each thread will call latch.count_down() when it starts (via Disruptor's
    // built-in latch support)
    disruptor_->start(processorStartupLatch_.get());

    // Wait for all processor threads to be running using std::latch (C++20)
    // This is the industrial-grade solution: threads signal when ready
    // No polling needed - latch.wait() blocks until all threads have started
    constexpr int maxWaitMs = 1000;
    auto deadline = utils::FastNanoTime::Now() + maxWaitMs * 1'000'000LL;

    const int expectedProcessors = disruptor_->getProcessorCount();

    // Wait for latch with timeout (defensive: prevent infinite wait)
    // Use wait() with timeout instead of polling try_wait()
    int64_t startTimeNs = utils::FastNanoTime::Now();
    bool allStarted = false;
    while (!allStarted) {
      if (processorStartupLatch_->try_wait()) {
        allStarted = true;
        break;
      }
      int64_t elapsedNs = utils::FastNanoTime::Now() - startTimeNs;
      if (elapsedNs >= maxWaitMs * 1'000'000LL) {
        LOG_WARN("[ExchangeCore] Processor startup latch timeout after {}ms. "
                 "Expected {} processors, but not all started.",
                 maxWaitMs, expectedProcessors);
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (allStarted) {
      int64_t elapsedNs = utils::FastNanoTime::Now() - startTimeNs;
      LOG_DEBUG("[ExchangeCore] All {} processors have started in {}ms",
                expectedProcessors, elapsedNs / 1'000'000LL);
    }

    // MatchingEngine sequence values available after startup
    // This is not in hot path, only executed once during initialization
    for (size_t i = 0; i < matchingEngineHandlers_.size(); i++) {
      try {
        int64_t seqValue =
            disruptor_->getSequenceValueFor(*matchingEngineHandlers_[i].get());
        // Sequence value logged only if needed for debugging
      } catch (const std::exception &e) {
        // Sequence not available - expected in some configurations
      }
    }

    if (serializationProcessor_) {
      serializationProcessor_->ReplayJournalFullAndThenEnableJouraling(
          &exchangeConfiguration_->initStateCfg, api_.get());
    }
  }

  void Shutdown(int64_t timeoutMs) override {
    // Match Java: check if already stopped
    bool expected = false;
    if (!stopped_.compare_exchange_strong(expected, true)) {
      return; // Already stopped
    }
    // Match Java: simple shutdown logic
    // TODO stop accepting new events first
    try {
      LOG_INFO("[ExchangeCore] Shutdown: publishing SHUTDOWN_SIGNAL");
      static ShutdownSignalTranslator shutdownTranslator;
      // Match Java: ringBuffer.publishEvent(SHUTDOWN_SIGNAL_TRANSLATOR);
      disruptor_->getRingBuffer().publishEvent(shutdownTranslator);
      // Match Java: disruptor.shutdown(timeout, timeUnit);
      if (timeoutMs < 0)
        disruptor_->shutdown();
      else
        disruptor_->shutdown(timeoutMs);
      // CRITICAL: Wait for all processor threads to exit before allowing
      // destruction. This ensures eventHandler_ pointers remain valid during
      // onShutdown() calls. In Java, GC keeps objects alive, but in C++ we
      // must explicitly wait for threads to finish.
      disruptor_->join();
      LOG_INFO("[ExchangeCore] Shutdown: completed");
    } catch (const disruptor::TimeoutException &e) {
      // Match Java: throw IllegalStateException on timeout
      // In C++, we use std::runtime_error as equivalent
      throw std::runtime_error(
          "could not stop a disruptor gracefully. Not all events may be "
          "executed.");
    }
  }

  IExchangeApi *GetApi() override { return api_.get(); }

private:
  const common::config::ExchangeConfiguration *exchangeConfiguration_;
  // CRITICAL: Store barriers created by factories to ensure they outlive
  // processors. Must be declared before disruptor_ so it's destroyed after
  // disruptor_ (barriers are accessed during Disruptor::halt() in destructor).
  // In Java, GC manages object lifetime. In C++, we must explicitly manage
  // ownership. Barriers contain FixedSequenceGroup which holds Sequence*
  // pointers that must remain valid.
  using BarrierPtr = typename DisruptorT::BarrierPtr;
  std::vector<BarrierPtr> ownedBarriers_;
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
  std::vector<std::shared_ptr<processors::GroupingProcessor<WaitStrategyT>>>
      groupingProcessors_;
  std::vector<void *> r1Processors_;
  std::vector<void *> r2Processors_;
  std::vector<
      std::shared_ptr<processors::TwoStepMasterProcessor<WaitStrategyT>>>
      r1ProcessorsOwned_;
  std::vector<std::shared_ptr<processors::TwoStepSlaveProcessor<WaitStrategyT>>>
      r2ProcessorsOwned_;
  std::vector<disruptor::EventProcessor *> r1EventProcessors_;
  std::vector<std::unique_ptr<processors::SimpleEventHandler>> riskHandlers_;
  std::vector<
      std::unique_ptr<disruptor::EventHandler<common::cmd::OrderCommand>>>
      eventHandlers_;
  std::vector<
      std::unique_ptr<disruptor::EventHandler<common::cmd::OrderCommand>>>
      matchingEngineHandlers_;

  // Lifecycle flags - match Java behavior
  // core can be started and stopped only once
  std::atomic<bool> started_;
  std::atomic<bool> stopped_;

  // Startup synchronization using std::latch (C++20)
  // Pointer because latch is created in Startup(), not in constructor
  std::unique_ptr<std::latch> processorStartupLatch_;
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
