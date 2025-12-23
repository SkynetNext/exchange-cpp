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

#include "TestOrdersGenerator.h"
#include "TestConstants.h"
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/orderbook/IOrderBook.h>
#include <exchange/core/orderbook/OrderBookNaiveImpl.h>
#include <random>

using namespace exchange::core::common;
using namespace exchange::core::common::cmd;
using namespace exchange::core::orderbook;

namespace exchange {
namespace core2 {
namespace tests {
namespace util {

// UID mapper: i -> i + 1
std::function<int32_t(int32_t)> TestOrdersGenerator::UID_PLAIN_MAPPER =
    [](int32_t i) { return i + 1; };

// Store combined commands for GetCommands() to return reference
// Using thread_local to avoid copying OrderCommand (which contains unique_ptr)
static thread_local std::vector<OrderCommand> g_combinedCommands;

std::vector<OrderCommand> &TestOrdersGenerator::GenResult::GetCommands() const {
  g_combinedCommands.clear();
  g_combinedCommands.reserve(commandsFill.size() + commandsBenchmark.size());

  // Use Copy() method to create copies of OrderCommand (which contains
  // unique_ptr)
  for (const auto &cmd : commandsFill) {
    g_combinedCommands.push_back(cmd.Copy());
  }
  for (const auto &cmd : commandsBenchmark) {
    g_combinedCommands.push_back(cmd.Copy());
  }

  return g_combinedCommands;
}

size_t TestOrdersGenerator::GenResult::Size() const {
  return commandsFill.size() + commandsBenchmark.size();
}

std::function<void(int64_t)>
TestOrdersGenerator::CreateAsyncProgressLogger(int64_t total) {
  // Simple no-op progress logger for now
  return [](int64_t) {};
}

// Simplified implementation - generates basic order commands
// Full implementation would require TestOrdersGeneratorSession and more complex
// logic
TestOrdersGenerator::GenResult TestOrdersGenerator::GenerateCommands(
    int benchmarkTransactionsNumber, int targetOrderBookOrders, int numUsers,
    std::function<int32_t(int32_t)> uidMapper, int32_t symbol,
    bool enableSlidingPrice, bool avalancheIOC,
    std::function<void(int64_t)> asyncProgressConsumer, int seed) {

  GenResult result;
  result.commandsFill.reserve(targetOrderBookOrders);
  result.commandsBenchmark.reserve(benchmarkTransactionsNumber);

  // Create a reference orderbook to simulate order generation
  auto symbolSpec = TestConstants::CreateSymbolSpecEurUsd();
  std::unique_ptr<IOrderBook> orderBook =
      std::make_unique<OrderBookNaiveImpl>(&symbolSpec, nullptr, nullptr);

  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> userDist(0, numUsers - 1);
  std::uniform_int_distribution<int> sizeDist(1, 100);
  std::uniform_int_distribution<int> priceOffsetDist(-100, 100);

  const int64_t basePrice = 100000; // Base price for orders
  int64_t currentPrice = basePrice;
  int orderId = 1;

  // Generate fill orders (GTC limit orders)
  for (int i = 0; i < targetOrderBookOrders; i++) {
    int32_t uid = uidMapper(userDist(rng));
    OrderAction action = (rng() % 2 == 0) ? OrderAction::BID : OrderAction::ASK;

    int64_t price = currentPrice + priceOffsetDist(rng);
    int64_t size = sizeDist(rng);

    auto cmd = OrderCommand::NewOrder(
        OrderType::GTC, orderId++, uid, price,
        action == OrderAction::BID ? price * 2 : 0, size, action);
    cmd.symbol = symbol;
    cmd.resultCode = CommandResultCode::VALID_FOR_MATCHING_ENGINE;

    // Process command in reference orderbook first (before pushing to vector)
    IOrderBook::ProcessCommand(orderBook.get(), &cmd);

    // Use Copy() to create a copy for storage (OrderCommand contains
    // unique_ptr)
    result.commandsFill.push_back(cmd.Copy());

    // Update price for sliding
    if (enableSlidingPrice && i % 10 == 0) {
      currentPrice += priceOffsetDist(rng);
    }
  }

  // Generate benchmark orders (mix of GTC, IOC, cancel, move, reduce)
  for (int i = 0; i < benchmarkTransactionsNumber; i++) {
    int32_t uid = uidMapper(userDist(rng));
    int cmdType = rng() % 10;

    if (cmdType < 5) {
      // Place GTC order
      OrderAction action =
          (rng() % 2 == 0) ? OrderAction::BID : OrderAction::ASK;
      int64_t price = currentPrice + priceOffsetDist(rng);
      int64_t size = sizeDist(rng);

      auto cmd = OrderCommand::NewOrder(
          OrderType::GTC, orderId++, uid, price,
          action == OrderAction::BID ? price * 2 : 0, size, action);
      cmd.symbol = symbol;
      cmd.resultCode = CommandResultCode::VALID_FOR_MATCHING_ENGINE;
      IOrderBook::ProcessCommand(orderBook.get(), &cmd);
      // Use Copy() to create a copy for storage (OrderCommand contains
      // unique_ptr)
      result.commandsBenchmark.push_back(cmd.Copy());

    } else if (cmdType < 8) {
      // Place IOC order
      OrderAction action =
          (rng() % 2 == 0) ? OrderAction::BID : OrderAction::ASK;
      int64_t price = action == OrderAction::BID ? basePrice * 2 : basePrice;
      int64_t size = sizeDist(rng);

      auto cmd = OrderCommand::NewOrder(OrderType::IOC, orderId++, uid, price,
                                        action == OrderAction::BID ? price : 0,
                                        size, action);
      cmd.symbol = symbol;
      cmd.resultCode = CommandResultCode::VALID_FOR_MATCHING_ENGINE;
      IOrderBook::ProcessCommand(orderBook.get(), &cmd);
      // Use Copy() to create a copy for storage (OrderCommand contains
      // unique_ptr)
      result.commandsBenchmark.push_back(cmd.Copy());

    } else if (cmdType == 8 && !result.commandsFill.empty()) {
      // Cancel order (simplified - cancel from fill orders)
      int idx = rng() % result.commandsFill.size();
      auto &fillCmd = result.commandsFill[idx];
      if (fillCmd.command == OrderCommandType::PLACE_ORDER) {
        auto cmd = OrderCommand::Cancel(fillCmd.orderId, fillCmd.uid);
        cmd.symbol = symbol;
        cmd.resultCode = CommandResultCode::VALID_FOR_MATCHING_ENGINE;
        IOrderBook::ProcessCommand(orderBook.get(), &cmd);
        // Use Copy() to create a copy for storage (OrderCommand contains
        // unique_ptr)
        result.commandsBenchmark.push_back(cmd.Copy());
      }
    } else {
      // Move order (simplified)
      if (!result.commandsFill.empty()) {
        int idx = rng() % result.commandsFill.size();
        auto &fillCmd = result.commandsFill[idx];
        if (fillCmd.command == OrderCommandType::PLACE_ORDER) {
          int64_t newPrice = currentPrice + priceOffsetDist(rng);
          auto cmd =
              OrderCommand::Update(fillCmd.orderId, fillCmd.uid, newPrice);
          cmd.symbol = symbol;
          cmd.resultCode = CommandResultCode::VALID_FOR_MATCHING_ENGINE;
          IOrderBook::ProcessCommand(orderBook.get(), &cmd);
          // Use Copy() to create a copy for storage (OrderCommand contains
          // unique_ptr)
          result.commandsBenchmark.push_back(cmd.Copy());
        }
      }
    }

    if (enableSlidingPrice && i % 100 == 0) {
      currentPrice += priceOffsetDist(rng);
    }

    if (asyncProgressConsumer && i % 10000 == 9999) {
      asyncProgressConsumer(10000);
    }
  }

  if (asyncProgressConsumer) {
    asyncProgressConsumer(benchmarkTransactionsNumber % 10000);
  }

  // Get final orderbook state
  result.finalOrderBookSnapshot = orderBook->GetL2MarketDataSnapshot(INT32_MAX);
  result.finalOrderbookHash = orderBook->GetStateHash();

  return result;
}

} // namespace util
} // namespace tests
} // namespace core2
} // namespace exchange
