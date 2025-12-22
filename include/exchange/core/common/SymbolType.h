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

namespace exchange {
namespace core {
namespace common {

enum class SymbolType : uint8_t {
  CURRENCY_EXCHANGE_PAIR = 0,
  FUTURES_CONTRACT = 1,
  OPTION = 2
};

inline SymbolType SymbolTypeFromCode(int32_t code) {
  switch (code) {
  case 0:
    return SymbolType::CURRENCY_EXCHANGE_PAIR;
  case 1:
    return SymbolType::FUTURES_CONTRACT;
  case 2:
    return SymbolType::OPTION;
  default:
    throw std::invalid_argument("unknown SymbolType code: " +
                                std::to_string(code));
  }
}

inline uint8_t SymbolTypeToCode(SymbolType type) {
  return static_cast<uint8_t>(type);
}

} // namespace common
} // namespace core
} // namespace exchange
