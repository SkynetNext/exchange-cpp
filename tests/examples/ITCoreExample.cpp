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

#include <exchange/core/ExchangeApi.h>
#include <exchange/core/ExchangeCore.h>
#include <exchange/core/IEventsHandler.h>
#include <exchange/core/SimpleEventsProcessor.h>
#include <exchange/core/common/CoreSymbolSpecification.h>
#include <exchange/core/common/L2MarketData.h>
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/SymbolType.h>
#include <exchange/core/common/api/ApiAddUser.h>
#include <exchange/core/common/api/ApiAdjustUserBalance.h>
#include <exchange/core/common/api/ApiCancelOrder.h>
// ApiCommandResult is defined in IEventsHandler.h
#include <disruptor/BlockingWaitStrategy.h>
#include <disruptor/BusySpinWaitStrategy.h>
#include <disruptor/YieldingWaitStrategy.h>
#include <exchange/core/common/api/ApiBinaryDataCommand.h>
#include <exchange/core/common/api/ApiMoveOrder.h>
#include <exchange/core/common/api/ApiOrderBookRequest.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/api/binary/BatchAddSymbolsCommand.h>

// Report queries - need to implement WriteMarshallable for ReportQuery classes
#include <exchange/core/common/api/reports/SingleUserReportQuery.h>
#include <exchange/core/common/api/reports/SingleUserReportResult.h>
#include <exchange/core/common/api/reports/TotalCurrencyBalanceReportQuery.h>
#include <exchange/core/common/api/reports/TotalCurrencyBalanceReportResult.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/config/ExchangeConfiguration.h>
#include <exchange/core/utils/Logger.h>
#include <gtest/gtest.h>
#include <iostream>

using namespace exchange::core;
using namespace exchange::core::common;
using namespace exchange::core::common::api;
using namespace exchange::core::common::api::binary;
using namespace exchange::core::common::api::reports;
using namespace exchange::core::common::cmd;

// Simple events handler for testing
class TestEventsHandler : public IEventsHandler {
public:
  void TradeEvent(const struct TradeEvent &tradeEvent) override {
    LOG_INFO("Trade event: {}", static_cast<const void*>(&tradeEvent));
  }

  void ReduceEvent(const struct ReduceEvent &reduceEvent) override {
    LOG_INFO("Reduce event: {}", static_cast<const void*>(&reduceEvent));
  }

  void RejectEvent(const struct RejectEvent &rejectEvent) override {
    LOG_INFO("Reject event: {}", static_cast<const void*>(&rejectEvent));
  }

  void CommandResult(const struct ApiCommandResult &commandResult) override {
    LOG_INFO("Command result: {}", static_cast<const void*>(&commandResult));
  }

  void OrderBook(const struct OrderBook &orderBook) override {
    LOG_INFO("OrderBook event: {}", static_cast<const void*>(&orderBook));
  }
};

