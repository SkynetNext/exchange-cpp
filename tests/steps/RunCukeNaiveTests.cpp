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

// Matches Java: RunCukeNaiveTests
// Runs tests with base/default performance configuration
class RunCukeNaiveTests : public ::testing::Test {
protected:
  void SetUp() override {
    OrderStepdefs::testPerformanceConfiguration =
        PerformanceConfiguration::Default();
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

// @BasicRiskCheck from risk.feature
TEST_F(RunCukeNaiveTests, BasicRiskCheck) {
  // Given New client Alice has a balance:
  std::vector<std::pair<std::string, int64_t>> aliceBalance;
  aliceBalance.push_back({"XBT", 2000000L});
  stepdefs_->NewClientHasBalance(1440001L, aliceBalance);

  // And New client Bob has a balance:
  std::vector<std::pair<std::string, int64_t>> bobBalance;
  bobBalance.push_back({"ETH", 699999L});
  stepdefs_->NewClientHasBalance(1440002L, bobBalance);

  // When A client Alice could not place an BID order 101 at 30000@7 (type:
  // GTC, symbol: ETH_XBT, reservePrice: 30000) due to RISK_NSF
  stepdefs_->ClientCouldNotPlaceOrder(1440001L, "BID", 101L, 30000L, 7L, "GTC",
                                      TestConstants::SYMBOLSPEC_ETH_XBT(),
                                      30000L, "RISK_NSF");

  // And A balance of a client Alice:
  std::vector<std::pair<std::string, int64_t>> expectedBalance;
  expectedBalance.push_back({"XBT", 2000000L});
  stepdefs_->ClientBalanceIs(1440001L, expectedBalance);

  // And A client Alice orders: (empty)
  std::vector<std::map<std::string, std::string>> emptyOrders;
  stepdefs_->ClientOrders(1440001L, emptyOrders);

  // Given 100000 XBT is added to the balance of a client Alice
  stepdefs_->AddBalanceToClient(100000L, "XBT", 1440001L);

  // When A client Alice places an BID order 101 at 30000@7 (type: GTC,
  // symbol: ETH_XBT, reservePrice: 30000)
  stepdefs_->ClientPlacesOrderWithReservePrice(
      1440001L, "BID", 101L, 30000L, 7L, "GTC",
      TestConstants::SYMBOLSPEC_ETH_XBT(), 30000L);

  // Then An ETH_XBT order book is:
  L2MarketDataHelper expectedOrderBook;
  expectedOrderBook.AddBid(30000L, 7L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_ETH_XBT(),
                         expectedOrderBook);

  // And A balance of a client Alice:
  std::vector<std::pair<std::string, int64_t>> expectedBalance2;
  expectedBalance2.push_back({"XBT", 0L});
  stepdefs_->ClientBalanceIs(1440001L, expectedBalance2);

  // And A client Alice orders:
  std::vector<std::map<std::string, std::string>> aliceOrders;
  aliceOrders.push_back({{"id", "101"},
                         {"price", "30000"},
                         {"size", "7"},
                         {"filled", "0"},
                         {"reservePrice", "30000"},
                         {"side", "BID"}});
  stepdefs_->ClientOrders(1440001L, aliceOrders);

  // When A client Bob could not place an ASK order 102 at 30000@7 (type: IOC,
  // symbol: ETH_XBT, reservePrice: 30000) due to RISK_NSF
  stepdefs_->ClientCouldNotPlaceOrder(1440002L, "ASK", 102L, 30000L, 7L, "IOC",
                                      TestConstants::SYMBOLSPEC_ETH_XBT(),
                                      30000L, "RISK_NSF");

  // Then A balance of a client Bob:
  std::vector<std::pair<std::string, int64_t>> expectedBalance3;
  expectedBalance3.push_back({"ETH", 699999L});
  stepdefs_->ClientBalanceIs(1440002L, expectedBalance3);

  // And A client Bob does not have active orders
  stepdefs_->ClientHasNoActiveOrders(1440002L);

  // Given 1 ETH is added to the balance of a client Bob
  stepdefs_->AddBalanceToClient(1L, "ETH", 1440002L);

  // When A client Bob places an ASK order 102 at 30000@7 (type: IOC, symbol:
  // ETH_XBT, reservePrice: 30000)
  stepdefs_->ClientPlacesOrderWithReservePrice(
      1440002L, "ASK", 102L, 30000L, 7L, "IOC",
      TestConstants::SYMBOLSPEC_ETH_XBT(), 30000L);

  // Then The order 101 is fully matched. LastPx: 30000, LastQty: 7
  stepdefs_->OrderIsFullyMatched(101L, 30000L, 7L);

  // And A balance of a client Alice:
  std::vector<std::pair<std::string, int64_t>> expectedBalance4;
  expectedBalance4.push_back({"ETH", 700000L});
  stepdefs_->ClientBalanceIs(1440001L, expectedBalance4);

  // And A balance of a client Bob:
  std::vector<std::pair<std::string, int64_t>> expectedBalance5;
  expectedBalance5.push_back({"XBT", 2100000L});
  stepdefs_->ClientBalanceIs(1440002L, expectedBalance5);

  // And A client Alice does not have active orders
  stepdefs_->ClientHasNoActiveOrders(1440001L);

  // And A client Bob does not have active orders
  stepdefs_->ClientHasNoActiveOrders(1440002L);
}

// @MoveOrdersUpAndDown from risk.feature
TEST_F(RunCukeNaiveTests, MoveOrdersUpAndDown) {
  // Given New client Alice has a balance:
  std::vector<std::pair<std::string, int64_t>> aliceBalance;
  aliceBalance.push_back({"ETH", 100000000L});
  stepdefs_->NewClientHasBalance(1440001L, aliceBalance);

  // When A client Alice could not place an ASK order 202 at 30000@1001 (type:
  // GTC, symbol: ETH_XBT, reservePrice: 30000) due to RISK_NSF
  stepdefs_->ClientCouldNotPlaceOrder(
      1440001L, "ASK", 202L, 30000L, 1001L, "GTC",
      TestConstants::SYMBOLSPEC_ETH_XBT(), 30000L, "RISK_NSF");

  // Then A balance of a client Alice:
  std::vector<std::pair<std::string, int64_t>> expectedBalance;
  expectedBalance.push_back({"ETH", 100000000L});
  stepdefs_->ClientBalanceIs(1440001L, expectedBalance);

  // And A client Alice does not have active orders
  stepdefs_->ClientHasNoActiveOrders(1440001L);

  // When A client Alice places an ASK order 202 at 30000@1000 (type: GTC,
  // symbol: ETH_XBT, reservePrice: 30000)
  stepdefs_->ClientPlacesOrderWithReservePrice(
      1440001L, "ASK", 202L, 30000L, 1000L, "GTC",
      TestConstants::SYMBOLSPEC_ETH_XBT(), 30000L);

  // Then A balance of a client Alice:
  std::vector<std::pair<std::string, int64_t>> expectedBalance2;
  expectedBalance2.push_back({"ETH", 0L});
  stepdefs_->ClientBalanceIs(1440001L, expectedBalance2);

  // And A client Alice orders:
  std::vector<std::map<std::string, std::string>> aliceOrders;
  aliceOrders.push_back({{"id", "202"},
                         {"price", "30000"},
                         {"size", "1000"},
                         {"filled", "0"},
                         {"reservePrice", "30000"},
                         {"side", "ASK"}});
  stepdefs_->ClientOrders(1440001L, aliceOrders);

  // When A client Alice moves a price to 40000 of the order 202
  stepdefs_->ClientMovesOrderPrice(1440001L, 40000L, 202L);

  // Then A balance of a client Alice:
  stepdefs_->ClientBalanceIs(1440001L, expectedBalance2);

  // And A client Alice orders:
  std::vector<std::map<std::string, std::string>> aliceOrders2;
  aliceOrders2.push_back({{"id", "202"},
                          {"price", "40000"},
                          {"size", "1000"},
                          {"filled", "0"},
                          {"reservePrice", "30000"},
                          {"side", "ASK"}});
  stepdefs_->ClientOrders(1440001L, aliceOrders2);

  // When A client Alice moves a price to 20000 of the order 202
  stepdefs_->ClientMovesOrderPrice(1440001L, 20000L, 202L);

  // Then A balance of a client Alice:
  stepdefs_->ClientBalanceIs(1440001L, expectedBalance2);

  // And A client Alice orders:
  std::vector<std::map<std::string, std::string>> aliceOrders3;
  aliceOrders3.push_back({{"id", "202"},
                          {"price", "20000"},
                          {"size", "1000"},
                          {"filled", "0"},
                          {"reservePrice", "30000"},
                          {"side", "ASK"}});
  stepdefs_->ClientOrders(1440001L, aliceOrders3);

  // Given New client Bob has a balance:
  std::vector<std::pair<std::string, int64_t>> bobBalance;
  bobBalance.push_back({"XBT", 94000000L});
  stepdefs_->NewClientHasBalance(1440002L, bobBalance);

  // When A client Bob could not place an BID order 203 at 18000@500 (type:
  // GTC, symbol: ETH_XBT, reservePrice: 19000) due to RISK_NSF
  stepdefs_->ClientCouldNotPlaceOrder(
      1440002L, "BID", 203L, 18000L, 500L, "GTC",
      TestConstants::SYMBOLSPEC_ETH_XBT(), 19000L, "RISK_NSF");

  // Then A balance of a client Bob:
  std::vector<std::pair<std::string, int64_t>> expectedBalance3;
  expectedBalance3.push_back({"XBT", 94000000L});
  stepdefs_->ClientBalanceIs(1440002L, expectedBalance3);

  // And A client Bob does not have active orders
  stepdefs_->ClientHasNoActiveOrders(1440002L);

  // When A client Bob places an BID order 203 at 18000@500 (type: GTC,
  // symbol: ETH_XBT, reservePrice: 18500)
  stepdefs_->ClientPlacesOrderWithReservePrice(
      1440002L, "BID", 203L, 18000L, 500L, "GTC",
      TestConstants::SYMBOLSPEC_ETH_XBT(), 18500L);

  // Then No trade events
  stepdefs_->NoTradeEvents();

  // And An ETH_XBT order book is:
  L2MarketDataHelper expectedOrderBook;
  expectedOrderBook.AddAsk(20000L, 1000L);
  expectedOrderBook.AddBid(18000L, 500L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_ETH_XBT(),
                         expectedOrderBook);

  // And A balance of a client Bob:
  std::vector<std::pair<std::string, int64_t>> expectedBalance4;
  expectedBalance4.push_back({"XBT", 1500000L});
  stepdefs_->ClientBalanceIs(1440002L, expectedBalance4);

  // And A client Bob orders:
  std::vector<std::map<std::string, std::string>> bobOrders;
  bobOrders.push_back({{"id", "203"},
                       {"price", "18000"},
                       {"size", "500"},
                       {"filled", "0"},
                       {"reservePrice", "18500"},
                       {"side", "BID"}});
  stepdefs_->ClientOrders(1440002L, bobOrders);

  // When A client Bob could not move a price to 18501 of the order 203 due to
  // MATCHING_MOVE_FAILED_PRICE_OVER_RISK_LIMIT
  stepdefs_->ClientCouldNotMoveOrderPrice(
      1440002L, 18501L, 203L, "MATCHING_MOVE_FAILED_PRICE_OVER_RISK_LIMIT");

  // Then A balance of a client Bob:
  stepdefs_->ClientBalanceIs(1440002L, expectedBalance4);

  // And A client Bob orders:
  stepdefs_->ClientOrders(1440002L, bobOrders);

  // And An ETH_XBT order book is:
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_ETH_XBT(),
                         expectedOrderBook);

  // When A client Bob moves a price to 18500 of the order 203
  stepdefs_->ClientMovesOrderPrice(1440002L, 18500L, 203L);

  // Then A balance of a client Bob:
  stepdefs_->ClientBalanceIs(1440002L, expectedBalance4);

  // And An ETH_XBT order book is:
  L2MarketDataHelper expectedOrderBook2;
  expectedOrderBook2.AddAsk(20000L, 1000L);
  expectedOrderBook2.AddBid(18500L, 500L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_ETH_XBT(),
                         expectedOrderBook2);

  // When A client Bob moves a price to 17500 of the order 203
  stepdefs_->ClientMovesOrderPrice(1440002L, 17500L, 203L);

  // Then A balance of a client Bob:
  stepdefs_->ClientBalanceIs(1440002L, expectedBalance4);

  // And An ETH_XBT order book is:
  L2MarketDataHelper expectedOrderBook3;
  expectedOrderBook3.AddAsk(20000L, 1000L);
  expectedOrderBook3.AddBid(17500L, 500L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_ETH_XBT(),
                         expectedOrderBook3);

  // When A client Alice moves a price to 16900 of the order 202
  stepdefs_->ClientMovesOrderPrice(1440001L, 16900L, 202L);

  // Then The order 203 is fully matched. LastPx: 17500, LastQty: 500,
  // bidderHoldPrice: 18500
  stepdefs_->OrderIsFullyMatchedWithBidderHoldPrice(203L, 17500L, 500L, 18500L);

  // And A balance of a client Alice:
  std::vector<std::pair<std::string, int64_t>> expectedBalance5;
  expectedBalance5.push_back({"ETH", 0L});
  expectedBalance5.push_back({"XBT", 87500000L});
  stepdefs_->ClientBalanceIs(1440001L, expectedBalance5);

  // And A client Alice orders:
  std::vector<std::map<std::string, std::string>> aliceOrders4;
  aliceOrders4.push_back({{"id", "202"},
                          {"price", "16900"},
                          {"size", "1000"},
                          {"filled", "500"},
                          {"reservePrice", "30000"},
                          {"side", "ASK"}});
  stepdefs_->ClientOrders(1440001L, aliceOrders4);

  // And An ETH_XBT order book is:
  L2MarketDataHelper expectedOrderBook4;
  expectedOrderBook4.AddAsk(16900L, 500L);
  stepdefs_->OrderBookIs(TestConstants::SYMBOLSPEC_ETH_XBT(),
                         expectedOrderBook4);

  // Then A balance of a client Bob:
  std::vector<std::pair<std::string, int64_t>> expectedBalance6;
  expectedBalance6.push_back({"XBT", 6500000L});
  expectedBalance6.push_back({"ETH", 50000000L});
  stepdefs_->ClientBalanceIs(1440002L, expectedBalance6);

  // And A client Bob does not have active orders
  stepdefs_->ClientHasNoActiveOrders(1440002L);

  // When A client Alice cancels the remaining size 500 of the order 202
  stepdefs_->ClientCancelsOrder(1440001L, 500L, 202L);

  // Then A balance of a client Alice:
  std::vector<std::pair<std::string, int64_t>> expectedBalance7;
  expectedBalance7.push_back({"ETH", 50000000L});
  expectedBalance7.push_back({"XBT", 87500000L});
  stepdefs_->ClientBalanceIs(1440001L, expectedBalance7);

  // And A client Alice does not have active orders
  stepdefs_->ClientHasNoActiveOrders(1440001L);
}

} // namespace steps
} // namespace tests
} // namespace core
} // namespace exchange
