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

// IOrder is now a template class, forward declaration in common namespace
// Include IOrder.h only when needed in implementation files
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
class Order;
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
   * Template function for type-safe access - caller specifies the expected
   * order type. Returns nullptr if order not found or type mismatch.
   *
   * Usage:
   *   auto* order = orderBook->GetOrderById<DirectOrder>(orderId);
   *   auto* order = orderBook->GetOrderById<common::Order>(orderId);
   */
  // Template function - derived classes provide their own implementation (hides
  // this one)
  template <typename OrderT> OrderT *GetOrderById(int64_t orderId) {
    // Default implementation - derived classes should provide their own
    // This definition is required for template instantiation, but derived
    // classes will hide this with their own template implementation
    (void)orderId;  // Suppress unused parameter warning
    return nullptr; // Default: not found
  }

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
   * Returns shared_ptr to allow sharing between OrderCommands (matching Java
   * behavior)
   */
  virtual std::shared_ptr<common::L2MarketData>
  GetL2MarketDataSnapshot(int32_t size) = 0;

  virtual std::shared_ptr<common::L2MarketData> GetL2MarketDataSnapshot() {
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
   * Process all ask orders with a callback function
   * Matches Java askOrdersStream() - iterates over all ask orders
   * Note: Template function for compile-time polymorphism.
   * Consumer should accept the order type specific to the implementation.
   */
  // Template function - derived classes provide their own implementation (hides
  // this one)
  template <typename Consumer> void ProcessAskOrders(Consumer consumer) const {
    // Default implementation - derived classes should provide their own
    // This definition is required for template instantiation, but derived
    // classes will hide this with their own template implementation
    (void)consumer; // Suppress unused parameter warning
    // In practice, this should never be called as derived classes provide
    // implementation
  }

  /**
   * Process all bid orders with a callback function
   * Matches Java bidOrdersStream() - iterates over all bid orders
   * Note: Template function for compile-time polymorphism.
   * Consumer should accept the order type specific to the implementation.
   * Derived classes must provide implementation.
   */
  // Template function - derived classes provide their own implementation (hides
  // this one)
  template <typename Consumer> void ProcessBidOrders(Consumer consumer) const {
    // Default implementation - derived classes should provide their own
    // This definition is required for template instantiation, but derived
    // classes will hide this with their own template implementation
    (void)consumer; // Suppress unused parameter warning
    // In practice, this should never be called as derived classes provide
    // implementation
  }

  /**
   * Search for all orders for specified user
   * Matches Java findUserOrders(long uid)
   * Slow, because order book does not maintain uid-to-order index
   * @param uid user id
   * @return list of orders
   */
  virtual std::vector<common::Order *> FindUserOrders(int64_t uid) = 0;

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
