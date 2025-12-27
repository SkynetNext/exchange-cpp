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
namespace reports {

/**
 * ReportType - report type enumeration
 */
enum class ReportType : int32_t {
  STATE_HASH = 10001,
  SINGLE_USER_REPORT = 10002,
  TOTAL_CURRENCY_BALANCE = 10003
};

inline ReportType ReportTypeFromCode(int32_t code) {
  switch (code) {
  case 10001:
    return ReportType::STATE_HASH;
  case 10002:
    return ReportType::SINGLE_USER_REPORT;
  case 10003:
    return ReportType::TOTAL_CURRENCY_BALANCE;
  default:
    throw std::invalid_argument("unknown ReportType: " + std::to_string(code));
  }
}

inline int32_t ReportTypeToCode(ReportType type) {
  return static_cast<int32_t>(type);
}

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
