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

#include <memory>
#include <optional>
#include "../../BytesIn.h"
#include "../../BytesOut.h"
#include "ReportQuery.h"
#include "ReportType.h"
#include "TotalCurrencyBalanceReportResult.h"

namespace exchange {
namespace core {
namespace processors {
class MatchingEngineRouter;
class RiskEngine;
}  // namespace processors

namespace common {
namespace api {
namespace reports {

/**
 * TotalCurrencyBalanceReportQuery - total currency balance report query
 * WriteBytesMarshallable is inherited from ReportQuery base class
 */
class TotalCurrencyBalanceReportQuery : public ReportQuery<TotalCurrencyBalanceReportResult> {
public:
  TotalCurrencyBalanceReportQuery() {}

  explicit TotalCurrencyBalanceReportQuery(BytesIn& bytesIn) {
    // do nothing
  }

  int32_t GetReportTypeCode() const override {
    return static_cast<int32_t>(ReportType::TOTAL_CURRENCY_BALANCE);
  }

  std::optional<std::unique_ptr<TotalCurrencyBalanceReportResult>>
  Process(::exchange::core::processors::MatchingEngineRouter* matchingEngine) override;

  std::optional<std::unique_ptr<TotalCurrencyBalanceReportResult>>
  Process(::exchange::core::processors::RiskEngine* riskEngine) override;

  // CreateResult implementation (matches Java createResult)
  std::unique_ptr<TotalCurrencyBalanceReportResult>
  CreateResult(const std::vector<BytesIn*>& sections) override;

  // WriteMarshallable implementation (matches Java writeMarshallable)
  void WriteMarshallable(BytesOut& bytes) const override;
};

}  // namespace reports
}  // namespace api
}  // namespace common
}  // namespace core
}  // namespace exchange
