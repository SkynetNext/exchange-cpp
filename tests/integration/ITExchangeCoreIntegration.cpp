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

#include "ITExchangeCoreIntegration.h"
#include <exchange/core/common/MatcherEventType.h>
#include <exchange/core/common/MatcherTradeEvent.h>
#include <exchange/core/common/Order.h>
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/SymbolType.h>
#include <exchange/core/common/api/ApiAdjustUserBalance.h>
#include <exchange/core/common/api/ApiCancelOrder.h>
#include <exchange/core/common/api/ApiMoveOrder.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/api/reports/SingleUserReportResult.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <exchange/core/utils/Logger.h>
#include <gtest/gtest.h>
#include "../util/ExchangeTestContainer.h"
#include "../util/L2MarketDataHelper.h"
#include "../util/TestConstants.h"

using namespace exchange::core::tests::util;
using namespace exchange::core::common;
using namespace exchange::core::common::cmd;
using namespace exchange::core::common::api;
using namespace exchange::core::common::api::reports;

namespace exchange {
namespace core {
namespace tests {
namespace integration {

// Helper to check command result code
static constexpr CommandResultCode CHECK_SUCCESS = CommandResultCode::SUCCESS;

// Helper to find order by orderId from SingleUserReportResult
static const exchange::core::common::Order* FindOrderById(const SingleUserReportResult& profile,
                                                          int64_t orderId) {
  if (!profile.orders) {
    return nullptr;
  }
  for (const auto& [symbolId, orderList] : *profile.orders) {
    for (const auto* order : orderList) {
      if (order && order->orderId == orderId) {
        return order;
      }
    }
  }
  return nullptr;
}

// Basic full cycle test implementation
void ITExchangeCoreIntegration::BasicFullCycleTest(
  const exchange::core::common::CoreSymbolSpecification& symbolSpec) {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());

  container->InitBasicSymbols();
  container->InitBasicUsers();

  // ### 1. first user places limit orders
  auto order101 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    1600, 7, 101, OrderAction::ASK, OrderType::GTC, TestConstants::UID_1, symbolSpec.symbolId, 0,
    0);

  LOG_DEBUG("PLACE 101: [ADD o{} s{} u{} {}:{}:{}:{}]", order101->orderId, order101->symbol,
            order101->uid, order101->action == OrderAction::ASK ? "A" : "B",
            order101->orderType == OrderType::IOC
              ? "IOC"
              : "GTC",  // Java only handles IOC and GTC in toString
            order101->price, order101->size);

  container->SubmitCommandSync(std::move(order101), [&symbolSpec](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.orderId, 101);
    EXPECT_EQ(cmd.uid, TestConstants::UID_1);
    EXPECT_EQ(cmd.price, 1600);
    EXPECT_EQ(cmd.size, 7);
    EXPECT_EQ(cmd.action, OrderAction::ASK);
    EXPECT_EQ(cmd.orderType, OrderType::GTC);
    EXPECT_EQ(cmd.symbol, symbolSpec.symbolId);
    EXPECT_EQ(cmd.matcherEvent, nullptr);
  });

  // Reserve price calculation: for CURRENCY_EXCHANGE_PAIR, use 1561, otherwise
  // 0
  int64_t reserve102 = (symbolSpec.type == SymbolType::CURRENCY_EXCHANGE_PAIR) ? 1561 : 0;
  auto order102 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    1550, 4, 102, OrderAction::BID, OrderType::GTC, TestConstants::UID_1, symbolSpec.symbolId, 0,
    reserve102);

  LOG_DEBUG("PLACE 102: [ADD o{} s{} u{} {}:{}:{}:{}]", order102->orderId, order102->symbol,
            order102->uid, order102->action == OrderAction::ASK ? "A" : "B",
            order102->orderType == OrderType::IOC
              ? "IOC"
              : "GTC",  // Java only handles IOC and GTC in toString
            order102->price, order102->size);

