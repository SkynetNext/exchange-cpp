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
#include <functional>
#include <map>
#include <memory>
#include "../collections/objpool/ObjectsPool.h"
#include "../common/CoreSymbolSpecification.h"
#include "../common/Order.h"
#include "IOrderBook.h"
#include "OrderBookEventsHelper.h"
#include "OrdersBucket.h"

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
  OrderBookNaiveImpl(const common::CoreSymbolSpecification* symbolSpec,
                     ::exchange::core::collections::objpool::ObjectsPool* objectsPool = nullptr,
                     OrderBookEventsHelper* eventsHelper = nullptr);

  /**
   * Constructor from BytesIn (deserialization)
   */
  OrderBookNaiveImpl(common::BytesIn* bytes,
                     const common::config::LoggingConfiguration* loggingCfg);

  // IOrderBook interface
  void NewOrder(common::cmd::OrderCommand* cmd) override;
  common::cmd::CommandResultCode CancelOrder(common::cmd::OrderCommand* cmd) override;
  common::cmd::CommandResultCode ReduceOrder(common::cmd::OrderCommand* cmd) override;
  common::cmd::CommandResultCode MoveOrder(common::cmd::OrderCommand* cmd) override;

  int32_t GetOrdersNum(common::OrderAction action) override;
  int64_t GetTotalOrdersVolume(common::OrderAction action) override;
  common::IOrder* GetOrderById(int64_t orderId) override;
  void ValidateInternalState() override;

  OrderBookImplType GetImplementationType() const override {
    return OrderBookImplType::NAIVE;
  }

  const common::CoreSymbolSpecification* GetSymbolSpec() const override {
    return symbolSpec_;
  }

  /**
   * WriteMarshallable interface
   */
  void WriteMarshallable(common::BytesOut& bytes) const override;

  std::shared_ptr<common::L2MarketData> GetL2MarketDataSnapshot(int32_t size) override;
  void FillAsks(int32_t size, common::L2MarketData* data) override;
  void FillBids(int32_t size, common::L2MarketData* data) override;
  int32_t GetTotalAskBuckets(int32_t limit) override;
  int32_t GetTotalBidBuckets(int32_t limit) override;

  // StateHash interface
  int32_t GetStateHash() const override;

  // Debug methods (IOrderBook interface)
  std::string PrintAskBucketsDiagram() const override;
  std::string PrintBidBucketsDiagram() const override;

  // Process orders methods (IOrderBook interface)
  void ProcessAskOrders(std::function<void(const common::IOrder*)> consumer) const override;
  void ProcessBidOrders(std::function<void(const common::IOrder*)> consumer) const override;

  // Find user orders (IOrderBook interface)
  std::vector<common::Order*> FindUserOrders(int64_t uid) override;

private:
  const common::CoreSymbolSpecification* symbolSpec_;
  OrderBookEventsHelper* eventsHelper_;
  bool logDebug_ = false;

  // Price-indexed buckets (ask: ascending, bid: descending)
  std::map<int64_t, std::unique_ptr<OrdersBucket>> askBuckets_;
  std::map<int64_t, std::unique_ptr<OrdersBucket>, std::greater<int64_t>> bidBuckets_;

  // Fast lookup by order ID
  // Using ankerl::unordered_dense for better performance (2-3x faster than
  // std::unordered_map) Equivalent to Java's LongObjectHashMap<Order>
  ankerl::unordered_dense::map<int64_t, common::Order*> idMap_;

  // Helper methods - use template to handle both map types safely
  template <typename MapType>
  MapType* GetBucketsByActionImpl(common::OrderAction action);

  // Wrapper methods for backward compatibility
  std::map<int64_t, std::unique_ptr<OrdersBucket>>* GetBucketsByAction(common::OrderAction action);

  const std::map<int64_t, std::unique_ptr<OrdersBucket>>*
  GetBucketsByAction(common::OrderAction action) const;

  // Direct access methods that return the correct type
  std::map<int64_t, std::unique_ptr<OrdersBucket>>* GetAskBuckets() {
    return &askBuckets_;
  }

  std::map<int64_t, std::unique_ptr<OrdersBucket>, std::greater<int64_t>>* GetBidBuckets() {
    return &bidBuckets_;
  }

  // Iterator range for matching buckets (avoids type mismatch between ask/bid
  // maps)
  struct MatchingRange {
    // Use type erasure to handle both ascending and descending maps
    std::function<void(std::function<void(int64_t, OrdersBucket*)>)> forEach;
    std::function<void(int64_t)> erasePrice;
    bool empty;

    MatchingRange() : empty(true) {}

    template <typename Iterator>
    MatchingRange(Iterator begin,
                  Iterator end,
                  std::function<bool(int64_t)> shouldContinue,
                  std::function<void(int64_t)> eraseFunc)
      : erasePrice(eraseFunc) {
      // Check if range is empty or if first element doesn't satisfy condition
      if (begin == end) {
        empty = true;
        forEach = [](std::function<void(int64_t, OrdersBucket*)>) {};
      } else {
        // Check if first element satisfies condition
        int64_t firstPrice = begin->first;
        empty = !shouldContinue(firstPrice);
        forEach = [begin, end,
                   shouldContinue](std::function<void(int64_t, OrdersBucket*)> callback) {
          for (auto it = begin; it != end; ++it) {
            int64_t price = it->first;
            if (!shouldContinue(price)) {
              break;
            }
            callback(price, it->second.get());
          }
        };
      }
    }
  };

  // Get matching buckets range (price <= limit for ASK, price >= limit for BID)
  MatchingRange GetMatchingRange(common::OrderAction action, int64_t price);

  // Try to match order instantly against matching buckets range
  int64_t TryMatchInstantly(const common::IOrder* activeOrder,
                            MatchingRange& matchingRange,
                            int64_t filled,
                            common::cmd::OrderCommand* triggerCmd);

  // Order type specific handlers
  void NewOrderPlaceGtc(common::cmd::OrderCommand* cmd);
  void NewOrderMatchIoc(common::cmd::OrderCommand* cmd);
  void NewOrderMatchFokBudget(common::cmd::OrderCommand* cmd);

  // Check budget for FOK_BUDGET orders
  bool CheckBudgetToFill(int64_t size, MatchingRange& matchingRange, int64_t* budgetOut);

  // Check if budget limit is satisfied (matches Java isBudgetLimitSatisfied)
  bool IsBudgetLimitSatisfied(common::OrderAction orderAction, int64_t calculated, int64_t limit);
};

}  // namespace orderbook
}  // namespace core
}  // namespace exchange
