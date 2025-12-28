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

#include "../collections/art/LongAdaptiveRadixTreeMap.h"
#include "../common/CoreSymbolSpecification.h"
#include "../common/IOrder.h"
#include "../common/MatcherTradeEvent.h"
#include "../common/Order.h"
#include "../common/cmd/OrderCommand.h"
#include "../common/config/LoggingConfiguration.h"
#include "IOrderBook.h"
#include "OrderBookEventsHelper.h"
#include <algorithm>
#include <cstdint>
#include <type_traits>
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
    int64_t price = 0; // Price level for this bucket
    OrderBookDirectImpl::DirectOrder *lastOrder =
        nullptr;             // Tail order (worst priority in this price level)
    int64_t totalVolume = 0; // Total volume of all orders at this price level
    int32_t numOrders = 0;   // Number of orders at this price level
  };

  struct DirectOrder : public common::IOrder<DirectOrder>,
                       public common::WriteBytesMarshallable,
                       public common::StateHash {
    int64_t orderId = 0; // Unique order identifier
    int64_t price = 0;   // Order price
    int64_t size = 0;    // Original order size
    int64_t filled = 0;  // Filled quantity
    int64_t reserveBidPrice =
        0; // Reserved price for fast moves of GTC bid orders in exchange mode
    int64_t uid = 0; // User ID who placed this order
    common::OrderAction action =
        common::OrderAction::ASK; // Order side (ASK/BID)
    int64_t timestamp = 0;        // Order timestamp

    DirectOrder *next = nullptr; // Next order in global price-sorted linked
                                 // list (towards better price for matching, or
                                 // older order within same price)
    DirectOrder *prev =
        nullptr; // Previous order in global price-sorted linked list
                 // (towards worse price, or newer order within same price)
    Bucket *bucket =
        nullptr; // Price bucket index entry (ART tree maps price -> bucket)

    DirectOrder() = default;

    /**
     * Constructor from BytesIn (deserialization)
     */
    DirectOrder(common::BytesIn &bytes);

    // CRTP methods - used by IOrderBase<DirectOrder> for compile-time
    // polymorphism These are non-virtual methods, eliminating virtual function
    // overhead in hot paths
    int64_t GetOrderId() const { return orderId; }
    int64_t GetPrice() const { return price; }
    int64_t GetSize() const { return size; }
    int64_t GetFilled() const { return filled; }
    int64_t GetReserveBidPrice() const { return reserveBidPrice; }
    common::OrderAction GetAction() const { return action; }
    int64_t GetUid() const { return uid; }
    int64_t GetTimestamp() const { return timestamp; }

    // StateHash interface
    int32_t GetStateHash() const override;

    // WriteBytesMarshallable interface
    void WriteMarshallable(common::BytesOut &bytes) const override;
  };

  OrderBookDirectImpl(
      const common::CoreSymbolSpecification *symbolSpec,
      ::exchange::core::collections::objpool::ObjectsPool *objectsPool,
      OrderBookEventsHelper *eventsHelper,
      const common::config::LoggingConfiguration *loggingCfg);

  /**
   * Constructor from BytesIn (deserialization)
   */
  OrderBookDirectImpl(
      common::BytesIn *bytes,
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
  std::shared_ptr<common::L2MarketData>
  GetL2MarketDataSnapshot(int32_t size) override;
  int32_t GetOrdersNum(common::OrderAction action) override;
  int64_t GetTotalOrdersVolume(common::OrderAction action) override;

  // Type-safe GetOrderById - specialization for DirectOrder
  DirectOrder *GetOrderById(int64_t orderId);

  // Generic template implementation (hides base class template)
  template <typename OrderT> OrderT *GetOrderById(int64_t orderId) {
    // Only DirectOrder is supported for OrderBookDirectImpl
    if constexpr (std::is_same_v<OrderT, DirectOrder>) {
      return GetOrderById(orderId); // Call non-template overload
    } else {
      // For other types, return nullptr (type mismatch)
      return nullptr;
    }
  }

  void ValidateInternalState() override;
  OrderBookImplType GetImplementationType() const override;
  void FillAsks(int32_t size, common::L2MarketData *data) override;
  void FillBids(int32_t size, common::L2MarketData *data) override;
  int32_t GetTotalAskBuckets(int32_t limit) override;
  int32_t GetTotalBidBuckets(int32_t limit) override;
  void Reset();

  // StateHash interface
  int32_t GetStateHash() const override;

  // Debug methods (IOrderBook interface)
  std::string PrintAskBucketsDiagram() const override;
  std::string PrintBidBucketsDiagram() const override;

  // Process orders methods - template functions for compile-time polymorphism
  template <typename Consumer> void ProcessAskOrders(Consumer consumer) const {
    // Match Java: askOrdersStream() uses OrdersSpliterator which starts from
    // bestAskOrder and traverses via prev pointer
    DirectOrder *current = bestAskOrder_;
    while (current != nullptr) {
      consumer(current);
      current = current->prev;
    }
  }

  template <typename Consumer> void ProcessBidOrders(Consumer consumer) const {
    // Match Java: bidOrdersStream() uses OrdersSpliterator which starts from
    // bestBidOrder and traverses via prev pointer
    DirectOrder *current = bestBidOrder_;
    while (current != nullptr) {
      consumer(current);
      current = current->prev;
    }
  }

  // Find user orders (IOrderBook interface)
  std::vector<common::Order *> FindUserOrders(int64_t uid) override;

  // WriteBytesMarshallable interface
  void WriteMarshallable(common::BytesOut &bytes) const override;

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
  // Template function for hot path - uses CRTP, no virtual function overhead
  // Accepts any type that implements IOrder<Self> or has direct field
  // access
  template <typename OrderT>
  int64_t tryMatchInstantly(OrderT *takerOrder,
                            common::cmd::OrderCommand *triggerCmd);
  int64_t checkBudgetToFill(common::OrderAction action, int64_t size);
  bool isBudgetLimitSatisfied(common::OrderAction orderAction,
                              int64_t calculated, int64_t limit);
};

