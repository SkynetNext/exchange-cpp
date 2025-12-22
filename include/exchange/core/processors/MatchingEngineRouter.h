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

#include "../common/cmd/OrderCommand.h"
#include "../orderbook/IOrderBook.h"
#include "../orderbook/OrderBookEventsHelper.h"
#include "SymbolSpecificationProvider.h"
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <functional>
#include <memory>

namespace exchange {
namespace core {
namespace processors {

// Forward declarations
class SharedPool;

/**
 * MatchingEngineRouter - routes orders to appropriate OrderBook based on
 * SymbolID
 *
 * This is the core matching engine component that:
 * - Routes commands to the correct OrderBook by SymbolID
 * - Manages multiple OrderBooks (one per symbol)
 * - Supports sharding for parallel processing
 */
class MatchingEngineRouter {
public:
  // OrderBook factory function type
  using OrderBookFactory = std::function<std::unique_ptr<orderbook::IOrderBook>(
      const common::CoreSymbolSpecification *spec,
      orderbook::OrderBookEventsHelper *eventsHelper)>;

  MatchingEngineRouter(int32_t shardId, int64_t numShards,
                       OrderBookFactory orderBookFactory,
                       SharedPool *sharedPool,
                       SymbolSpecificationProvider *symbolSpecProvider);

  /**
   * Process an order command
   * Routes to appropriate OrderBook based on symbol ID
   */
  void ProcessOrder(int64_t seq, common::cmd::OrderCommand *cmd);

  /**
   * Add a symbol and create its OrderBook
   */
  void AddSymbol(const common::CoreSymbolSpecification *spec);

  /**
   * Get OrderBook for a symbol
   */
  orderbook::IOrderBook *GetOrderBook(int32_t symbol);

  /**
   * Reset - clear all order books
   */
  void Reset();

  /**
   * Get shard ID
   */
  int32_t GetShardId() const { return shardId_; }

  /**
   * Get all order books (for iteration)
   */
  std::vector<orderbook::IOrderBook *> GetOrderBooks() const;

private:
  int32_t shardId_;
  int64_t shardMask_; // numShards - 1 (must be power of 2)

  SymbolSpecificationProvider *symbolSpecProvider_;
  OrderBookFactory orderBookFactory_;

  // symbol ID -> OrderBook
  // Using ankerl::unordered_dense for better performance
  ankerl::unordered_dense::map<int32_t, std::unique_ptr<orderbook::IOrderBook>>
      orderBooks_;

  // Events helper (shared across all order books)
  std::unique_ptr<orderbook::OrderBookEventsHelper> eventsHelper_;

  // Configuration flags
  bool cfgMarginTradingEnabled_;
  bool cfgSendL2ForEveryCmd_;
  int32_t cfgL2RefreshDepth_;

  /**
   * Check if symbol belongs to this shard
   */
  bool SymbolForThisHandler(int32_t symbol) const;

  /**
   * Process matching command (PLACE_ORDER, CANCEL_ORDER, etc.)
   */
  void ProcessMatchingCommand(common::cmd::OrderCommand *cmd);
};

} // namespace processors
} // namespace core
} // namespace exchange
