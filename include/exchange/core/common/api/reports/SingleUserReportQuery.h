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

#include "../../BytesIn.h"
#include "ReportQuery.h"
#include "ReportType.h"
#include "SingleUserReportResult.h"
#include <cstdint>
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
 * SingleUserReportQuery - single user report query
 */
class SingleUserReportQuery
    : public ReportQuery<SingleUserReportResult> {
public:
  int64_t uid;

  explicit SingleUserReportQuery(int64_t uid) : uid(uid) {}
  explicit SingleUserReportQuery(BytesIn &bytesIn);

  int32_t GetReportTypeCode() const override {
    return static_cast<int32_t>(ReportType::SINGLE_USER_REPORT);
  }

  std::optional<std::unique_ptr<SingleUserReportResult>>
  Process(processors::MatchingEngineRouter *matchingEngine) override;

  std::optional<std::unique_ptr<SingleUserReportResult>>
  Process(processors::RiskEngine *riskEngine) override;
};

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange

