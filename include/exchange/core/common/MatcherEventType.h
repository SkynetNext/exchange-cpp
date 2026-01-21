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

enum class MatcherEventType : uint8_t { TRADE = 0, REDUCE = 1, REJECT = 2, BINARY_EVENT = 3 };

inline MatcherEventType MatcherEventTypeFromCode(uint8_t code) {
  switch (code) {
    case 0:
      return MatcherEventType::TRADE;
    case 1:
      return MatcherEventType::REDUCE;
    case 2:
      return MatcherEventType::REJECT;
    case 3:
      return MatcherEventType::BINARY_EVENT;
    default:
      throw std::invalid_argument("unknown MatcherEventType: " + std::to_string(code));
  }
}

inline uint8_t MatcherEventTypeToCode(MatcherEventType type) {
  return static_cast<uint8_t>(type);
}

}  // namespace exchange::core::common
