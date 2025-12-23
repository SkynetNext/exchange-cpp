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

#include "../collections/objpool/ObjectsPool.h"
#include "../common/CoreSymbolSpecification.h"
#include "../common/Order.h"
#include "IOrderBook.h"
#include "OrderBookEventsHelper.h"
#include "OrdersBucket.h"
#include <ankerl/unordered_dense.h>
#include <map>
#include <memory>

namespace exchange {
namespace core {
namespace orderbook {

/**
 * Naive implementation of OrderBook using std::map for price indexing
 * This is a straightforward implementation for correctness verification
 * Performance-optimized version (OrderBookDirectImpl) will be implemented later
 */
class OrderBookNaiveImpl : public IOrderBook {
public:
  OrderBookNaiveImpl(const common::CoreSymbolSpecification *symbolSpec,
                     ::exchange::core::collections::objpool::ObjectsPool
                         *objectsPool = nullptr,
                     OrderBookEventsHelper *eventsHelper = nullptr);

  // IOrderBook interface
  void NewOrder(common::cmd::OrderCommand *cmd) override;
  common::cmd::CommandResultCode
  CancelOrder(common::cmd::OrderCommand *cmd) override;
  common::cmd::CommandResultCode
  ReduceOrder(common::cmd::OrderCommand *cmd) override;
  common::cmd::CommandResultCode
  MoveOrder(common::cmd::OrderCommand *cmd) override;

  int32_t GetOrdersNum(common::OrderAction action) override;
  int64_t GetTotalOrdersVolume(common::OrderAction action) override;
  common::IOrder *GetOrderById(int64_t orderId) override;
  void ValidateInternalState() override;

  OrderBookImplType GetImplementationType() const override {
    return OrderBookImplType::NAIVE;
  }

  const common::CoreSymbolSpecification *GetSymbolSpec() const override {
    return symbolSpec_;
  }

  std::unique_ptr<common::L2MarketData>
  GetL2MarketDataSnapshot(int32_t size) override;
  void FillAsks(int32_t size, common::L2MarketData *data) override;
  void FillBids(int32_t size, common::L2MarketData *data) override;
  int32_t GetTotalAskBuckets(int32_t limit) override;
  int32_t GetTotalBidBuckets(int32_t limit) override;

  // StateHash interface
  int32_t GetStateHash() const override;

private:
  const common::CoreSymbolSpecification *symbolSpec_;
  OrderBookEventsHelper *eventsHelper_;

  // Price-indexed buckets (ask: ascending, bid: descending)
  std::map<int64_t, std::unique_ptr<OrdersBucket>> askBuckets_;
  std::map<int64_t, std::unique_ptr<OrdersBucket>, std::greater<int64_t>>
      bidBuckets_;

  // Fast lookup by order ID
  // Using ankerl::unordered_dense for better performance (2-3x faster than
  // std::unordered_map) Equivalent to Java's LongObjectHashMap<Order>
  ankerl::unordered_dense::map<int64_t, common::Order *> idMap_;

  // Helper methods
  std::map<int64_t, std::unique_ptr<OrdersBucket>> *
  GetBucketsByAction(common::OrderAction action);

  const std::map<int64_t, std::unique_ptr<OrdersBucket>> *
  GetBucketsByAction(common::OrderAction action) const;

  // Get matching buckets (price <= limit for ASK, price >= limit for BID)
  std::map<int64_t, std::unique_ptr<OrdersBucket>> *
  SubtreeForMatching(common::OrderAction action, int64_t price);

  // Try to match order instantly against matching buckets
  int64_t TryMatchInstantly(
      const common::IOrder *activeOrder,
      std::map<int64_t, std::unique_ptr<OrdersBucket>> *matchingBuckets,
      int64_t filled, common::cmd::OrderCommand *triggerCmd);

  // Order type specific handlers
  void NewOrderPlaceGtc(common::cmd::OrderCommand *cmd);
  void NewOrderMatchIoc(common::cmd::OrderCommand *cmd);
  void NewOrderMatchFokBudget(common::cmd::OrderCommand *cmd);

  // Check budget for FOK_BUDGET orders
  bool CheckBudgetToFill(
      int64_t size,
      const std::map<int64_t, std::unique_ptr<OrdersBucket>> *matchingBuckets,
      int64_t *budgetOut);
};

} // namespace orderbook
} // namespace core
} // namespace exchange
