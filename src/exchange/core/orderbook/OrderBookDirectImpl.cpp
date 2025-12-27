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

#include "exchange/core/orderbook/OrderBookDirectImpl.h"
#include "exchange/core/collections/objpool/ObjectsPool.h"
#include "exchange/core/common/L2MarketData.h"
#include "exchange/core/common/MatcherTradeEvent.h"
#include "exchange/core/common/OrderAction.h"
#include "exchange/core/common/OrderType.h"
#include "exchange/core/common/cmd/OrderCommand.h"
#include <algorithm>
#include <sstream>
#include <vector>

namespace exchange {
namespace core {
namespace orderbook {

using namespace exchange::core::common;
using namespace exchange::core::common::cmd;

OrderBookDirectImpl::OrderBookDirectImpl(
    const common::CoreSymbolSpecification *symbolSpec,
    ::exchange::core::collections::objpool::ObjectsPool *objectsPool,
    OrderBookEventsHelper *eventsHelper,
    const common::config::LoggingConfiguration *loggingCfg)
    : askPriceBuckets_(objectsPool), bidPriceBuckets_(objectsPool),
      symbolSpec_(symbolSpec), objectsPool_(objectsPool),
      orderIdIndex_(objectsPool), bestAskOrder_(nullptr),
      bestBidOrder_(nullptr), eventsHelper_(eventsHelper) {
  logDebug_ = loggingCfg != nullptr &&
              loggingCfg->Contains(common::config::LoggingConfiguration::
                                       LoggingLevel::LOGGING_MATCHING_DEBUG);
}

OrderBookDirectImpl::OrderBookDirectImpl(
    common::BytesIn *bytes,
    ::exchange::core::collections::objpool::ObjectsPool *objectsPool,
    OrderBookEventsHelper *eventsHelper,
    const common::config::LoggingConfiguration *loggingCfg)
    : askPriceBuckets_(objectsPool), bidPriceBuckets_(objectsPool),
      objectsPool_(objectsPool), orderIdIndex_(objectsPool),
      bestAskOrder_(nullptr), bestBidOrder_(nullptr),
      eventsHelper_(eventsHelper) {
  if (bytes == nullptr) {
    throw std::invalid_argument("BytesIn cannot be nullptr");
  }
  if (objectsPool == nullptr) {
    throw std::invalid_argument("ObjectsPool cannot be nullptr");
  }
  if (loggingCfg == nullptr) {
    throw std::invalid_argument("LoggingConfiguration cannot be nullptr");
  }

  // Read symbolSpec (implementation type was already read by
  // IOrderBook::Create)
  symbolSpec_ = new common::CoreSymbolSpecification(*bytes);

  logDebug_ =
      loggingCfg->loggingLevels.count(common::config::LoggingConfiguration::
                                          LoggingLevel::LOGGING_MATCHING_DEBUG);

  // Read number of orders
  int32_t size = bytes->ReadInt();

  // Read and insert all orders
  for (int32_t i = 0; i < size; i++) {
    // Create DirectOrder from bytes (deserialization)
    DirectOrder *order = new DirectOrder(*bytes);

    // Insert order into the order book structure
    insertOrder(order, nullptr);
    orderIdIndex_.Put(order->orderId, order);
  }
}

const common::CoreSymbolSpecification *
OrderBookDirectImpl::GetSymbolSpec() const {
  return symbolSpec_;
}

void OrderBookDirectImpl::NewOrder(OrderCommand *cmd) {
  switch (cmd->orderType) {
  case OrderType::GTC: {
    const int64_t size = cmd->size;
    // Create temporary DirectOrder for matching
    DirectOrder tempOrder;
    tempOrder.orderId = cmd->orderId;
    tempOrder.price = cmd->price;
    tempOrder.size = size;
    tempOrder.filled = 0;
    tempOrder.action = cmd->action;
    tempOrder.uid = cmd->uid;
    tempOrder.timestamp = cmd->timestamp;
    tempOrder.reserveBidPrice = cmd->reserveBidPrice;
    tempOrder.next = nullptr;
    tempOrder.prev = nullptr;
    tempOrder.bucket = nullptr;
    const int64_t filledSize = this->tryMatchInstantly(&tempOrder, cmd);
    if (filledSize == size)
      return;

    const int64_t orderId = cmd->orderId;
    if (orderIdIndex_.Get(orderId) != nullptr) {
      eventsHelper_->AttachRejectEvent(cmd, size - filledSize);
      return;
    }

    auto *orderRecord = objectsPool_->Get<DirectOrder>(
        ::exchange::core::collections::objpool::ObjectsPool::DIRECT_ORDER,
        []() { return new DirectOrder(); });

    orderRecord->orderId = orderId;
    orderRecord->price = cmd->price;
    orderRecord->size = size;
    orderRecord->reserveBidPrice = cmd->reserveBidPrice;
    orderRecord->action = cmd->action;
    orderRecord->uid = cmd->uid;
    orderRecord->timestamp = cmd->timestamp;
    orderRecord->filled = filledSize;

    orderIdIndex_.Put(orderId, orderRecord);
    this->insertOrder(orderRecord, nullptr);
    break;
  }
  case OrderType::IOC: {
    // Create temporary DirectOrder for matching
    DirectOrder tempOrder;
    tempOrder.orderId = cmd->orderId;
    tempOrder.price = cmd->price;
    tempOrder.size = cmd->size;
    tempOrder.filled = 0;
    tempOrder.action = cmd->action;
    tempOrder.uid = cmd->uid;
    tempOrder.timestamp = cmd->timestamp;
    tempOrder.reserveBidPrice = cmd->reserveBidPrice;
    tempOrder.next = nullptr;
    tempOrder.prev = nullptr;
    tempOrder.bucket = nullptr;
    const int64_t filledSize = this->tryMatchInstantly(&tempOrder, cmd);
    const int64_t rejectedSize = cmd->size - filledSize;
    if (rejectedSize != 0) {
      eventsHelper_->AttachRejectEvent(cmd, rejectedSize);
    }
    break;
  }
  case OrderType::FOK_BUDGET: {
    const int64_t budget = this->checkBudgetToFill(cmd->action, cmd->size);
    if (this->isBudgetLimitSatisfied(cmd->action, budget, cmd->price)) {
      // Create temporary DirectOrder for matching
      DirectOrder tempOrder;
      tempOrder.orderId = cmd->orderId;
      tempOrder.price = cmd->price;
      tempOrder.size = cmd->size;
      tempOrder.filled = 0;
      tempOrder.action = cmd->action;
      tempOrder.uid = cmd->uid;
      tempOrder.timestamp = cmd->timestamp;
      tempOrder.reserveBidPrice = cmd->reserveBidPrice;
      tempOrder.next = nullptr;
      tempOrder.prev = nullptr;
      tempOrder.bucket = nullptr;
      this->tryMatchInstantly(&tempOrder, cmd);
    } else {
      eventsHelper_->AttachRejectEvent(cmd, cmd->size);
    }
    break;
  }
  default:
    eventsHelper_->AttachRejectEvent(cmd, cmd->size);
  }
}

CommandResultCode OrderBookDirectImpl::CancelOrder(OrderCommand *cmd) {
  DirectOrder *order = orderIdIndex_.Get(cmd->orderId);
  if (order == nullptr || order->uid != cmd->uid) {
    return CommandResultCode::MATCHING_UNKNOWN_ORDER_ID;
  }

  // Save order data before releasing it to avoid use-after-free
  int64_t orderPrice = order->GetPrice();
  int64_t orderReserveBidPrice = order->GetReserveBidPrice();
  int64_t reduceSize = order->size - order->filled;
  OrderAction orderAction = order->action;

  orderIdIndex_.Remove(cmd->orderId);

  Bucket *freeBucket = this->RemoveOrder(order);
  if (freeBucket != nullptr) {
    objectsPool_->Put(
        ::exchange::core::collections::objpool::ObjectsPool::DIRECT_BUCKET,
        freeBucket);
  }

  // Release order before creating event
  objectsPool_->Put(
      ::exchange::core::collections::objpool::ObjectsPool::DIRECT_ORDER, order);

  // Use saved data to create event
  cmd->action = orderAction;
  cmd->matcherEvent = eventsHelper_->SendReduceEvent(
      orderPrice, orderReserveBidPrice, reduceSize, true);

  return CommandResultCode::SUCCESS;
}

CommandResultCode OrderBookDirectImpl::MoveOrder(OrderCommand *cmd) {
  DirectOrder *orderToMove = orderIdIndex_.Get(cmd->orderId);
  if (orderToMove == nullptr || orderToMove->uid != cmd->uid) {
    return CommandResultCode::MATCHING_UNKNOWN_ORDER_ID;
  }

  // Risk check for exchange bids
  if (symbolSpec_->type == common::SymbolType::CURRENCY_EXCHANGE_PAIR &&
      orderToMove->action == common::OrderAction::BID &&
      cmd->price > orderToMove->GetReserveBidPrice()) {
    return CommandResultCode::MATCHING_MOVE_FAILED_PRICE_OVER_RISK_LIMIT;
  }

  Bucket *freeBucket = this->RemoveOrder(orderToMove);
  orderToMove->price = cmd->price;
  cmd->action = orderToMove->action;

  const int64_t filled =
      this->tryMatchInstantly(static_cast<common::IOrder *>(orderToMove), cmd);
  if (filled == orderToMove->size) {
    orderIdIndex_.Remove(cmd->orderId);
    objectsPool_->Put(
        ::exchange::core::collections::objpool::ObjectsPool::DIRECT_ORDER,
        orderToMove);
    if (freeBucket)
      objectsPool_->Put(
          ::exchange::core::collections::objpool::ObjectsPool::DIRECT_BUCKET,
          freeBucket);
    return CommandResultCode::SUCCESS;
  }

  orderToMove->filled = filled;
  this->insertOrder(orderToMove, freeBucket);
  return CommandResultCode::SUCCESS;
}

CommandResultCode OrderBookDirectImpl::ReduceOrder(OrderCommand *cmd) {
  const int64_t orderId = cmd->orderId;
  const int64_t requestedReduceSize = cmd->size;
  if (requestedReduceSize <= 0) {
    return CommandResultCode::MATCHING_REDUCE_FAILED_WRONG_SIZE;
  }

  DirectOrder *order = orderIdIndex_.Get(orderId);
  if (order == nullptr || order->uid != cmd->uid) {
    return CommandResultCode::MATCHING_UNKNOWN_ORDER_ID;
  }

  const int64_t remainingSize = order->size - order->filled;
  const int64_t reduceBy = std::min(remainingSize, requestedReduceSize);
  const bool canRemove = (reduceBy == remainingSize);

  if (canRemove) {
    // Save order data before releasing it to avoid use-after-free
    int64_t orderPrice = order->GetPrice();
    int64_t orderReserveBidPrice = order->GetReserveBidPrice();
    OrderAction orderAction = order->action;

    orderIdIndex_.Remove(orderId);
    Bucket *freeBucket = this->RemoveOrder(order);
    if (freeBucket)
      objectsPool_->Put(
          ::exchange::core::collections::objpool::ObjectsPool::DIRECT_BUCKET,
          freeBucket);

    // Release order before creating event
    objectsPool_->Put(
        ::exchange::core::collections::objpool::ObjectsPool::DIRECT_ORDER,
        order);

    // Use saved data to create event
    cmd->matcherEvent = eventsHelper_->SendReduceEvent(
        orderPrice, orderReserveBidPrice, reduceBy, true);
    cmd->action = orderAction;
  } else {
    order->size -= reduceBy;
    order->bucket->totalVolume -= reduceBy;
    cmd->matcherEvent = eventsHelper_->SendReduceEvent(order, reduceBy, false);
    cmd->action = order->action;
  }
  return CommandResultCode::SUCCESS;
}

int64_t OrderBookDirectImpl::tryMatchInstantly(common::IOrder *takerOrder,
                                               OrderCommand *triggerCmd) {
  const bool isBidAction = takerOrder->GetAction() == OrderAction::BID;
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
  MatcherTradeEvent *eventsTail = nullptr;

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
    MatcherTradeEvent *tradeEvent = eventsHelper_->SendTradeEvent(
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

OrderBookDirectImpl::Bucket *
OrderBookDirectImpl::RemoveOrder(DirectOrder *order) {
  Bucket *bucket = order->bucket;
  bucket->totalVolume -= (order->size - order->filled);
  bucket->numOrders--;
  Bucket *bucketRemoved = nullptr;

  if (bucket->lastOrder == order) {
    if (order->next == nullptr || order->next->bucket != bucket) {
      auto &buckets = (order->action == OrderAction::ASK) ? askPriceBuckets_
                                                          : bidPriceBuckets_;
      buckets.Remove(order->price);
      bucketRemoved = bucket;
    } else {
      bucket->lastOrder = order->next;
    }
  }

  if (order->next != nullptr)
    order->next->prev = order->prev;
  if (order->prev != nullptr)
    order->prev->next = order->next;

  if (order == bestAskOrder_)
    bestAskOrder_ = order->prev;
  else if (order == bestBidOrder_)
    bestBidOrder_ = order->prev;

  return bucketRemoved;
}

void OrderBookDirectImpl::insertOrder(DirectOrder *order, Bucket *freeBucket) {
  const bool isAsk = (order->action == OrderAction::ASK);
  auto &buckets = isAsk ? askPriceBuckets_ : bidPriceBuckets_;
  Bucket *toBucket = buckets.Get(order->price);

  if (toBucket != nullptr) {
    if (freeBucket)
      objectsPool_->Put(
          ::exchange::core::collections::objpool::ObjectsPool::DIRECT_BUCKET,
          freeBucket);

    toBucket->totalVolume += (order->size - order->filled);
    toBucket->numOrders++;
    DirectOrder *oldTail = toBucket->lastOrder;
    DirectOrder *prevOrder = oldTail->prev;

    toBucket->lastOrder = order;
    oldTail->prev = order;
    if (prevOrder)
      prevOrder->next = order;

    order->next = oldTail;
    order->prev = prevOrder;
    order->bucket = toBucket;
  } else {
    Bucket *newBucket = freeBucket;
    if (!newBucket) {
      newBucket = objectsPool_->Get<Bucket>(
          ::exchange::core::collections::objpool::ObjectsPool::DIRECT_BUCKET,
          []() { return new Bucket(); });
    }
    newBucket->lastOrder = order;
    newBucket->totalVolume = order->size - order->filled;
    newBucket->numOrders = 1;
    newBucket->price = order->price;
    order->bucket = newBucket;
    buckets.Put(order->price, newBucket);

    Bucket *lowerBucket = isAsk ? buckets.GetLowerValue(order->price)
                                : buckets.GetHigherValue(order->price);
    if (lowerBucket != nullptr) {
      DirectOrder *lowerTail = lowerBucket->lastOrder;
      DirectOrder *prevOrder = lowerTail->prev;
      lowerTail->prev = order;
      if (prevOrder)
        prevOrder->next = order;
      order->next = lowerTail;
      order->prev = prevOrder;
    } else {
      DirectOrder *oldBestOrder = isAsk ? bestAskOrder_ : bestBidOrder_;
      if (oldBestOrder)
        oldBestOrder->next = order;
      if (isAsk)
        bestAskOrder_ = order;
      else
        bestBidOrder_ = order;
      order->next = nullptr;
      order->prev = oldBestOrder;
    }
  }
}

int32_t OrderBookDirectImpl::GetOrdersNum(OrderAction action) {
  auto &buckets =
      (action == OrderAction::ASK) ? askPriceBuckets_ : bidPriceBuckets_;
  int32_t count = 0;
  buckets.ForEach([&count](int64_t, Bucket *b) { count += b->numOrders; },
                  INT32_MAX);
  return count;
}

int64_t OrderBookDirectImpl::GetTotalOrdersVolume(OrderAction action) {
  auto &buckets =
      (action == OrderAction::ASK) ? askPriceBuckets_ : bidPriceBuckets_;
  int64_t volume = 0;
  buckets.ForEach([&volume](int64_t, Bucket *b) { volume += b->totalVolume; },
                  INT32_MAX);
  return volume;
}

void OrderBookDirectImpl::FillAsks(int32_t size, common::L2MarketData *data) {
  data->askSize = 0;
  askPriceBuckets_.ForEach(
      [data, size](int64_t, Bucket *b) {
        if (data->askSize < size) {
          int i = data->askSize++;
          data->askPrices[i] = b->price;
          data->askVolumes[i] = b->totalVolume;
          data->askOrders[i] = b->numOrders;
        }
      },
      size);
}

void OrderBookDirectImpl::FillBids(int32_t size, common::L2MarketData *data) {
  data->bidSize = 0;
  bidPriceBuckets_.ForEachDesc(
      [data, size](int64_t, Bucket *b) {
        if (data->bidSize < size) {
          int i = data->bidSize++;
          data->bidPrices[i] = b->price;
          data->bidVolumes[i] = b->totalVolume;
          data->bidOrders[i] = b->numOrders;
        }
      },
      size);
}

int32_t OrderBookDirectImpl::GetTotalAskBuckets(int32_t limit) {
  return askPriceBuckets_.Size(limit);
}

int32_t OrderBookDirectImpl::GetTotalBidBuckets(int32_t limit) {
  return bidPriceBuckets_.Size(limit);
}

std::shared_ptr<common::L2MarketData>
OrderBookDirectImpl::GetL2MarketDataSnapshot(int32_t size) {
  // Match Java default implementation in IOrderBook interface:
  // default L2MarketData getL2MarketDataSnapshot(final int size) {
  //     final int asksSize = getTotalAskBuckets(size);
  //     final int bidsSize = getTotalBidBuckets(size);
  //     final L2MarketData data = new L2MarketData(asksSize, bidsSize);
  //     fillAsks(asksSize, data);
  //     fillBids(bidsSize, data);
  //     return data;
  // }
  int32_t asksSize = GetTotalAskBuckets(size);
  int32_t bidsSize = GetTotalBidBuckets(size);
  auto data = std::make_shared<common::L2MarketData>(asksSize, bidsSize);
  FillAsks(asksSize, data.get());
  FillBids(bidsSize, data.get());
  return data;
}

std::vector<common::Order *> OrderBookDirectImpl::FindUserOrders(int64_t uid) {
  std::vector<common::Order *> list;
  orderIdIndex_.ForEach(
      [&list, uid](int64_t orderId, DirectOrder *order) {
        if (order->uid == uid) {
          list.push_back(
              new common::Order(orderId, order->price, order->size,
                                order->filled, order->reserveBidPrice,
                                order->action, order->uid, order->timestamp));
        }
      },
      INT32_MAX);
  return list;
}

common::IOrder *OrderBookDirectImpl::GetOrderById(int64_t orderId) {
  return orderIdIndex_.Get(orderId);
}

void OrderBookDirectImpl::ValidateInternalState() {
  // Logic from Java can be ported here if needed
}

OrderBookImplType OrderBookDirectImpl::GetImplementationType() const {
  return OrderBookImplType::DIRECT;
}

// Helper function to collect orders from a linked list
// Note: Java OrdersSpliterator uses prev pointer, not next
static void
CollectOrders(OrderBookDirectImpl::DirectOrder *startOrder,
              std::vector<OrderBookDirectImpl::DirectOrder *> &orders) {
  OrderBookDirectImpl::DirectOrder *current = startOrder;
  while (current != nullptr) {
    orders.push_back(current);
    current = current->prev;
  }
}

void OrderBookDirectImpl::WriteMarshallable(common::BytesOut &bytes) const {
  // Write implementation type
  bytes.WriteByte(static_cast<int8_t>(GetImplementationType()));

  // Write symbolSpec
  if (symbolSpec_ != nullptr) {
    // Create a non-const reference for WriteMarshallable call
    // Since WriteMarshallable is not const in the interface, we need to call it
    // on non-const
    const_cast<common::CoreSymbolSpecification *>(symbolSpec_)
        ->WriteMarshallable(bytes);
  }

  // Collect all orders from orderIdIndex
  std::vector<DirectOrder *> allOrders;
  orderIdIndex_.ForEach(
      [&allOrders](int64_t, DirectOrder *order) {
        if (order != nullptr) {
          allOrders.push_back(order);
        }
      },
      INT32_MAX);

  // Write total number of orders
  bytes.WriteInt(static_cast<int32_t>(allOrders.size()));

  // Write ask orders (sorted by price ascending)
  std::vector<DirectOrder *> askOrders;
  CollectOrders(bestAskOrder_, askOrders);
  for (DirectOrder *order : askOrders) {
    order->WriteMarshallable(bytes);
  }

  // Write bid orders (sorted by price descending)
  std::vector<DirectOrder *> bidOrders;
  CollectOrders(bestBidOrder_, bidOrders);
  for (DirectOrder *order : bidOrders) {
    order->WriteMarshallable(bytes);
  }
}

OrderBookDirectImpl::DirectOrder::DirectOrder(common::BytesIn &bytes) {
  orderId = bytes.ReadLong();
  price = bytes.ReadLong();
  size = bytes.ReadLong();
  filled = bytes.ReadLong();
  reserveBidPrice = bytes.ReadLong();
  action = common::OrderActionFromCode(bytes.ReadByte());
  uid = bytes.ReadLong();
  timestamp = bytes.ReadLong();
  next = nullptr;
  prev = nullptr;
  bucket = nullptr;
}

void OrderBookDirectImpl::DirectOrder::WriteMarshallable(
    common::BytesOut &bytes) const {
  bytes.WriteLong(orderId);
  bytes.WriteLong(price);
  bytes.WriteLong(size);
  bytes.WriteLong(filled);
  bytes.WriteLong(reserveBidPrice);
  bytes.WriteByte(common::OrderActionToCode(action));
  bytes.WriteLong(uid);
  bytes.WriteLong(timestamp);
}

int32_t OrderBookDirectImpl::DirectOrder::GetStateHash() const {
  // Match Java DirectOrder.stateHash() implementation:
  // Objects.hash(orderId, action, price, size, reserveBidPrice, filled, uid)
  // Match Order::GetStateHash() implementation for consistency
  int32_t result = 1;
  result = result * 31 + static_cast<int32_t>(orderId ^ (orderId >> 32));
  result = result * 31 + static_cast<int32_t>(static_cast<int8_t>(action));
  result = result * 31 + static_cast<int32_t>(price ^ (price >> 32));
  result = result * 31 + static_cast<int32_t>(size ^ (size >> 32));
  result = result * 31 +
           static_cast<int32_t>(reserveBidPrice ^ (reserveBidPrice >> 32));
  result = result * 31 + static_cast<int32_t>(filled ^ (filled >> 32));
  result = result * 31 + static_cast<int32_t>(uid ^ (uid >> 32));
  return result;
}

int32_t OrderBookDirectImpl::GetStateHash() const {
  // Match Java IOrderBook.default stateHash() implementation:
  // Objects.hash(
  //     HashingUtils.stateHashStream(askOrdersStream(true)),
  //     HashingUtils.stateHashStream(bidOrdersStream(true)),
  //     getSymbolSpec().stateHash());
  //
  // Java askOrdersStream() uses OrdersSpliterator which starts from
  // bestAskOrder and traverses via prev pointer (from best price to worst
  // price, oldest to newest within each price) This matches the order chain
  // structure where:
  // - bestAskOrder points to best price's first (oldest) order
  // - Following prev gives worse prices (and within same price, newer orders)
  // - Following next gives better prices (and within same price, older orders)

  // Calculate hash for ask orders stream
  // Start from bestAskOrder and traverse via prev (matching OrdersSpliterator)
  int32_t askHash = 0;
  DirectOrder *current = bestAskOrder_;
  while (current != nullptr) {
    askHash = askHash * 31 + current->GetStateHash();
    current = current->prev;
  }

  // Calculate hash for bid orders stream
  // Start from bestBidOrder and traverse via prev (matching OrdersSpliterator)
  int32_t bidHash = 0;
  current = bestBidOrder_;
  while (current != nullptr) {
    bidHash = bidHash * 31 + current->GetStateHash();
    current = current->prev;
  }

  // Combine using Objects.hash algorithm (31x multiplier)
  int32_t result = 1;
  result = result * 31 + askHash;
  result = result * 31 + bidHash;
  if (symbolSpec_ != nullptr) {
    result = result * 31 + symbolSpec_->GetStateHash();
  } else {
    result = result * 31 + 0;
  }

  return result;
}

int64_t OrderBookDirectImpl::checkBudgetToFill(common::OrderAction action,
                                               int64_t size) {
  DirectOrder *makerOrder =
      (action == OrderAction::BID) ? bestAskOrder_ : bestBidOrder_;

  int64_t budget = 0L;

  // iterate through all orders
  while (makerOrder != nullptr) {
    const Bucket *bucket = makerOrder->bucket;

    const int64_t availableSize = bucket->totalVolume;
    const int64_t price = makerOrder->price;

    if (size > availableSize) {
      size -= availableSize;
      budget += availableSize * price;
    } else {
      return budget + size * price;
    }

    // switch to next order (can be null)
    makerOrder = bucket->lastOrder->prev;
  }
  return INT64_MAX;
}

bool OrderBookDirectImpl::isBudgetLimitSatisfied(
    common::OrderAction orderAction, int64_t calculated, int64_t limit) {
  return calculated != INT64_MAX &&
         (calculated == limit ||
          (orderAction == OrderAction::BID ^ calculated > limit));
}

std::string OrderBookDirectImpl::PrintAskBucketsDiagram() const {
  std::ostringstream oss;
  oss << "DirectImpl Ask Buckets (ART tree, ascending order):\n";

  // Print actual prices and bucket info
  auto entriesList = askPriceBuckets_.EntriesList();
  if (entriesList.empty()) {
    oss << "  (empty)\n";
  } else {
    // Convert to vector for sorting (std::sort requires random access
    // iterators)
    std::vector<std::pair<int64_t, Bucket *>> entries(entriesList.begin(),
                                                      entriesList.end());
    // Sort by price (ascending for asks)
    std::sort(entries.begin(), entries.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });

    for (const auto &entry : entries) {
      int64_t price = entry.first;
      const Bucket *bucket = entry.second;
      if (bucket != nullptr) {
        oss << "  Price: " << price << " -> Bucket: " << bucket
            << " (orders: " << bucket->numOrders
            << ", volume: " << bucket->totalVolume << ")\n";
      }
    }
  }

  oss << "\nART Tree Structure:\n";
  oss << askPriceBuckets_.PrintDiagram();
  return oss.str();
}

void OrderBookDirectImpl::ProcessAskOrders(
    std::function<void(const common::IOrder *)> consumer) const {
  // Match Java: askOrdersStream() uses OrdersSpliterator which starts from
  // bestAskOrder and traverses via prev pointer
  DirectOrder *current = bestAskOrder_;
  while (current != nullptr) {
    consumer(current);
    current = current->prev;
  }
}

void OrderBookDirectImpl::ProcessBidOrders(
    std::function<void(const common::IOrder *)> consumer) const {
  // Match Java: bidOrdersStream() uses OrdersSpliterator which starts from
  // bestBidOrder and traverses via prev pointer
  DirectOrder *current = bestBidOrder_;
  while (current != nullptr) {
    consumer(current);
    current = current->prev;
  }
}

std::string OrderBookDirectImpl::PrintBidBucketsDiagram() const {
  std::ostringstream oss;
  oss << "DirectImpl Bid Buckets (ART tree, descending order):\n";

  // Print actual prices and bucket info
  auto entriesList = bidPriceBuckets_.EntriesList();
  if (entriesList.empty()) {
    oss << "  (empty)\n";
  } else {
    // Convert to vector for sorting (std::sort requires random access
    // iterators)
    std::vector<std::pair<int64_t, Bucket *>> entries(entriesList.begin(),
                                                      entriesList.end());
    // Sort by price (descending for bids)
    std::sort(entries.begin(), entries.end(),
              [](const auto &a, const auto &b) { return a.first > b.first; });

    for (const auto &entry : entries) {
      int64_t price = entry.first;
      const Bucket *bucket = entry.second;
      if (bucket != nullptr) {
        oss << "  Price: " << price << " -> Bucket: " << bucket
            << " (orders: " << bucket->numOrders
            << ", volume: " << bucket->totalVolume << ")\n";
      }
    }
  }

  oss << "\nART Tree Structure:\n";
  oss << bidPriceBuckets_.PrintDiagram();
  return oss.str();
}

} // namespace orderbook
} // namespace core
} // namespace exchange
