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
#include <memory>
#include <optional>
#include <vector>
#include "../../BytesIn.h"
#include "../../WriteBytesMarshallable.h"
#include "ReportResult.h"

namespace exchange {
namespace core {
// Forward declarations
namespace processors {
class MatchingEngineRouter;
class RiskEngine;
}  // namespace processors

namespace common {
namespace api {
namespace reports {

/**
 * ReportQueryBase - non-template base class for type erasure
 * Provides virtual methods that work with ReportResult* instead of template
 * types
 */
class ReportQueryBase : public WriteBytesMarshallable {
public:
  virtual ~ReportQueryBase() = default;

  /**
   * @return report type code (integer)
   */
  virtual int32_t GetReportTypeCode() const = 0;

  /**
   * Type-erased Process method for MatchingEngineRouter
   * Returns ReportResult* instead of template type
   */
  virtual std::optional<std::unique_ptr<ReportResult>>
  ProcessTypeErased(::exchange::core::processors::MatchingEngineRouter* matchingEngine) = 0;

  /**
   * Type-erased Process method for RiskEngine
   * Returns ReportResult* instead of template type
   */
  virtual std::optional<std::unique_ptr<ReportResult>>
  ProcessTypeErased(::exchange::core::processors::RiskEngine* riskEngine) = 0;

  /**
   * Create result from sections (matches Java createResult)
   * @param sections Vector of BytesIn sections (one per shard/section)
   * @return Merged result as ReportResult*
   */
  virtual std::unique_ptr<ReportResult>
  CreateResultTypeErased(const std::vector<BytesIn*>& sections) = 0;
};

/**
 * ReportQuery - reports query interface
 * @tparam T corresponding result type
 * Matches Java: public interface ReportQuery<T extends ReportResult> extends
 * WriteBytesMarshallable
 */
template <typename T>
class ReportQuery : public ReportQueryBase {
public:
  virtual ~ReportQuery() = default;

  /**
   * Report main logic.
   * This method is executed by matcher engine thread.
   *
   * @param matchingEngine matcher engine instance
   * @return custom result
   */
  virtual std::optional<std::unique_ptr<T>>
  Process(::exchange::core::processors::MatchingEngineRouter* matchingEngine) = 0;

  /**
   * Report main logic
   * This method is executed by risk engine thread.
   *
   * @param riskEngine risk engine instance.
   * @return custom result
   */
  virtual std::optional<std::unique_ptr<T>>
  Process(::exchange::core::processors::RiskEngine* riskEngine) = 0;

  /**
   * Create result from sections (matches Java createResult)
   * @param sections Vector of BytesIn sections (one per shard/section)
   * @return Merged result
   */
  virtual std::unique_ptr<T> CreateResult(const std::vector<BytesIn*>& sections) = 0;

  // Implementation of type-erased methods from ReportQueryBase
  std::optional<std::unique_ptr<ReportResult>>
  ProcessTypeErased(::exchange::core::processors::MatchingEngineRouter* matchingEngine) override {
    auto result = Process(matchingEngine);
    if (result.has_value()) {
      return std::optional<std::unique_ptr<ReportResult>>(
        std::unique_ptr<ReportResult>(result.value().release()));
    }
    return std::nullopt;
  }

  std::optional<std::unique_ptr<ReportResult>>
  ProcessTypeErased(::exchange::core::processors::RiskEngine* riskEngine) override {
    auto result = Process(riskEngine);
    if (result.has_value()) {
      return std::optional<std::unique_ptr<ReportResult>>(
        std::unique_ptr<ReportResult>(result.value().release()));
    }
    return std::nullopt;
  }

  std::unique_ptr<ReportResult>
  CreateResultTypeErased(const std::vector<BytesIn*>& sections) override {
    auto result = CreateResult(sections);
    return std::unique_ptr<ReportResult>(result.release());
  }
};

}  // namespace reports
}  // namespace api
}  // namespace common
}  // namespace core
}  // namespace exchange
