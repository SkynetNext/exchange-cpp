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

#include "OrderAction.h"
#include <cstdint>

namespace exchange {
namespace core {
namespace common {

// Forward declarations
class StateHash;

/**
 * CRTP base class for IOrder - eliminates virtual function calls in hot paths
 *
 * Usage:
 *   struct MyOrder : public IOrder<MyOrder> {
 *       int64_t price;
 *       int64_t GetPrice() const { return price; }
 *       // ... implement all IOrder methods
 *   };
 */
template <typename Derived> class IOrder {
public:
  // CRTP methods - compile-time polymorphism, no virtual function overhead
  // These methods forward to Derived's implementation (which can be
  // non-virtual)
  int64_t GetPrice() const {
    return static_cast<const Derived *>(this)->GetPrice();
  }

  int64_t GetSize() const {
    return static_cast<const Derived *>(this)->GetSize();
  }

  int64_t GetFilled() const {
    return static_cast<const Derived *>(this)->GetFilled();
  }

  int64_t GetUid() const {
    return static_cast<const Derived *>(this)->GetUid();
  }

  OrderAction GetAction() const {
    return static_cast<const Derived *>(this)->GetAction();
  }

  int64_t GetOrderId() const {
    return static_cast<const Derived *>(this)->GetOrderId();
  }

  int64_t GetTimestamp() const {
    return static_cast<const Derived *>(this)->GetTimestamp();
  }

  int64_t GetReserveBidPrice() const {
    return static_cast<const Derived *>(this)->GetReserveBidPrice();
  }
};

} // namespace common
} // namespace core
} // namespace exchange
