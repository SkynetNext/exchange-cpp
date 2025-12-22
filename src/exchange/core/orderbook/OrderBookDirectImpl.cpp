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
#include <algorithm>

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
      orderIdIndex_(objectsPool), bestAskOrder_(nullptr), bestBidOrder_(nullptr),
      eventsHelper_(eventsHelper) {
  logDebug_ = loggingCfg->loggingLevels.count(
      common::config::LoggingConfiguration::LoggingLevel::
          LOGGING_MATCHING_DEBUG);
}

const common::CoreSymbolSpecification *
OrderBookDirectImpl::GetSymbolSpec() const {
  return symbolSpec_;
}

void OrderBookDirectImpl::NewOrder(OrderCommand *cmd) {
  switch (cmd->orderType) {
  case OrderType::GTC:
    // TODO: implement newOrderPlaceGtc
    break;
  case OrderType::IOC:
    // TODO: implement newOrderMatchIoc
    break;
  case OrderType::FOK_BUDGET:
    // TODO: implement newOrderMatchFokBudget
    break;
  default:
    // log.warn("Unsupported order type: {}", cmd);
    eventsHelper_->AttachRejectEvent(cmd, cmd->size);
  }
}

CommandResultCode OrderBookDirectImpl::CancelOrder(OrderCommand *cmd) {
  DirectOrder *order = orderIdIndex_.Get(cmd->orderId);
  if (order == nullptr || order->uid != cmd->uid) {
    return CommandResultCode::MATCHING_UNKNOWN_ORDER_ID;
  }
  orderIdIndex_.Remove(cmd->orderId);
  // objectsPool_->Put(ObjectsPool::DIRECT_ORDER, order);

  // TODO: implement removeOrder logic
  // Bucket *freeBucket = RemoveOrder(order);
  // if (freeBucket != nullptr) {
  //     objectsPool_->Put(ObjectsPool::DIRECT_BUCKET, freeBucket);
  // }

  cmd->action = order->action;
  cmd->matcherEvent = eventsHelper_->SendReduceEvent(
      order, order->size - order->filled, true);

  return CommandResultCode::SUCCESS;
}

CommandResultCode OrderBookDirectImpl::MoveOrder(OrderCommand *cmd) {
  // TODO
  return CommandResultCode::SUCCESS;
}

CommandResultCode OrderBookDirectImpl::ReduceOrder(OrderCommand *cmd) {
  // TODO
  return CommandResultCode::SUCCESS;
}

std::unique_ptr<common::L2MarketData>
OrderBookDirectImpl::GetL2MarketDataSnapshot(int32_t size) {
  auto data = std::make_unique<common::L2MarketData>(size, size);
  FillAsks(size, data.get());
  FillBids(size, data.get());
  return data;
}

int32_t OrderBookDirectImpl::GetOrdersNum(OrderAction action) {
  auto &buckets = (action == OrderAction::ASK) ? askPriceBuckets_ : bidPriceBuckets_;
  int32_t count = 0;
  // collections::art::LongObjConsumer<Bucket> consumer
  // TODO: use ForEach with lambda adapter
  return count;
}

int64_t OrderBookDirectImpl::GetTotalOrdersVolume(OrderAction action) {
  // TODO
  return 0;
}

common::IOrder *OrderBookDirectImpl::GetOrderById(int64_t orderId) {
  return orderIdIndex_.Get(orderId);
}

void OrderBookDirectImpl::ValidateInternalState() {
  // TODO
}

OrderBookImplType OrderBookDirectImpl::GetImplementationType() const {
  return OrderBookImplType::DIRECT;
}

void OrderBookDirectImpl::FillAsks(int32_t size, common::L2MarketData *data) {
  data->askSize = 0;
  // TODO: use ForEach
}

void OrderBookDirectImpl::FillBids(int32_t size, common::L2MarketData *data) {
  data->bidSize = 0;
  // TODO: use ForEachDesc
}

int32_t OrderBookDirectImpl::GetTotalAskBuckets(int32_t limit) {
  return askPriceBuckets_.Size(limit);
}

int32_t OrderBookDirectImpl::GetTotalBidBuckets(int32_t limit) {
  return bidPriceBuckets_.Size(limit);
}

std::vector<common::Order *> OrderBookDirectImpl::FindUserOrders(int64_t uid) {
  std::vector<common::Order *> list;
  // TODO: use ForEach
  return list;
}

void OrderBookDirectImpl::Reset() {
  askPriceBuckets_.Clear();
  bidPriceBuckets_.Clear();
  orderIdIndex_.Clear();
  bestAskOrder_ = nullptr;
  bestBidOrder_ = nullptr;
}

OrderBookDirectImpl::DirectOrder *
OrderBookDirectImpl::FindOrder(int64_t orderId) {
  return orderIdIndex_.Get(orderId);
}

OrderBookDirectImpl::Bucket *
OrderBookDirectImpl::GetOrCreateBucket(int64_t price, bool isAsk) {
  auto &buckets = isAsk ? askPriceBuckets_ : bidPriceBuckets_;
  Bucket *bucket = buckets.Get(price);
  if (bucket == nullptr) {
    // bucket = objectsPool_->Get<Bucket>(ObjectsPool::DIRECT_BUCKET, []() {
    // return new Bucket(); });
    bucket = new Bucket(); // Placeholder
    bucket->price = price;
    bucket->numOrders = 0;
    bucket->totalVolume = 0;
    bucket->firstOrder = nullptr;
    bucket->lastOrder = nullptr;
    buckets.Put(price, bucket);
  }
  return bucket;
}

void OrderBookDirectImpl::RemoveOrder(DirectOrder *order) {
  // TODO
}

} // namespace orderbook
} // namespace core
} // namespace exchange

