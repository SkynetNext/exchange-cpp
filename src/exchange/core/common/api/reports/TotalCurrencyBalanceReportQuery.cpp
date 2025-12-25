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
#include <exchange/core/common/Order.h>
#include <exchange/core/common/SymbolPositionRecord.h>
#include <exchange/core/common/SymbolType.h>
#include <exchange/core/common/api/reports/ReportQueryFactory.h>
#include <exchange/core/common/api/reports/TotalCurrencyBalanceReportQuery.h>
#include <exchange/core/common/api/reports/TotalCurrencyBalanceReportResult.h>
#include <exchange/core/orderbook/IOrderBook.h>
#include <exchange/core/processors/MatchingEngineRouter.h>
#include <exchange/core/processors/RiskEngine.h>
#include <exchange/core/utils/CoreArithmeticUtils.h>
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

REGISTER_REPORT_QUERY_TYPE(TotalCurrencyBalanceReportQuery,
                           ReportType::TOTAL_CURRENCY_BALANCE);

void TotalCurrencyBalanceReportQuery::WriteMarshallable(BytesOut &bytes) const {
  // Match Java: do nothing (empty implementation)
  // TotalCurrencyBalanceReportQuery has no fields to serialize
}

std::unique_ptr<TotalCurrencyBalanceReportResult>
TotalCurrencyBalanceReportQuery::CreateResult(
    const std::vector<BytesIn *> &sections) {
  // Match Java: createResult(final Stream<BytesIn> sections)
  return TotalCurrencyBalanceReportResult::Merge(sections);
}

std::optional<std::unique_ptr<TotalCurrencyBalanceReportResult>>
TotalCurrencyBalanceReportQuery::Process(
    MatchingEngineRouterType *matchingEngine) {
  // Match Java: process(MatchingEngineRouter matchingEngine)
  ankerl::unordered_dense::map<int32_t, int64_t> currencyBalance;

  const auto &orderBooks = matchingEngine->GetOrderBooks();
  for (auto *orderBook : orderBooks) {
    if (!orderBook) {
      continue;
    }
    const auto *spec = orderBook->GetSymbolSpec();
    if (!spec || spec->type != common::SymbolType::CURRENCY_EXCHANGE_PAIR) {
      continue;
    }

    // Process ask orders
    orderBook->ProcessAskOrders([&](const common::IOrder *order) {
      if (!order) {
        return;
      }
      int64_t remainingSize = order->GetSize() - order->GetFilled();
      if (remainingSize > 0) {
        int64_t amount =
            utils::CoreArithmeticUtils::CalculateAmountAsk(remainingSize, spec);
        currencyBalance[spec->baseCurrency] += amount;
      }
    });

    // Process bid orders
    orderBook->ProcessBidOrders([&](const common::IOrder *order) {
      if (!order) {
        return;
      }
      int64_t remainingSize = order->GetSize() - order->GetFilled();
      if (remainingSize > 0) {
        int64_t amount = utils::CoreArithmeticUtils::CalculateAmountBidTakerFee(
            remainingSize, order->GetReserveBidPrice(), spec);
        currencyBalance[spec->quoteCurrency] += amount;
      }
    });
  }

  return std::make_optional(std::unique_ptr<TotalCurrencyBalanceReportResult>(
      TotalCurrencyBalanceReportResult::OfOrderBalances(currencyBalance)));
}

std::optional<std::unique_ptr<TotalCurrencyBalanceReportResult>>
TotalCurrencyBalanceReportQuery::Process(RiskEngineType *riskEngine) {
  // Match Java: process(RiskEngine riskEngine)
  // Prepare fast price cache for profit estimation
  ankerl::unordered_dense::map<
      int32_t, ::exchange::core::processors::LastPriceCacheRecord>
      dummyLastPriceCache;
  const auto &lastPriceCache = riskEngine->GetLastPriceCache();
  for (const auto &[symbolId, record] : lastPriceCache) {
    dummyLastPriceCache[symbolId] = record.AveragingRecord();
  }

  ankerl::unordered_dense::map<int32_t, int64_t> currencyBalance;
  ankerl::unordered_dense::map<int32_t, int64_t> symbolOpenInterestLong;
  ankerl::unordered_dense::map<int32_t, int64_t> symbolOpenInterestShort;

  auto *symbolSpecProvider = riskEngine->GetSymbolSpecificationProvider();
  auto *userProfileService = riskEngine->GetUserProfileService();

  if (!symbolSpecProvider || !userProfileService) {
    return std::nullopt;
  }

  // Process all user profiles
  // Match Java:
  // riskEngine.getUserProfileService().getUserProfiles().forEach(...)
  auto userProfiles = userProfileService->GetUserProfiles();
  for (const auto *profile : userProfiles) {
    if (!profile) {
      continue;
    }

    // Add accounts
    for (const auto &[currency, balance] : profile->accounts) {
      currencyBalance[currency] += balance;
    }

    // Process positions
    for (const auto &[symbolId, positionRecord] : profile->positions) {
      if (!positionRecord) {
        continue;
      }
      const auto *spec = symbolSpecProvider->GetSymbolSpecification(symbolId);
      if (!spec) {
        continue;
      }

      auto avgPriceIt = dummyLastPriceCache.find(symbolId);
      // Convert RiskEngine::LastPriceCacheRecord to
      // common::processors::LastPriceCacheRecord (as done in RiskEngine.cpp)
      common::processors::LastPriceCacheRecord avgPrice;
      if (avgPriceIt != dummyLastPriceCache.end()) {
        avgPrice.askPrice = avgPriceIt->second.askPrice;
        avgPrice.bidPrice = avgPriceIt->second.bidPrice;
      } else {
        // Use dummy values
        avgPrice.askPrice = 42;
        avgPrice.bidPrice = 42;
      }

      int64_t profit = positionRecord->EstimateProfit(*spec, &avgPrice);
      currencyBalance[positionRecord->currency] += profit;

      if (positionRecord->direction == common::PositionDirection::LONG) {
        symbolOpenInterestLong[symbolId] += positionRecord->openVolume;
      } else if (positionRecord->direction ==
                 common::PositionDirection::SHORT) {
        symbolOpenInterestShort[symbolId] += positionRecord->openVolume;
      }
    }
  }

  // Create result with all data
  // Match Java: new TotalCurrencyBalanceReportResult(...)
  // Constructor takes raw pointers, not unique_ptr
  auto result = std::make_unique<TotalCurrencyBalanceReportResult>(
      new ankerl::unordered_dense::map<int32_t, int64_t>(currencyBalance),
      new ankerl::unordered_dense::map<int32_t, int64_t>(riskEngine->GetFees()),
      new ankerl::unordered_dense::map<int32_t, int64_t>(
          riskEngine->GetAdjustments()),
      new ankerl::unordered_dense::map<int32_t, int64_t>(
          riskEngine->GetSuspends()),
      nullptr, // ordersBalances - not set in RiskEngine process
      new ankerl::unordered_dense::map<int32_t, int64_t>(
          symbolOpenInterestLong),
      new ankerl::unordered_dense::map<int32_t, int64_t>(
          symbolOpenInterestShort));

  return std::make_optional(std::move(result));
}

} // namespace reports
} // namespace api
} // namespace common
} // namespace core
} // namespace exchange
