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
#include <memory>
#include <vector>
#include "../collections/art/LongAdaptiveRadixTreeMap.h"
#include "../common/CoreSymbolSpecification.h"
#include "../common/Order.h"
#include "../common/config/LoggingConfiguration.h"
#include "IOrderBook.h"
#include "OrderBookDirectTypes.h"
#include "OrderBookEventsHelper.h"
#include "OrderBookPoolContext.h"

namespace exchange::core::orderbook {

/**
 * OrderBookDirectImpl - direct order book implementation using ART tree
 * High-performance implementation with custom data structures
 */
class OrderBookDirectImpl : public IOrderBook {
public:
  OrderBookDirectImpl(const OrderBookDirectImpl&) = delete;
  OrderBookDirectImpl& operator=(const OrderBookDirectImpl&) = delete;

  OrderBookDirectImpl(const common::CoreSymbolSpecification* symbolSpec,
                      const OrderBookPoolContext* poolContext,
                      OrderBookEventsHelper* eventsHelper,
                      const common::config::LoggingConfiguration* loggingCfg);

  /**
   * Constructor from BytesIn (deserialization)
   */
  OrderBookDirectImpl(common::BytesIn* bytes,
                      const OrderBookPoolContext* poolContext,
                      OrderBookEventsHelper* eventsHelper,
                      const common::config::LoggingConfiguration* loggingCfg);

  // ... (rest of public interface remains same)
  const common::CoreSymbolSpecification* GetSymbolSpec() const override;
  void NewOrder(common::cmd::OrderCommand* cmd) override;
  common::cmd::CommandResultCode CancelOrder(common::cmd::OrderCommand* cmd) override;
  common::cmd::CommandResultCode MoveOrder(common::cmd::OrderCommand* cmd) override;
  common::cmd::CommandResultCode ReduceOrder(common::cmd::OrderCommand* cmd) override;
  std::shared_ptr<common::L2MarketData> GetL2MarketDataSnapshot(int32_t size) override;
  int32_t GetOrdersNum(common::OrderAction action) override;
  int64_t GetTotalOrdersVolume(common::OrderAction action) override;
  common::IOrder* GetOrderById(int64_t orderId) override;
  void ValidateInternalState() override;
  OrderBookImplType GetImplementationType() const override;
  void FillAsks(int32_t size, common::L2MarketData* data) override;
  void FillBids(int32_t size, common::L2MarketData* data) override;
  int32_t GetTotalAskBuckets(int32_t limit) override;
  int32_t GetTotalBidBuckets(int32_t limit) override;
  void Reset();

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

  // WriteBytesMarshallable interface
  void WriteMarshallable(common::BytesOut& bytes) const override;

private:
  // ART node pool for ask/bid price trees (created and owned here)
  std::unique_ptr<::exchange::core::collections::art::ArtPoolContext<Bucket>> artPoolContext_;
  // Price buckets using ART tree
  ::exchange::core::collections::art::LongAdaptiveRadixTreeMap<Bucket> askPriceBuckets_;
  ::exchange::core::collections::art::LongAdaptiveRadixTreeMap<Bucket> bidPriceBuckets_;

  const common::CoreSymbolSpecification* symbolSpec_;
  const OrderBookPoolContext* poolContext_;

  // Order ID index using hash map for O(1) lookup performance
  ankerl::unordered_dense::map<int64_t, DirectOrder*> orderIdIndex_;

  // Best orders
  DirectOrder* bestAskOrder_;
  DirectOrder* bestBidOrder_;

  OrderBookEventsHelper* eventsHelper_;
  bool logDebug_;

  // Internal methods
  DirectOrder* FindOrder(int64_t orderId);
  Bucket* GetOrCreateBucket(int64_t price, bool isAsk);
  Bucket* RemoveOrder(DirectOrder* order);
  void insertOrder(DirectOrder* order, Bucket* freeBucket);
  int64_t tryMatchInstantly(common::IOrder* takerOrder, common::cmd::OrderCommand* triggerCmd);
  int64_t checkBudgetToFill(common::OrderAction action, int64_t size);
  bool isBudgetLimitSatisfied(common::OrderAction orderAction, int64_t calculated, int64_t limit);
};

}  // namespace exchange::core::orderbook
