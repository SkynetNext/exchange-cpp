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

#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include "../collections/objpool/ObjectsPool.h"
#include "../common/WriteBytesMarshallable.h"
#include "../common/api/reports/ReportQuery.h"
#include "../common/cmd/OrderCommand.h"
#include "../common/config/ExchangeConfiguration.h"
#include "../orderbook/IOrderBook.h"
#include "../orderbook/OrderBookEventsHelper.h"
#include "../utils/Logger.h"
#include "BinaryCommandsProcessor.h"
#include "SymbolSpecificationProvider.h"
#include "journaling/ISerializationProcessor.h"

namespace exchange::core::processors {

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
class MatchingEngineRouter : public common::WriteBytesMarshallable {
public:
  // OrderBook factory function type
  // Matches Java IOrderBook.OrderBookFactory signature
  using OrderBookFactory = std::function<std::unique_ptr<orderbook::IOrderBook>(
    const common::CoreSymbolSpecification* spec,
    ::exchange::core::collections::objpool::ObjectsPool* objectsPool,
    orderbook::OrderBookEventsHelper* eventsHelper)>;

  MatchingEngineRouter(int32_t shardId,
                       int64_t numShards,
                       OrderBookFactory orderBookFactory,
                       SharedPool* sharedPool,
                       const common::config::ExchangeConfiguration* exchangeCfg,
                       journaling::ISerializationProcessor* serializationProcessor,
                       SymbolSpecificationProvider* symbolSpecProvider = nullptr);

  /**
   * Process an order command
   * Routes to appropriate OrderBook based on symbol ID
   */
  void ProcessOrder(int64_t seq, common::cmd::OrderCommand* cmd);

  /**
   * Add a symbol and create its OrderBook
   */
  void AddSymbol(const common::CoreSymbolSpecification* spec);

  /**
   * Get OrderBook for a symbol
   */
  orderbook::IOrderBook* GetOrderBook(int32_t symbol);

  /**
   * Reset - clear all order books
   */
  void Reset();

  /**
   * Get shard ID
   */
  int32_t GetShardId() const {
    return shardId_;
  }

  /**
   * Get shard mask
   */
  int64_t GetShardMask() const {
    return shardMask_;
  }

  /**
   * Get all order books (for iteration)
   */
  std::vector<orderbook::IOrderBook*> GetOrderBooks() const;

  /**
   * Get BinaryCommandsProcessor (for external access)
   */
  BinaryCommandsProcessor* GetBinaryCommandsProcessor() {
    return binaryCommandsProcessor_.get();
  }

  /**
   * WriteMarshallable interface
   */
  void WriteMarshallable(common::BytesOut& bytes) const override;

private:
  int32_t shardId_;
  int64_t shardMask_;  // numShards - 1 (must be power of 2)

  std::string exchangeId_;  // TODO validate
  std::filesystem::path folder_;

  SymbolSpecificationProvider* symbolSpecProvider_;
  OrderBookFactory orderBookFactory_;

  // Object pool for order book operations
  // Created in constructor with production configuration
  std::unique_ptr<::exchange::core::collections::objpool::ObjectsPool> objectsPool_;

  // symbol ID -> OrderBook
  // Using ankerl::unordered_dense for better performance
  ankerl::unordered_dense::map<int32_t, std::unique_ptr<orderbook::IOrderBook>> orderBooks_;

  // Events helper (shared across all order books)
  std::unique_ptr<orderbook::OrderBookEventsHelper> eventsHelper_;

  // Binary commands processor
  std::unique_ptr<BinaryCommandsProcessor> binaryCommandsProcessor_;

  // Report queries handler (adapter for BinaryCommandsProcessor)
  std::unique_ptr<common::api::reports::ReportQueriesHandler> reportQueriesHandler_;

  // Serialization processor
  journaling::ISerializationProcessor* serializationProcessor_;

  // Configuration flags
  bool cfgMarginTradingEnabled_;
  bool cfgSendL2ForEveryCmd_;
  int32_t cfgL2RefreshDepth_;
  bool logDebug_;

  /**
   * Check if symbol belongs to this shard
   */
  bool SymbolForThisHandler(int32_t symbol) const;

  /**
   * Process matching command (PLACE_ORDER, CANCEL_ORDER, etc.)
   */
  void ProcessMatchingCommand(common::cmd::OrderCommand* cmd);

  /**
   * Handle binary message (BatchAddSymbolsCommand, BatchAddAccountsCommand)
   */
  void HandleBinaryMessage(common::api::binary::BinaryDataCommand* message);

  /**
   * Handle report query
   */
  template <typename R>
  std::optional<std::unique_ptr<R>> HandleReportQuery(common::api::reports::ReportQuery<R>* query) {
    if (query == nullptr) {
      return std::nullopt;
    }
    // Note: LOG_DEBUG is defined in Logger.h and can be used in headers
    LOG_DEBUG("MatchingEngineRouter::HandleReportQuery: calling "
              "query->Process(this)");
    auto result = query->Process(this);
    LOG_DEBUG("MatchingEngineRouter::HandleReportQuery: Process returned "
              "has_value={}",
              result.has_value());
    return result;
  }
};

}  // namespace exchange::core::processors
