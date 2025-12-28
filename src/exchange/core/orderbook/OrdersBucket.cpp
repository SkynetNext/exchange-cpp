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

#include <algorithm>
#include <exchange/core/common/BytesIn.h>
#include <exchange/core/common/BytesOut.h>
#include <exchange/core/common/MatcherTradeEvent.h>
#include <exchange/core/common/Order.h>
#include <exchange/core/orderbook/OrderBookEventsHelper.h>
#include <exchange/core/orderbook/OrdersBucket.h>
#include <exchange/core/utils/Logger.h>
#include <exchange/core/utils/SerializationUtils.h>
#include <stdexcept>
#include <vector>

namespace exchange {
namespace core {
namespace orderbook {

OrdersBucket::OrdersBucket(int64_t price) : price_(price), totalVolume_(0) {}

OrdersBucket::OrdersBucket(common::BytesIn *bytes) {
  if (bytes == nullptr) {
    throw std::invalid_argument("BytesIn cannot be nullptr");
  }
  price_ = bytes->ReadLong();

  // Read orders map (long -> Order*)
  int length = bytes->ReadInt();
  for (int i = 0; i < length; i++) {
    int64_t orderId = bytes->ReadLong();
    common::Order *order = new common::Order(*bytes);
    Put(order);
  }

  totalVolume_ = bytes->ReadLong();
}

void OrdersBucket::Put(::exchange::core::common::Order *order) {
  if (order == nullptr) {
    throw std::invalid_argument("Order cannot be null");
  }

  orderList_.push_back(order);
  orderMap_[order->orderId] = std::prev(orderList_.end());
  totalVolume_ += order->size - order->filled;
}

::exchange::core::common::Order *OrdersBucket::Remove(int64_t orderId,
                                                      int64_t uid) {
  auto it = orderMap_.find(orderId);
  if (it == orderMap_.end()) {
    return nullptr;
  }

  ::exchange::core::common::Order *order = *it->second;
  if (order->uid != uid) {
    return nullptr;
  }

  totalVolume_ -= order->size - order->filled;
  orderList_.erase(it->second);
  orderMap_.erase(it);

  return order;
}

// Template implementation moved to header file for compile-time polymorphism

void OrdersBucket::ReduceSize(int64_t reduceSize) {
  totalVolume_ -= reduceSize;
}

::exchange::core::common::Order *OrdersBucket::FindOrder(int64_t orderId) {
  auto it = orderMap_.find(orderId);
  if (it == orderMap_.end()) {
    return nullptr;
  }
  return *it->second;
}

void OrdersBucket::ForEachOrder(
    std::function<void(::exchange::core::common::Order *)> consumer) {
  for (::exchange::core::common::Order *order : orderList_) {
    consumer(order);
  }
}

std::vector<::exchange::core::common::Order *>
OrdersBucket::GetAllOrders() const {
  std::vector<::exchange::core::common::Order *> result;
  result.reserve(orderList_.size());
  for (::exchange::core::common::Order *order : orderList_) {
    result.push_back(order);
  }
  return result;
}

void OrdersBucket::Validate() const {
  int64_t calculatedVolume = 0;
  for (const ::exchange::core::common::Order *order : orderList_) {
    calculatedVolume += order->size - order->filled;
  }

  if (calculatedVolume != totalVolume_) {
    throw std::runtime_error("OrdersBucket validation failed: totalVolume=" +
                             std::to_string(totalVolume_) +
                             " calculated=" + std::to_string(calculatedVolume));
  }
}

void OrdersBucket::WriteMarshallable(common::BytesOut &bytes) const {
  // Write price
  bytes.WriteLong(price_);

  // Convert orderList_ to map for serialization (orderId -> Order*)
  ankerl::unordered_dense::map<int64_t, common::Order *> orderMap;
  for (common::Order *order : orderList_) {
    if (order != nullptr) {
      orderMap[order->orderId] = order;
    }
  }

  // Write orders map
  utils::SerializationUtils::MarshallLongHashMap(orderMap, bytes);

  // Write totalVolume
  bytes.WriteLong(totalVolume_);
}

} // namespace orderbook
} // namespace core
} // namespace exchange
