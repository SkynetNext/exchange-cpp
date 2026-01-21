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

namespace exchange {
namespace core {
namespace common {

enum class OrderAction : uint8_t { ASK = 0, BID = 1 };

inline OrderAction OrderActionFromCode(uint8_t code) {
  switch (code) {
    case 0:
      return OrderAction::ASK;
    case 1:
      return OrderAction::BID;
    default:
      throw std::invalid_argument("unknown OrderAction: " + std::to_string(code));
  }
}

inline uint8_t OrderActionToCode(OrderAction action) {
  return static_cast<uint8_t>(action);
}

inline OrderAction Opposite(OrderAction action) {
  return action == OrderAction::ASK ? OrderAction::BID : OrderAction::ASK;
}

}  // namespace common
}  // namespace core
}  // namespace exchange
