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

#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/api/ApiCancelOrder.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/api/reports/TotalCurrencyBalanceReportResult.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <gtest/gtest.h>
#include "../util/ExchangeTestContainer.h"
#include "../util/TestConstants.h"
#include "ITFeesExchangeBasic.h"

using namespace exchange::core::tests::util;
using namespace exchange::core::common;
using namespace exchange::core::common::cmd;

namespace exchange::core::tests::integration {

using namespace exchange::core::common;
using namespace exchange::core::common::cmd;
using namespace exchange::core::tests::util;

// Helper to get clients balances sum (matches Java getClientsBalancesSum)
// Java: return SerializationUtils.mergeSum(accountBalances, ordersBalances,
// suspends);
static ankerl::unordered_dense::map<int32_t, int64_t> GetClientsBalancesSum(
  const exchange::core::common::api::reports::TotalCurrencyBalanceReportResult& result) {
  ankerl::unordered_dense::map<int32_t, int64_t> sum;

  // Add accountBalances
  if (result.accountBalances) {
    for (const auto& [currency, balance] : *result.accountBalances) {
      sum[currency] += balance;
    }
  }

  // Add ordersBalances (locked balances from orders)
  if (result.ordersBalances) {
    for (const auto& [currency, balance] : *result.ordersBalances) {
      sum[currency] += balance;
    }
  }

  // Add suspends
  if (result.suspends) {
    for (const auto& [currency, balance] : *result.suspends) {
      sum[currency] += balance;
    }
  }

  return sum;
}

// Helper to get fees map
static const ankerl::unordered_dense::map<int32_t, int64_t>&
GetFees(const exchange::core::common::api::reports::TotalCurrencyBalanceReportResult& result) {
  if (!result.fees) {
    static const ankerl::unordered_dense::map<int32_t, int64_t> empty;
    return empty;
  }
  return *result.fees;
}

// Test: shouldRequireTakerFees_GtcCancel
TEST_F(ITFeesExchangeBasic, ShouldRequireTakerFees_GtcCancel) {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  container->InitFeeSymbols();

  const auto& symbolSpec = TestConstants::SYMBOLSPECFEE_XBT_LTC();
  const int64_t step = symbolSpec.quoteScaleK;
  const int64_t makerFee = symbolSpec.makerFee;
  const int64_t takerFee = symbolSpec.takerFee;

  // ----------------- 1 test GTC BID cancel ------------------

  // create user - 3.42B litoshi (34.2 LTC)
  const int64_t ltcAmount = 3'420'000'000L;
  container->CreateUserWithMoney(TestConstants::UID_2, TestConstants::CURRENCY_LTC, ltcAmount);

  // submit BID order for 30 lots - should be rejected because of the fee
  auto order203 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    11'400, 30, 203, OrderAction::BID, OrderType::GTC, TestConstants::UID_2,
    TestConstants::SYMBOL_EXCHANGE_FEE, 0, 11'400);
  container->SubmitCommandSync(std::move(order203), CommandResultCode::RISK_NSF);

  // add fee-1 - NSF
  container->AddMoneyToUser(TestConstants::UID_2, TestConstants::CURRENCY_LTC, takerFee * 30 - 1);
  order203 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    11'400, 30, 203, OrderAction::BID, OrderType::GTC, TestConstants::UID_2,
    TestConstants::SYMBOL_EXCHANGE_FEE, 0, 11'400);
  container->SubmitCommandSync(std::move(order203), CommandResultCode::RISK_NSF);

  // add 1 extra - SUCCESS
  container->AddMoneyToUser(TestConstants::UID_2, TestConstants::CURRENCY_LTC, 1);
  order203 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    11'400, 30, 203, OrderAction::BID, OrderType::GTC, TestConstants::UID_2,
    TestConstants::SYMBOL_EXCHANGE_FEE, 0, 11'400);
  container->SubmitCommandSync(std::move(order203), CommandResultCode::SUCCESS);

  // cancel bid
  auto cancelCmd = std::make_unique<exchange::core::common::api::ApiCancelOrder>(
    203, TestConstants::UID_2, TestConstants::SYMBOL_EXCHANGE_FEE);
  container->SubmitCommandSync(std::move(cancelCmd), CommandResultCode::SUCCESS);

  container->ValidateUserState(
    TestConstants::UID_2,
    [ltcAmount,
     takerFee](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_LTC);
      EXPECT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, ltcAmount + takerFee * 30);
      // Check orders are empty
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

  auto totalBal1 = container->TotalBalanceReport();
  ASSERT_NE(totalBal1, nullptr);
  EXPECT_TRUE(totalBal1->IsGlobalBalancesAllZero());
  const auto& clientsBal1 = GetClientsBalancesSum(*totalBal1);
  auto ltcIt1 = clientsBal1.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(ltcIt1, clientsBal1.end());
  EXPECT_EQ(ltcIt1->second, ltcAmount + takerFee * 30);
  const auto& fees1 = GetFees(*totalBal1);
  auto feeIt1 = fees1.find(TestConstants::CURRENCY_LTC);
  if (feeIt1 != fees1.end()) {
    EXPECT_EQ(feeIt1->second, 0);
  }

  // ----------------- 2 test GTC ASK cancel ------------------

  // add 100M satoshi (1 BTC)
  const int64_t btcAmount = 100'000'000L;
  container->AddMoneyToUser(TestConstants::UID_2, TestConstants::CURRENCY_XBT, btcAmount);

  // can place ASK order, no extra is fee required for lock hold
  auto order204 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    11'400, 100, 204, OrderAction::ASK, OrderType::GTC, TestConstants::UID_2,
    TestConstants::SYMBOL_EXCHANGE_FEE, 0, 11'400);
  container->SubmitCommandSync(std::move(order204), CommandResultCode::SUCCESS);

  // cancel ask
  auto cancelCmd2 = std::make_unique<exchange::core::common::api::ApiCancelOrder>(
    204, TestConstants::UID_2, TestConstants::SYMBOL_EXCHANGE_FEE);
  container->SubmitCommandSync(std::move(cancelCmd2), CommandResultCode::SUCCESS);

  container->ValidateUserState(
    TestConstants::UID_2,
    [btcAmount](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_XBT);
      EXPECT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, btcAmount);
      // Check orders are empty
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

  // no fees collected
  auto totalBal2 = container->TotalBalanceReport();
  ASSERT_NE(totalBal2, nullptr);
  EXPECT_TRUE(totalBal2->IsGlobalBalancesAllZero());
  const auto& clientsBal2 = GetClientsBalancesSum(*totalBal2);
  auto ltcIt2 = clientsBal2.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(ltcIt2, clientsBal2.end());
  EXPECT_EQ(ltcIt2->second, ltcAmount + takerFee * 30);
  auto xbtIt2 = clientsBal2.find(TestConstants::CURRENCY_XBT);
  EXPECT_NE(xbtIt2, clientsBal2.end());
  EXPECT_EQ(xbtIt2->second, btcAmount);
  const auto& fees2 = GetFees(*totalBal2);
  auto feeLtcIt2 = fees2.find(TestConstants::CURRENCY_LTC);
  if (feeLtcIt2 != fees2.end()) {
    EXPECT_EQ(feeLtcIt2->second, 0);
  }
  auto feeXbtIt2 = fees2.find(TestConstants::CURRENCY_XBT);
  if (feeXbtIt2 != fees2.end()) {
    EXPECT_EQ(feeXbtIt2->second, 0);
  }
}

// Test: shouldProcessFees_BidGtcMaker_AskIocTakerPartial
TEST_F(ITFeesExchangeBasic, ShouldProcessFees_BidGtcMaker_AskIocTakerPartial) {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  container->InitFeeSymbols();

  const auto& symbolSpec = TestConstants::SYMBOLSPECFEE_XBT_LTC();
  const int64_t step = symbolSpec.quoteScaleK;
  const int64_t makerFee = symbolSpec.makerFee;
  const int64_t takerFee = symbolSpec.takerFee;

  const int64_t ltcAmount = 200'000'000'000L;
  container->CreateUserWithMoney(TestConstants::UID_1, TestConstants::CURRENCY_LTC,
                                 ltcAmount);  // 200B litoshi (2,000 LTC)

  // submit an GtC order - limit BUY 1,731 lots, price 115M (11,500 x10,000
  // step)
  auto order101 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    11'500L, 1731L, 101L, OrderAction::BID, OrderType::GTC, TestConstants::UID_1,
    TestConstants::SYMBOL_EXCHANGE_FEE, 0, 11'553L);
  container->SubmitCommandSync(std::move(order101),
                               [](const exchange::core::common::cmd::OrderCommand& cmd) {
                                 EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
                               });

  const int64_t expectedFundsLtc = ltcAmount - (11'553L * step + takerFee) * 1731L;
  // verify order placed with correct reserve price and account balance is
  // updated accordingly
  container->ValidateUserState(
    TestConstants::UID_1,
    [expectedFundsLtc](
      const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_LTC);
      EXPECT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, expectedFundsLtc);
      // Check order exists
      ASSERT_NE(profile.orders, nullptr);
      bool foundOrder = false;
      for (const auto& [symbolId, orderList] : *profile.orders) {
        for (const auto* order : orderList) {
          if (order && order->orderId == 101L) {
            EXPECT_EQ(order->price, 11'500L);
            foundOrder = true;
            break;
          }
        }
        if (foundOrder)
          break;
      }
      EXPECT_TRUE(foundOrder);
    });

  // create second user
  const int64_t btcAmount = 2'000'000'000L;
  container->CreateUserWithMoney(TestConstants::UID_2, TestConstants::CURRENCY_XBT, btcAmount);

  // no fees collected
  auto totalBal1 = container->TotalBalanceReport();
  ASSERT_NE(totalBal1, nullptr);
  EXPECT_TRUE(totalBal1->IsGlobalBalancesAllZero());
  const auto& clientsBal1 = GetClientsBalancesSum(*totalBal1);
  auto ltcIt1 = clientsBal1.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(ltcIt1, clientsBal1.end());
  EXPECT_EQ(ltcIt1->second, ltcAmount);
  auto xbtIt1 = clientsBal1.find(TestConstants::CURRENCY_XBT);
  EXPECT_NE(xbtIt1, clientsBal1.end());
  EXPECT_EQ(xbtIt1->second, btcAmount);
  const auto& fees1 = GetFees(*totalBal1);
  auto feeIt1 = fees1.find(TestConstants::CURRENCY_LTC);
  if (feeIt1 != fees1.end()) {
    EXPECT_EQ(feeIt1->second, 0);
  }

  // submit an IoC order - sell 2,000 lots, price 114,930K (11,493 x10,000 step)
  auto order102 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    11'493L, 2000L, 102, OrderAction::ASK, OrderType::IOC, TestConstants::UID_2,
    TestConstants::SYMBOL_EXCHANGE_FEE, 0, 0);
  container->SubmitCommandSync(std::move(order102),
                               [](const exchange::core::common::cmd::OrderCommand& cmd) {
                                 EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
                               });