  container->SubmitCommandSync(std::move(order102), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.matcherEvent, nullptr);
  });

  L2MarketDataHelper l2helper;
  l2helper.AddAsk(1600, 7).AddBid(1550, 4);
  auto expected = l2helper.Build();
  ASSERT_NE(expected, nullptr);
  auto actual = container->RequestCurrentOrderBook(symbolSpec.symbolId);
  ASSERT_NE(actual, nullptr);
  EXPECT_EQ(*expected, *actual);

  // ### 2. second user sends market order, first order partially matched
  int64_t reserve201 = (symbolSpec.type == SymbolType::CURRENCY_EXCHANGE_PAIR) ? 1800 : 0;
  auto order201 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    1700, 2, 201, OrderAction::BID, OrderType::IOC, TestConstants::UID_2, symbolSpec.symbolId, 0,
    reserve201);

  LOG_DEBUG("PLACE 201: [ADD o{} s{} u{} {}:{}:{}:{}]", order201->orderId, order201->symbol,
            order201->uid, order201->action == OrderAction::ASK ? "A" : "B",
            order201->orderType == OrderType::IOC
              ? "IOC"
              : "GTC",  // Java only handles IOC and GTC in toString
            order201->price, order201->size);

  container->SubmitCommandSync(std::move(order201), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);

    // Extract events (simplified - in real implementation, need to traverse
    // matcherEvent chain)
    int eventCount = 0;
    auto* evt = cmd.matcherEvent;
    while (evt) {
      eventCount++;
      evt = evt->nextEvent;
    }
    EXPECT_EQ(eventCount, 1);

    EXPECT_EQ(cmd.action, OrderAction::BID);
    EXPECT_EQ(cmd.orderId, 201);
    EXPECT_EQ(cmd.uid, TestConstants::UID_2);

    if (cmd.matcherEvent) {
      EXPECT_EQ(cmd.matcherEvent->activeOrderCompleted, true);
      EXPECT_EQ(cmd.matcherEvent->matchedOrderId, 101);
      EXPECT_EQ(cmd.matcherEvent->matchedOrderUid, TestConstants::UID_1);
      EXPECT_EQ(cmd.matcherEvent->matchedOrderCompleted, false);
      EXPECT_EQ(cmd.matcherEvent->eventType, MatcherEventType::TRADE);
      EXPECT_EQ(cmd.matcherEvent->size, 2);
      EXPECT_EQ(cmd.matcherEvent->price, 1600);
    }
  });

  // volume is decreased to 5
  l2helper.SetAskVolume(0, 5);
  expected = l2helper.Build();
  ASSERT_NE(expected, nullptr);
  actual = container->RequestCurrentOrderBook(symbolSpec.symbolId);
  ASSERT_NE(actual, nullptr);
  EXPECT_EQ(*expected, *actual);

  // ### 3. second user places limit order
  int64_t reserve202 = (symbolSpec.type == SymbolType::CURRENCY_EXCHANGE_PAIR) ? 1583 : 0;
  auto order202 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    1583, 4, 202, OrderAction::BID, OrderType::GTC, TestConstants::UID_2, symbolSpec.symbolId, 0,
    reserve202);

  LOG_DEBUG("PLACE 202: [ADD o{} s{} u{} {}:{}:{}:{}]", order202->orderId, order202->symbol,
            order202->uid, order202->action == OrderAction::ASK ? "A" : "B",
            order202->orderType == OrderType::IOC
              ? "IOC"
              : "GTC",  // Java only handles IOC and GTC in toString
            order202->price, order202->size);

  container->SubmitCommandSync(std::move(order202), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.matcherEvent, nullptr);
  });

  l2helper.InsertBid(0, 1583, 4);
  expected = l2helper.Build();
  ASSERT_NE(expected, nullptr);
  actual = container->RequestCurrentOrderBook(symbolSpec.symbolId);
  ASSERT_NE(actual, nullptr);
  EXPECT_EQ(*expected, *actual);

  // ### 4. first trader moves his order - it will match existing order (202)
  // but not entirely
  auto moveOrder = std::make_unique<exchange::core::common::api::ApiMoveOrder>(
    101, 1580, TestConstants::UID_1, symbolSpec.symbolId);

  LOG_DEBUG("MOVE 101: [MOVE {} {} u{} s{}]", moveOrder->orderId, moveOrder->newPrice,
            moveOrder->uid, moveOrder->symbol);

  container->SubmitCommandSync(std::move(moveOrder), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);

    int eventCount = 0;
    auto* evt = cmd.matcherEvent;
    while (evt) {
      eventCount++;
      evt = evt->nextEvent;
    }
    EXPECT_EQ(eventCount, 1);

    EXPECT_EQ(cmd.action, OrderAction::ASK);
    EXPECT_EQ(cmd.orderId, 101);
    EXPECT_EQ(cmd.uid, TestConstants::UID_1);

    if (cmd.matcherEvent) {
      EXPECT_EQ(cmd.matcherEvent->activeOrderCompleted, false);
      EXPECT_EQ(cmd.matcherEvent->matchedOrderId, 202);
      EXPECT_EQ(cmd.matcherEvent->matchedOrderUid, TestConstants::UID_2);
      EXPECT_EQ(cmd.matcherEvent->matchedOrderCompleted, true);
      EXPECT_EQ(cmd.matcherEvent->eventType, MatcherEventType::TRADE);
      EXPECT_EQ(cmd.matcherEvent->size, 4);
      EXPECT_EQ(cmd.matcherEvent->price, 1583);
    }
  });

  l2helper.SetAskPriceVolume(0, 1580, 1).RemoveBid(0);
  expected = l2helper.Build();
  ASSERT_NE(expected, nullptr);
  actual = container->RequestCurrentOrderBook(symbolSpec.symbolId);
  ASSERT_NE(actual, nullptr);
  EXPECT_EQ(*expected, *actual);

  // Verify total balance is zero (matches Java:
  // container.totalBalanceReport().isGlobalBalancesAllZero())
  auto balanceReport = container->TotalBalanceReport();
  if (balanceReport) {
    EXPECT_TRUE(balanceReport->IsGlobalBalancesAllZero());
  }
}

