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

#include <cstdint>
#include <vector>

namespace exchange {
namespace core {
namespace common {

enum class MatcherEventType : uint8_t;

struct MatcherTradeEvent {
  MatcherEventType eventType;  // TRADE, REDUCE, REJECT (rare) or BINARY_EVENT (reports data)

  int32_t section;

  // false, except when activeOrder is completely filled, removed or rejected
  // it is always true for REJECT event
  // it is true for REDUCE event if reduce was triggered by COMMAND
  bool activeOrderCompleted;

  // maker (for TRADE event type only)
  int64_t matchedOrderId;
  int64_t matchedOrderUid;     // 0 for rejection
  bool matchedOrderCompleted;  // false, except when matchedOrder is completely
                               // filled

  // actual price of the deal (from maker order), 0 for rejection
  int64_t price;

  // TRADE - trade size
  // REDUCE - effective reduce size of REDUCE command, or not filled size for
  // CANCEL command REJECT - unmatched size of rejected order
  int64_t size;

  // frozen price from BID order owner (depends on activeOrderAction)
  int64_t bidderHoldPrice;

  // reference to next event in chain
  MatcherTradeEvent* nextEvent = nullptr;

  // testing only
  MatcherTradeEvent Copy() const;

  // testing only
  MatcherTradeEvent* FindTail();

  int32_t GetChainSize() const;

  static MatcherTradeEvent* CreateEventChain(int32_t chainLength);

  // Delete an entire event chain
  // @param head Head of chain to delete, or nullptr
  static void DeleteChain(MatcherTradeEvent* head);

  // testing only
  static std::vector<MatcherTradeEvent*> AsList(MatcherTradeEvent* head);
};

}  // namespace common
}  // namespace core
}  // namespace exchange
