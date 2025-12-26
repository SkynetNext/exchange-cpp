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

#include "../common/api/reports/ReportQueriesHandler.h"
#include "../common/api/reports/ReportQuery.h"
#include "../common/api/reports/ReportResult.h"
#include "../utils/Logger.h"
#include "MatchingEngineRouter.h"
#include <memory>
#include <optional>

namespace exchange {
namespace core {
namespace processors {

/**
 * MatchingEngineReportQueriesHandler - adapter to connect
 * MatchingEngineRouter to ReportQueriesHandler interface
 *
 * Uses type erasure: overrides HandleReportImpl to work with ReportQueryBase*,
 * then calls the type-erased ProcessTypeErased method.
 */
class MatchingEngineReportQueriesHandler
    : public common::api::reports::ReportQueriesHandler {
public:
  explicit MatchingEngineReportQueriesHandler(
      MatchingEngineRouter *matchingEngine)
      : matchingEngine_(matchingEngine) {}

protected:
  // Override HandleReportImpl to use type erasure
  std::optional<std::unique_ptr<common::api::reports::ReportResult>>
  HandleReportImpl(common::api::reports::ReportQueryBase *reportQuery) override {
    if (matchingEngine_ == nullptr || reportQuery == nullptr) {
      LOG_WARN("[MatchingEngineReportQueriesHandler] HandleReportImpl: matchingEngine_ or reportQuery is nullptr");
      return std::nullopt;
    }
    
    return reportQuery->ProcessTypeErased(matchingEngine_);
  }

private:
  MatchingEngineRouter *matchingEngine_;
};

} // namespace processors
} // namespace core
} // namespace exchange
