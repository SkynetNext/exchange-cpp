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

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace exchange {
namespace core {
namespace common {
namespace cmd {

enum class OrderCommandType : int8_t {
  PLACE_ORDER = 1,
  CANCEL_ORDER = 2,
  MOVE_ORDER = 3,
  REDUCE_ORDER = 4,

  ORDER_BOOK_REQUEST = 6,

  ADD_USER = 10,
  BALANCE_ADJUSTMENT = 11,
  SUSPEND_USER = 12,
  RESUME_USER = 13,

  BINARY_DATA_QUERY = 90,
  BINARY_DATA_COMMAND = 91,

  PERSIST_STATE_MATCHING = 110,
  PERSIST_STATE_RISK = 111,

  GROUPING_CONTROL = 118,
  NOP = 120,
  RESET = 124,
  SHUTDOWN_SIGNAL = 127,

  RESERVED_COMPRESSED = -1
};

inline bool IsMutate(OrderCommandType type) {
  switch (type) {
  case OrderCommandType::PLACE_ORDER:
  case OrderCommandType::CANCEL_ORDER:
  case OrderCommandType::MOVE_ORDER:
  case OrderCommandType::REDUCE_ORDER:
  case OrderCommandType::ADD_USER:
  case OrderCommandType::BALANCE_ADJUSTMENT:
  case OrderCommandType::SUSPEND_USER:
  case OrderCommandType::RESUME_USER:
  case OrderCommandType::BINARY_DATA_COMMAND:
  case OrderCommandType::PERSIST_STATE_MATCHING:
  case OrderCommandType::PERSIST_STATE_RISK:
  case OrderCommandType::RESET:
    return true;
  default:
    return false;
  }
}

inline OrderCommandType OrderCommandTypeFromCode(int8_t code) {
  switch (code) {
  case 1:
    return OrderCommandType::PLACE_ORDER;
  case 2:
    return OrderCommandType::CANCEL_ORDER;
  case 3:
    return OrderCommandType::MOVE_ORDER;
  case 4:
    return OrderCommandType::REDUCE_ORDER;
  case 6:
    return OrderCommandType::ORDER_BOOK_REQUEST;
  case 10:
    return OrderCommandType::ADD_USER;
  case 11:
    return OrderCommandType::BALANCE_ADJUSTMENT;
  case 12:
    return OrderCommandType::SUSPEND_USER;
  case 13:
    return OrderCommandType::RESUME_USER;
  case 90:
    return OrderCommandType::BINARY_DATA_QUERY;
  case 91:
    return OrderCommandType::BINARY_DATA_COMMAND;
  case 110:
    return OrderCommandType::PERSIST_STATE_MATCHING;
  case 111:
    return OrderCommandType::PERSIST_STATE_RISK;
  case 118:
    return OrderCommandType::GROUPING_CONTROL;
  case 120:
    return OrderCommandType::NOP;
  case 124:
    return OrderCommandType::RESET;
  case 127:
    return OrderCommandType::SHUTDOWN_SIGNAL;
  case -1:
    return OrderCommandType::RESERVED_COMPRESSED;
  default:
    throw std::invalid_argument("Unknown order command type code: " +
                                std::to_string(code));
  }
}

inline int8_t OrderCommandTypeToCode(OrderCommandType type) {
  return static_cast<int8_t>(type);
}

} // namespace cmd
} // namespace common
} // namespace core
} // namespace exchange