  // verify buyer maker balance
  container->ValidateUserState(
    TestConstants::UID_1,
    [ltcAmount, makerFee, step,
     &symbolSpec](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto ltcIt = profile.accounts->find(TestConstants::CURRENCY_LTC);
      EXPECT_NE(ltcIt, profile.accounts->end());
      EXPECT_EQ(ltcIt->second, ltcAmount - (11'500L * step + makerFee) * 1731L);
      auto xbtIt = profile.accounts->find(TestConstants::CURRENCY_XBT);
      EXPECT_NE(xbtIt, profile.accounts->end());
      EXPECT_EQ(xbtIt->second, 1731L * symbolSpec.baseScaleK);
      // Check orders are empty
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

  // verify seller taker balance
  container->ValidateUserState(
    TestConstants::UID_2,
    [btcAmount, takerFee, step,
     &symbolSpec](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto ltcIt = profile.accounts->find(TestConstants::CURRENCY_LTC);
      EXPECT_NE(ltcIt, profile.accounts->end());
      EXPECT_EQ(ltcIt->second, (11'500L * step - takerFee) * 1731L);
      auto xbtIt = profile.accounts->find(TestConstants::CURRENCY_XBT);
      EXPECT_NE(xbtIt, profile.accounts->end());
      EXPECT_EQ(xbtIt->second, btcAmount - 1731L * symbolSpec.baseScaleK);
      // Check orders are empty
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

  // total balance remains the same
  auto totalBal2 = container->TotalBalanceReport();
  ASSERT_NE(totalBal2, nullptr);
  const int64_t ltcFees = (makerFee + takerFee) * 1731L;
  EXPECT_TRUE(totalBal2->IsGlobalBalancesAllZero());
  const auto& fees2 = GetFees(*totalBal2);
  auto feeIt2 = fees2.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(feeIt2, fees2.end());
  EXPECT_EQ(feeIt2->second, ltcFees);
  const auto& clientsBal2 = GetClientsBalancesSum(*totalBal2);
  auto ltcIt2 = clientsBal2.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(ltcIt2, clientsBal2.end());
  EXPECT_EQ(ltcIt2->second, ltcAmount - ltcFees);
  auto xbtIt2 = clientsBal2.find(TestConstants::CURRENCY_XBT);
  EXPECT_NE(xbtIt2, clientsBal2.end());
  EXPECT_EQ(xbtIt2->second, btcAmount);
}

// Test: shouldProcessFees_BidGtcMakerPartial_AskIocTaker
TEST_F(ITFeesExchangeBasic, ShouldProcessFees_BidGtcMakerPartial_AskIocTaker) {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  container->InitFeeSymbols();

  const auto& symbolSpec = TestConstants::SYMBOLSPECFEE_XBT_LTC();
  const int64_t step = symbolSpec.quoteScaleK;
  const int64_t makerFee = symbolSpec.makerFee;
  const int64_t takerFee = symbolSpec.takerFee;

  const int64_t ltcAmount = 200'000'000'000L;
  container->CreateUserWithMoney(TestConstants::UID_1, TestConstants::CURRENCY_LTC,
                                 ltcAmount);  // 200B litoshi (2,000 LTC)

  // submit an GtC order - limit BUY 1,731 lots, price 115M (11,500 x10,000
  // step)
  auto order101 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    11'500L, 1731L, 101L, OrderAction::BID, OrderType::GTC, TestConstants::UID_1,
    TestConstants::SYMBOL_EXCHANGE_FEE, 0, 11'553L);
  container->SubmitCommandSync(std::move(order101),
                               [](const exchange::core::common::cmd::OrderCommand& cmd) {
                                 EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
                               });

  const int64_t expectedFundsLtc = ltcAmount - (11'553L * step + takerFee) * 1731L;
  // verify order placed with correct reserve price and account balance is
  // updated accordingly
  container->ValidateUserState(
    TestConstants::UID_1,
    [expectedFundsLtc](
      const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_LTC);
      EXPECT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, expectedFundsLtc);
      // Check order exists
      ASSERT_NE(profile.orders, nullptr);
      bool foundOrder = false;
      for (const auto& [symbolId, orderList] : *profile.orders) {
        for (const auto* order : orderList) {
          if (order && order->orderId == 101L) {
            EXPECT_EQ(order->price, 11'500L);
            foundOrder = true;
            break;
          }
        }
        if (foundOrder)
          break;
      }
      EXPECT_TRUE(foundOrder);
    });

  // create second user
  const int64_t btcAmount = 2'000'000'000L;
  container->CreateUserWithMoney(TestConstants::UID_2, TestConstants::CURRENCY_XBT, btcAmount);

  // no fees collected
  auto totalBal1 = container->TotalBalanceReport();
  ASSERT_NE(totalBal1, nullptr);
  EXPECT_TRUE(totalBal1->IsGlobalBalancesAllZero());
  const auto& clientsBal1 = GetClientsBalancesSum(*totalBal1);
  auto ltcIt1 = clientsBal1.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(ltcIt1, clientsBal1.end());
  EXPECT_EQ(ltcIt1->second, ltcAmount);
  auto xbtIt1 = clientsBal1.find(TestConstants::CURRENCY_XBT);
  EXPECT_NE(xbtIt1, clientsBal1.end());
  EXPECT_EQ(xbtIt1->second, btcAmount);
  const auto& fees1 = GetFees(*totalBal1);
  auto feeIt1 = fees1.find(TestConstants::CURRENCY_LTC);
  if (feeIt1 != fees1.end()) {
    EXPECT_EQ(feeIt1->second, 0);
  }

  // submit an IoC order - sell 1,000 lots, price 114,930K (11,493 x10,000 step)
  auto order102 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    11'493L, 1000L, 102, OrderAction::ASK, OrderType::IOC, TestConstants::UID_2,
    TestConstants::SYMBOL_EXCHANGE_FEE, 0, 0);
  container->SubmitCommandSync(std::move(order102),
                               [](const exchange::core::common::cmd::OrderCommand& cmd) {
                                 EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
                               });

  // verify buyer maker balance
  container->ValidateUserState(
    TestConstants::UID_1,
    [ltcAmount, makerFee, takerFee, step,
     &symbolSpec](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto ltcIt = profile.accounts->find(TestConstants::CURRENCY_LTC);
      EXPECT_NE(ltcIt, profile.accounts->end());
      EXPECT_EQ(ltcIt->second, ltcAmount - (11'500L * step + makerFee) * 1000L
                                 - (11'553L * step + takerFee) * 731L);
      auto xbtIt = profile.accounts->find(TestConstants::CURRENCY_XBT);
      EXPECT_NE(xbtIt, profile.accounts->end());
      EXPECT_EQ(xbtIt->second, 1000L * symbolSpec.baseScaleK);
      // Check orders are not empty
      ASSERT_NE(profile.orders, nullptr);
      bool hasOrders = false;
      for (const auto& [symbolId, orderList] : *profile.orders) {
        if (!orderList.empty()) {
          hasOrders = true;
          break;
        }
      }
      EXPECT_TRUE(hasOrders);
    });

  // verify seller taker balance
  container->ValidateUserState(
    TestConstants::UID_2,
    [btcAmount, takerFee, step,
     &symbolSpec](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto ltcIt = profile.accounts->find(TestConstants::CURRENCY_LTC);
      EXPECT_NE(ltcIt, profile.accounts->end());
      EXPECT_EQ(ltcIt->second, (11'500L * step - takerFee) * 1000L);
      auto xbtIt = profile.accounts->find(TestConstants::CURRENCY_XBT);
      EXPECT_NE(xbtIt, profile.accounts->end());
      EXPECT_EQ(xbtIt->second, btcAmount - 1000L * symbolSpec.baseScaleK);
      // Check orders are empty
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

  // total balance remains the same
  auto totalBal2 = container->TotalBalanceReport();
  ASSERT_NE(totalBal2, nullptr);
  EXPECT_TRUE(totalBal2->IsGlobalBalancesAllZero());
  const int64_t ltcFees = (makerFee + takerFee) * 1000L;
  const auto& fees2 = GetFees(*totalBal2);
  auto feeIt2 = fees2.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(feeIt2, fees2.end());
  EXPECT_EQ(feeIt2->second, ltcFees);
  const auto& clientsBal2 = GetClientsBalancesSum(*totalBal2);
  auto ltcIt2 = clientsBal2.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(ltcIt2, clientsBal2.end());
  EXPECT_EQ(ltcIt2->second, ltcAmount - ltcFees);
  auto xbtIt2 = clientsBal2.find(TestConstants::CURRENCY_XBT);
  EXPECT_NE(xbtIt2, clientsBal2.end());
  EXPECT_EQ(xbtIt2->second, btcAmount);
}

// Test: shouldProcessFees_AskGtcMaker_BidIocTakerPartial
TEST_F(ITFeesExchangeBasic, ShouldProcessFees_AskGtcMaker_BidIocTakerPartial) {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  container->InitFeeSymbols();

  const auto& symbolSpec = TestConstants::SYMBOLSPECFEE_XBT_LTC();
  const int64_t step = symbolSpec.quoteScaleK;
  const int64_t makerFee = symbolSpec.makerFee;
  const int64_t takerFee = symbolSpec.takerFee;

  const int64_t btcAmount = 2'000'000'000L;
  container->CreateUserWithMoney(TestConstants::UID_1, TestConstants::CURRENCY_XBT, btcAmount);

  // submit an ASK GtC order, no fees, sell 2,000 lots, price 115,000K (11,500
  // x10,000 step)
  auto order101 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    11'500L, 2000L, 101L, OrderAction::ASK, OrderType::GTC, TestConstants::UID_1,
    TestConstants::SYMBOL_EXCHANGE_FEE, 0, 11'500L);
  container->SubmitCommandSync(std::move(order101),
                               [](const exchange::core::common::cmd::OrderCommand& cmd) {
                                 EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
                               });

  // verify order placed
  container->ValidateUserState(
    TestConstants::UID_1,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_XBT);
      EXPECT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, 0L);
      // Check order exists
      ASSERT_NE(profile.orders, nullptr);
      bool foundOrder = false;
      for (const auto& [symbolId, orderList] : *profile.orders) {
        for (const auto* order : orderList) {
          if (order && order->orderId == 101L) {
            EXPECT_EQ(order->price, 11'500L);
            foundOrder = true;
            break;
          }
        }
        if (foundOrder)
          break;
      }
      EXPECT_TRUE(foundOrder);
    });

  // create second user
  const int64_t ltcAmount = 260'000'000'000L;  // 260B litoshi (2,600 LTC)
  container->CreateUserWithMoney(TestConstants::UID_2, TestConstants::CURRENCY_LTC, ltcAmount);

  auto totalBal1 = container->TotalBalanceReport();
  ASSERT_NE(totalBal1, nullptr);
  EXPECT_TRUE(totalBal1->IsGlobalBalancesAllZero());
  const auto& clientsBal1 = GetClientsBalancesSum(*totalBal1);
  auto ltcIt1 = clientsBal1.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(ltcIt1, clientsBal1.end());
  EXPECT_EQ(ltcIt1->second, ltcAmount);
  auto xbtIt1 = clientsBal1.find(TestConstants::CURRENCY_XBT);
  EXPECT_NE(xbtIt1, clientsBal1.end());
  EXPECT_EQ(xbtIt1->second, btcAmount);
  const auto& fees1 = GetFees(*totalBal1);
  auto feeIt1 = fees1.find(TestConstants::CURRENCY_LTC);
  if (feeIt1 != fees1.end()) {
    EXPECT_EQ(feeIt1->second, 0);
  }

  // submit an IoC order - ASK 2,197 lots, price 115,210K (11,521 x10,000 step)
  auto order102 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    11'521L, 2197L, 102, OrderAction::BID, OrderType::IOC, TestConstants::UID_2,
    TestConstants::SYMBOL_EXCHANGE_FEE, 0, 11'659L);
  container->SubmitCommandSync(std::move(order102),
                               [](const exchange::core::common::cmd::OrderCommand& cmd) {
                                 EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
                               });

  // verify seller maker balance
  container->ValidateUserState(
    TestConstants::UID_1,
    [makerFee, step,
     &symbolSpec](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto xbtIt = profile.accounts->find(TestConstants::CURRENCY_XBT);
      EXPECT_NE(xbtIt, profile.accounts->end());
      EXPECT_EQ(xbtIt->second, 0L);
      auto ltcIt = profile.accounts->find(TestConstants::CURRENCY_LTC);
      EXPECT_NE(ltcIt, profile.accounts->end());
      EXPECT_EQ(ltcIt->second, (11'500L * step - makerFee) * 2000L);
      // Check orders are empty
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

  // verify buyer taker balance
  container->ValidateUserState(
    TestConstants::UID_2,
    [ltcAmount, btcAmount, takerFee, step,
     &symbolSpec](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto xbtIt = profile.accounts->find(TestConstants::CURRENCY_XBT);
      EXPECT_NE(xbtIt, profile.accounts->end());
      EXPECT_EQ(xbtIt->second, symbolSpec.baseScaleK * 2000L);
      auto ltcIt = profile.accounts->find(TestConstants::CURRENCY_LTC);
      EXPECT_NE(ltcIt, profile.accounts->end());
      EXPECT_EQ(ltcIt->second, ltcAmount - (11'500L * step + takerFee) * 2000L);
      // Check orders are empty
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

  // total balance remains the same
  auto totalBal2 = container->TotalBalanceReport();
  ASSERT_NE(totalBal2, nullptr);
  const int64_t ltcFees = (makerFee + takerFee) * 2000L;
  EXPECT_TRUE(totalBal2->IsGlobalBalancesAllZero());
  const auto& fees2 = GetFees(*totalBal2);
  auto feeIt2 = fees2.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(feeIt2, fees2.end());
  EXPECT_EQ(feeIt2->second, ltcFees);
  const auto& clientsBal2 = GetClientsBalancesSum(*totalBal2);
  auto ltcIt2 = clientsBal2.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(ltcIt2, clientsBal2.end());
  EXPECT_EQ(ltcIt2->second, ltcAmount - ltcFees);
  auto xbtIt2 = clientsBal2.find(TestConstants::CURRENCY_XBT);
  EXPECT_NE(xbtIt2, clientsBal2.end());
  EXPECT_EQ(xbtIt2->second, btcAmount);
}

// Test: shouldProcessFees_AskGtcMakerPartial_BidIocTaker
TEST_F(ITFeesExchangeBasic, ShouldProcessFees_AskGtcMakerPartial_BidIocTaker) {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  container->InitFeeSymbols();

  const auto& symbolSpec = TestConstants::SYMBOLSPECFEE_XBT_LTC();
  const int64_t step = symbolSpec.quoteScaleK;
  const int64_t makerFee = symbolSpec.makerFee;
  const int64_t takerFee = symbolSpec.takerFee;

  const int64_t btcAmount = 2'000'000'000L;
  container->CreateUserWithMoney(TestConstants::UID_1, TestConstants::CURRENCY_XBT, btcAmount);

  // submit an ASK GtC order, no fees, sell 2,000 lots, price 115,000K (11,500
  // x10,000 step)
  auto order101 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    11'500L, 2000L, 101L, OrderAction::ASK, OrderType::GTC, TestConstants::UID_1,
    TestConstants::SYMBOL_EXCHANGE_FEE, 0, 11'500L);
  container->SubmitCommandSync(std::move(order101),
                               [](const exchange::core::common::cmd::OrderCommand& cmd) {
                                 EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
                               });

  // verify order placed
  container->ValidateUserState(
    TestConstants::UID_1,
    [](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto it = profile.accounts->find(TestConstants::CURRENCY_XBT);
      EXPECT_NE(it, profile.accounts->end());
      EXPECT_EQ(it->second, 0L);
      // Check order exists
      ASSERT_NE(profile.orders, nullptr);
      bool foundOrder = false;
      for (const auto& [symbolId, orderList] : *profile.orders) {
        for (const auto* order : orderList) {
          if (order && order->orderId == 101L) {
            EXPECT_EQ(order->price, 11'500L);
            foundOrder = true;
            break;
          }
        }
        if (foundOrder)
          break;
      }
      EXPECT_TRUE(foundOrder);
    });

  // create second user
  const int64_t ltcAmount = 260'000'000'000L;  // 260B litoshi (2,600 LTC)
  container->CreateUserWithMoney(TestConstants::UID_2, TestConstants::CURRENCY_LTC, ltcAmount);

  auto totalBal1 = container->TotalBalanceReport();
  ASSERT_NE(totalBal1, nullptr);
  const auto& fees1 = GetFees(*totalBal1);
  auto feeIt1 = fees1.find(TestConstants::CURRENCY_LTC);
  if (feeIt1 != fees1.end()) {
    EXPECT_EQ(feeIt1->second, 0);
  }
  EXPECT_TRUE(totalBal1->IsGlobalBalancesAllZero());
  const auto& clientsBal1 = GetClientsBalancesSum(*totalBal1);
  auto ltcIt1 = clientsBal1.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(ltcIt1, clientsBal1.end());
  EXPECT_EQ(ltcIt1->second, ltcAmount);
  auto xbtIt1 = clientsBal1.find(TestConstants::CURRENCY_XBT);
  EXPECT_NE(xbtIt1, clientsBal1.end());
  EXPECT_EQ(xbtIt1->second, btcAmount);

  // submit an IoC order - ASK 1,997 lots, price 115,210K (11,521 x10,000 step)
  auto order102 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    11'521L, 1997L, 102, OrderAction::BID, OrderType::IOC, TestConstants::UID_2,
    TestConstants::SYMBOL_EXCHANGE_FEE, 0, 11'659L);
  container->SubmitCommandSync(std::move(order102),
                               [](const exchange::core::common::cmd::OrderCommand& cmd) {
                                 EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
                               });

  // verify seller maker balance
  container->ValidateUserState(
    TestConstants::UID_1,
    [makerFee, step](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto xbtIt = profile.accounts->find(TestConstants::CURRENCY_XBT);
      EXPECT_NE(xbtIt, profile.accounts->end());
      EXPECT_EQ(xbtIt->second, 0L);
      auto ltcIt = profile.accounts->find(TestConstants::CURRENCY_LTC);
      EXPECT_NE(ltcIt, profile.accounts->end());
      EXPECT_EQ(ltcIt->second, (11'500L * step - makerFee) * 1997L);
      // Check orders are not empty
      ASSERT_NE(profile.orders, nullptr);
      bool hasOrders = false;
      for (const auto& [symbolId, orderList] : *profile.orders) {
        if (!orderList.empty()) {
          hasOrders = true;
          break;
        }
      }
      EXPECT_TRUE(hasOrders);
    });

  // verify buyer taker balance
  container->ValidateUserState(
    TestConstants::UID_2,
    [ltcAmount, takerFee, step,
     &symbolSpec](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto xbtIt = profile.accounts->find(TestConstants::CURRENCY_XBT);
      EXPECT_NE(xbtIt, profile.accounts->end());
      EXPECT_EQ(xbtIt->second, symbolSpec.baseScaleK * 1997L);
      auto ltcIt = profile.accounts->find(TestConstants::CURRENCY_LTC);
      EXPECT_NE(ltcIt, profile.accounts->end());
      EXPECT_EQ(ltcIt->second, ltcAmount - (11'500L * step + takerFee) * 1997L);
      // Check orders are empty
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

  // total balance remains the same
  const int64_t ltcFees = (makerFee + takerFee) * 1997L;
  auto totalBal2 = container->TotalBalanceReport();
  ASSERT_NE(totalBal2, nullptr);
  EXPECT_TRUE(totalBal2->IsGlobalBalancesAllZero());
  const auto& clientsBal2 = GetClientsBalancesSum(*totalBal2);
  auto ltcIt2 = clientsBal2.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(ltcIt2, clientsBal2.end());
  EXPECT_EQ(ltcIt2->second, ltcAmount - ltcFees);
  auto xbtIt2 = clientsBal2.find(TestConstants::CURRENCY_XBT);
  EXPECT_NE(xbtIt2, clientsBal2.end());
  EXPECT_EQ(xbtIt2->second, btcAmount);
  const auto& fees2 = GetFees(*totalBal2);
  auto feeIt2 = fees2.find(TestConstants::CURRENCY_LTC);
  EXPECT_NE(feeIt2, fees2.end());
  EXPECT_EQ(feeIt2->second, ltcFees);
}

}  // namespace exchange::core::tests::integration
