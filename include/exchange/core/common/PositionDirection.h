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
#include <stdexcept>
#include <string>

namespace exchange {
namespace core {
namespace common {

enum class PositionDirection : int8_t { LONG = 1, SHORT = -1, EMPTY = 0 };

inline int32_t GetMultiplier(PositionDirection direction) {
  return static_cast<int32_t>(direction);
}

inline PositionDirection PositionDirectionFromOrderAction(OrderAction action) {
  return action == OrderAction::BID ? PositionDirection::LONG
                                    : PositionDirection::SHORT;
}

inline PositionDirection PositionDirectionFromCode(int8_t code) {
  switch (code) {
  case 1:
    return PositionDirection::LONG;
  case -1:
    return PositionDirection::SHORT;
  case 0:
    return PositionDirection::EMPTY;
  default:
    throw std::invalid_argument("unknown PositionDirection: " +
                                std::to_string(code));
  }
}

inline bool IsOppositeToAction(PositionDirection direction,
                               OrderAction action) {
  return (direction == PositionDirection::LONG && action == OrderAction::ASK) ||
         (direction == PositionDirection::SHORT && action == OrderAction::BID);
}

inline bool IsSameAsAction(PositionDirection direction, OrderAction action) {
  return (direction == PositionDirection::LONG && action == OrderAction::BID) ||
         (direction == PositionDirection::SHORT && action == OrderAction::ASK);
}

} // namespace common
} // namespace core
} // namespace exchange
