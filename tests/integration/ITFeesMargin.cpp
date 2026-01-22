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
#include <exchange/core/common/PositionDirection.h>
#include <exchange/core/common/api/ApiCancelOrder.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/api/reports/TotalCurrencyBalanceReportResult.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <gtest/gtest.h>
#include "../util/ExchangeTestContainer.h"
#include "../util/TestConstants.h"
#include "ITFeesMarginBasic.h"

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

// Helper to get open interest long
static const ankerl::unordered_dense::map<int32_t, int64_t>& GetOpenInterestLong(
  const exchange::core::common::api::reports::TotalCurrencyBalanceReportResult& result) {
  if (!result.openInterestLong) {
    static const ankerl::unordered_dense::map<int32_t, int64_t> empty;
    return empty;
  }
  return *result.openInterestLong;
}

// Test: shouldProcessFees_AskGtcMakerPartial_BidIocTaker
TEST_F(ITFeesMarginBasic, ShouldProcessFees_AskGtcMakerPartial_BidIocTaker) {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  const auto& symbolSpec = TestConstants::SYMBOLSPECFEE_USD_JPY();
  container->AddSymbol(symbolSpec);

  const int64_t makerFee = symbolSpec.makerFee;
  const int64_t takerFee = symbolSpec.takerFee;
  const int32_t symbolId = symbolSpec.symbolId;

  const int64_t jpyAmount1 = 240'000L;
  container->CreateUserWithMoney(TestConstants::UID_1, TestConstants::CURRENCY_JPY, jpyAmount1);

  auto order101 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    10770L, 40L, 101L, OrderAction::ASK, OrderType::GTC, TestConstants::UID_1, symbolId, 0, 0L);
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
      if (it != profile.accounts->end()) {
        EXPECT_EQ(it->second, 0L);
      }
      // Check order exists
      ASSERT_NE(profile.orders, nullptr);
      bool foundOrder = false;
      for (const auto& [symId, orderList] : *profile.orders) {
        for (const auto* order : orderList) {
          if (order && order->orderId == 101L) {
            EXPECT_EQ(order->price, 10770L);
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
  const int64_t jpyAmount2 = 150'000L;
  container->CreateUserWithMoney(TestConstants::UID_2, TestConstants::CURRENCY_JPY, jpyAmount2);

  auto totalBal1 = container->TotalBalanceReport();
  ASSERT_NE(totalBal1, nullptr);
  const auto& clientsBal1 = GetClientsBalancesSum(*totalBal1);
  auto usdIt1 = clientsBal1.find(TestConstants::CURRENCY_USD);
  if (usdIt1 != clientsBal1.end()) {
    EXPECT_EQ(usdIt1->second, 0L);
  }
  auto jpyIt1 = clientsBal1.find(TestConstants::CURRENCY_JPY);
  EXPECT_NE(jpyIt1, clientsBal1.end());
  EXPECT_EQ(jpyIt1->second, jpyAmount1 + jpyAmount2);
  const auto& fees1 = GetFees(*totalBal1);
  auto feeUsdIt1 = fees1.find(TestConstants::CURRENCY_USD);
  if (feeUsdIt1 != fees1.end()) {
    EXPECT_EQ(feeUsdIt1->second, 0);
  }
  auto feeJpyIt1 = fees1.find(TestConstants::CURRENCY_JPY);
  if (feeJpyIt1 != fees1.end()) {
    EXPECT_EQ(feeJpyIt1->second, 0);
  }
  const auto& openInt1 = GetOpenInterestLong(*totalBal1);
  auto oiIt1 = openInt1.find(symbolId);
  if (oiIt1 != openInt1.end()) {
    EXPECT_EQ(oiIt1->second, 0);
  }

  auto order102 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    10770L, 30L, 102, OrderAction::BID, OrderType::IOC, TestConstants::UID_2, symbolId, 0, 10770L);
  container->SubmitCommandSync(std::move(order102),
                               [](const exchange::core::common::cmd::OrderCommand& cmd) {
                                 EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
                               });

  // verify seller maker balance
  container->ValidateUserState(
    TestConstants::UID_1,
    [jpyAmount1, makerFee,
     symbolId](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto jpyIt = profile.accounts->find(TestConstants::CURRENCY_JPY);
      EXPECT_NE(jpyIt, profile.accounts->end());
      EXPECT_EQ(jpyIt->second, jpyAmount1 - makerFee * 30);
      auto usdIt = profile.accounts->find(TestConstants::CURRENCY_USD);
      if (usdIt != profile.accounts->end()) {
        EXPECT_EQ(usdIt->second, 0L);
      }
      ASSERT_NE(profile.positions, nullptr);
      auto posIt = profile.positions->find(symbolId);
      EXPECT_NE(posIt, profile.positions->end());
      EXPECT_EQ(posIt->second.direction, PositionDirection::SHORT);
      EXPECT_EQ(posIt->second.openVolume, 30L);
      EXPECT_EQ(posIt->second.pendingBuySize, 0L);
      EXPECT_EQ(posIt->second.pendingSellSize, 10L);
      // Check orders are not empty
      ASSERT_NE(profile.orders, nullptr);
      bool hasOrders = false;
      for (const auto& [symId, orderList] : *profile.orders) {
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
    [jpyAmount2, takerFee,
     symbolId](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto jpyIt = profile.accounts->find(TestConstants::CURRENCY_JPY);
      EXPECT_NE(jpyIt, profile.accounts->end());
      EXPECT_EQ(jpyIt->second, jpyAmount2 - takerFee * 30);
      auto usdIt = profile.accounts->find(TestConstants::CURRENCY_USD);
      if (usdIt != profile.accounts->end()) {
        EXPECT_EQ(usdIt->second, 0L);
      }
      ASSERT_NE(profile.positions, nullptr);
      auto posIt = profile.positions->find(symbolId);
      EXPECT_NE(posIt, profile.positions->end());
      EXPECT_EQ(posIt->second.direction, PositionDirection::LONG);
      EXPECT_EQ(posIt->second.openVolume, 30L);
      EXPECT_EQ(posIt->second.pendingBuySize, 0L);
      EXPECT_EQ(posIt->second.pendingSellSize, 0L);
      // Check orders are empty
      if (profile.orders) {
        bool hasOrders = false;
        for (const auto& [symId, orderList] : *profile.orders) {
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
  const int64_t jpyFees = (makerFee + takerFee) * 30;
  const auto& fees2 = GetFees(*totalBal2);
  auto feeUsdIt2 = fees2.find(TestConstants::CURRENCY_USD);
  if (feeUsdIt2 != fees2.end()) {
    EXPECT_EQ(feeUsdIt2->second, 0);
  }
  auto feeJpyIt2 = fees2.find(TestConstants::CURRENCY_JPY);
  EXPECT_NE(feeJpyIt2, fees2.end());
  EXPECT_EQ(feeJpyIt2->second, jpyFees);
  const auto& clientsBal2 = GetClientsBalancesSum(*totalBal2);
  auto usdIt2 = clientsBal2.find(TestConstants::CURRENCY_USD);
  if (usdIt2 != clientsBal2.end()) {
    EXPECT_EQ(usdIt2->second, 0);
  }
  auto jpyIt2 = clientsBal2.find(TestConstants::CURRENCY_JPY);
  EXPECT_NE(jpyIt2, clientsBal2.end());
  EXPECT_EQ(jpyIt2->second, jpyAmount1 + jpyAmount2 - jpyFees);
  const auto& openInt2 = GetOpenInterestLong(*totalBal2);
  auto oiIt2 = openInt2.find(symbolId);
  EXPECT_NE(oiIt2, openInt2.end());
  EXPECT_EQ(oiIt2->second, 30L);
}

// Test: shouldProcessFees_BidGtcMakerPartial_AskIocTaker
TEST_F(ITFeesMarginBasic, ShouldProcessFees_BidGtcMakerPartial_AskIocTaker) {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  const auto& symbolSpec = TestConstants::SYMBOLSPECFEE_USD_JPY();
  container->AddSymbol(symbolSpec);

  const int64_t makerFee = symbolSpec.makerFee;
  const int64_t takerFee = symbolSpec.takerFee;
  const int32_t symbolId = symbolSpec.symbolId;

  const int64_t jpyAmount1 = 250'000L;
  container->CreateUserWithMoney(TestConstants::UID_1, TestConstants::CURRENCY_JPY, jpyAmount1);

  auto order101 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    10770L, 50L, 101L, OrderAction::BID, OrderType::GTC, TestConstants::UID_1, symbolId, 0, 0L);
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
      if (it != profile.accounts->end()) {
        EXPECT_EQ(it->second, 0L);
      }
      // Check order exists
      ASSERT_NE(profile.orders, nullptr);
      bool foundOrder = false;
      for (const auto& [symId, orderList] : *profile.orders) {
        for (const auto* order : orderList) {
          if (order && order->orderId == 101L) {
            EXPECT_EQ(order->price, 10770L);
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
  const int64_t jpyAmount2 = 200'000L;
  container->CreateUserWithMoney(TestConstants::UID_2, TestConstants::CURRENCY_JPY, jpyAmount2);

  auto totalBal1 = container->TotalBalanceReport();
  ASSERT_NE(totalBal1, nullptr);
  const auto& clientsBal1 = GetClientsBalancesSum(*totalBal1);
  auto usdIt1 = clientsBal1.find(TestConstants::CURRENCY_USD);
  if (usdIt1 != clientsBal1.end()) {
    EXPECT_EQ(usdIt1->second, 0L);
  }
  auto jpyIt1 = clientsBal1.find(TestConstants::CURRENCY_JPY);
  EXPECT_NE(jpyIt1, clientsBal1.end());
  EXPECT_EQ(jpyIt1->second, jpyAmount1 + jpyAmount2);
  const auto& fees1 = GetFees(*totalBal1);
  auto feeUsdIt1 = fees1.find(TestConstants::CURRENCY_USD);
  if (feeUsdIt1 != fees1.end()) {
    EXPECT_EQ(feeUsdIt1->second, 0);
  }
  auto feeJpyIt1 = fees1.find(TestConstants::CURRENCY_JPY);
  if (feeJpyIt1 != fees1.end()) {
    EXPECT_EQ(feeJpyIt1->second, 0);
  }
  const auto& openInt1 = GetOpenInterestLong(*totalBal1);
  auto oiIt1 = openInt1.find(symbolId);
  if (oiIt1 != openInt1.end()) {
    EXPECT_EQ(oiIt1->second, 0);
  }

  auto order102 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    10770L, 30L, 102, OrderAction::ASK, OrderType::IOC, TestConstants::UID_2, symbolId, 0, 10770L);
  container->SubmitCommandSync(std::move(order102),
                               [](const exchange::core::common::cmd::OrderCommand& cmd) {
                                 EXPECT_EQ(cmd.resultCode, CommandResultCode::SUCCESS);
                               });

  // verify buyer maker balance
  container->ValidateUserState(
    TestConstants::UID_1,
    [jpyAmount1, makerFee,
     symbolId](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto jpyIt = profile.accounts->find(TestConstants::CURRENCY_JPY);
      EXPECT_NE(jpyIt, profile.accounts->end());
      EXPECT_EQ(jpyIt->second, jpyAmount1 - makerFee * 30);
      auto usdIt = profile.accounts->find(TestConstants::CURRENCY_USD);
      if (usdIt != profile.accounts->end()) {
        EXPECT_EQ(usdIt->second, 0L);
      }
      ASSERT_NE(profile.positions, nullptr);
      auto posIt = profile.positions->find(symbolId);
      EXPECT_NE(posIt, profile.positions->end());
      EXPECT_EQ(posIt->second.direction, PositionDirection::LONG);
      EXPECT_EQ(posIt->second.openVolume, 30L);
      EXPECT_EQ(posIt->second.pendingBuySize, 20L);
      EXPECT_EQ(posIt->second.pendingSellSize, 0L);
      // Check orders are not empty
      ASSERT_NE(profile.orders, nullptr);
      bool hasOrders = false;
      for (const auto& [symId, orderList] : *profile.orders) {
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
    [jpyAmount2, takerFee,
     symbolId](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto jpyIt = profile.accounts->find(TestConstants::CURRENCY_JPY);
      EXPECT_NE(jpyIt, profile.accounts->end());
      EXPECT_EQ(jpyIt->second, jpyAmount2 - takerFee * 30);
      auto usdIt = profile.accounts->find(TestConstants::CURRENCY_USD);
      if (usdIt != profile.accounts->end()) {
        EXPECT_EQ(usdIt->second, 0L);
      }
      ASSERT_NE(profile.positions, nullptr);
      auto posIt = profile.positions->find(symbolId);
      EXPECT_NE(posIt, profile.positions->end());
      EXPECT_EQ(posIt->second.direction, PositionDirection::SHORT);
      EXPECT_EQ(posIt->second.openVolume, 30L);
      EXPECT_EQ(posIt->second.pendingBuySize, 0L);
      EXPECT_EQ(posIt->second.pendingSellSize, 0L);
      // Check orders are empty
      if (profile.orders) {
        bool hasOrders = false;
        for (const auto& [symId, orderList] : *profile.orders) {
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
  const int64_t jpyFees = (makerFee + takerFee) * 30;
  const auto& fees2 = GetFees(*totalBal2);
  auto feeUsdIt2 = fees2.find(TestConstants::CURRENCY_USD);
  if (feeUsdIt2 != fees2.end()) {
    EXPECT_EQ(feeUsdIt2->second, 0);
  }
  auto feeJpyIt2 = fees2.find(TestConstants::CURRENCY_JPY);
  EXPECT_NE(feeJpyIt2, fees2.end());
  EXPECT_EQ(feeJpyIt2->second, jpyFees);
  const auto& clientsBal2 = GetClientsBalancesSum(*totalBal2);
  auto usdIt2 = clientsBal2.find(TestConstants::CURRENCY_USD);
  if (usdIt2 != clientsBal2.end()) {
    EXPECT_EQ(usdIt2->second, 0);
  }
  auto jpyIt2 = clientsBal2.find(TestConstants::CURRENCY_JPY);
  EXPECT_NE(jpyIt2, clientsBal2.end());
  EXPECT_EQ(jpyIt2->second, jpyAmount1 + jpyAmount2 - jpyFees);
  const auto& openInt2 = GetOpenInterestLong(*totalBal2);
  auto oiIt2 = openInt2.find(symbolId);
  EXPECT_NE(oiIt2, openInt2.end());
  EXPECT_EQ(oiIt2->second, 30L);
}

// Test: shouldNotTakeFeesForCancelAsk
TEST_F(ITFeesMarginBasic, ShouldNotTakeFeesForCancelAsk) {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  const auto& symbolSpec = TestConstants::SYMBOLSPECFEE_USD_JPY();
  container->AddSymbol(symbolSpec);

  const int32_t symbolId = symbolSpec.symbolId;

  const int64_t jpyAmount1 = 240'000L;
  container->CreateUserWithMoney(TestConstants::UID_1, TestConstants::CURRENCY_JPY, jpyAmount1);

  auto order101 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
    10770L, 40L, 101L, OrderAction::ASK, OrderType::GTC, TestConstants::UID_1, symbolId, 0, 0L);
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
      if (it != profile.accounts->end()) {
        EXPECT_EQ(it->second, 0L);
      }
      // Check order exists
      ASSERT_NE(profile.orders, nullptr);
      bool foundOrder = false;
      for (const auto& [symId, orderList] : *profile.orders) {
        for (const auto* order : orderList) {
          if (order && order->orderId == 101L) {
            EXPECT_EQ(order->price, 10770L);
            foundOrder = true;
            break;
          }
        }
        if (foundOrder)
          break;
      }
      EXPECT_TRUE(foundOrder);
    });

  // verify balance
  container->ValidateUserState(
    TestConstants::UID_1,
    [jpyAmount1,
     symbolId](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto jpyIt = profile.accounts->find(TestConstants::CURRENCY_JPY);
      EXPECT_NE(jpyIt, profile.accounts->end());
      EXPECT_EQ(jpyIt->second, jpyAmount1);
      auto usdIt = profile.accounts->find(TestConstants::CURRENCY_USD);
      if (usdIt != profile.accounts->end()) {
        EXPECT_EQ(usdIt->second, 0L);
      }
      ASSERT_NE(profile.positions, nullptr);
      auto posIt = profile.positions->find(symbolId);
      EXPECT_NE(posIt, profile.positions->end());
      EXPECT_EQ(posIt->second.direction, PositionDirection::EMPTY);
      EXPECT_EQ(posIt->second.openVolume, 0L);
      EXPECT_EQ(posIt->second.pendingBuySize, 0L);
      EXPECT_EQ(posIt->second.pendingSellSize, 40L);
      // Check orders are not empty
      ASSERT_NE(profile.orders, nullptr);
      bool hasOrders = false;
      for (const auto& [symId, orderList] : *profile.orders) {
        if (!orderList.empty()) {
          hasOrders = true;
          break;
        }
      }
      EXPECT_TRUE(hasOrders);
    });

  // cancel
  auto cancelCmd = std::make_unique<exchange::core::common::api::ApiCancelOrder>(
    101L, TestConstants::UID_1, symbolId);
  container->SubmitCommandSync(std::move(cancelCmd), CommandResultCode::SUCCESS);

  // verify balance
  container->ValidateUserState(
    TestConstants::UID_1,
    [jpyAmount1](const exchange::core::common::api::reports::SingleUserReportResult& profile) {
      ASSERT_NE(profile.accounts, nullptr);
      auto jpyIt = profile.accounts->find(TestConstants::CURRENCY_JPY);
      EXPECT_NE(jpyIt, profile.accounts->end());
      EXPECT_EQ(jpyIt->second, jpyAmount1);
      auto usdIt = profile.accounts->find(TestConstants::CURRENCY_USD);
      if (usdIt != profile.accounts->end()) {
        EXPECT_EQ(usdIt->second, 0L);
      }
      // Check positions are empty
      if (profile.positions) {
        EXPECT_TRUE(profile.positions->empty());
      }
      // Check orders are empty
      if (profile.orders) {
        bool hasOrders = false;
        for (const auto& [symId, orderList] : *profile.orders) {
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
  const auto& clientsBal2 = GetClientsBalancesSum(*totalBal2);
  auto usdIt2 = clientsBal2.find(TestConstants::CURRENCY_USD);
  if (usdIt2 != clientsBal2.end()) {
    EXPECT_EQ(usdIt2->second, 0);
  }
  auto jpyIt2 = clientsBal2.find(TestConstants::CURRENCY_JPY);
  EXPECT_NE(jpyIt2, clientsBal2.end());
  EXPECT_EQ(jpyIt2->second, jpyAmount1);
  const auto& fees2 = GetFees(*totalBal2);
  auto feeUsdIt2 = fees2.find(TestConstants::CURRENCY_USD);
  if (feeUsdIt2 != fees2.end()) {
    EXPECT_EQ(feeUsdIt2->second, 0);
  }
  auto feeJpyIt2 = fees2.find(TestConstants::CURRENCY_JPY);
  if (feeJpyIt2 != fees2.end()) {
    EXPECT_EQ(feeJpyIt2->second, 0);
  }
  const auto& openInt2 = GetOpenInterestLong(*totalBal2);
  auto oiIt2 = openInt2.find(symbolId);
  if (oiIt2 != openInt2.end()) {
    EXPECT_EQ(oiIt2->second, 0);
  }
}

}  // namespace exchange::core::tests::integration
