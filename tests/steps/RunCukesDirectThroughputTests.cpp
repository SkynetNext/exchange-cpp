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

#include "../util/L2MarketDataHelper.h"
#include "../util/TestConstants.h"
#include "OrderStepdefs.h"
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <gtest/gtest.h>

using namespace exchange::core::tests::steps;
using namespace exchange::core::tests::util;
using namespace exchange::core::common::config;

namespace exchange {
namespace core {
namespace tests {
namespace steps {

// Matches Java: RunCukesDirectThroughputTests
// Runs tests with throughput performance configuration
class RunCukesDirectThroughputTests : public ::testing::Test {
protected:
  void SetUp() override {
    // Set performance configuration (matches Java LifeCycleHandler)
    OrderStepdefs::testPerformanceConfiguration =
        PerformanceConfiguration::ThroughputPerformanceBuilder();
    stepdefs_ = std::make_unique<OrderStepdefs>();
    stepdefs_->Before();
  }

  void TearDown() override {
    if (stepdefs_) {
      stepdefs_->After();
      stepdefs_.reset();
    }
    OrderStepdefs::testPerformanceConfiguration =
        PerformanceConfiguration::Default();
  }

  std::unique_ptr<OrderStepdefs> stepdefs_;
};

// @BasicFullCycleTest from basic.feature
// Scenario Outline: basic full cycle test
TEST_F(RunCukesDirectThroughputTests, BasicFullCycleTest_EUR_USD) {
  // Background
  std::vector<std::pair<std::string, int64_t>> aliceBalance;
  aliceBalance.push_back({"USD", 1000000L});
  aliceBalance.push_back({"XBT", 100000000L});
  aliceBalance.push_back({"ETH", 100000000L});
  stepdefs_->NewClientHasBalance(1440001L, aliceBalance);

  std::vector<std::pair<std::string, int64_t>> bobBalance;
  bobBalance.push_back({"USD", 2000000L});
  bobBalance.push_back({"XBT", 100000000L});
  bobBalance.push_back({"ETH", 100000000L});
  stepdefs_->NewClientHasBalance(1440002L, bobBalance);

  // When A client Alice places an ASK order 101 at 1600@7 (type: GTC, symbol:
  // EUR_USD)
  stepdefs_->ClientPlacesOrder(1440001L, "ASK", 101L, 1600L, 7L, "GTC",
                               TestConstants::SYMBOLSPEC_EUR_USD());

  // And A client Alice places an BID order 102 at 1550@4 (type: GTC, symbol:
  // EUR_USD, reservePrice: 1561)
  stepdefs_->ClientPlacesOrderWithReservePrice(
      1440001L, "BID", 102L, 1550L, 4L, "GTC",
      TestConstants::SYMBOLSPEC_EUR_USD(), 1561L);

  // Then An EUR_USD order book is:
  L2MarketDataHelper expectedOrderBook;
  expectedOrderBook.AddAsk(1600L, 7L);
  expectedOrderBook.AddBid(1550L, 4L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_EUR_USD(),
                         expectedOrderBook);

  // And No trade events
  stepdefs_->NoTradeEvents();

  // And A client Alice orders:
  std::vector<std::map<std::string, std::string>> aliceOrders;
  aliceOrders.push_back({{"id", "101"},
                         {"price", "1600"},
                         {"size", "7"},
                         {"filled", "0"},
                         {"reservePrice", "0"},
                         {"side", "ASK"}});
  aliceOrders.push_back({{"id", "102"},
                         {"price", "1550"},
                         {"size", "4"},
                         {"filled", "0"},
                         {"reservePrice", "1561"},
                         {"side", "BID"}});
  stepdefs_->ClientOrders(1440001L, aliceOrders);

  // When A client Bob places an BID order 201 at 1700@2 (type: IOC, symbol:
  // EUR_USD, reservePrice: 1800)
  stepdefs_->ClientPlacesOrderWithReservePrice(
      1440002L, "BID", 201L, 1700L, 2L, "IOC",
      TestConstants::SYMBOLSPEC_EUR_USD(), 1800L);

  // Then The order 101 is partially matched. LastPx: 1600, LastQty: 2
  stepdefs_->OrderIsPartiallyMatched(101L, 1600L, 2L);

  // And An EUR_USD order book is:
  L2MarketDataHelper expectedOrderBook2;
  expectedOrderBook2.AddAsk(1600L, 5L);
  expectedOrderBook2.AddBid(1550L, 4L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_EUR_USD(),
                         expectedOrderBook2);

  // When A client Bob places an BID order 202 at 1583@4 (type: GTC, symbol:
  // EUR_USD, reservePrice: 1583)
  stepdefs_->ClientPlacesOrderWithReservePrice(
      1440002L, "BID", 202L, 1583L, 4L, "GTC",
      TestConstants::SYMBOLSPEC_EUR_USD(), 1583L);

  // Then An EUR_USD order book is:
  L2MarketDataHelper expectedOrderBook3;
  expectedOrderBook3.AddAsk(1600L, 5L);
  expectedOrderBook3.AddBid(1583L, 4L);
  expectedOrderBook3.AddBid(1550L, 4L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_EUR_USD(),
                         expectedOrderBook3);

  // And No trade events
  stepdefs_->NoTradeEvents();

  // When A client Alice moves a price to 1580 of the order 101
  stepdefs_->ClientMovesOrderPrice(1440001L, 1580L, 101L);

  // Then The order 202 is fully matched. LastPx: 1583, LastQty: 4
  stepdefs_->OrderIsFullyMatched(202L, 1583L, 4L);

  // And An EUR_USD order book is:
  L2MarketDataHelper expectedOrderBook4;
  expectedOrderBook4.AddAsk(1580L, 1L);
  expectedOrderBook4.AddBid(1550L, 4L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_EUR_USD(),
                         expectedOrderBook4);
}

TEST_F(RunCukesDirectThroughputTests, BasicFullCycleTest_ETH_XBT) {
  // Same as EUR_USD but with ETH_XBT symbol
  std::vector<std::pair<std::string, int64_t>> aliceBalance;
  aliceBalance.push_back({"USD", 1000000L});
  aliceBalance.push_back({"XBT", 100000000L});
  aliceBalance.push_back({"ETH", 100000000L});
  stepdefs_->NewClientHasBalance(1440001L, aliceBalance);

  std::vector<std::pair<std::string, int64_t>> bobBalance;
  bobBalance.push_back({"USD", 2000000L});
  bobBalance.push_back({"XBT", 100000000L});
  bobBalance.push_back({"ETH", 100000000L});
  stepdefs_->NewClientHasBalance(1440002L, bobBalance);

  stepdefs_->ClientPlacesOrder(1440001L, "ASK", 101L, 1600L, 7L, "GTC",
                               TestConstants::SYMBOLSPEC_ETH_XBT());
  stepdefs_->ClientPlacesOrderWithReservePrice(
      1440001L, "BID", 102L, 1550L, 4L, "GTC",
      TestConstants::SYMBOLSPEC_ETH_XBT(), 1561L);

  L2MarketDataHelper expectedOrderBook;
  expectedOrderBook.AddAsk(1600L, 7L);
  expectedOrderBook.AddBid(1550L, 4L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_ETH_XBT(),
                         expectedOrderBook);
  stepdefs_->NoTradeEvents();

  std::vector<std::map<std::string, std::string>> aliceOrders;
  aliceOrders.push_back({{"id", "101"},
                         {"price", "1600"},
                         {"size", "7"},
                         {"filled", "0"},
                         {"reservePrice", "0"},
                         {"side", "ASK"}});
  aliceOrders.push_back({{"id", "102"},
                         {"price", "1550"},
                         {"size", "4"},
                         {"filled", "0"},
                         {"reservePrice", "1561"},
                         {"side", "BID"}});
  stepdefs_->ClientOrders(1440001L, aliceOrders);

  stepdefs_->ClientPlacesOrderWithReservePrice(
      1440002L, "BID", 201L, 1700L, 2L, "IOC",
      TestConstants::SYMBOLSPEC_ETH_XBT(), 1800L);
  stepdefs_->OrderIsPartiallyMatched(101L, 1600L, 2L);

  L2MarketDataHelper expectedOrderBook2;
  expectedOrderBook2.AddAsk(1600L, 5L);
  expectedOrderBook2.AddBid(1550L, 4L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_ETH_XBT(),
                         expectedOrderBook2);

  stepdefs_->ClientPlacesOrderWithReservePrice(
      1440002L, "BID", 202L, 1583L, 4L, "GTC",
      TestConstants::SYMBOLSPEC_ETH_XBT(), 1583L);

  L2MarketDataHelper expectedOrderBook3;
  expectedOrderBook3.AddAsk(1600L, 5L);
  expectedOrderBook3.AddBid(1583L, 4L);
  expectedOrderBook3.AddBid(1550L, 4L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_ETH_XBT(),
                         expectedOrderBook3);
  stepdefs_->NoTradeEvents();

  stepdefs_->ClientMovesOrderPrice(1440001L, 1580L, 101L);
  stepdefs_->OrderIsFullyMatched(202L, 1583L, 4L);

  L2MarketDataHelper expectedOrderBook4;
  expectedOrderBook4.AddAsk(1580L, 1L);
  expectedOrderBook4.AddBid(1550L, 4L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_ETH_XBT(),
                         expectedOrderBook4);
}

// @CancelBidOrder from basic.feature
TEST_F(RunCukesDirectThroughputTests, CancelBidOrder) {
  // Given New client Charlie has a balance:
  std::vector<std::pair<std::string, int64_t>> charlieBalance;
  charlieBalance.push_back({"XBT", 94000000L});
  stepdefs_->NewClientHasBalance(1440003L, charlieBalance);

  // When A client Charlie places an BID order 203 at 18500@500 (type: GTC,
  // symbol: ETH_XBT, reservePrice: 18500)
  stepdefs_->ClientPlacesOrderWithReservePrice(
      1440003L, "BID", 203L, 18500L, 500L, "GTC",
      TestConstants::SYMBOLSPEC_ETH_XBT(), 18500L);

  // Then A balance of a client Charlie:
  std::vector<std::pair<std::string, int64_t>> expectedBalance;
  expectedBalance.push_back({"ETH", 0L});
  expectedBalance.push_back({"XBT", 1500000L});
  stepdefs_->ClientBalanceIs(1440003L, expectedBalance);

  // And A client Charlie orders:
  std::vector<std::map<std::string, std::string>> charlieOrders;
  charlieOrders.push_back({{"id", "203"},
                           {"price", "18500"},
                           {"size", "500"},
                           {"filled", "0"},
                           {"reservePrice", "18500"},
                           {"side", "BID"}});
  stepdefs_->ClientOrders(1440003L, charlieOrders);

  // And An ETH_XBT order book is:
  L2MarketDataHelper expectedOrderBook;
  expectedOrderBook.AddBid(18500L, 500L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_ETH_XBT(),
                         expectedOrderBook);

  // When A client Charlie cancels the remaining size 500 of the order 203
  stepdefs_->ClientCancelsOrder(1440003L, 500L, 203L);

  // Then A client Charlie does not have active orders
  stepdefs_->ClientHasNoActiveOrders(1440003L);

  // And A balance of a client Charlie:
  std::vector<std::pair<std::string, int64_t>> expectedBalance2;
  expectedBalance2.push_back({"ETH", 0L});
  expectedBalance2.push_back({"XBT", 94000000L});
  stepdefs_->ClientBalanceIs(1440003L, expectedBalance2);
}

} // namespace steps
} // namespace tests
} // namespace core
} // namespace exchange
