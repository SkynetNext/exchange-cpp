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

#include <algorithm>
#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <exchange/core/common/MatcherTradeEvent.h>
#include <exchange/core/orderbook/OrderBookNaiveImpl.h>
#include <stdexcept>

namespace exchange {
namespace core {
namespace orderbook {

OrderBookNaiveImpl::OrderBookNaiveImpl(
    const common::CoreSymbolSpecification *symbolSpec,
    ::exchange::core::collections::objpool::ObjectsPool *objectsPool,
    OrderBookEventsHelper *eventsHelper)
    : symbolSpec_(symbolSpec),
      eventsHelper_(eventsHelper != nullptr
                        ? eventsHelper
                        : OrderBookEventsHelper::NonPooledEventsHelper()) {
  // Note: OrderBookNaiveImpl doesn't use ObjectsPool (uses std::map instead
  // of ART tree), but accepts it for interface consistency with
  // OrderBookDirectImpl
  (void)objectsPool; // Suppress unused parameter warning
}

void OrderBookNaiveImpl::NewOrder(common::cmd::OrderCommand *cmd) {
  switch (cmd->orderType) {
  case common::OrderType::GTC:
    NewOrderPlaceGtc(cmd);
    break;
  case common::OrderType::IOC:
    NewOrderMatchIoc(cmd);
    break;
  case common::OrderType::FOK_BUDGET:
    NewOrderMatchFokBudget(cmd);
    break;
  default:
    // Unsupported order type
    eventsHelper_->AttachRejectEvent(cmd, cmd->size);
    break;
  }
}

void OrderBookNaiveImpl::NewOrderPlaceGtc(common::cmd::OrderCommand *cmd) {
  common::OrderAction action = cmd->action;
  int64_t price = cmd->price;
  int64_t size = cmd->size;

  // Try to match instantly
  auto *matchingBuckets = SubtreeForMatching(action, price);
  int64_t filledSize = TryMatchInstantly(cmd, matchingBuckets, 0, cmd);

  if (filledSize == size) {
    // Order was matched completely
    return;
  }

  // Check for duplicate order ID
  int64_t newOrderId = cmd->orderId;
  if (idMap_.find(newOrderId) != idMap_.end()) {
    eventsHelper_->AttachRejectEvent(cmd, size - filledSize);
    return;
  }

  // Create order record
  auto *orderRecord =
      new common::Order(newOrderId, price, size, filledSize,
                        cmd->reserveBidPrice, action, cmd->uid, cmd->timestamp);

  // Place in appropriate bucket
  auto *buckets = GetBucketsByAction(action);
  auto it = buckets->find(price);
  if (it == buckets->end()) {
    buckets->emplace(price, std::make_unique<OrdersBucket>(price));
    it = buckets->find(price);
  }
  it->second->Put(orderRecord);

  idMap_[newOrderId] = orderRecord;
}

void OrderBookNaiveImpl::NewOrderMatchIoc(common::cmd::OrderCommand *cmd) {
  auto *matchingBuckets = SubtreeForMatching(cmd->action, cmd->price);
  int64_t filledSize = TryMatchInstantly(cmd, matchingBuckets, 0, cmd);

  int64_t rejectedSize = cmd->size - filledSize;
  if (rejectedSize != 0) {
    eventsHelper_->AttachRejectEvent(cmd, rejectedSize);
  }
}

void OrderBookNaiveImpl::NewOrderMatchFokBudget(
    common::cmd::OrderCommand *cmd) {
  int64_t size = cmd->size;
  auto *matchingBuckets = SubtreeForMatching(cmd->action, cmd->price);

  int64_t budget = 0;
  if (!CheckBudgetToFill(size, matchingBuckets, &budget)) {
    eventsHelper_->AttachRejectEvent(cmd, size);
    return;
  }

  // Check if budget limit is satisfied
  bool budgetOk = false;
  if (budget == cmd->price) {
    budgetOk = true;
  } else {
    // For BID: budget should be <= price, for ASK: budget should be >= price
    budgetOk = (cmd->action == common::OrderAction::BID)
                   ? (budget <= cmd->price)
                   : (budget >= cmd->price);
  }

  if (budgetOk) {
    TryMatchInstantly(cmd, matchingBuckets, 0, cmd);
  } else {
    eventsHelper_->AttachRejectEvent(cmd, size);
  }
}

bool OrderBookNaiveImpl::CheckBudgetToFill(
    int64_t size,
    const std::map<int64_t, std::unique_ptr<OrdersBucket>> *matchingBuckets,
    int64_t *budgetOut) {
  int64_t budget = 0;
  int64_t remainingSize = size;

  for (const auto &pair : *matchingBuckets) {
    int64_t availableSize = pair.second->GetTotalVolume();
    int64_t price = pair.first;

    if (remainingSize > availableSize) {
      remainingSize -= availableSize;
      budget += availableSize * price;
    } else {
      *budgetOut = budget + remainingSize * price;
      return true;
    }
  }

  // Not enough liquidity
  return false;
}

int64_t OrderBookNaiveImpl::TryMatchInstantly(
    const common::IOrder *activeOrder,
    std::map<int64_t, std::unique_ptr<OrdersBucket>> *matchingBuckets,
    int64_t filled, common::cmd::OrderCommand *triggerCmd) {

  if (matchingBuckets->empty()) {
    return filled;
  }

  int64_t orderSize = activeOrder->GetSize();
  ::exchange::core::common::MatcherTradeEvent *eventsTail = nullptr;

  std::vector<int64_t> emptyBuckets;

  for (auto &pair : *matchingBuckets) {
    OrdersBucket *bucket = pair.second.get();
    int64_t sizeLeft = orderSize - filled;

    OrdersBucket::MatcherResult bucketMatchings =
        bucket->Match(sizeLeft, activeOrder, eventsHelper_);

    // Remove fully matched orders from idMap
    for (int64_t orderId : bucketMatchings.ordersToRemove) {
      idMap_.erase(orderId);
    }

    filled += bucketMatchings.volume;

    // Attach event chain
    if (eventsTail == nullptr) {
      triggerCmd->matcherEvent = bucketMatchings.eventsChainHead;
    } else {
      eventsTail->nextEvent = bucketMatchings.eventsChainHead;
    }
    eventsTail = bucketMatchings.eventsChainTail;

    // Mark empty buckets for removal
    if (bucket->GetTotalVolume() == 0) {
      emptyBuckets.push_back(pair.first);
    }

    if (filled == orderSize) {
      break;
    }
  }

  // Remove empty buckets
  for (int64_t price : emptyBuckets) {
    matchingBuckets->erase(price);
  }

  return filled;
}

common::cmd::CommandResultCode
OrderBookNaiveImpl::CancelOrder(common::cmd::OrderCommand *cmd) {
  int64_t orderId = cmd->orderId;

  auto it = idMap_.find(orderId);
  if (it == idMap_.end() || it->second->uid != cmd->uid) {
    return common::cmd::CommandResultCode::MATCHING_UNKNOWN_ORDER_ID;
  }

  common::Order *order = it->second;
  idMap_.erase(it);

  auto *buckets = GetBucketsByAction(order->action);
  int64_t price = order->price;
  auto bucketIt = buckets->find(price);
  if (bucketIt == buckets->end()) {
    throw std::runtime_error("Cannot find bucket for order price=" +
                             std::to_string(price));
  }

  OrdersBucket *ordersBucket = bucketIt->second.get();
  ordersBucket->Remove(orderId, cmd->uid);
  if (ordersBucket->GetTotalVolume() == 0) {
    buckets->erase(price);
  }

  // Send reduce event
  cmd->matcherEvent =
      eventsHelper_->SendReduceEvent(order, order->size - order->filled, true);

  // Fill action field
  cmd->action = order->action;

  delete order;
  return common::cmd::CommandResultCode::SUCCESS;
}

common::cmd::CommandResultCode
OrderBookNaiveImpl::ReduceOrder(common::cmd::OrderCommand *cmd) {
  int64_t orderId = cmd->orderId;
  int64_t requestedReduceSize = cmd->size;

  if (requestedReduceSize <= 0) {
    return common::cmd::CommandResultCode::MATCHING_REDUCE_FAILED_WRONG_SIZE;
  }

  auto it = idMap_.find(orderId);
  if (it == idMap_.end() || it->second->uid != cmd->uid) {
    return common::cmd::CommandResultCode::MATCHING_UNKNOWN_ORDER_ID;
  }

  common::Order *order = it->second;
  int64_t remainingSize = order->size - order->filled;
  int64_t reduceBy = std::min(remainingSize, requestedReduceSize);

  auto *buckets = GetBucketsByAction(order->action);
  auto bucketIt = buckets->find(order->price);
  if (bucketIt == buckets->end()) {
    throw std::runtime_error("Cannot find bucket for order price=" +
                             std::to_string(order->price));
  }

  OrdersBucket *ordersBucket = bucketIt->second.get();
  bool canRemove = (reduceBy == remainingSize);

  if (canRemove) {
    idMap_.erase(orderId);
    ordersBucket->Remove(orderId, cmd->uid);
    if (ordersBucket->GetTotalVolume() == 0) {
      buckets->erase(order->price);
    }
    delete order;
  } else {
    order->size -= reduceBy;
    ordersBucket->ReduceSize(reduceBy);
  }

  cmd->matcherEvent =
      eventsHelper_->SendReduceEvent(order, reduceBy, canRemove);
  cmd->action = order->action;

  return common::cmd::CommandResultCode::SUCCESS;
}

common::cmd::CommandResultCode
OrderBookNaiveImpl::MoveOrder(common::cmd::OrderCommand *cmd) {
  int64_t orderId = cmd->orderId;
  int64_t newPrice = cmd->price;

  auto it = idMap_.find(orderId);
  if (it == idMap_.end() || it->second->uid != cmd->uid) {
    return common::cmd::CommandResultCode::MATCHING_UNKNOWN_ORDER_ID;
  }

  common::Order *order = it->second;
  int64_t price = order->price;
  auto *buckets = GetBucketsByAction(order->action);

  cmd->action = order->action;

  // Reserved price risk check for exchange bids
  if (symbolSpec_->type == common::SymbolType::CURRENCY_EXCHANGE_PAIR &&
      order->action == common::OrderAction::BID &&
      cmd->price > order->reserveBidPrice) {
    return common::cmd::CommandResultCode::
        MATCHING_MOVE_FAILED_PRICE_OVER_RISK_LIMIT;
  }

  // Remove from original bucket
  auto bucketIt = buckets->find(price);
  if (bucketIt == buckets->end()) {
    throw std::runtime_error("Cannot find bucket for order price=" +
                             std::to_string(price));
  }

  OrdersBucket *bucket = bucketIt->second.get();
  bucket->Remove(orderId, cmd->uid);
  if (bucket->GetTotalVolume() == 0) {
    buckets->erase(price);
  }

  order->price = newPrice;

  // Try match with new price
  auto *matchingArea = SubtreeForMatching(order->action, newPrice);
  int64_t filled = TryMatchInstantly(order, matchingArea, order->filled, cmd);
  if (filled == order->size) {
    // Order was fully matched
    idMap_.erase(orderId);
    delete order;
    return common::cmd::CommandResultCode::SUCCESS;
  }

  order->filled = filled;

  // Place in new bucket
  auto newBucketIt = buckets->find(newPrice);
  if (newBucketIt == buckets->end()) {
    buckets->emplace(newPrice, std::make_unique<OrdersBucket>(newPrice));
    newBucketIt = buckets->find(newPrice);
  }
  newBucketIt->second->Put(order);

  return common::cmd::CommandResultCode::SUCCESS;
}

int32_t OrderBookNaiveImpl::GetOrdersNum(common::OrderAction action) {
  const auto *buckets = GetBucketsByAction(action);
  int32_t total = 0;
  for (const auto &pair : *buckets) {
    total += pair.second->GetNumOrders();
  }
  return total;
}

int64_t OrderBookNaiveImpl::GetTotalOrdersVolume(common::OrderAction action) {
  const auto *buckets = GetBucketsByAction(action);
  int64_t total = 0;
  for (const auto &pair : *buckets) {
    total += pair.second->GetTotalVolume();
  }
  return total;
}

common::IOrder *OrderBookNaiveImpl::GetOrderById(int64_t orderId) {
  auto it = idMap_.find(orderId);
  return (it != idMap_.end()) ? it->second : nullptr;
}

void OrderBookNaiveImpl::ValidateInternalState() {
  // Validate all buckets
  for (const auto &pair : askBuckets_) {
    pair.second->Validate();
  }
  for (const auto &pair : bidBuckets_) {
    pair.second->Validate();
  }
}

std::unique_ptr<common::L2MarketData>
OrderBookNaiveImpl::GetL2MarketDataSnapshot(int32_t size) {
  int32_t asksSize = GetTotalAskBuckets(size);
  int32_t bidsSize = GetTotalBidBuckets(size);
  auto data = std::make_unique<common::L2MarketData>(asksSize, bidsSize);
  FillAsks(asksSize, data.get());
  FillBids(bidsSize, data.get());
  return data;
}

void OrderBookNaiveImpl::FillAsks(int32_t size, common::L2MarketData *data) {
  if (size == 0) {
    data->askSize = 0;
    return;
  }

  int32_t i = 0;
  for (const auto &pair : askBuckets_) {
    data->askPrices[i] = pair.second->GetPrice();
    data->askVolumes[i] = pair.second->GetTotalVolume();
    data->askOrders[i] = pair.second->GetNumOrders();
    if (++i == size) {
      break;
    }
  }
  data->askSize = i;
}

void OrderBookNaiveImpl::FillBids(int32_t size, common::L2MarketData *data) {
  if (size == 0) {
    data->bidSize = 0;
    return;
  }

  int32_t i = 0;
  for (const auto &pair : bidBuckets_) {
    data->bidPrices[i] = pair.second->GetPrice();
    data->bidVolumes[i] = pair.second->GetTotalVolume();
    data->bidOrders[i] = pair.second->GetNumOrders();
    if (++i == size) {
      break;
    }
  }
  data->bidSize = i;
}

int32_t OrderBookNaiveImpl::GetTotalAskBuckets(int32_t limit) {
  return std::min(limit, static_cast<int32_t>(askBuckets_.size()));
}

int32_t OrderBookNaiveImpl::GetTotalBidBuckets(int32_t limit) {
  return std::min(limit, static_cast<int32_t>(bidBuckets_.size()));
}

int32_t OrderBookNaiveImpl::GetStateHash() const {
  // Simple hash combining ask and bid buckets
  int32_t hash = 0;
  for (const auto &pair : askBuckets_) {
    hash ^= static_cast<int32_t>(pair.first) ^
            static_cast<int32_t>(pair.second->GetTotalVolume());
  }
  for (const auto &pair : bidBuckets_) {
    hash ^= static_cast<int32_t>(pair.first) ^
            static_cast<int32_t>(pair.second->GetTotalVolume());
  }
  return hash;
}

std::map<int64_t, std::unique_ptr<OrdersBucket>> *
OrderBookNaiveImpl::GetBucketsByAction(common::OrderAction action) {
  if (action == common::OrderAction::ASK) {
    return &askBuckets_;
  } else {
    // bidBuckets_ has different comparator, need to cast
    // This is safe because we only use it for iteration, not for type-dependent
    // operations
    return reinterpret_cast<std::map<int64_t, std::unique_ptr<OrdersBucket>> *>(
        &bidBuckets_);
  }
}

const std::map<int64_t, std::unique_ptr<OrdersBucket>> *
OrderBookNaiveImpl::GetBucketsByAction(common::OrderAction action) const {
  if (action == common::OrderAction::ASK) {
    return &askBuckets_;
  } else {
    // bidBuckets_ has different comparator, need to cast
    return reinterpret_cast<
        const std::map<int64_t, std::unique_ptr<OrdersBucket>> *>(&bidBuckets_);
  }
}

std::map<int64_t, std::unique_ptr<OrdersBucket>> *
OrderBookNaiveImpl::SubtreeForMatching(common::OrderAction action,
                                       int64_t price) {
  if (action == common::OrderAction::ASK) {
    // For ASK orders, match against BID buckets with price <= order price
    // bidBuckets_ is in descending order, so we need to iterate from beginning
    // Need to cast because bidBuckets_ has different comparator
    return reinterpret_cast<std::map<int64_t, std::unique_ptr<OrdersBucket>> *>(
        &bidBuckets_);
  } else {
    // For BID orders, match against ASK buckets with price >= order price
    // askBuckets_ is in ascending order, so we need to iterate from beginning
    return &askBuckets_;
  }
}

} // namespace orderbook
} // namespace core
} // namespace exchange
