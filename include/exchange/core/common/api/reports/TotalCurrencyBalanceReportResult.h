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

#include "ReportResult.h"
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <memory>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace reports {

/**
 * TotalCurrencyBalanceReportResult - total currency balance report result
 */
class TotalCurrencyBalanceReportResult : public ReportResult {
public:
  // currency -> balance
  ankerl::unordered_dense::map<int32_t, int64_t> *accountBalances;
  ankerl::unordered_dense::map<int32_t, int64_t> *fees;
  ankerl::unordered_dense::map<int32_t, int64_t> *adjustments;
  ankerl::unordered_dense::map<int32_t, int64_t> *suspends;
  ankerl::unordered_dense::map<int32_t, int64_t> *ordersBalances;

  // symbol -> volume
  ankerl::unordered_dense::map<int32_t, int64_t> *openInterestLong;
  ankerl::unordered_dense::map<int32_t, int64_t> *openInterestShort;

  TotalCurrencyBalanceReportResult(
      ankerl::unordered_dense::map<int32_t, int64_t> *accountBalances,
      ankerl::unordered_dense::map<int32_t, int64_t> *fees,
      ankerl::unordered_dense::map<int32_t, int64_t> *adjustments,
      ankerl::unordered_dense::map<int32_t, int64_t> *suspends,
      ankerl::unordered_dense::map<int32_t, int64_t> *ordersBalances,
      ankerl::unordered_dense::map<int32_t, int64_t> *openInterestLong,
      ankerl::unordered_dense::map<int32_t, int64_t> *openInterestShort)
      : accountBalances(accountBalances), fees(fees),
        adjustments(adjustments), suspends(suspends),
        ordersBalances(ordersBalances), openInterestLong(openInterestLong),
        openInterestShort(openInterestShort) {}

  static TotalCurrencyBalanceReportResult *CreateEmpty();

  static TotalCurrencyBalanceReportResult *
  OfOrderBalances(const ankerl::unordered_dense::map<int32_t, int64_t> &currencyBalance);
};

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange

