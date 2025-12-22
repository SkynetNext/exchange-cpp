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
#include "RiskEngine.h"
#include <memory>
#include <optional>

namespace exchange {
namespace core {
namespace processors {

/**
 * RiskEngineReportQueriesHandler - adapter to connect
 * RiskEngine::HandleReportQuery to ReportQueriesHandler interface
 *
 * Since ReportQueriesHandler::HandleReport is a template method (not virtual),
 * we inherit from ReportQueriesHandler and provide template method
 * implementation that forwards to RiskEngine::HandleReportQuery.
 */
class RiskEngineReportQueriesHandler
    : public common::api::reports::ReportQueriesHandler {
public:
  explicit RiskEngineReportQueriesHandler(RiskEngine *riskEngine)
      : riskEngine_(riskEngine) {}

  // Template method implementation - forwards to RiskEngine::HandleReportQuery
  // Note: This is not a virtual override, but a template method specialization
  template <typename R>
  std::optional<std::unique_ptr<R>>
  HandleReport(common::api::reports::ReportQuery<R> *reportQuery) {
    if (riskEngine_ == nullptr || reportQuery == nullptr) {
      return std::nullopt;
    }
    return riskEngine_->HandleReportQuery(reportQuery);
  }

private:
  RiskEngine *riskEngine_;
};

} // namespace processors
} // namespace core
} // namespace exchange
