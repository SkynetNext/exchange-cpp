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
#include <exchange/core/common/BytesIn.h>
#include <exchange/core/common/MatcherTradeEvent.h>
#include <exchange/core/common/config/LoggingConfiguration.h>
#include <exchange/core/orderbook/OrderBookEventsHelper.h>
#include <exchange/core/orderbook/OrderBookNaiveImpl.h>
#include <exchange/core/utils/SerializationUtils.h>
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
  auto matchingRange = GetMatchingRange(action, price);
  int64_t filledSize = TryMatchInstantly(cmd, matchingRange, 0, cmd);

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
  if (action == common::OrderAction::ASK) {
    auto it = askBuckets_.find(price);
    if (it == askBuckets_.end()) {
      askBuckets_.emplace(price, std::make_unique<OrdersBucket>(price));
      it = askBuckets_.find(price);
    }
    it->second->Put(orderRecord);
  } else {
    auto it = bidBuckets_.find(price);
    if (it == bidBuckets_.end()) {
      bidBuckets_.emplace(price, std::make_unique<OrdersBucket>(price));
      it = bidBuckets_.find(price);
    }
    it->second->Put(orderRecord);
  }

  idMap_[newOrderId] = orderRecord;
}

void OrderBookNaiveImpl::NewOrderMatchIoc(common::cmd::OrderCommand *cmd) {
  auto matchingRange = GetMatchingRange(cmd->action, cmd->price);
  int64_t filledSize = TryMatchInstantly(cmd, matchingRange, 0, cmd);

  int64_t rejectedSize = cmd->size - filledSize;
  if (rejectedSize != 0) {
    eventsHelper_->AttachRejectEvent(cmd, rejectedSize);
  }
}

void OrderBookNaiveImpl::NewOrderMatchFokBudget(
    common::cmd::OrderCommand *cmd) {
  int64_t size = cmd->size;
  auto matchingRange = GetMatchingRange(cmd->action, cmd->price);

  int64_t budget = 0;
  if (!CheckBudgetToFill(size, matchingRange, &budget)) {
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
    TryMatchInstantly(cmd, matchingRange, 0, cmd);
  } else {
    eventsHelper_->AttachRejectEvent(cmd, size);
  }
}

bool OrderBookNaiveImpl::CheckBudgetToFill(int64_t size,
                                           MatchingRange &matchingRange,
                                           int64_t *budgetOut) {
  if (matchingRange.empty) {
    return false;
  }

  int64_t budget = 0;
  int64_t remainingSize = size;
  bool found = false;

  matchingRange.forEach([&](int64_t price, OrdersBucket *bucket) {
    if (found) {
      return; // Already found enough liquidity
    }

    int64_t availableSize = bucket->GetTotalVolume();

    if (remainingSize > availableSize) {
      remainingSize -= availableSize;
      budget += availableSize * price;
    } else {
      *budgetOut = budget + remainingSize * price;
      found = true;
    }
  });

  return found;
}

