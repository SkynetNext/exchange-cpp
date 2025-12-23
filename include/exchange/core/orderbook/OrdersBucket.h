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

#include "../common/IOrder.h"
#include "../common/MatcherTradeEvent.h"
#include "../common/Order.h"
#include "../common/WriteBytesMarshallable.h"
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <functional>
#include <list>
#include <vector>

namespace exchange {
namespace core {
namespace orderbook {

class OrderBookEventsHelper;

/**
 * OrdersBucket - manages orders at the same price level
 * Maintains FIFO order (time priority) using insertion order
 */
class OrdersBucket : public common::WriteBytesMarshallable {
public:
  explicit OrdersBucket(int64_t price);

  /**
   * Constructor from BytesIn (deserialization)
   */
  OrdersBucket(common::BytesIn *bytes);

  int64_t GetPrice() const { return price_; }
  int64_t GetTotalVolume() const { return totalVolume_; }
  int32_t GetNumOrders() const {
    return static_cast<int32_t>(orderList_.size());
  }

  /**
   * Put a new order into bucket
   */
  void Put(::exchange::core::common::Order *order);

  /**
   * Remove order from the bucket
   * @return order if removed, or nullptr if not found
   */
  ::exchange::core::common::Order *Remove(int64_t orderId, int64_t uid);

  /**
   * Match orders starting from eldest (FIFO)
   * @param volumeToCollect - volume to collect
   * @param activeOrder - active order being matched
   * @param helper - events helper
   * @return matching result with events chain and volume
   */
  struct MatcherResult {
    ::exchange::core::common::MatcherTradeEvent *eventsChainHead = nullptr;
    ::exchange::core::common::MatcherTradeEvent *eventsChainTail = nullptr;
    int64_t volume = 0;
    std::vector<int64_t> ordersToRemove;
  };

  MatcherResult Match(int64_t volumeToCollect,
                      const ::exchange::core::common::IOrder *activeOrder,
                      OrderBookEventsHelper *helper);

  /**
   * Reduce size of an order
   */
  void ReduceSize(int64_t reduceSize);

  /**
   * Find order by ID
   */
  ::exchange::core::common::Order *FindOrder(int64_t orderId);

  /**
   * Execute action for each order (preserving FIFO order)
   */
  void
  ForEachOrder(std::function<void(::exchange::core::common::Order *)> consumer);

  /**
   * Get all orders (for testing only - creates new list)
   */
  std::vector<::exchange::core::common::Order *> GetAllOrders() const;

  /**
   * Validate internal state
   */
  void Validate() const;

  /**
   * WriteMarshallable interface
   */
  void WriteMarshallable(common::BytesOut &bytes) override;

private:
  int64_t price_;
  int64_t totalVolume_;

  // Maintain insertion order using list, but also have fast lookup by orderId
  // Using ankerl::unordered_dense for better performance (2-3x faster than
  // std::unordered_map)
  std::list<::exchange::core::common::Order *> orderList_;
  ankerl::unordered_dense::map<
      int64_t, std::list<::exchange::core::common::Order *>::iterator>
      orderMap_;
};

} // namespace orderbook
} // namespace core
} // namespace exchange
