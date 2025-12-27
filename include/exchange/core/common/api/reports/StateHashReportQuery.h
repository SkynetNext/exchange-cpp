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

#include "../../BytesIn.h"
#include "ReportQuery.h"
#include "ReportType.h"
#include "StateHashReportResult.h"
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
 * StateHashReportQuery - state hash report query
 */
class StateHashReportQuery : public ReportQuery<StateHashReportResult> {
public:
  StateHashReportQuery() {}
  explicit StateHashReportQuery(BytesIn &bytesIn) {
    // do nothing
  }

  int32_t GetReportTypeCode() const override {
    return static_cast<int32_t>(ReportType::STATE_HASH);
  }

  std::unique_ptr<StateHashReportResult>
  CreateResult(const std::vector<BytesIn *> &sections) override;

  std::optional<std::unique_ptr<StateHashReportResult>>
  Process(::exchange::core::processors::MatchingEngineRouter *matchingEngine)
      override;

  std::optional<std::unique_ptr<StateHashReportResult>>
  Process(::exchange::core::processors::RiskEngine *riskEngine) override;

  // WriteMarshallable implementation (matches Java writeMarshallable)
  void WriteMarshallable(BytesOut &bytes) const override;
};

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
