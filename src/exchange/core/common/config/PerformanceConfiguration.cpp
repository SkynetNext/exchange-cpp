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

#include <disruptor/dsl/ThreadFactory.h>
#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <exchange/core/common/config/LoggingConfiguration.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <exchange/core/orderbook/OrderBookDirectImpl.h>
#include <exchange/core/orderbook/OrderBookNaiveImpl.h>
#include <exchange/core/utils/AffinityThreadFactory.h>
#include <functional>
#include <memory>

namespace exchange::core::common::config {

PerformanceConfiguration PerformanceConfiguration::Default() {
  // Java: .threadFactory(Thread::new) - simple thread factory
  class SimpleThreadFactory : public disruptor::dsl::ThreadFactory {
  public:
    std::thread newThread(std::function<void()> r) override {
      return std::thread(r);
    }
  };

  return PerformanceConfiguration(
    16 * 1024,  // ringBufferSize
    1,          // matchingEnginesNum
    1,          // riskEnginesNum
    256,        // msgsInGroupLimit
    10'000,     // maxGroupDurationNs (10 microseconds)
    false,      // sendL2ForEveryCmd
    8,          // l2RefreshDepth
    CoreWaitStrategy::BLOCKING, std::make_shared<SimpleThreadFactory>(),
    [](const CoreSymbolSpecification* spec,
       ::exchange::core::collections::objpool::ObjectsPool* objectsPool,
       orderbook::OrderBookEventsHelper* eventsHelper) {
      // OrderBookNaiveImpl doesn't use ObjectsPool, but accepts it for
      // interface consistency
      return std::make_unique<orderbook::OrderBookNaiveImpl>(spec, objectsPool, eventsHelper);
    });
}

PerformanceConfiguration PerformanceConfiguration::LatencyPerformanceBuilder() {
  // Java: .threadFactory(new
  // AffinityThreadFactory(THREAD_AFFINITY_ENABLE_PER_LOGICAL_CORE))
  return PerformanceConfiguration(
    2 * 1024,  // ringBufferSize
    1,         // matchingEnginesNum
    1,         // riskEnginesNum
    256,       // msgsInGroupLimit
    10'000,    // maxGroupDurationNs (10 microseconds)
    false,     // sendL2ForEveryCmd
    8,         // l2RefreshDepth
    CoreWaitStrategy::BUSY_SPIN,
    std::make_shared<utils::AffinityThreadFactory>(
      utils::ThreadAffinityMode::THREAD_AFFINITY_ENABLE_PER_LOGICAL_CORE),
    [](const CoreSymbolSpecification* spec,
       ::exchange::core::collections::objpool::ObjectsPool* objectsPool,
       orderbook::OrderBookEventsHelper* eventsHelper) {
      static const auto defaultLoggingCfg = LoggingConfiguration::Default();
      return std::make_unique<orderbook::OrderBookDirectImpl>(spec, objectsPool, eventsHelper,
                                                              &defaultLoggingCfg);
    });
}

PerformanceConfiguration PerformanceConfiguration::ThroughputPerformanceBuilder() {
  // Java: .threadFactory(new
  // AffinityThreadFactory(THREAD_AFFINITY_ENABLE_PER_LOGICAL_CORE))
  return PerformanceConfiguration(
    64 * 1024,  // ringBufferSize
    4,          // matchingEnginesNum
    2,          // riskEnginesNum
    4096,       // msgsInGroupLimit
    4'000'000,  // maxGroupDurationNs (4 milliseconds)
    false,      // sendL2ForEveryCmd
    8,          // l2RefreshDepth
    CoreWaitStrategy::BUSY_SPIN,
    std::make_shared<utils::AffinityThreadFactory>(
      utils::ThreadAffinityMode::THREAD_AFFINITY_ENABLE_PER_LOGICAL_CORE),
    [](const CoreSymbolSpecification* spec,
       ::exchange::core::collections::objpool::ObjectsPool* objectsPool,
       orderbook::OrderBookEventsHelper* eventsHelper) {
      static const auto defaultLoggingCfg = LoggingConfiguration::Default();
      return std::make_unique<orderbook::OrderBookDirectImpl>(spec, objectsPool, eventsHelper,
                                                              &defaultLoggingCfg);
    });
}

}  // namespace exchange::core::common::config