int64_t OrderBookNaiveImpl::TryMatchInstantly(
    const common::IOrder *activeOrder, MatchingRange &matchingRange,
    int64_t filled, common::cmd::OrderCommand *triggerCmd) {

  if (matchingRange.empty) {
    return filled;
  }

  int64_t orderSize = activeOrder->GetSize();
  ::exchange::core::common::MatcherTradeEvent *eventsTail = nullptr;

  std::vector<int64_t> emptyBuckets;

  matchingRange.forEach([&](int64_t bucketPrice, OrdersBucket *bucket) {
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
      emptyBuckets.push_back(bucketPrice);
    }
  });

  // Remove empty buckets
  for (int64_t price : emptyBuckets) {
    matchingRange.erasePrice(price);
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

  int64_t price = order->price;
  if (order->action == common::OrderAction::ASK) {
    auto bucketIt = askBuckets_.find(price);
    if (bucketIt == askBuckets_.end()) {
      throw std::runtime_error("Cannot find bucket for order price=" +
                               std::to_string(price));
    }
    OrdersBucket *ordersBucket = bucketIt->second.get();
    ordersBucket->Remove(orderId, cmd->uid);
    if (ordersBucket->GetTotalVolume() == 0) {
      askBuckets_.erase(price);
    }
  } else {
    auto bucketIt = bidBuckets_.find(price);
    if (bucketIt == bidBuckets_.end()) {
      throw std::runtime_error("Cannot find bucket for order price=" +
                               std::to_string(price));
    }
    OrdersBucket *ordersBucket = bucketIt->second.get();
    ordersBucket->Remove(orderId, cmd->uid);
    if (ordersBucket->GetTotalVolume() == 0) {
      bidBuckets_.erase(price);
    }
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

  OrdersBucket *ordersBucket = nullptr;
  int64_t orderPrice = order->price;
  if (order->action == common::OrderAction::ASK) {
    auto bucketIt = askBuckets_.find(orderPrice);
    if (bucketIt == askBuckets_.end()) {
      throw std::runtime_error("Cannot find bucket for order price=" +
                               std::to_string(orderPrice));
    }
    ordersBucket = bucketIt->second.get();
  } else {
    auto bucketIt = bidBuckets_.find(orderPrice);
    if (bucketIt == bidBuckets_.end()) {
      throw std::runtime_error("Cannot find bucket for order price=" +
                               std::to_string(orderPrice));
    }
    ordersBucket = bucketIt->second.get();
  }
  bool canRemove = (reduceBy == remainingSize);

  if (canRemove) {
    idMap_.erase(orderId);
    ordersBucket->Remove(orderId, cmd->uid);
    if (ordersBucket->GetTotalVolume() == 0) {
      if (order->action == common::OrderAction::ASK) {
        askBuckets_.erase(orderPrice);
      } else {
        bidBuckets_.erase(orderPrice);
      }
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
  cmd->action = order->action;

  // Reserved price risk check for exchange bids
  if (symbolSpec_->type == common::SymbolType::CURRENCY_EXCHANGE_PAIR &&
      order->action == common::OrderAction::BID &&
      cmd->price > order->reserveBidPrice) {
    return common::cmd::CommandResultCode::
        MATCHING_MOVE_FAILED_PRICE_OVER_RISK_LIMIT;
  }

  // Remove from original bucket
  if (order->action == common::OrderAction::ASK) {
    auto bucketIt = askBuckets_.find(price);
    if (bucketIt == askBuckets_.end()) {
      throw std::runtime_error("Cannot find bucket for order price=" +
                               std::to_string(price));
    }
    OrdersBucket *bucket = bucketIt->second.get();
    bucket->Remove(orderId, cmd->uid);
    if (bucket->GetTotalVolume() == 0) {
      askBuckets_.erase(price);
    }
  } else {
    auto bucketIt = bidBuckets_.find(price);
    if (bucketIt == bidBuckets_.end()) {
      throw std::runtime_error("Cannot find bucket for order price=" +
                               std::to_string(price));
    }
    OrdersBucket *bucket = bucketIt->second.get();
    bucket->Remove(orderId, cmd->uid);
    if (bucket->GetTotalVolume() == 0) {
      bidBuckets_.erase(price);
    }
  }

  order->price = newPrice;

  // Try match with new price
  auto matchingArea = GetMatchingRange(order->action, newPrice);
  int64_t filled = TryMatchInstantly(order, matchingArea, order->filled, cmd);
  if (filled == order->size) {
    // Order was fully matched
    idMap_.erase(orderId);
    delete order;
    return common::cmd::CommandResultCode::SUCCESS;
  }

  order->filled = filled;

  // Place in new bucket
  if (order->action == common::OrderAction::ASK) {
    auto newBucketIt = askBuckets_.find(newPrice);
    if (newBucketIt == askBuckets_.end()) {
      askBuckets_.emplace(newPrice, std::make_unique<OrdersBucket>(newPrice));
      newBucketIt = askBuckets_.find(newPrice);
    }
    newBucketIt->second->Put(order);
  } else {
    auto newBucketIt = bidBuckets_.find(newPrice);
    if (newBucketIt == bidBuckets_.end()) {
      bidBuckets_.emplace(newPrice, std::make_unique<OrdersBucket>(newPrice));
      newBucketIt = bidBuckets_.find(newPrice);
    }
    newBucketIt->second->Put(order);
  }

  return common::cmd::CommandResultCode::SUCCESS;
}

int32_t OrderBookNaiveImpl::GetOrdersNum(common::OrderAction action) {
  if (action == common::OrderAction::ASK) {
    int32_t total = 0;
    for (const auto &pair : askBuckets_) {
      total += pair.second->GetNumOrders();
    }
    return total;
  } else {
    int32_t total = 0;
    for (const auto &pair : bidBuckets_) {
      total += pair.second->GetNumOrders();
    }
    return total;
  }
}

int64_t OrderBookNaiveImpl::GetTotalOrdersVolume(common::OrderAction action) {
  if (action == common::OrderAction::ASK) {
    int64_t total = 0;
    for (const auto &pair : askBuckets_) {
      total += pair.second->GetTotalVolume();
    }
    return total;
  } else {
    int64_t total = 0;
    for (const auto &pair : bidBuckets_) {
      total += pair.second->GetTotalVolume();
    }
    return total;
  }
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

OrderBookNaiveImpl::MatchingRange
OrderBookNaiveImpl::GetMatchingRange(common::OrderAction action,
                                     int64_t price) {
  if (action == common::OrderAction::ASK) {
    // For ASK orders, match against BID buckets with price >= order price
    // bidBuckets_ is in descending order (std::greater), so we iterate from
    // highest to lowest, stopping when price < order price
    auto begin = bidBuckets_.begin();
    auto end = bidBuckets_.end();

    auto shouldContinue = [price](int64_t bucketPrice) {
      return bucketPrice >=
             price; // For ASK: match BID with price >= order price
    };

    auto eraseFunc = [this](int64_t p) { bidBuckets_.erase(p); };

    return MatchingRange(begin, end, shouldContinue, eraseFunc);
  } else {
    // For BID orders, match against ASK buckets with price <= order price
    // askBuckets_ is in ascending order, so we iterate from lowest to highest,
    // stopping when price > order price
    auto begin = askBuckets_.begin();
    auto end = askBuckets_.end();

    auto shouldContinue = [price](int64_t bucketPrice) {
      return bucketPrice <=
             price; // For BID: match ASK with price <= order price
    };

    auto eraseFunc = [this](int64_t p) { askBuckets_.erase(p); };

    return MatchingRange(begin, end, shouldContinue, eraseFunc);
  }
}

OrderBookNaiveImpl::OrderBookNaiveImpl(
    common::BytesIn *bytes,
    const common::config::LoggingConfiguration *loggingCfg) {
  if (bytes == nullptr) {
    throw std::invalid_argument("BytesIn cannot be nullptr");
  }
  if (loggingCfg == nullptr) {
    throw std::invalid_argument("LoggingConfiguration cannot be nullptr");
  }

  // Read symbolSpec
  symbolSpec_ = new common::CoreSymbolSpecification(*bytes);

  // Read askBuckets (long -> OrdersBucket*)
  int askLength = bytes->ReadInt();
  for (int i = 0; i < askLength; i++) {
    int64_t price = bytes->ReadLong();
    OrdersBucket *bucket = new OrdersBucket(bytes);
    askBuckets_[price] = std::unique_ptr<OrdersBucket>(bucket);
  }

  // Read bidBuckets (long -> OrdersBucket*)
  int bidLength = bytes->ReadInt();
  for (int i = 0; i < bidLength; i++) {
    int64_t price = bytes->ReadLong();
    OrdersBucket *bucket = new OrdersBucket(bytes);
    bidBuckets_[price] = std::unique_ptr<OrdersBucket>(bucket);
  }

  // Reconstruct orderId -> Order cache
  for (const auto &pair : askBuckets_) {
    pair.second->ForEachOrder([this](common::Order *order) {
      if (order != nullptr) {
        idMap_[order->orderId] = order;
      }
    });
  }
  for (const auto &pair : bidBuckets_) {
    pair.second->ForEachOrder([this](common::Order *order) {
      if (order != nullptr) {
        idMap_[order->orderId] = order;
      }
    });
  }

  eventsHelper_ = OrderBookEventsHelper::NonPooledEventsHelper();
  logDebug_ = loggingCfg->Contains(common::config::LoggingConfiguration::
                                       LoggingLevel::LOGGING_MATCHING_DEBUG);
}

void OrderBookNaiveImpl::WriteMarshallable(common::BytesOut &bytes) const {
  // Match Java: bytes.writeByte(getImplementationType().getCode());
  bytes.WriteByte(static_cast<int8_t>(GetImplementationType()));

  // Match Java: symbolSpec.writeMarshallable(bytes);
  if (symbolSpec_ != nullptr) {
    // Create a non-const reference for WriteMarshallable call
    // Since WriteMarshallable is not const in the interface, we need to call it
    // on non-const
    const_cast<common::CoreSymbolSpecification *>(symbolSpec_)
        ->WriteMarshallable(bytes);
  }

  // Match Java: SerializationUtils.marshallLongMap(askBuckets, bytes);
  // Convert std::map to ankerl::unordered_dense::map for marshalling
  ankerl::unordered_dense::map<int64_t, OrdersBucket *> askBucketsMap;
  for (const auto &pair : askBuckets_) {
    askBucketsMap[pair.first] = pair.second.get();
  }
  utils::SerializationUtils::MarshallLongHashMap(askBucketsMap, bytes);

  // Match Java: SerializationUtils.marshallLongMap(bidBuckets, bytes);
  // Convert std::map to ankerl::unordered_dense::map for marshalling
  ankerl::unordered_dense::map<int64_t, OrdersBucket *> bidBucketsMap;
  for (const auto &pair : bidBuckets_) {
    bidBucketsMap[pair.first] = pair.second.get();
  }
  utils::SerializationUtils::MarshallLongHashMap(bidBucketsMap, bytes);
}

} // namespace orderbook
} // namespace core
} // namespace exchange
