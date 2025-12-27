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
namespace api {
namespace binary {

/**
 * BinaryCommandType - binary command type enumeration
 */
enum class BinaryCommandType : int32_t {
  ADD_ACCOUNTS = 1002,
  ADD_SYMBOLS = 1003
};

inline BinaryCommandType BinaryCommandTypeFromCode(int32_t code) {
  switch (code) {
  case 1002:
    return BinaryCommandType::ADD_ACCOUNTS;
  case 1003:
    return BinaryCommandType::ADD_SYMBOLS;
  default:
    throw std::invalid_argument("unknown BinaryCommandType: " +
                                std::to_string(code));
  }
}

inline int32_t BinaryCommandTypeToCode(BinaryCommandType type) {
  return static_cast<int32_t>(type);
}

} // namespace binary
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