TEST(ITCoreExample, SampleTest) {
  // Simple async events handler
  auto handler = std::make_unique<TestEventsHandler>();
  auto eventsProcessor = std::make_unique<SimpleEventsProcessor>(handler.get());

  // Default exchange configuration
  auto conf = exchange::core::common::config::ExchangeConfiguration::Default();

  // Build exchange core
  LOG_INFO("[TEST] Building ExchangeCore");
  auto exchangeCore = std::make_unique<ExchangeCore>(
      [&eventsProcessor](exchange::core::common::cmd::OrderCommand *cmd,
                         int64_t seq) { eventsProcessor->Accept(cmd, seq); },
      &conf);
  LOG_INFO("[TEST] ExchangeCore built");

  // Start up disruptor threads
  LOG_INFO("[TEST] Calling Startup()");
  exchangeCore->Startup();
  LOG_INFO("[TEST] Startup() completed");

  // Get exchange API for publishing commands
  auto api = exchangeCore->GetApi();
  if (!api) {
    FAIL() << "GetApi() returned nullptr";
  }

  // Currency code constants
  const int32_t currencyCodeXbt = 11;
  const int32_t currencyCodeLtc = 15;

  // Symbol constants
  const int32_t symbolXbtLtc = 241;

  // Create symbol specification and publish it
  exchange::core::common::CoreSymbolSpecification symbolSpecXbtLtc;
  symbolSpecXbtLtc.symbolId = symbolXbtLtc;
  symbolSpecXbtLtc.type = SymbolType::CURRENCY_EXCHANGE_PAIR;
  symbolSpecXbtLtc.baseCurrency = currencyCodeXbt;  // base = satoshi (1E-8)
  symbolSpecXbtLtc.quoteCurrency = currencyCodeLtc; // quote = litoshi (1E-8)
  symbolSpecXbtLtc.baseScaleK = 1'000'000L; // 1 lot = 1M satoshi (0.01 BTC)
  symbolSpecXbtLtc.quoteScaleK = 10'000L;   // 1 price step = 10K litoshi
  symbolSpecXbtLtc.takerFee = 1900L;        // taker fee 1900 litoshi per 1 lot
  symbolSpecXbtLtc.makerFee = 700L;         // maker fee 700 litoshi per 1 lot

  // Submit binary data command (BatchAddSymbolsCommand)
  auto batchCmd = std::make_unique<BatchAddSymbolsCommand>(&symbolSpecXbtLtc);
  auto binaryCmd =
      std::make_unique<ApiBinaryDataCommand>(1, std::move(batchCmd));
  auto future1 = api->SubmitCommandAsync(binaryCmd.release());
  LOG_INFO("BatchAddSymbolsCommand result: {}", static_cast<int>(future1.get()));

  // Create user uid=301
  auto future2 = api->SubmitCommandAsync(new ApiAddUser(301L));
  LOG_INFO("ApiAddUser 1 result: {}", static_cast<int>(future2.get()));

  // Create user uid=302
  auto future3 = api->SubmitCommandAsync(new ApiAddUser(302L));
  LOG_INFO("ApiAddUser 2 result: {}", static_cast<int>(future3.get()));

  // First user deposits 20 LTC
  auto future4 = api->SubmitCommandAsync(
      new ApiAdjustUserBalance(301L, currencyCodeLtc, 2'000'000'000L, 1L));
  LOG_INFO("ApiAdjustUserBalance 1 result: {}", static_cast<int>(future4.get()));

  // Second user deposits 0.10 BTC
  auto future5 = api->SubmitCommandAsync(
      new ApiAdjustUserBalance(302L, currencyCodeXbt, 10'000'000L, 2L));
  LOG_INFO("ApiAdjustUserBalance 2 result: {}", static_cast<int>(future5.get()));

  // First user places Good-till-Cancel Bid order
  // He assumes BTCLTC exchange rate 154 LTC for 1 BTC
  // Bid price for 1 lot (0.01BTC) is 1.54 LTC => 1_5400_0000 litoshi => 10K *
  // 15_400 (in price steps)
  auto future6 = api->SubmitCommandAsync(
      new ApiPlaceOrder(15'400L, 12L, 5001L, OrderAction::BID, OrderType::GTC,
                        301L, symbolXbtLtc, 0, 15'600L));
  LOG_INFO("ApiPlaceOrder 1 result: {}", static_cast<int>(future6.get()));

  // Second user places Immediate-or-Cancel Ask (Sell) order
  // He assumes worst rate to sell 152.5 LTC for 1 BTC
  auto future7 = api->SubmitCommandAsync(
      new ApiPlaceOrder(15'250L, 10L, 5002L, OrderAction::ASK, OrderType::IOC,
                        302L, symbolXbtLtc, 0, 0));
  LOG_INFO("ApiPlaceOrder 2 result: {}", static_cast<int>(future7.get()));

  // Request order book
  auto futureOrderBook =
      api->SubmitCommandAsync(new ApiOrderBookRequest(symbolXbtLtc, 10));
  LOG_INFO("ApiOrderBookRequest result: {}", static_cast<int>(futureOrderBook.get()));

  // First user moves remaining order to price 1.53 LTC
  auto future8 = api->SubmitCommandAsync(
      new ApiMoveOrder(5001L, 15'300L, 301L, symbolXbtLtc));
  LOG_INFO("ApiMoveOrder 2 result: {}", static_cast<int>(future8.get()));

  // First user cancel remaining order
  auto future9 =
      api->SubmitCommandAsync(new ApiCancelOrder(5001L, 301L, symbolXbtLtc));
  LOG_INFO("ApiCancelOrder 2 result: {}", static_cast<int>(future9.get()));

  // Check balances
  // Check user 301 balances
  auto reportQuery1 = std::make_unique<SingleUserReportQuery>(301L);
  auto report1 =
      ProcessReportHelper<SingleUserReportQuery, SingleUserReportResult>(
          api, std::move(reportQuery1), 0);
  auto result1 = report1.get();
  if (!result1) {
    LOG_INFO("SingleUserReportQuery 1: result is nullptr");
  } else if (!result1->accounts) {
    LOG_INFO("SingleUserReportQuery 1: accounts is nullptr");
  } else {
    std::string accountsStr = "SingleUserReportQuery 1 accounts: ";
    for (const auto &pair : *result1->accounts) {
      accountsStr += "currency=" + std::to_string(pair.first) + ", balance=" + std::to_string(pair.second) + " ";
    }
    LOG_INFO("{}", accountsStr);
  }

  // Check user 302 balances
  auto reportQuery2 = std::make_unique<SingleUserReportQuery>(302L);
  auto report2 =
      ProcessReportHelper<SingleUserReportQuery, SingleUserReportResult>(
          api, std::move(reportQuery2), 0);
  auto result2 = report2.get();
  if (!result2) {
    LOG_INFO("SingleUserReportQuery 2: result is nullptr");
  } else if (!result2->accounts) {
    LOG_INFO("SingleUserReportQuery 2: accounts is nullptr");
  } else {
    std::string accountsStr = "SingleUserReportQuery 2 accounts: ";
    for (const auto &pair : *result2->accounts) {
      accountsStr += "currency=" + std::to_string(pair.first) + ", balance=" + std::to_string(pair.second) + " ";
    }
    LOG_INFO("{}", accountsStr);
  }

  // First user withdraws 0.10 BTC
  auto future10 = api->SubmitCommandAsync(
      new ApiAdjustUserBalance(301L, currencyCodeXbt, -10'000'000L, 3L));
  LOG_INFO("ApiAdjustUserBalance 1 result: {}", static_cast<int>(future10.get()));

  // Check fees collected
  auto totalsReportQuery = std::make_unique<TotalCurrencyBalanceReportQuery>();
  auto totalsReport = ProcessReportHelper<TotalCurrencyBalanceReportQuery,
                                          TotalCurrencyBalanceReportResult>(
      api, std::move(totalsReportQuery), 0);
  auto totalsResult = totalsReport.get();
  if (!totalsResult) {
    LOG_INFO("TotalCurrencyBalanceReportQuery: result is nullptr");
  } else if (!totalsResult->fees) {
    LOG_INFO("TotalCurrencyBalanceReportQuery: fees is nullptr");
  } else {
    auto it = totalsResult->fees->find(currencyCodeLtc);
    if (it != totalsResult->fees->end()) {
      LOG_INFO("LTC fees collected: {}", it->second);
    } else {
      LOG_INFO("TotalCurrencyBalanceReportQuery: LTC fees not found");
    }
  }

  // Shutdown
  exchangeCore->Shutdown();
}
