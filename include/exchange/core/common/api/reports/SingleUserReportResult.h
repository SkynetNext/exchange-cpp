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

#include "../../Order.h"
#include "../../PositionDirection.h"
#include "../../UserStatus.h"
#include "ReportResult.h"
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace reports {

/**
 * SingleUserReportResult - single user report result
 */
class SingleUserReportResult : public ReportResult {
public:
  enum class QueryExecutionStatus : int32_t { OK = 0, USER_NOT_FOUND = 1 };

  struct Position {
    int32_t quoteCurrency;
    PositionDirection direction;
    int64_t openVolume;
    int64_t openPriceSum;
    int64_t profit;
    int64_t pendingSellSize;
    int64_t pendingBuySize;

    Position(int32_t quoteCurrency, PositionDirection direction,
             int64_t openVolume, int64_t openPriceSum, int64_t profit,
             int64_t pendingSellSize, int64_t pendingBuySize)
        : quoteCurrency(quoteCurrency), direction(direction),
          openVolume(openVolume), openPriceSum(openPriceSum), profit(profit),
          pendingSellSize(pendingSellSize), pendingBuySize(pendingBuySize) {}
  };

  int64_t uid;
  UserStatus *userStatus;
  std::unique_ptr<ankerl::unordered_dense::map<int32_t, int64_t>> accounts;
  std::unique_ptr<ankerl::unordered_dense::map<int32_t, Position>> positions;
  std::unique_ptr<ankerl::unordered_dense::map<int32_t, std::vector<Order *>>>
      orders;
  QueryExecutionStatus queryExecutionStatus;

  static SingleUserReportResult *CreateFromMatchingEngine(
      int64_t uid,
      const ankerl::unordered_dense::map<int32_t, std::vector<Order *>>
          &orders);

  static SingleUserReportResult *CreateFromRiskEngineFound(
      int64_t uid, UserStatus *userStatus,
      const ankerl::unordered_dense::map<int32_t, int64_t> &accounts,
      const ankerl::unordered_dense::map<int32_t, Position> &positions);

  static std::unique_ptr<SingleUserReportResult>
  CreateFromRiskEngineNotFound(int64_t uid);
};

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
