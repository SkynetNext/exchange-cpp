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

#include "../common/IOrder.h"
#include "../common/MatcherTradeEvent.h"
#include "../common/Wire.h"
#include "../common/cmd/OrderCommand.h"
#include <cstdint>
#include <functional>
#include <map>

namespace exchange {
namespace core {
// Forward declaration
namespace common {
struct MatcherTradeEvent;
}

namespace orderbook {

/**
 * Helper class for creating and managing MatcherTradeEvent chains
 * Match Java: OrderBookEventsHelper with EVENTS_POOLING support
 */
class OrderBookEventsHelper {
public:
  // Enable MatcherTradeEvent pooling (matches Java EVENTS_POOLING)
  // Set to true to enable SharedPool-based event pooling
  static constexpr bool EVENTS_POOLING = true;

  // Factory function type for creating events
  using EventFactory = std::function<common::MatcherTradeEvent *()>;

  /**
   * Constructor
   * @param factory Factory function for creating events
   * When EVENTS_POOLING is true, factory should return chains from SharedPool.
   * When EVENTS_POOLING is false, factory should return single events.
   */
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

  // Create a reduce event (overload with price parameters to avoid
  // use-after-free) Use this version when the order will be released/deleted
  // after the call
  common::MatcherTradeEvent *SendReduceEvent(int64_t price,
                                             int64_t reserveBidPrice,
                                             int64_t reduceSize,
                                             bool completed);

  // Attach a reject event to command
  void AttachRejectEvent(common::cmd::OrderCommand *cmd, int64_t rejectedSize);

  // Create binary events chain from bytes
  // Match Java: createBinaryEventsChain()
  common::MatcherTradeEvent *
  CreateBinaryEventsChain(int64_t timestamp, int32_t section,
                          const std::vector<uint8_t> &bytes);

  // Static instance for non-pooled events
  static OrderBookEventsHelper *NonPooledEventsHelper();

  /**
   * Deserialize events from OrderCommand (matches Java deserializeEvents)
   * Extracts binary events from matcherEvent chain and groups by section
   * @param cmd OrderCommand with matcherEvent chain
   * @return Map of section -> Wire (matches Java NavigableMap<Integer, Wire>)
   */
  static std::map<int32_t, common::Wire>
  DeserializeEvents(const common::cmd::OrderCommand *cmd);

private:
  EventFactory eventFactory_;
  // Current chain head for event pooling (matches Java eventsChainHead)
  // When EVENTS_POOLING is true, we maintain a current chain and consume nodes
  // sequentially
  common::MatcherTradeEvent *eventsChainHead_ = nullptr;

  common::MatcherTradeEvent *NewMatcherEvent();
};

} // namespace orderbook
} // namespace core
} // namespace exchange
