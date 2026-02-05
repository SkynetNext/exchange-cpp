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
#include "OrderAction.h"

namespace exchange::core::common {

// Forward declarations
class StateHash;

class IOrder {
public:
  virtual ~IOrder() = default;

  [[nodiscard]] virtual int64_t GetPrice() const = 0;
  [[nodiscard]] virtual int64_t GetSize() const = 0;
  [[nodiscard]] virtual int64_t GetFilled() const = 0;
  [[nodiscard]] virtual int64_t GetUid() const = 0;
  [[nodiscard]] virtual OrderAction GetAction() const = 0;
  [[nodiscard]] virtual int64_t GetOrderId() const = 0;
  [[nodiscard]] virtual int64_t GetTimestamp() const = 0;
  [[nodiscard]] virtual int64_t GetReserveBidPrice() const = 0;
};

}  // namespace exchange::core::common
