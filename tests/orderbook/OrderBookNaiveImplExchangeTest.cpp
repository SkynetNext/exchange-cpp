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

#include "../util/TestConstants.h"
#include "OrderBookBaseTest.h"
#include <exchange/core/common/config/LoggingConfiguration.h>
#include <exchange/core/orderbook/OrderBookNaiveImpl.h>

using namespace exchange::core::common;
using namespace exchange::core::orderbook;
using namespace exchange::core::tests::util;

namespace exchange {
namespace core {
namespace tests {
namespace orderbook {

class OrderBookNaiveImplExchangeTest : public OrderBookBaseTest {
protected:
  std::unique_ptr<IOrderBook> CreateNewOrderBook() override {
    // Use member variable symbolSpec_ from base class
    return std::make_unique<OrderBookNaiveImpl>(&symbolSpec_, nullptr, nullptr);
  }

  CoreSymbolSpecification GetCoreSymbolSpec() override {
    return TestConstants::CreateSymbolSpecEthXbt();
  }
};

// Register all tests from base class
TEST_F(OrderBookNaiveImplExchangeTest, ShouldInitializeWithoutErrors) {
  TestShouldInitializeWithoutErrors();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldAddGtcOrders) {
  TestShouldAddGtcOrders();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldIgnoredDuplicateOrder) {
  TestShouldIgnoredDuplicateOrder();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldRemoveBidOrder) {
  TestShouldRemoveBidOrder();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldRemoveAskOrder) {
  TestShouldRemoveAskOrder();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldReduceBidOrder) {
  TestShouldReduceBidOrder();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldReduceAskOrder) {
  TestShouldReduceAskOrder();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldRemoveOrderAndEmptyBucket) {
  TestShouldRemoveOrderAndEmptyBucket();
}

TEST_F(OrderBookNaiveImplExchangeTest,
       ShouldReturnErrorWhenDeletingUnknownOrder) {
  TestShouldReturnErrorWhenDeletingUnknownOrder();
}

TEST_F(OrderBookNaiveImplExchangeTest,
       ShouldReturnErrorWhenDeletingOtherUserOrder) {
  TestShouldReturnErrorWhenDeletingOtherUserOrder();
}

TEST_F(OrderBookNaiveImplExchangeTest,
       ShouldReturnErrorWhenUpdatingOtherUserOrder) {
  TestShouldReturnErrorWhenUpdatingOtherUserOrder();
}

TEST_F(OrderBookNaiveImplExchangeTest,
       ShouldReturnErrorWhenUpdatingUnknownOrder) {
  TestShouldReturnErrorWhenUpdatingUnknownOrder();
}

TEST_F(OrderBookNaiveImplExchangeTest,
       ShouldReturnErrorWhenReducingUnknownOrder) {
  TestShouldReturnErrorWhenReducingUnknownOrder();
}

TEST_F(OrderBookNaiveImplExchangeTest,
       ShouldReturnErrorWhenReducingByZeroOrNegativeSize) {
  TestShouldReturnErrorWhenReducingByZeroOrNegativeSize();
}

TEST_F(OrderBookNaiveImplExchangeTest,
       ShouldReturnErrorWhenReducingOtherUserOrder) {
  TestShouldReturnErrorWhenReducingOtherUserOrder();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldMoveOrderExistingBucket) {
  TestShouldMoveOrderExistingBucket();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldMoveOrderNewBucket) {
  TestShouldMoveOrderNewBucket();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldMatchIocOrderPartialBBO) {
  TestShouldMatchIocOrderPartialBBO();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldMatchIocOrderFullBBO) {
  TestShouldMatchIocOrderFullBBO();
}

TEST_F(OrderBookNaiveImplExchangeTest,
       ShouldMatchIocOrderWithTwoLimitOrdersPartial) {
  TestShouldMatchIocOrderWithTwoLimitOrdersPartial();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldMatchIocOrderFullLiquidity) {
  TestShouldMatchIocOrderFullLiquidity();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldMatchIocOrderWithRejection) {
  TestShouldMatchIocOrderWithRejection();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldRejectFokBidOrderOutOfBudget) {
  TestShouldRejectFokBidOrderOutOfBudget();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldMatchFokBidOrderExactBudget) {
  TestShouldMatchFokBidOrderExactBudget();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldMatchFokBidOrderExtraBudget) {
  TestShouldMatchFokBidOrderExtraBudget();
}

TEST_F(OrderBookNaiveImplExchangeTest,
       ShouldRejectFokAskOrderBelowExpectation) {
  TestShouldRejectFokAskOrderBelowExpectation();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldMatchFokAskOrderExactExpectation) {
  TestShouldMatchFokAskOrderExactExpectation();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldMatchFokAskOrderExtraBudget) {
  TestShouldMatchFokAskOrderExtraBudget();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldFullyMatchMarketableGtcOrder) {
  TestShouldFullyMatchMarketableGtcOrder();
}

TEST_F(OrderBookNaiveImplExchangeTest,
       ShouldPartiallyMatchMarketableGtcOrderAndPlace) {
  TestShouldPartiallyMatchMarketableGtcOrderAndPlace();
}

TEST_F(OrderBookNaiveImplExchangeTest,
       ShouldFullyMatchMarketableGtcOrder2Prices) {
  TestShouldFullyMatchMarketableGtcOrder2Prices();
}

TEST_F(OrderBookNaiveImplExchangeTest,
       ShouldFullyMatchMarketableGtcOrderWithAllLiquidity) {
  TestShouldFullyMatchMarketableGtcOrderWithAllLiquidity();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldMoveOrderFullyMatchAsMarketable) {
  TestShouldMoveOrderFullyMatchAsMarketable();
}

TEST_F(OrderBookNaiveImplExchangeTest,
       ShouldMoveOrderFullyMatchAsMarketable2Prices) {
  TestShouldMoveOrderFullyMatchAsMarketable2Prices();
}

TEST_F(OrderBookNaiveImplExchangeTest, ShouldMoveOrderMatchesAllLiquidity) {
  TestShouldMoveOrderMatchesAllLiquidity();
}

} // namespace orderbook
} // namespace tests
} // namespace core
} // namespace exchange