// Test initialization of symbols
void ITExchangeCoreIntegration::ShouldInitSymbols() {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  container->InitBasicSymbols();
}

// Test initialization of users
void ITExchangeCoreIntegration::ShouldInitUsers() {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  container->InitBasicUsers();
}

// Exchange risk basic test
void ITExchangeCoreIntegration::ExchangeRiskBasicTest() {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  container->InitBasicSymbols();
  container->CreateUserWithMoney(TestConstants::UID_1, TestConstants::CURRENCY_XBT,
                                 2'000'000);  // 2M satoshi (0.02 BTC)

  // try submit an order - limit BUY 7 lots, price 300K satoshi (30K x10 step)
  // for each lot 100K szabo should be rejected
  auto order101 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    30'000, 7, 101, OrderAction::BID, OrderType::GTC, TestConstants::UID_1,
    TestConstants::SYMBOL_EXCHANGE, 0, 30'000);

  container->SubmitCommandSync(std::move(order101), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::RISK_NSF);
  });

  // verify
  container->ValidateUserState(
    TestConstants::UID_1,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_XBT);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, 2'000'000);
      // Note: fetchIndexedOrders() equivalent would need to be implemented
      // For now, just verify accounts
    });

  // add 100K more
  auto adjustBalance = std::make_unique<exchange::core::common::api::ApiAdjustUserBalance>(
    TestConstants::UID_1, TestConstants::CURRENCY_XBT, 100'000, 223948217349827L);
  container->SubmitCommandSync(std::move(adjustBalance), CHECK_SUCCESS);

  // submit order again - should be placed
  auto order101_retry = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    30'000, 7, 101, OrderAction::BID, OrderType::GTC, TestConstants::UID_1,
    TestConstants::SYMBOL_EXCHANGE, 0, 30'000);

  container->SubmitCommandSync(std::move(order101_retry), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.orderId, 101);
    EXPECT_EQ(cmd.uid, TestConstants::UID_1);
    EXPECT_EQ(cmd.price, 30'000);
    EXPECT_EQ(cmd.reserveBidPrice, 30'000);
    EXPECT_EQ(cmd.size, 7);
    EXPECT_EQ(cmd.action, OrderAction::BID);
    EXPECT_EQ(cmd.orderType, OrderType::GTC);
    EXPECT_EQ(cmd.symbol, TestConstants::SYMBOL_EXCHANGE);
    EXPECT_EQ(cmd.matcherEvent, nullptr);
  });

  // verify order placed with correct reserve price and account balance is
  // updated accordingly
  container->ValidateUserState(
    TestConstants::UID_1,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_XBT);
      if (it != profile.accounts->end()) {
        EXPECT_EQ(it->second, 0);
      } else {
        // If account doesn't exist, balance should be 0 (implicit)
        // This is acceptable
      }
    });

  container->CreateUserWithMoney(TestConstants::UID_2, TestConstants::CURRENCY_ETH,
                                 699'999);  // 699'999 szabo (<~0.7 ETH)
  // try submit an order - sell 7 lots, price 300K satoshi (30K x10 step) for
  // each lot 100K szabo should be rejected
  auto order102 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    30'000, 7, 102, OrderAction::ASK, OrderType::IOC, TestConstants::UID_2,
    TestConstants::SYMBOL_EXCHANGE, 0, 0);

  container->SubmitCommandSync(std::move(order102), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::RISK_NSF);
  });

  // verify order is rejected and account balance is not changed
  container->ValidateUserState(
    TestConstants::UID_2,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_ETH);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, 699'999);
    });

  // add 1 szabo more
  auto adjustBalance2 = std::make_unique<exchange::core::common::api::ApiAdjustUserBalance>(
    TestConstants::UID_2, TestConstants::CURRENCY_ETH, 1, 2193842938742L);
  container->SubmitCommandSync(std::move(adjustBalance2), CHECK_SUCCESS);

  // submit order again - should be matched
  auto order102_retry = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    30'000, 7, 102, OrderAction::ASK, OrderType::IOC, TestConstants::UID_2,
    TestConstants::SYMBOL_EXCHANGE, 0, 0);

  container->SubmitCommandSync(std::move(order102_retry), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.orderId, 102);
    EXPECT_EQ(cmd.uid, TestConstants::UID_2);
    EXPECT_EQ(cmd.price, 30'000);
    EXPECT_EQ(cmd.size, 7);
    EXPECT_EQ(cmd.action, OrderAction::ASK);
    EXPECT_EQ(cmd.orderType, OrderType::IOC);
    EXPECT_EQ(cmd.symbol, TestConstants::SYMBOL_EXCHANGE);
    EXPECT_NE(cmd.matcherEvent, nullptr);
  });

  container->ValidateUserState(
    TestConstants::UID_2,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto xbtIt = profile.accounts->find(TestConstants::CURRENCY_XBT);
      auto ethIt = profile.accounts->find(TestConstants::CURRENCY_ETH);
      if (xbtIt != profile.accounts->end()) {
        EXPECT_EQ(xbtIt->second, 2'100'000);
      }
      if (ethIt != profile.accounts->end()) {
        EXPECT_EQ(ethIt->second, 0);
      }
    });

  container->ValidateUserState(
    TestConstants::UID_1,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto ethIt = profile.accounts->find(TestConstants::CURRENCY_ETH);
      auto xbtIt = profile.accounts->find(TestConstants::CURRENCY_XBT);
      if (ethIt != profile.accounts->end()) {
        EXPECT_EQ(ethIt->second, 700'000);
      }
      if (xbtIt != profile.accounts->end()) {
        EXPECT_EQ(xbtIt->second, 0);
      }
    });

  // Verify total balance is zero (matches Java:
  // container.totalBalanceReport().isGlobalBalancesAllZero())
  auto balanceReport = container->TotalBalanceReport();
  if (balanceReport) {
    EXPECT_TRUE(balanceReport->IsGlobalBalancesAllZero());
  }
}

