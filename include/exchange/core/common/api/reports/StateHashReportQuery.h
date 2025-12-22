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

#include "ReportQuery.h"
#include "ReportType.h"
#include "StateHashReportResult.h"
#include <memory>
#include <optional>

namespace exchange {
namespace core {
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

  int32_t GetReportTypeCode() const override {
    return static_cast<int32_t>(ReportType::STATE_HASH);
  }

  std::optional<std::unique_ptr<StateHashReportResult>>
  Process(processors::MatchingEngineRouter *matchingEngine) override;

  std::optional<std::unique_ptr<StateHashReportResult>>
  Process(processors::RiskEngine *riskEngine) override;
};

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