// Template implementation of tryMatchInstantly - uses CRTP, no virtual function
// overhead
template <typename OrderT>
int64_t
OrderBookDirectImpl::tryMatchInstantly(OrderT *takerOrder,
                                       common::cmd::OrderCommand *triggerCmd) {
  // Use CRTP methods from IOrderBase<OrderT> - compile-time polymorphism
  const bool isBidAction = takerOrder->GetAction() == common::OrderAction::BID;
  // For FOK_BUDGET ASK orders, use 0 as limitPrice to match all available bids
  const int64_t limitPrice =
      (triggerCmd->command == common::cmd::OrderCommandType::PLACE_ORDER &&
       triggerCmd->orderType == common::OrderType::FOK_BUDGET && !isBidAction)
          ? 0L
          : takerOrder->GetPrice();

  DirectOrder *makerOrder;
  if (isBidAction) {
    makerOrder = bestAskOrder_;
    if (makerOrder == nullptr || makerOrder->price > limitPrice) {
      return takerOrder->GetFilled();
    }
  } else {
    makerOrder = bestBidOrder_;
    if (makerOrder == nullptr || makerOrder->price < limitPrice) {
      return takerOrder->GetFilled();
    }
  }

  int64_t remainingSize = takerOrder->GetSize() - takerOrder->GetFilled();
  if (remainingSize == 0)
    return takerOrder->GetFilled();

  DirectOrder *priceBucketTail = makerOrder->bucket->lastOrder;
  common::MatcherTradeEvent *eventsTail = nullptr;

  const int64_t takerReserveBidPrice = takerOrder->GetReserveBidPrice();

  do {
    const int64_t tradeSize =
        std::min(remainingSize, makerOrder->size - makerOrder->filled);
    makerOrder->filled += tradeSize;
    makerOrder->bucket->totalVolume -= tradeSize;
    remainingSize -= tradeSize;

    const bool makerCompleted = (makerOrder->size == makerOrder->filled);
    if (makerCompleted) {
      makerOrder->bucket->numOrders--;
    }

    const int64_t bidderHoldPrice =
        isBidAction ? takerReserveBidPrice : makerOrder->reserveBidPrice;
    // Use template function for SendTradeEvent - compile-time polymorphism
    common::MatcherTradeEvent *tradeEvent = eventsHelper_->SendTradeEvent(
        makerOrder, makerCompleted, remainingSize == 0, tradeSize,
        bidderHoldPrice);

    if (eventsTail == nullptr) {
      triggerCmd->matcherEvent = tradeEvent;
    } else {
      eventsTail->nextEvent = tradeEvent;
    }
    eventsTail = tradeEvent;

    if (!makerCompleted)
      break;

    orderIdIndex_.Remove(makerOrder->orderId);
    DirectOrder *toRecycle = makerOrder;

    if (makerOrder == priceBucketTail) {
      auto &buckets = isBidAction ? askPriceBuckets_ : bidPriceBuckets_;
      buckets.Remove(makerOrder->price);
      objectsPool_->Put(
          ::exchange::core::collections::objpool::ObjectsPool::DIRECT_BUCKET,
          makerOrder->bucket);

      if (makerOrder->prev != nullptr) {
        priceBucketTail = makerOrder->prev->bucket->lastOrder;
      }
    }

    makerOrder = makerOrder->prev;
    objectsPool_->Put(
        ::exchange::core::collections::objpool::ObjectsPool::DIRECT_ORDER,
        toRecycle);

  } while (makerOrder != nullptr && remainingSize > 0 &&
           (isBidAction ? makerOrder->price <= limitPrice
                        : makerOrder->price >= limitPrice));

  if (makerOrder != nullptr) {
    makerOrder->next = nullptr;
  }

  if (isBidAction) {
    bestAskOrder_ = makerOrder;
  } else {
    bestBidOrder_ = makerOrder;
  }

  return takerOrder->GetSize() - remainingSize;
}

} // namespace orderbook
} // namespace core
} // namespace exchange