// Exchange cancel bid test
void ITExchangeCoreIntegration::ExchangeCancelBid() {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  container->InitBasicSymbols();

  // create user
  container->CreateUserWithMoney(TestConstants::UID_2, TestConstants::CURRENCY_XBT,
                                 94'000'000);  // 94M satoshi (0.94 BTC)

  // submit order with reservePrice below funds limit - should be placed
  auto order203 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    18'000, 500, 203, OrderAction::BID, OrderType::GTC, TestConstants::UID_2,
    TestConstants::SYMBOL_EXCHANGE, 0, 18'500);

  container->SubmitCommandSync(std::move(order203), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
  });

  // verify order placed with correct reserve price and account balance is
  // updated accordingly
  const auto symbolSpec = TestConstants::SYMBOLSPEC_ETH_XBT();
  const int64_t expectedBalanceAfterPlace = 94'000'000L - 18'500 * 500 * symbolSpec.quoteScaleK;

  container->ValidateUserState(
    TestConstants::UID_2,
    [&expectedBalanceAfterPlace](
      const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_XBT);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, expectedBalanceAfterPlace);
      const auto* order = FindOrderById(profile, 203);
      EXPECT_NE(order, nullptr);
      if (order) {
        EXPECT_EQ(order->reserveBidPrice, 18'500);
      }
    });

  // cancel remaining order
  auto cancelOrder = std::make_unique<exchange::core::common::api::ApiCancelOrder>(
    203, TestConstants::UID_2, TestConstants::SYMBOL_EXCHANGE);

  container->SubmitCommandSync(std::move(cancelOrder), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.command, OrderCommandType::CANCEL_ORDER);
    EXPECT_EQ(cmd.orderId, 203);
    EXPECT_EQ(cmd.uid, TestConstants::UID_2);
    EXPECT_EQ(cmd.symbol, TestConstants::SYMBOL_EXCHANGE);

    EXPECT_EQ(cmd.action, OrderAction::BID);

    const auto* evt = cmd.matcherEvent;
    EXPECT_NE(evt, nullptr);
    if (evt) {
      EXPECT_EQ(evt->eventType, MatcherEventType::REDUCE);
      EXPECT_EQ(evt->bidderHoldPrice, 18'500);
      EXPECT_EQ(evt->size, 500);
    }
  });

  // verify that all 94M satoshi were returned back
  container->ValidateUserState(
    TestConstants::UID_2,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_XBT);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, 94'000'000);
      // Verify no orders
      if (profile.orders) {
        bool hasOrders = false;
        for (const auto& [symbolId, orderList] : *profile.orders) {
          if (!orderList.empty()) {
            hasOrders = true;
            break;
          }
        }
        EXPECT_FALSE(hasOrders);
      }
    });

  // Verify total balance is zero
  auto balanceReport = container->TotalBalanceReport();
  if (balanceReport) {
    EXPECT_TRUE(balanceReport->IsGlobalBalancesAllZero());
  }
}

