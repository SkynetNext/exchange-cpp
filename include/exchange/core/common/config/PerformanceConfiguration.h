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

#include "../../orderbook/IOrderBook.h"
#include "../CoreWaitStrategy.h"
#include <cstdint>
#include <disruptor/dsl/ThreadFactory.h>
#include <functional>
#include <memory>

namespace exchange {
namespace core {
// Forward declarations
class CoreSymbolSpecification;
namespace collections {
namespace objpool {
class ObjectsPool;
}
} // namespace collections
namespace orderbook {
class OrderBookEventsHelper;
}
namespace common {
namespace config {

/**
 * PerformanceConfiguration - exchange performance configuration
 */
class PerformanceConfiguration {
public:
  // Disruptor ring buffer size (number of commands). Must be power of 2.
  int32_t ringBufferSize;

  // Number of matching engines. Each instance requires extra CPU core.
  int32_t matchingEnginesNum;

  // Number of risk engines. Each instance requires extra CPU core.
  int32_t riskEnginesNum;

  // max number of messages not processed by R2 stage
  int32_t msgsInGroupLimit;

  // max interval when messages not processed by R2 stage (nanoseconds)
  int64_t maxGroupDurationNs;

  // send L2 for every successfully executed command
  bool sendL2ForEveryCmd;

  // Depth of Regular L2 updates
  int32_t l2RefreshDepth;

  // Wait strategy
  CoreWaitStrategy waitStrategy;

  // ThreadFactory (matches Java version)
  // Note: Disruptor only stores a reference to ThreadFactory, not ownership
  // We use shared_ptr here to allow multiple PerformanceConfiguration instances
  // to share the same ThreadFactory, matching Java's reference semantics
  std::shared_ptr<disruptor::dsl::ThreadFactory> threadFactory;

  // OrderBook factory (matches Java IOrderBook.OrderBookFactory signature)
  using OrderBookFactory = std::function<std::unique_ptr<orderbook::IOrderBook>(
      const CoreSymbolSpecification *spec,
      ::exchange::core::collections::objpool::ObjectsPool *objectsPool,
      orderbook::OrderBookEventsHelper *eventsHelper)>;
  OrderBookFactory orderBookFactory;

  PerformanceConfiguration(
      int32_t ringBufferSize, int32_t matchingEnginesNum,
      int32_t riskEnginesNum, int32_t msgsInGroupLimit,
      int64_t maxGroupDurationNs, bool sendL2ForEveryCmd,
      int32_t l2RefreshDepth, CoreWaitStrategy waitStrategy,
      std::shared_ptr<disruptor::dsl::ThreadFactory> threadFactory,
      OrderBookFactory orderBookFactory)
      : ringBufferSize(ringBufferSize), matchingEnginesNum(matchingEnginesNum),
        riskEnginesNum(riskEnginesNum), msgsInGroupLimit(msgsInGroupLimit),
        maxGroupDurationNs(maxGroupDurationNs),
        sendL2ForEveryCmd(sendL2ForEveryCmd), l2RefreshDepth(l2RefreshDepth),
        waitStrategy(waitStrategy), threadFactory(threadFactory),
        orderBookFactory(std::move(orderBookFactory)) {}

  static PerformanceConfiguration Default();
  static PerformanceConfiguration LatencyPerformanceBuilder();
  static PerformanceConfiguration ThroughputPerformanceBuilder();
};

} // namespace config
} // namespace common
} // namespace core
} // namespace exchange
