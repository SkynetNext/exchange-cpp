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

#include "ITExchangeCoreIntegrationStress.h"
#include "../util/ExchangeTestContainer.h"
#include "../util/TestConstants.h"
#include "../util/TestOrdersGenerator.h"
#include "ITExchangeCoreIntegrationStressBasic.h"
#include <atomic>
#include <chrono>
#include <exchange/core/common/L2MarketData.h>
#include <exchange/core/common/api/ApiCancelOrder.h>
#include <exchange/core/common/api/ApiMoveOrder.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/api/ApiReduceOrder.h>
#include <gtest/gtest.h>
#include <set>
#include <thread>

using namespace exchange::core::tests::util;

namespace exchange {
namespace core {
namespace tests {
namespace integration {

void ITExchangeCoreIntegrationStress::ManyOperations(
    const exchange::core::common::CoreSymbolSpecification &symbolSpec) {
  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  container->InitBasicSymbols();

  const int numOrders = 1'000'000;
  const int targetOrderBookOrders = 1000;
  const int numUsers = 1000;

  // Generate commands
  auto genResult = TestOrdersGenerator::GenerateCommands(
      numOrders, targetOrderBookOrders, numUsers,
      TestOrdersGenerator::UID_PLAIN_MAPPER, symbolSpec.symbolId, false, false,
      TestOrdersGenerator::CreateAsyncProgressLogger(numOrders), 288379917);

  // Get all commands (fill + benchmark)
  auto &allCommands = genResult.GetCommands();

  // Convert to ApiCommand*
  std::vector<exchange::core::common::api::ApiCommand *> apiCommands;
  apiCommands.reserve(allCommands.size());

  for (const auto &cmd : allCommands) {
    exchange::core::common::api::ApiCommand *apiCmd = nullptr;
    switch (cmd.command) {
    case exchange::core::common::cmd::OrderCommandType::PLACE_ORDER:
      apiCmd = new exchange::core::common::api::ApiPlaceOrder(
          cmd.price, cmd.size, cmd.orderId, cmd.action, cmd.orderType, cmd.uid,
          cmd.symbol, cmd.userCookie, cmd.reserveBidPrice);
      break;
    case exchange::core::common::cmd::OrderCommandType::MOVE_ORDER:
      apiCmd = new exchange::core::common::api::ApiMoveOrder(
          cmd.orderId, cmd.price, cmd.uid, cmd.symbol);
      break;
    case exchange::core::common::cmd::OrderCommandType::CANCEL_ORDER:
      apiCmd = new exchange::core::common::api::ApiCancelOrder(
          cmd.orderId, cmd.uid, cmd.symbol);
      break;
    case exchange::core::common::cmd::OrderCommandType::REDUCE_ORDER:
      apiCmd = new exchange::core::common::api::ApiReduceOrder(
          cmd.orderId, cmd.uid, cmd.symbol, cmd.size);
      break;
    default:
      throw std::runtime_error("Unsupported command type: " +
                               std::to_string(static_cast<int>(cmd.command)));
    }
    if (apiCmd) {
      apiCommands.push_back(apiCmd);
    }
  }

  // Get allowed currencies
  std::set<int32_t> allowedCurrencies;
  allowedCurrencies.insert(symbolSpec.quoteCurrency);
  allowedCurrencies.insert(symbolSpec.baseCurrency);

  // Initialize users
  container->UsersInit(numUsers, allowedCurrencies);

  // Helper to get clients balances sum (matches Java getClientsBalancesSum)
  auto GetClientsBalancesSum = [](const exchange::core::common::api::reports::
                                      TotalCurrencyBalanceReportResult &result)
      -> ankerl::unordered_dense::map<int32_t, int64_t> {
    ankerl::unordered_dense::map<int32_t, int64_t> sum;

    // Add accountBalances
    if (result.accountBalances) {
      for (const auto &[currency, balance] : *result.accountBalances) {
        sum[currency] += balance;
      }
    }

    // Add ordersBalances (locked balances from orders)
    if (result.ordersBalances) {
      for (const auto &[currency, balance] : *result.ordersBalances) {
        sum[currency] += balance;
      }
    }

    // Add suspends
    if (result.suspends) {
      for (const auto &[currency, balance] : *result.suspends) {
        sum[currency] += balance;
      }
    }

    return sum;
  };

  // Validate total balance as a sum of loaded funds
  auto totalBal1 = container->TotalBalanceReport();
  ASSERT_NE(totalBal1, nullptr);
  const auto &clientsBal1 = GetClientsBalancesSum(*totalBal1);
  for (int32_t currency : allowedCurrencies) {
    auto it = clientsBal1.find(currency);
    if (it != clientsBal1.end()) {
      // 10_0000_0000L = 1,000,000,000 (1 billion)
      const int64_t expectedBalance = 1'000'000'000LL * numUsers;
      EXPECT_EQ(it->second, expectedBalance);
    }
  }

  // Set up consumer to track command completion
  std::atomic<int64_t> commandsCompleted{0};
  container->SetConsumer(
      [&commandsCompleted](exchange::core::common::cmd::OrderCommand *cmd,
                           int64_t seq) {
        (void)cmd; // unused
        (void)seq; // unused
        commandsCompleted.fetch_add(1);
      });

  // Submit all commands
  auto api = container->GetApi();
  auto startTime = std::chrono::system_clock::now();
  for (auto *cmd : apiCommands) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();
    cmd->timestamp = timestamp;
    api->SubmitCommand(cmd);
  }

  // Wait for all commands to complete
  const int64_t expectedCommands = static_cast<int64_t>(apiCommands.size());
  while (commandsCompleted.load() < expectedCommands) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Compare order book final state
  auto l2MarketData = container->RequestCurrentOrderBook(symbolSpec.symbolId);
  ASSERT_NE(l2MarketData, nullptr);
  ASSERT_NE(genResult.finalOrderBookSnapshot, nullptr);

  // Compare order book snapshots (match Java:
  // assertEquals(genResult.getFinalOrderBookSnapshot(), l2MarketData))
  EXPECT_EQ(*l2MarketData, *genResult.finalOrderBookSnapshot);
  EXPECT_GT(l2MarketData->askSize, 10);
  EXPECT_GT(l2MarketData->bidSize, 10);

  // Verify that total balance was not changed (using getClientsBalancesSum)
  auto totalBal2 = container->TotalBalanceReport();
  ASSERT_NE(totalBal2, nullptr);
  const auto &clientsBal2 = GetClientsBalancesSum(*totalBal2);
  for (int32_t currency : allowedCurrencies) {
    auto it1 = clientsBal1.find(currency);
    auto it2 = clientsBal2.find(currency);
    if (it1 != clientsBal1.end() && it2 != clientsBal2.end()) {
      EXPECT_EQ(it2->second, it1->second);
    }
  }
}

// Test: manyOperationsMargin
TEST_F(ITExchangeCoreIntegrationStressBasic, ManyOperationsMargin) {
  ManyOperations(TestConstants::SYMBOLSPEC_EUR_USD());
}

// Test: manyOperationsExchange
TEST_F(ITExchangeCoreIntegrationStressBasic, ManyOperationsExchange) {
  ManyOperations(TestConstants::SYMBOLSPEC_ETH_XBT());
}

} // namespace integration
} // namespace tests
} // namespace core
} // namespace exchange