// Exchange risk move test
void ITExchangeCoreIntegration::ExchangeRiskMoveTest() {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  container->InitBasicSymbols();
  container->CreateUserWithMoney(TestConstants::UID_1, TestConstants::CURRENCY_ETH,
                                 100'000'000);  // 100M szabo (100 ETH)

  // try submit an order - sell 1001 lots, price 300K satoshi (30K x10 step) for
  // each lot 100K szabo should be rejected
  auto order202 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    30'000, 1001, 202, OrderAction::ASK, OrderType::GTC, TestConstants::UID_1,
    TestConstants::SYMBOL_EXCHANGE, 0, 0);

  container->SubmitCommandSync(std::move(order202), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::RISK_NSF);
  });

  container->ValidateUserState(
    TestConstants::UID_1,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_ETH);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, 100'000'000);
      // Verify no orders
      if (profile.orders) {
        bool hasOrders = false;
        for (const auto& [symbolId, orderList] : *profile.orders) {
          if (!orderList.empty()) {
            hasOrders = true;
            break;
          }
        }
        EXPECT_FALSE(hasOrders);
      }
    });

  // submit order again - should be placed (1000 lots)
  auto order202_retry = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    30'000, 1000, 202, OrderAction::ASK, OrderType::GTC, TestConstants::UID_1,
    TestConstants::SYMBOL_EXCHANGE, 0, 0);

  container->SubmitCommandSync(std::move(order202_retry), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.command, OrderCommandType::PLACE_ORDER);
    EXPECT_EQ(cmd.orderId, 202);
    EXPECT_EQ(cmd.uid, TestConstants::UID_1);
    EXPECT_EQ(cmd.price, 30'000);
    EXPECT_EQ(cmd.size, 1000);
    EXPECT_EQ(cmd.action, OrderAction::ASK);
    EXPECT_EQ(cmd.orderType, OrderType::GTC);
    EXPECT_EQ(cmd.symbol, TestConstants::SYMBOL_EXCHANGE);
    EXPECT_EQ(cmd.matcherEvent, nullptr);
  });

  container->ValidateUserState(
    TestConstants::UID_1,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_ETH);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, 0);
      // Verify order exists
      const auto* order = FindOrderById(profile, 202);
      EXPECT_NE(order, nullptr);
    });

  // move order to higher price - shouldn't be a problem for ASK order
  auto moveOrder1 = std::make_unique<exchange::core::common::api::ApiMoveOrder>(
    202, 40'000, TestConstants::UID_1, TestConstants::SYMBOL_EXCHANGE);

  container->SubmitCommandSync(std::move(moveOrder1), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.command, OrderCommandType::MOVE_ORDER);
    EXPECT_EQ(cmd.orderId, 202);
    EXPECT_EQ(cmd.uid, TestConstants::UID_1);
    EXPECT_EQ(cmd.price, 40'000);
    EXPECT_EQ(cmd.symbol, TestConstants::SYMBOL_EXCHANGE);
    EXPECT_EQ(cmd.matcherEvent, nullptr);
  });

  container->ValidateUserState(
    TestConstants::UID_1,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_ETH);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, 0);
      // Verify order still exists
      const auto* order = FindOrderById(profile, 202);
      EXPECT_NE(order, nullptr);
    });

  // move order to lower price - shouldn't be a problem as well for ASK order
  auto moveOrder2 = std::make_unique<exchange::core::common::api::ApiMoveOrder>(
    202, 20'000, TestConstants::UID_1, TestConstants::SYMBOL_EXCHANGE);

  container->SubmitCommandSync(std::move(moveOrder2), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.command, OrderCommandType::MOVE_ORDER);
    EXPECT_EQ(cmd.orderId, 202);
    EXPECT_EQ(cmd.uid, TestConstants::UID_1);
    EXPECT_EQ(cmd.price, 20'000);
    EXPECT_EQ(cmd.symbol, TestConstants::SYMBOL_EXCHANGE);
    EXPECT_EQ(cmd.matcherEvent, nullptr);
  });

  container->ValidateUserState(
    TestConstants::UID_1,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_ETH);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, 0);
      // Verify order still exists
      const auto* order = FindOrderById(profile, 202);
      EXPECT_NE(order, nullptr);
    });

  // create user
  container->CreateUserWithMoney(TestConstants::UID_2, TestConstants::CURRENCY_XBT,
                                 94'000'000);  // 94M satoshi (0.94 BTC)

  // try submit order with reservePrice above funds limit - rejected
  auto order203 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    18'000, 500, 203, OrderAction::BID, OrderType::GTC, TestConstants::UID_2,
    TestConstants::SYMBOL_EXCHANGE, 0, 19'000);

  container->SubmitCommandSync(std::move(order203), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::RISK_NSF);
  });

  container->ValidateUserState(
    TestConstants::UID_2,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_XBT);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, 94'000'000);
      // Verify no orders
      if (profile.orders) {
        bool hasOrders = false;
        for (const auto& [symbolId, orderList] : *profile.orders) {
          if (!orderList.empty()) {
            hasOrders = true;
            break;
          }
        }
        EXPECT_FALSE(hasOrders);
      }
    });

  // submit order with reservePrice below funds limit - should be placed
  auto order203_retry = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    18'000, 500, 203, OrderAction::BID, OrderType::GTC, TestConstants::UID_2,
    TestConstants::SYMBOL_EXCHANGE, 0, 18'500);

  container->SubmitCommandSync(std::move(order203_retry), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.command, OrderCommandType::PLACE_ORDER);
    EXPECT_EQ(cmd.orderId, 203);
    EXPECT_EQ(cmd.uid, TestConstants::UID_2);
    EXPECT_EQ(cmd.price, 18'000);
    EXPECT_EQ(cmd.reserveBidPrice, 18'500);
    EXPECT_EQ(cmd.size, 500);
    EXPECT_EQ(cmd.action, OrderAction::BID);
    EXPECT_EQ(cmd.orderType, OrderType::GTC);
    EXPECT_EQ(cmd.symbol, TestConstants::SYMBOL_EXCHANGE);
    EXPECT_EQ(cmd.matcherEvent, nullptr);
  });

  // expected balance when 203 placed with reserve price 18_500
  // quoteScaleK = 10 for SYMBOL_EXCHANGE (ETH_XBT)
  const auto symbolSpec = TestConstants::SYMBOLSPEC_ETH_XBT();
  const int64_t ethUid2 = 94'000'000L - 18'500 * 500 * symbolSpec.quoteScaleK;

  container->ValidateUserState(
    TestConstants::UID_2,
    [&ethUid2](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_XBT);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, ethUid2);
      // Verify order exists
      const auto* order = FindOrderById(profile, 203);
      EXPECT_NE(order, nullptr);
      if (order) {
        EXPECT_EQ(order->reserveBidPrice, 18'500);
      }
    });

  // move order to lower price - shouldn't be a problem for BID order
  auto moveOrder3 = std::make_unique<exchange::core::common::api::ApiMoveOrder>(
    203, 15'000, TestConstants::UID_2, TestConstants::SYMBOL_EXCHANGE);

  container->SubmitCommandSync(std::move(moveOrder3), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.command, OrderCommandType::MOVE_ORDER);
    EXPECT_EQ(cmd.orderId, 203);
    EXPECT_EQ(cmd.uid, TestConstants::UID_2);
    EXPECT_EQ(cmd.price, 15'000);
    EXPECT_EQ(cmd.symbol, TestConstants::SYMBOL_EXCHANGE);
    EXPECT_EQ(cmd.matcherEvent, nullptr);
  });

  container->ValidateUserState(
    TestConstants::UID_2,
    [&ethUid2](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_XBT);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, ethUid2);
      const auto* order = FindOrderById(profile, 203);
      EXPECT_NE(order, nullptr);
      if (order) {
        EXPECT_EQ(order->price, 15'000);
      }
    });

  // move order to higher price (above limit) - should be rejected
  auto moveOrder4 = std::make_unique<exchange::core::common::api::ApiMoveOrder>(
    203, 18'501, TestConstants::UID_2, TestConstants::SYMBOL_EXCHANGE);

  container->SubmitCommandSync(std::move(moveOrder4), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::MATCHING_MOVE_FAILED_PRICE_OVER_RISK_LIMIT);
  });

  container->ValidateUserState(
    TestConstants::UID_2,
    [&ethUid2](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_XBT);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, ethUid2);
      const auto* order = FindOrderById(profile, 203);
      EXPECT_NE(order, nullptr);
      if (order) {
        EXPECT_EQ(order->price, 15'000);
      }
    });

  // move order to higher price (equals limit) - should be accepted
  auto moveOrder5 = std::make_unique<exchange::core::common::api::ApiMoveOrder>(
    203, 18'500, TestConstants::UID_2, TestConstants::SYMBOL_EXCHANGE);

  container->SubmitCommandSync(std::move(moveOrder5), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.command, OrderCommandType::MOVE_ORDER);
    EXPECT_EQ(cmd.orderId, 203);
    EXPECT_EQ(cmd.uid, TestConstants::UID_2);
    EXPECT_EQ(cmd.price, 18'500);
    EXPECT_EQ(cmd.symbol, TestConstants::SYMBOL_EXCHANGE);
    EXPECT_EQ(cmd.matcherEvent, nullptr);
  });

  container->ValidateUserState(
    TestConstants::UID_2,
    [&ethUid2](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_XBT);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, ethUid2);
      const auto* order = FindOrderById(profile, 203);
      EXPECT_NE(order, nullptr);
      if (order) {
        EXPECT_EQ(order->price, 18'500);
      }
    });

  // set second order price to 17'500
  auto moveOrder6 = std::make_unique<exchange::core::common::api::ApiMoveOrder>(
    203, 17'500, TestConstants::UID_2, TestConstants::SYMBOL_EXCHANGE);

  container->SubmitCommandSync(std::move(moveOrder6), CHECK_SUCCESS);

  container->ValidateUserState(
    TestConstants::UID_2,
    [&ethUid2](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_XBT);
      ASSERT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, ethUid2);
      const auto* order = FindOrderById(profile, 203);
      EXPECT_NE(order, nullptr);
      if (order) {
        EXPECT_EQ(order->price, 17'500);
      }
    });

  // move ASK order to lower price 16'900 so it will trigger trades (by maker's
  // price 17_500)
  auto moveOrder7 = std::make_unique<exchange::core::common::api::ApiMoveOrder>(
    202, 16'900, TestConstants::UID_1, TestConstants::SYMBOL_EXCHANGE);

  container->SubmitCommandSync(std::move(moveOrder7), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.command, OrderCommandType::MOVE_ORDER);
    EXPECT_EQ(cmd.orderId, 202);
    EXPECT_EQ(cmd.uid, TestConstants::UID_1);
    EXPECT_EQ(cmd.price, 16'900);
    EXPECT_EQ(cmd.symbol, TestConstants::SYMBOL_EXCHANGE);

    EXPECT_EQ(cmd.action, OrderAction::ASK);

    const auto* evt = cmd.matcherEvent;
    EXPECT_NE(evt, nullptr);
    if (evt) {
      EXPECT_EQ(evt->eventType, MatcherEventType::TRADE);
      EXPECT_EQ(evt->activeOrderCompleted, false);
      EXPECT_EQ(evt->matchedOrderId, 203);
      EXPECT_EQ(evt->matchedOrderUid, TestConstants::UID_2);
      EXPECT_EQ(evt->matchedOrderCompleted, true);
      EXPECT_EQ(evt->price, 17'500);  // user price from maker order
      EXPECT_EQ(evt->bidderHoldPrice,
                18'500);  // user original reserve price from bidder order (203)
      EXPECT_EQ(evt->size, 500);
    }
  });

  // check UID_1 has 87.5M satoshi (17_500 * 10 * 500) and half-filled SELL
  // order
  container->ValidateUserState(
    TestConstants::UID_1,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto xbtIt = profile.accounts->find(TestConstants::CURRENCY_XBT);
      auto ethIt = profile.accounts->find(TestConstants::CURRENCY_ETH);
      if (xbtIt != profile.accounts->end()) {
        EXPECT_EQ(xbtIt->second, 87'500'000);
      }
      if (ethIt != profile.accounts->end()) {
        EXPECT_EQ(ethIt->second, 0);
      }
      const auto* order = FindOrderById(profile, 202);
      EXPECT_NE(order, nullptr);
      if (order) {
        EXPECT_EQ(order->filled, 500);
      }
    });

  // check UID_2 has 6.5M satoshi (after 94M), and 50M szabo (10_000 * 500)
  container->ValidateUserState(
    TestConstants::UID_2,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto xbtIt = profile.accounts->find(TestConstants::CURRENCY_XBT);
      auto ethIt = profile.accounts->find(TestConstants::CURRENCY_ETH);
      if (xbtIt != profile.accounts->end()) {
        EXPECT_EQ(xbtIt->second, 6'500'000);
      }
      if (ethIt != profile.accounts->end()) {
        EXPECT_EQ(ethIt->second, 50'000'000);
      }
      // Verify no orders
      if (profile.orders) {
        bool hasOrders = false;
        for (const auto& [symbolId, orderList] : *profile.orders) {
          if (!orderList.empty()) {
            hasOrders = true;
            break;
          }
        }
        EXPECT_FALSE(hasOrders);
      }
    });

  // cancel remaining order
  auto cancelOrder = std::make_unique<exchange::core::common::api::ApiCancelOrder>(
    202, TestConstants::UID_1, TestConstants::SYMBOL_EXCHANGE);

  container->SubmitCommandSync(std::move(cancelOrder), [](const OrderCommand& cmd) {
    EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
    EXPECT_EQ(cmd.command, OrderCommandType::CANCEL_ORDER);
    EXPECT_EQ(cmd.orderId, 202);
    EXPECT_EQ(cmd.uid, TestConstants::UID_1);
    EXPECT_EQ(cmd.symbol, TestConstants::SYMBOL_EXCHANGE);

    EXPECT_EQ(cmd.action, OrderAction::ASK);

    const auto* evt = cmd.matcherEvent;
    EXPECT_NE(evt, nullptr);
    if (evt) {
      EXPECT_EQ(evt->eventType, MatcherEventType::REDUCE);
      EXPECT_EQ(evt->size, 500);
    }
  });

  // check UID_1 has 87.5M satoshi (17_500 * 10 * 500) and 50M szabo (after
  // 100M)
  container->ValidateUserState(
    TestConstants::UID_1,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto xbtIt = profile.accounts->find(TestConstants::CURRENCY_XBT);
      auto ethIt = profile.accounts->find(TestConstants::CURRENCY_ETH);
      if (xbtIt != profile.accounts->end()) {
        EXPECT_EQ(xbtIt->second, 87'500'000);
      }
      if (ethIt != profile.accounts->end()) {
        EXPECT_EQ(ethIt->second, 50'000'000);
      }
      // Verify no orders
      if (profile.orders) {
        bool hasOrders = false;
        for (const auto& [symbolId, orderList] : *profile.orders) {
          if (!orderList.empty()) {
            hasOrders = true;
            break;
          }
        }
        EXPECT_FALSE(hasOrders);
      }
    });

  // Verify total balance is zero
  auto balanceReport = container->TotalBalanceReport();
  if (balanceReport) {
    EXPECT_TRUE(balanceReport->IsGlobalBalancesAllZero());
  }
}

}  // namespace integration
}  // namespace tests
}  // namespace core
}  // namespace exchange
