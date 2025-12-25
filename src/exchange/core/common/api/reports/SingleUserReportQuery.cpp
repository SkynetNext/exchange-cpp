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

#include <ankerl/unordered_dense.h>
#include <exchange/core/common/BytesIn.h>
#include <exchange/core/common/BytesOut.h>
#include <exchange/core/common/api/reports/ReportQueryFactory.h>
#include <exchange/core/common/api/reports/SingleUserReportQuery.h>
#include <exchange/core/common/api/reports/SingleUserReportResult.h>
#include <exchange/core/orderbook/IOrderBook.h>
#include <exchange/core/processors/MatchingEngineRouter.h>
#include <exchange/core/processors/RiskEngine.h>
#include <exchange/core/utils/Logger.h>
#include <vector>

namespace exchange {
namespace core {
namespace common {
namespace api {
namespace reports {

// Use fully qualified names to avoid namespace ambiguity
using MatchingEngineRouterType =
    ::exchange::core::processors::MatchingEngineRouter;
using RiskEngineType = ::exchange::core::processors::RiskEngine;

REGISTER_REPORT_QUERY_TYPE(SingleUserReportQuery,
                           ReportType::SINGLE_USER_REPORT);

SingleUserReportQuery::SingleUserReportQuery(BytesIn &bytesIn) {
  uid = bytesIn.ReadLong();
}

void SingleUserReportQuery::WriteMarshallable(BytesOut &bytes) const {
  // Match Java: bytes.writeLong(uid);
  bytes.WriteLong(uid);
}

std::unique_ptr<SingleUserReportResult>
SingleUserReportQuery::CreateResult(const std::vector<BytesIn *> &sections) {
  // Match Java: createResult(final Stream<BytesIn> sections)
  return SingleUserReportResult::Merge(sections);
}

std::optional<std::unique_ptr<SingleUserReportResult>>
SingleUserReportQuery::Process(MatchingEngineRouterType *matchingEngine) {
  // Match Java: process(MatchingEngineRouter matchingEngine)
  LOG_DEBUG("SingleUserReportQuery::Process(MatchingEngineRouter): uid={}",
            uid);
  ankerl::unordered_dense::map<int32_t, std::vector<common::Order *>> orders;

  const auto &orderBooks = matchingEngine->GetOrderBooks();
  LOG_DEBUG("SingleUserReportQuery::Process(MatchingEngineRouter): "
            "orderBooks.size()={}",
            orderBooks.size());
  for (auto *orderBook : orderBooks) {
    if (!orderBook) {
      continue;
    }
    auto userOrders = orderBook->FindUserOrders(uid);
    LOG_DEBUG("SingleUserReportQuery::Process(MatchingEngineRouter): "
              "userOrders.size()={} for orderBook",
              userOrders.size());
    // Don't put empty results, so that the report result merge procedure
    // would be simple
    if (!userOrders.empty()) {
      const auto *spec = orderBook->GetSymbolSpec();
      if (spec) {
        orders[spec->symbolId] = std::move(userOrders);
      }
    }
  }

  LOG_DEBUG("SingleUserReportQuery::Process(MatchingEngineRouter): final "
            "orders.size()={}",
            orders.size());
  auto result = std::make_optional(std::unique_ptr<SingleUserReportResult>(
      SingleUserReportResult::CreateFromMatchingEngine(uid, orders)));
  LOG_DEBUG("SingleUserReportQuery::Process(MatchingEngineRouter): returning "
            "result with has_value={}",
            result.has_value());
  return result;
}

std::optional<std::unique_ptr<SingleUserReportResult>>
SingleUserReportQuery::Process(RiskEngineType *riskEngine) {
  // Match Java: process(RiskEngine riskEngine)
  LOG_DEBUG("SingleUserReportQuery::Process(RiskEngine): uid={}", uid);
  if (!riskEngine->UidForThisHandler(uid)) {
    LOG_DEBUG("SingleUserReportQuery::Process: UidForThisHandler returned "
              "false for uid={}",
              uid);
    return std::nullopt;
  }

  auto *userProfileService = riskEngine->GetUserProfileService();
  if (!userProfileService) {
    LOG_DEBUG("SingleUserReportQuery::Process: userProfileService is nullptr");
    return std::nullopt;
  }

  auto *userProfile = userProfileService->GetUserProfile(uid);
  LOG_DEBUG("SingleUserReportQuery::Process: userProfile={} for uid={}",
            static_cast<void *>(userProfile), uid);
  if (userProfile != nullptr) {
    // Found user
    ankerl::unordered_dense::map<int32_t, SingleUserReportResult::Position>
        positions;
    positions.reserve(userProfile->positions.size());

    for (const auto &[symbolId, pos] : userProfile->positions) {
      if (!pos) {
        continue;
      }
      positions[symbolId] = SingleUserReportResult::Position(
          pos->currency, pos->direction, pos->openVolume, pos->openPriceSum,
          pos->profit, pos->pendingSellSize, pos->pendingBuySize);
    }

    // Debug: check accounts size
    LOG_DEBUG("SingleUserReportQuery::Process: uid={}, accounts.size()={}", uid,
              userProfile->accounts.size());

    return std::make_optional(std::unique_ptr<SingleUserReportResult>(
        SingleUserReportResult::CreateFromRiskEngineFound(
            uid, &userProfile->userStatus, userProfile->accounts, positions)));
  } else {
    // Not found
    LOG_DEBUG(
        "SingleUserReportQuery::Process: userProfile not found for uid={}",
        uid);
    return std::make_optional(std::unique_ptr<SingleUserReportResult>(
        SingleUserReportResult::CreateFromRiskEngineNotFound(uid)));
  }
}

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
