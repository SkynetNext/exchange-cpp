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

#include "OrderBookDirectImplTest.h"
#include <exchange/core/common/L2MarketData.h>
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/config/LoggingConfiguration.h>
#include <exchange/core/orderbook/IOrderBook.h>
#include <exchange/core/orderbook/OrderBookDirectImpl.h>
#include <exchange/core/orderbook/OrderBookNaiveImpl.h>
#include <exchange/core/utils/Logger.h>
#include <unordered_map>
#include "../util/MatcherTradeEventGuard.h"
#include "../util/TestOrdersGenerator.h"

using namespace exchange::core::common;
using namespace exchange::core::common::cmd;
using namespace exchange::core::orderbook;
using namespace exchange::core::tests::util;

namespace exchange::core::tests::orderbook {

void OrderBookDirectImplTest::TestSequentialAsks() {
  // Empty order book
  ClearOrderBook();
  orderBook_->ValidateInternalState();

  // Ask prices start from here, overlap with far ask area
  const int64_t topPrice = INITIAL_PRICE + 1000;
  // Ask prices stop from here, overlap with far bid area
  const int64_t bottomPrice = INITIAL_PRICE - 1000;

  int orderId = 100;

  // Collecting expected limit order volumes for each price
  std::unordered_map<int64_t, int64_t> results;

  // Placing limit bid orders
  for (int64_t price = bottomPrice; price < INITIAL_PRICE; price++) {
    auto cmd = OrderCommand::NewOrder(OrderType::GTC, orderId++, UID_1, price, price * 10, 1,
                                      OrderAction::BID);
    ProcessAndValidate(cmd, CommandResultCode::SUCCESS);
    results[price] = -1L;
  }

  for (int64_t price = topPrice; price >= bottomPrice; price--) {
    int64_t size = price * price;
    auto cmd =
      OrderCommand::NewOrder(OrderType::GTC, orderId++, UID_2, price, 0, size, OrderAction::ASK);
    ProcessAndValidate(cmd, CommandResultCode::SUCCESS);
    if (results.find(price) != results.end()) {
      results[price] += size;
    } else {
      results[price] = size;
    }
  }

  // Collecting full order book
  auto snapshot = orderBook_->GetL2MarketDataSnapshot(INT32_MAX);

  // Check the number of records, should match to expected results
  ASSERT_EQ(snapshot->askSize, static_cast<int32_t>(results.size()));

  // Verify expected size for each price
  for (int i = 0; i < snapshot->askSize; i++) {
    int64_t price = snapshot->askPrices[i];
    auto it = results.find(price);
    ASSERT_NE(it, results.end());
    ASSERT_EQ(snapshot->askVolumes[i], it->second) << "volume mismatch for price " << price;
  }

  // Obviously no bid records expected
  ASSERT_EQ(snapshot->bidSize, 0);
}

void OrderBookDirectImplTest::TestSequentialBids() {
  // Empty order book
  ClearOrderBook();
  orderBook_->ValidateInternalState();

  // Bid prices starts from here, overlap with far bid area
  const int64_t bottomPrice = INITIAL_PRICE - 1000;
  // Bid prices stop here, overlap with far ask area
  const int64_t topPrice = INITIAL_PRICE + 1000;

  int orderId = 100;

  // Collecting expected limit order volumes for each price
  std::unordered_map<int64_t, int64_t> results;

  // Placing limit ask orders
  for (int64_t price = topPrice; price > INITIAL_PRICE; price--) {
    auto cmd =
      OrderCommand::NewOrder(OrderType::GTC, orderId++, UID_1, price, 0, 1, OrderAction::ASK);
    ProcessAndValidate(cmd, CommandResultCode::SUCCESS);
    results[price] = -1L;
  }

  for (int64_t price = bottomPrice; price <= topPrice; price++) {
    int64_t size = price * price;
    auto cmd = OrderCommand::NewOrder(OrderType::GTC, orderId++, UID_2, price, price * 10, size,
                                      OrderAction::BID);
    ProcessAndValidate(cmd, CommandResultCode::SUCCESS);
    if (results.find(price) != results.end()) {
      results[price] += size;
    } else {
      results[price] = size;
    }
  }

  // Collecting full order book
  auto snapshot = orderBook_->GetL2MarketDataSnapshot(INT32_MAX);

  // Check the number of records, should match to expected results
  ASSERT_EQ(snapshot->bidSize, static_cast<int32_t>(results.size()));

  // Verify expected size for each price
  for (int i = 0; i < snapshot->bidSize; i++) {
    int64_t price = snapshot->bidPrices[i];
    auto it = results.find(price);
    ASSERT_NE(it, results.end());
    ASSERT_EQ(snapshot->bidVolumes[i], it->second) << "volume mismatch for price " << price;
  }

  // Obviously no ask records expected (they all should be matched)
  ASSERT_EQ(snapshot->askSize, 0);
}

void OrderBookDirectImplTest::TestMultipleCommandsCompare() {
  // TODO more efficient - multi-threaded executions with different seed and
  // order book type

  const int tranNum = 100000;
  const int targetOrderBookOrders = 500;
  const int numUsers = 100;

  // Create test orderbook and reference orderbook
  ClearOrderBook();
  // Use member variable symbolSpec_ from base class to ensure proper lifetime
  auto orderBookRef = std::make_unique<OrderBookNaiveImpl>(&symbolSpec_, nullptr, nullptr);

  ASSERT_EQ(orderBook_->GetStateHash(), orderBookRef->GetStateHash());

  // Generate test commands
  auto genResult = TestOrdersGenerator::GenerateCommands(
    tranNum, targetOrderBookOrders, numUsers, TestOrdersGenerator::UID_PLAIN_MAPPER, 0, true, false,
    TestOrdersGenerator::CreateAsyncProgressLogger(tranNum), 1825793762);

  auto& allCommands = genResult.GetCommands();
  int64_t i = 0;
  for (size_t idx = 0; idx < allCommands.size(); idx++) {
    i++;
    auto& cmd = allCommands[idx];
    cmd.orderId += 100;

    cmd.resultCode = CommandResultCode::VALID_FOR_MATCHING_ENGINE;
    IOrderBook::ProcessCommand(orderBook_.get(), &cmd);
    MatcherTradeEventGuard guard1(cmd);  // Auto-cleanup

    cmd.resultCode = CommandResultCode::VALID_FOR_MATCHING_ENGINE;
    CommandResultCode commandResultCode = IOrderBook::ProcessCommand(orderBookRef.get(), &cmd);
    MatcherTradeEventGuard guard2(cmd);  // Auto-cleanup

    ASSERT_EQ(commandResultCode, CommandResultCode::SUCCESS);

    // Compare state hash every 100 commands
    if (i % 100 == 0) {
      if (orderBook_->GetStateHash() != orderBookRef->GetStateHash()) {
        LOG_ERROR("\n=== State hash mismatch at command {} ===\n", i);
        // Use base class helper method to print both implementations
        PrintOrderBookComparison(orderBook_.get(), orderBookRef.get(), "OrderBookDirectImpl",
                                 "OrderBookNaiveImpl");
      }
      ASSERT_EQ(orderBook_->GetStateHash(), orderBookRef->GetStateHash());
    }
  }
}

}  // namespace exchange::core::tests::orderbook
