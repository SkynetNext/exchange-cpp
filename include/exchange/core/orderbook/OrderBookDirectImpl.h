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

#include "../collections/art/LongAdaptiveRadixTreeMap.h"
#include "../common/CoreSymbolSpecification.h"
#include "../common/Order.h"
#include "../common/config/LoggingConfiguration.h"
#include "IOrderBook.h"
#include "OrderBookEventsHelper.h"
#include <cstdint>
#include <vector>

namespace exchange {
namespace core {
namespace orderbook {

/**
 * OrderBookDirectImpl - direct order book implementation using ART tree
 * High-performance implementation with custom data structures
 */
class OrderBookDirectImpl : public IOrderBook {
public:
  // Forward declaration
  struct DirectOrder;

  struct Bucket {
    int64_t price;
    OrderBookDirectImpl::DirectOrder
        *lastOrder; // tail order (worst priority in this price level)
    int64_t totalVolume;
    int32_t numOrders;
  };

  struct DirectOrder : public common::IOrder {
    int64_t orderId;
    int64_t price;
    int64_t size;
    int64_t filled;
    int64_t uid;
    common::OrderAction action;
    int64_t timestamp;

    DirectOrder *next;
    DirectOrder *prev;
    Bucket *bucket;

    // IOrder interface
    int64_t GetOrderId() const override { return orderId; }
    int64_t GetPrice() const override { return price; }
    int64_t GetSize() const override { return size; }
    int64_t GetFilled() const override { return filled; }
    int64_t GetReserveBidPrice() const override { return 0; }
    common::OrderAction GetAction() const override { return action; }
    int64_t GetUid() const override { return uid; }
    int64_t GetTimestamp() const override { return timestamp; }
  };

  OrderBookDirectImpl(
      const common::CoreSymbolSpecification *symbolSpec,
      ::exchange::core::collections::objpool::ObjectsPool *objectsPool,
      OrderBookEventsHelper *eventsHelper,
      const common::config::LoggingConfiguration *loggingCfg);

  // ... (rest of public interface remains same)
  const common::CoreSymbolSpecification *GetSymbolSpec() const override;
  void NewOrder(common::cmd::OrderCommand *cmd) override;
  common::cmd::CommandResultCode
  CancelOrder(common::cmd::OrderCommand *cmd) override;
  common::cmd::CommandResultCode
  MoveOrder(common::cmd::OrderCommand *cmd) override;
  common::cmd::CommandResultCode
  ReduceOrder(common::cmd::OrderCommand *cmd) override;
  std::unique_ptr<common::L2MarketData>
  GetL2MarketDataSnapshot(int32_t size) override;
  int32_t GetOrdersNum(common::OrderAction action) override;
  int64_t GetTotalOrdersVolume(common::OrderAction action) override;
  common::IOrder *GetOrderById(int64_t orderId) override;
  void ValidateInternalState() override;
  OrderBookImplType GetImplementationType() const override;
  void FillAsks(int32_t size, common::L2MarketData *data) override;
  void FillBids(int32_t size, common::L2MarketData *data) override;
  int32_t GetTotalAskBuckets(int32_t limit) override;
  int32_t GetTotalBidBuckets(int32_t limit) override;
  std::vector<common::Order *> FindUserOrders(int64_t uid);
  void Reset();

private:
  // Price buckets using ART tree
  ::exchange::core::collections::art::LongAdaptiveRadixTreeMap<Bucket>
      askPriceBuckets_;
  ::exchange::core::collections::art::LongAdaptiveRadixTreeMap<Bucket>
      bidPriceBuckets_;

  const common::CoreSymbolSpecification *symbolSpec_;
  ::exchange::core::collections::objpool::ObjectsPool *objectsPool_;

  // Order ID index using ART tree for consistency and performance
  ::exchange::core::collections::art::LongAdaptiveRadixTreeMap<DirectOrder>
      orderIdIndex_;

  // Best orders
  DirectOrder *bestAskOrder_;
  DirectOrder *bestBidOrder_;

  OrderBookEventsHelper *eventsHelper_;
  bool logDebug_;

  // Internal methods
  DirectOrder *FindOrder(int64_t orderId);
  Bucket *GetOrCreateBucket(int64_t price, bool isAsk);
  Bucket *RemoveOrder(DirectOrder *order);
  void insertOrder(DirectOrder *order, Bucket *freeBucket);
  int64_t tryMatchInstantly(common::IOrder *takerOrder,
                            common::cmd::OrderCommand *triggerCmd);
};

} // namespace orderbook
} // namespace core
} // namespace exchange
