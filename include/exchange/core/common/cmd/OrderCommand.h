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

#include <cstdint>
#include <functional>
#include <memory>
#include <new> // for alignas
#include <stdexcept>
#include <vector>

// Forward declarations
namespace exchange {
namespace core {
namespace common {
class IOrder;
class StateHash;
class L2MarketData;
struct MatcherTradeEvent;
enum class OrderAction : uint8_t;
enum class OrderType : uint8_t;
} // namespace common
} // namespace core
} // namespace exchange

#include "../IOrder.h"
#include "../L2MarketData.h"
#include "../MatcherTradeEvent.h"
#include "../OrderAction.h"
#include "../OrderType.h"
#include "../StateHash.h"
#include "CommandResultCode.h"
#include "OrderCommandType.h"

namespace exchange {
namespace core {
namespace common {
namespace cmd {

/**
 * OrderCommand - Disruptor event core structure
 *
 * Memory layout optimized for cache-line alignment (64 bytes)
 * to avoid false sharing in multi-threaded environment
 */
struct alignas(64) OrderCommand : public IOrder, public StateHash {
  OrderCommandType command = OrderCommandType::NOP;

  int64_t orderId = 0;
  int32_t symbol = 0;
  int64_t price = 0;
  int64_t size = 0;

  // new orders INPUT - reserved price for fast moves of GTC bid orders in
  // exchange mode
  int64_t reserveBidPrice = 0;

  // required for PLACE_ORDER only;
  // for CANCEL/MOVE contains original order action (filled by orderbook)
  OrderAction action = OrderAction::ASK;
  OrderType orderType = OrderType::GTC;

  int64_t uid = 0;
  int64_t timestamp = 0;
  int32_t userCookie = 0;

  // filled by grouping processor:
  int64_t eventsGroup = 0;
  int32_t serviceFlags = 0;

  // result code of command execution - can also be used for saving intermediate
  // state
  CommandResultCode resultCode = CommandResultCode::NEW;

  // trade events chain
  MatcherTradeEvent *matcherEvent = nullptr;

  // optional market data
  std::unique_ptr<L2MarketData> marketData = nullptr;

  // ---- potential false sharing section ------

  // Static factory methods
  static OrderCommand NewOrder(OrderType orderType, int64_t orderId,
                               int64_t uid, int64_t price,
                               int64_t reserveBidPrice, int64_t size,
                               OrderAction action);

  static OrderCommand Cancel(int64_t orderId, int64_t uid);

  static OrderCommand Reduce(int64_t orderId, int64_t uid, int64_t reduceSize);

  static OrderCommand Update(int64_t orderId, int64_t uid, int64_t price);

  /**
   * Handles full MatcherTradeEvent chain, without removing/revoking them
   */
  template <typename Handler> void ProcessMatcherEvents(Handler &&handler) {
    MatcherTradeEvent *mte = this->matcherEvent;
    while (mte != nullptr) {
      handler(mte);
      mte = mte->nextEvent;
    }
  }

  /**
   * Produces garbage - For testing only !!!
   */
  std::vector<MatcherTradeEvent *> ExtractEvents();

  /**
   * Write only command data, not status or events
   */
  void WriteTo(OrderCommand &cmd2) const;

  // slow - testing only
  OrderCommand Copy() const;

  // IOrder interface implementation
  int64_t GetPrice() const override { return price; }
  int64_t GetSize() const override { return size; }
  int64_t GetFilled() const override { return 0; }
  int64_t GetUid() const override { return uid; }
  OrderAction GetAction() const override { return action; }
  int64_t GetOrderId() const override { return orderId; }
  int64_t GetTimestamp() const override { return timestamp; }
  int64_t GetReserveBidPrice() const override { return reserveBidPrice; }

  // StateHash interface implementation
  int32_t GetStateHash() const override {
    throw std::runtime_error("Command does not represents state");
  }
};

} // namespace cmd
} // namespace common
} // namespace core
} // namespace exchange
