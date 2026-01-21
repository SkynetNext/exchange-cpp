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
#include <stdexcept>
#include <string>

namespace exchange::core::common {

enum class OrderType : uint8_t {
  // Good till Cancel - equivalent to regular limit order
  GTC = 0,

  // Immediate or Cancel - equivalent to strict-risk market order
  IOC = 1,         // with price cap
  IOC_BUDGET = 2,  // with total amount cap

  // Fill or Kill - execute immediately completely or not at all
  FOK = 3,        // with price cap
  FOK_BUDGET = 4  // total amount cap
};

inline OrderType OrderTypeFromCode(uint8_t code) {
  switch (code) {
    case 0:
      return OrderType::GTC;
    case 1:
      return OrderType::IOC;
    case 2:
      return OrderType::IOC_BUDGET;
    case 3:
      return OrderType::FOK;
    case 4:
      return OrderType::FOK_BUDGET;
    default:
      throw std::invalid_argument("unknown OrderType: " + std::to_string(code));
  }
}

inline uint8_t OrderTypeToCode(OrderType type) {
  return static_cast<uint8_t>(type);
}

}  // namespace exchange::core::common
