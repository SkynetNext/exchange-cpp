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

#include <exchange/core/common/api/reports/ReportQueryFactory.h>
#include <exchange/core/common/api/reports/TotalCurrencyBalanceReportQuery.h>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace reports {

REGISTER_REPORT_QUERY_TYPE(TotalCurrencyBalanceReportQuery,
                           ReportType::TOTAL_CURRENCY_BALANCE);

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
