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
#include "../common/L2MarketData.h"
#include "../common/OrderAction.h"
#include "../common/StateHash.h"
#include "../common/WriteBytesMarshallable.h"
#include "../common/cmd/CommandResultCode.h"
#include "../common/cmd/OrderCommand.h"
#include <cstdint>
#include <memory>

namespace exchange {
namespace core {
namespace common {
class CoreSymbolSpecification;
class BytesIn;
namespace config {
class LoggingConfiguration;
}
} // namespace common
namespace collections {
namespace objpool {
class ObjectsPool;
}
} // namespace collections

namespace orderbook {

class OrderBookEventsHelper;

enum class OrderBookImplType : uint8_t { NAIVE = 0, DIRECT = 2 };

/**
 * OrderBook interface - manages buy and sell orders for a symbol
 */
class IOrderBook : public common::StateHash,
                   public common::WriteBytesMarshallable {
public:
  virtual ~IOrderBook() = default;

  /**
   * Process new order
   * Depending on price, order will be matched to existing opposite orders
   * IOC orders will be rejected if not fully matched
   * GTC orders will be placed in the order book if not fully matched
   */
  virtual void NewOrder(common::cmd::OrderCommand *cmd) = 0;

  /**
   * Cancel order completely
   * Fills cmd.action with original order action
   */
  virtual common::cmd::CommandResultCode
  CancelOrder(common::cmd::OrderCommand *cmd) = 0;

  /**
   * Decrease the size of the order
   * Fills cmd.action with original order action
   */
  virtual common::cmd::CommandResultCode
  ReduceOrder(common::cmd::OrderCommand *cmd) = 0;

  /**
   * Move order to new price
   * Fills cmd.action with original order action
   */
  virtual common::cmd::CommandResultCode
  MoveOrder(common::cmd::OrderCommand *cmd) = 0;

  /**
   * Get number of orders for action (testing only)
   */
  virtual int32_t GetOrdersNum(common::OrderAction action) = 0;

  /**
   * Get total orders volume for action (testing only)
   */
  virtual int64_t GetTotalOrdersVolume(common::OrderAction action) = 0;

  /**
   * Get order by ID (testing only)
   */
  virtual common::IOrder *GetOrderById(int64_t orderId) = 0;

  /**
   * Validate internal state (testing only)
   */
  virtual void ValidateInternalState() = 0;

  /**
   * Get implementation type
   */
  virtual OrderBookImplType GetImplementationType() const = 0;

  /**
   * Get symbol specification
   */
  virtual const common::CoreSymbolSpecification *GetSymbolSpec() const = 0;

  /**
   * Get L2 Market Data snapshot
   */
  virtual std::unique_ptr<common::L2MarketData>
  GetL2MarketDataSnapshot(int32_t size) = 0;

  virtual std::unique_ptr<common::L2MarketData> GetL2MarketDataSnapshot() {
    return GetL2MarketDataSnapshot(INT32_MAX);
  }

  /**
   * Fill asks into L2MarketData
   */
  virtual void FillAsks(int32_t size, common::L2MarketData *data) = 0;

  /**
   * Fill bids into L2MarketData
   */
  virtual void FillBids(int32_t size, common::L2MarketData *data) = 0;

  /**
   * Get total ask buckets
   */
  virtual int32_t GetTotalAskBuckets(int32_t limit) = 0;

  /**
   * Get total bid buckets
   */
  virtual int32_t GetTotalBidBuckets(int32_t limit) = 0;

  /**
   * Debug methods for testing - print internal state diagrams
   * These methods are useful for debugging and comparing implementations
   */
  virtual std::string PrintAskBucketsDiagram() const = 0;
  virtual std::string PrintBidBucketsDiagram() const = 0;

  /**
   * Process command - static helper method
   */
  static common::cmd::CommandResultCode
  ProcessCommand(IOrderBook *orderBook, common::cmd::OrderCommand *cmd);

  /**
   * Create OrderBook from BytesIn (deserialization)
   */
  static std::unique_ptr<IOrderBook>
  Create(common::BytesIn *bytes,
         ::exchange::core::collections::objpool::ObjectsPool *objectsPool,
         OrderBookEventsHelper *eventsHelper,
         const common::config::LoggingConfiguration *loggingCfg);
};

} // namespace orderbook
} // namespace core
} // namespace exchange
