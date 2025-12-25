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
 * MatchingEngineRouter::HandleReportQuery to ReportQueriesHandler interface
 */
class MatchingEngineReportQueriesHandler
    : public common::api::reports::ReportQueriesHandler {
public:
  explicit MatchingEngineReportQueriesHandler(
      MatchingEngineRouter *matchingEngine)
      : matchingEngine_(matchingEngine) {}

  template <typename R>
  std::optional<std::unique_ptr<R>>
  HandleReport(common::api::reports::ReportQuery<R> *reportQuery) {
    if (matchingEngine_ == nullptr || reportQuery == nullptr) {
      LOG_DEBUG("MatchingEngineReportQueriesHandler::HandleReport: "
                "matchingEngine_ or reportQuery is nullptr");
      return std::nullopt;
    }
    LOG_DEBUG("MatchingEngineReportQueriesHandler::HandleReportImpl: calling "
              "matchingEngine_->HandleReportQuery");
    // Use type erasure: call Process with MatchingEngineRouter
    // The actual type will be handled by the template HandleReport method
    auto result = reportQuery->Process(matchingEngine_);
    LOG_DEBUG("MatchingEngineReportQueriesHandler::HandleReportImpl: Process "
              "returned has_value={}",
              result.has_value());
    if (result.has_value()) {
      // Convert to ReportResult*
      return std::optional<std::unique_ptr<common::api::reports::ReportResult>>(
          std::unique_ptr<common::api::reports::ReportResult>(
              result.value().release()));
    }
    return std::nullopt;
  }

private:
  MatchingEngineRouter *matchingEngine_;
};

} // namespace processors
} // namespace core
} // namespace exchange
