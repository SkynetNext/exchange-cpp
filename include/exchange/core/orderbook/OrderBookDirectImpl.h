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

// Forward declarations
class ObjectsPool;

/**
 * OrderBookDirectImpl - direct order book implementation using ART tree
 * High-performance implementation with custom data structures
 */
class OrderBookDirectImpl : public IOrderBook {
public:
  struct DirectOrder {
    int64_t orderId;
    int64_t price;
    int64_t size;
    int64_t filled;
    int64_t uid;
    common::OrderAction action;
    int64_t timestamp;

    DirectOrder *next;
    DirectOrder *prev;
    struct Bucket *bucket;
  };

  struct Bucket {
    int64_t price;
    DirectOrder *firstOrder;
    DirectOrder *lastOrder;
    int64_t totalVolume;
    int32_t numOrders;
  };

  OrderBookDirectImpl(const common::CoreSymbolSpecification *symbolSpec,
                      ObjectsPool *objectsPool,
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
  collections::art::LongAdaptiveRadixTreeMap<Bucket> askPriceBuckets_;
  collections::art::LongAdaptiveRadixTreeMap<Bucket> bidPriceBuckets_;

  const common::CoreSymbolSpecification *symbolSpec_;
  ObjectsPool *objectsPool_;

  // Order ID index using ART tree for consistency and performance
  collections::art::LongAdaptiveRadixTreeMap<DirectOrder> orderIdIndex_;

  // Best orders
  DirectOrder *bestAskOrder_;
  DirectOrder *bestBidOrder_;

  OrderBookEventsHelper *eventsHelper_;
  bool logDebug_;

  // Internal methods
  DirectOrder *FindOrder(int64_t orderId);
  Bucket *GetOrCreateBucket(int64_t price, bool isAsk);
  void RemoveOrder(DirectOrder *order);
};

} // namespace orderbook
} // namespace core
} // namespace exchange
