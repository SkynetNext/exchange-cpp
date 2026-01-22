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

#include <exchange/core/common/config/LoggingConfiguration.h>
#include <exchange/core/orderbook/OrderBookNaiveImpl.h>
#include "../util/TestConstants.h"
#include "OrderBookBaseTest.h"

using namespace exchange::core::common;
using namespace exchange::core::orderbook;
using namespace exchange::core::tests::util;

namespace exchange::core::tests::orderbook {

class OrderBookNaiveImplMarginTest : public OrderBookBaseTest {
protected:
  std::unique_ptr<IOrderBook> CreateNewOrderBook() override {
    // Use member variable symbolSpec_ from base class
    return std::make_unique<OrderBookNaiveImpl>(&symbolSpec_, nullptr, nullptr);
  }

  CoreSymbolSpecification GetCoreSymbolSpec() override {
    return TestConstants::CreateSymbolSpecEurUsd();
  }
};

// Register all tests from base class
TEST_F(OrderBookNaiveImplMarginTest, ShouldInitializeWithoutErrors) {
  TestShouldInitializeWithoutErrors();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldAddGtcOrders) {
  TestShouldAddGtcOrders();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldIgnoredDuplicateOrder) {
  TestShouldIgnoredDuplicateOrder();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldRemoveBidOrder) {
  TestShouldRemoveBidOrder();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldRemoveAskOrder) {
  TestShouldRemoveAskOrder();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldReduceBidOrder) {
  TestShouldReduceBidOrder();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldReduceAskOrder) {
  TestShouldReduceAskOrder();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldRemoveOrderAndEmptyBucket) {
  TestShouldRemoveOrderAndEmptyBucket();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldReturnErrorWhenDeletingUnknownOrder) {
  TestShouldReturnErrorWhenDeletingUnknownOrder();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldReturnErrorWhenDeletingOtherUserOrder) {
  TestShouldReturnErrorWhenDeletingOtherUserOrder();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldReturnErrorWhenUpdatingOtherUserOrder) {
  TestShouldReturnErrorWhenUpdatingOtherUserOrder();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldReturnErrorWhenUpdatingUnknownOrder) {
  TestShouldReturnErrorWhenUpdatingUnknownOrder();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldReturnErrorWhenReducingUnknownOrder) {
  TestShouldReturnErrorWhenReducingUnknownOrder();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldReturnErrorWhenReducingByZeroOrNegativeSize) {
  TestShouldReturnErrorWhenReducingByZeroOrNegativeSize();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldReturnErrorWhenReducingOtherUserOrder) {
  TestShouldReturnErrorWhenReducingOtherUserOrder();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMoveOrderExistingBucket) {
  TestShouldMoveOrderExistingBucket();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMoveOrderNewBucket) {
  TestShouldMoveOrderNewBucket();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMatchIocOrderPartialBBO) {
  TestShouldMatchIocOrderPartialBBO();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMatchIocOrderFullBBO) {
  TestShouldMatchIocOrderFullBBO();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMatchIocOrderWithTwoLimitOrdersPartial) {
  TestShouldMatchIocOrderWithTwoLimitOrdersPartial();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMatchIocOrderFullLiquidity) {
  TestShouldMatchIocOrderFullLiquidity();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMatchIocOrderWithRejection) {
  TestShouldMatchIocOrderWithRejection();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldRejectFokBidOrderOutOfBudget) {
  TestShouldRejectFokBidOrderOutOfBudget();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMatchFokBidOrderExactBudget) {
  TestShouldMatchFokBidOrderExactBudget();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMatchFokBidOrderExtraBudget) {
  TestShouldMatchFokBidOrderExtraBudget();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldRejectFokAskOrderBelowExpectation) {
  TestShouldRejectFokAskOrderBelowExpectation();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMatchFokAskOrderExactExpectation) {
  TestShouldMatchFokAskOrderExactExpectation();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMatchFokAskOrderExtraBudget) {
  TestShouldMatchFokAskOrderExtraBudget();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldFullyMatchMarketableGtcOrder) {
  TestShouldFullyMatchMarketableGtcOrder();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldPartiallyMatchMarketableGtcOrderAndPlace) {
  TestShouldPartiallyMatchMarketableGtcOrderAndPlace();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldFullyMatchMarketableGtcOrder2Prices) {
  TestShouldFullyMatchMarketableGtcOrder2Prices();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldFullyMatchMarketableGtcOrderWithAllLiquidity) {
  TestShouldFullyMatchMarketableGtcOrderWithAllLiquidity();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMoveOrderFullyMatchAsMarketable) {
  TestShouldMoveOrderFullyMatchAsMarketable();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMoveOrderFullyMatchAsMarketable2Prices) {
  TestShouldMoveOrderFullyMatchAsMarketable2Prices();
}

TEST_F(OrderBookNaiveImplMarginTest, ShouldMoveOrderMatchesAllLiquidity) {
  TestShouldMoveOrderMatchesAllLiquidity();
}

}  // namespace exchange::core::tests::orderbook
