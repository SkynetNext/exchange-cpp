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

#include "../common/IOrder.h"
#include "../common/MatcherTradeEvent.h"
#include "../common/cmd/OrderCommand.h"
#include <cstdint>
#include <functional>

namespace exchange {
namespace core {
// Forward declaration
namespace common {
struct MatcherTradeEvent;
}

namespace orderbook {

/**
 * Helper class for creating and managing MatcherTradeEvent chains
 * Simplified version without object pooling for now
 */
class OrderBookEventsHelper {
public:
  // Factory function type for creating events
  using EventFactory = std::function<common::MatcherTradeEvent *()>;

  explicit OrderBookEventsHelper(EventFactory factory = []() {
    return new common::MatcherTradeEvent();
  });

  // Create a trade event
  common::MatcherTradeEvent *SendTradeEvent(const common::IOrder *matchingOrder,
                                            bool makerCompleted,
                                            bool takerCompleted, int64_t size,
                                            int64_t bidderHoldPrice);

  // Create a reduce event
  common::MatcherTradeEvent *SendReduceEvent(const common::IOrder *order,
                                             int64_t reduceSize,
                                             bool completed);

  // Attach a reject event to command
  void AttachRejectEvent(common::cmd::OrderCommand *cmd, int64_t rejectedSize);

  // Static instance for non-pooled events
  static OrderBookEventsHelper *NonPooledEventsHelper();

private:
  EventFactory eventFactory_;

  common::MatcherTradeEvent *NewMatcherEvent();
};

} // namespace orderbook
} // namespace core
} // namespace exchange
