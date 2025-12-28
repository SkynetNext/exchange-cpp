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

#include "BytesIn.h"
#include "IOrder.h"
#include "OrderAction.h"
#include "StateHash.h"
#include "WriteBytesMarshallable.h"
#include <cstdint>
#include <string>

namespace exchange {
namespace core {
namespace common {

/**
 * Extending OrderCommand allows to avoid creating new objects
 * for instantly matching orders (MARKET or marketable LIMIT orders)
 * as well as use same code for matching moved orders
 *
 * No external references allowed to such object - order objects only live
 * inside OrderBook.
 */
class Order : public IOrderBase<Order>,
              public IOrder,
              public StateHash,
              public WriteBytesMarshallable {
public:
  int64_t orderId = 0;
  int64_t price = 0;
  int64_t size = 0;
  int64_t filled = 0;

  // new orders - reserved price for fast moves of GTC bid orders in exchange
  // mode
  int64_t reserveBidPrice = 0;

  // required for PLACE_ORDER only;
  OrderAction action = OrderAction::ASK;

  int64_t uid = 0;
  int64_t timestamp = 0;

  Order() = default;

  Order(int64_t orderId, int64_t price, int64_t size, int64_t filled,
        int64_t reserveBidPrice, OrderAction action, int64_t uid,
        int64_t timestamp)
      : orderId(orderId), price(price), size(size), filled(filled),
        reserveBidPrice(reserveBidPrice), action(action), uid(uid),
        timestamp(timestamp) {}

  /**
   * Constructor from BytesIn (deserialization)
   */
  Order(BytesIn &bytes);

  // IOrder interface implementation
  int64_t GetPrice() const override { return price; }
  int64_t GetSize() const override { return size; }
  int64_t GetFilled() const override { return filled; }
  int64_t GetUid() const override { return uid; }
  OrderAction GetAction() const override { return action; }
  int64_t GetOrderId() const override { return orderId; }
  int64_t GetTimestamp() const override { return timestamp; }
  int64_t GetReserveBidPrice() const override { return reserveBidPrice; }

  // StateHash interface implementation
  int32_t GetStateHash() const override;

  // WriteBytesMarshallable interface
  void WriteMarshallable(BytesOut &bytes) const override;

  // Comparison operators
  bool operator==(const Order &other) const;
  bool operator!=(const Order &other) const { return !(*this == other); }

  // String representation
  std::string ToString() const;
};

} // namespace common
} // namespace core
} // namespace exchange
