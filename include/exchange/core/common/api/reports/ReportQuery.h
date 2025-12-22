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
#include <memory>
#include <optional>

namespace exchange {
namespace core {
// Forward declarations
namespace processors {
class MatchingEngineRouter;
class RiskEngine;
} // namespace processors
namespace common {
namespace api {
namespace reports {

/**
 * ReportQuery - reports query interface
 * @tparam T corresponding result type
 */
template <typename T> class ReportQuery {
public:
  virtual ~ReportQuery() = default;

  /**
   * @return report type code (integer)
   */
  virtual int32_t GetReportTypeCode() const = 0;

  /**
   * Report main logic.
   * This method is executed by matcher engine thread.
   *
   * @param matchingEngine matcher engine instance
   * @return custom result
   */
  virtual std::optional<std::unique_ptr<T>>
  Process(processors::MatchingEngineRouter *matchingEngine) = 0;

  /**
   * Report main logic
   * This method is executed by risk engine thread.
   *
   * @param riskEngine risk engine instance.
   * @return custom result
   */
  virtual std::optional<std::unique_ptr<T>>
  Process(processors::RiskEngine *riskEngine) = 0;
};

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
