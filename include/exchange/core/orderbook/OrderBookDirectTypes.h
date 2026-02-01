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

#include <cstdint>
#include "../common/BytesIn.h"
#include "../common/IOrder.h"
#include "../common/OrderAction.h"
#include "../common/StateHash.h"
#include "../common/WriteBytesMarshallable.h"

namespace exchange::core::orderbook {

struct DirectOrder;

struct Bucket {
  int64_t price = 0;
  DirectOrder* lastOrder = nullptr;
  int64_t totalVolume = 0;
  int32_t numOrders = 0;
};

struct DirectOrder : public common::IOrder,
                     public common::WriteBytesMarshallable,
                     public common::StateHash {
  int64_t orderId = 0;
  int64_t price = 0;
  int64_t size = 0;
  int64_t filled = 0;
  int64_t reserveBidPrice = 0;
  int64_t uid = 0;
  common::OrderAction action = common::OrderAction::ASK;
  int64_t timestamp = 0;

  DirectOrder* next = nullptr;
  DirectOrder* prev = nullptr;
  Bucket* bucket = nullptr;

  DirectOrder() = default;
  explicit DirectOrder(common::BytesIn& bytes);

  int64_t GetOrderId() const override {
    return orderId;
  }

  int64_t GetPrice() const override {
    return price;
  }

  int64_t GetSize() const override {
    return size;
  }

  int64_t GetFilled() const override {
    return filled;
  }

  int64_t GetReserveBidPrice() const override {
    return reserveBidPrice;
  }

  common::OrderAction GetAction() const override {
    return action;
  }

  int64_t GetUid() const override {
    return uid;
  }

  int64_t GetTimestamp() const override {
    return timestamp;
  }

  int32_t GetStateHash() const override;
  void WriteMarshallable(common::BytesOut& bytes) const override;
};

}  // namespace exchange::core::orderbook
