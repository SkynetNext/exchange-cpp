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

#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <exchange/core/orderbook/OrderBookNaiveImpl.h>


namespace exchange {
namespace core {
namespace common {
namespace config {

PerformanceConfiguration PerformanceConfiguration::Default() {
  return PerformanceConfiguration(
      16 * 1024, // ringBufferSize
      1,         // matchingEnginesNum
      1,         // riskEnginesNum
      256,       // msgsInGroupLimit
      10'000,    // maxGroupDurationNs (10 microseconds)
      false,     // sendL2ForEveryCmd
      8,         // l2RefreshDepth
      CoreWaitStrategy::BLOCKING,
      [](const CoreSymbolSpecification *spec,
         ::exchange::core::collections::objpool::ObjectsPool *objectsPool,
         orderbook::OrderBookEventsHelper *eventsHelper) {
        // OrderBookNaiveImpl doesn't use ObjectsPool, but accepts it for
        // interface consistency
        return std::make_unique<orderbook::OrderBookNaiveImpl>(
            spec, objectsPool, eventsHelper);
      });
}

} // namespace config
} // namespace common
} // namespace core
} // namespace exchange
