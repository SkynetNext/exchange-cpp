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
#include "OrderBookDirectImplTest.h"
#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <exchange/core/common/config/LoggingConfiguration.h>
#include <exchange/core/orderbook/OrderBookDirectImpl.h>
#include <exchange/core/orderbook/OrderBookEventsHelper.h>

using namespace exchange::core::common;
using namespace exchange::core::collections::objpool;
using namespace exchange::core::orderbook;
using namespace exchange::core2::tests::util;

namespace exchange {
namespace core2 {
namespace core {
namespace orderbook {

class OrderBookDirectImplMarginTest : public OrderBookDirectImplTest {
protected:
  std::unique_ptr<IOrderBook> CreateNewOrderBook() override {
    // Use member variable symbolSpec_ from base class
    auto pool = ObjectsPool::CreateDefaultTestPool();
    auto eventsHelper = OrderBookEventsHelper::NonPooledEventsHelper();
    return std::make_unique<OrderBookDirectImpl>(&symbolSpec_, pool, eventsHelper,
                                                 nullptr);
  }

  CoreSymbolSpecification GetCoreSymbolSpec() override {
    return TestConstants::CreateSymbolSpecEurUsd();
  }
};

// Register all tests from base class
TEST_F(OrderBookDirectImplMarginTest, ShouldInitializeWithoutErrors) {
  TestShouldInitializeWithoutErrors();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldAddGtcOrders) {
  TestShouldAddGtcOrders();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldIgnoredDuplicateOrder) {
  TestShouldIgnoredDuplicateOrder();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldRemoveBidOrder) {
  TestShouldRemoveBidOrder();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldRemoveAskOrder) {
  TestShouldRemoveAskOrder();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldReduceBidOrder) {
  TestShouldReduceBidOrder();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldReduceAskOrder) {
  TestShouldReduceAskOrder();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldRemoveOrderAndEmptyBucket) {
  TestShouldRemoveOrderAndEmptyBucket();
}

TEST_F(OrderBookDirectImplMarginTest,
       ShouldReturnErrorWhenDeletingUnknownOrder) {
  TestShouldReturnErrorWhenDeletingUnknownOrder();
}

TEST_F(OrderBookDirectImplMarginTest,
       ShouldReturnErrorWhenDeletingOtherUserOrder) {
  TestShouldReturnErrorWhenDeletingOtherUserOrder();
}

TEST_F(OrderBookDirectImplMarginTest,
       ShouldReturnErrorWhenUpdatingOtherUserOrder) {
  TestShouldReturnErrorWhenUpdatingOtherUserOrder();
}

TEST_F(OrderBookDirectImplMarginTest,
       ShouldReturnErrorWhenUpdatingUnknownOrder) {
  TestShouldReturnErrorWhenUpdatingUnknownOrder();
}

TEST_F(OrderBookDirectImplMarginTest,
       ShouldReturnErrorWhenReducingUnknownOrder) {
  TestShouldReturnErrorWhenReducingUnknownOrder();
}

TEST_F(OrderBookDirectImplMarginTest,
       ShouldReturnErrorWhenReducingByZeroOrNegativeSize) {
  TestShouldReturnErrorWhenReducingByZeroOrNegativeSize();
}

TEST_F(OrderBookDirectImplMarginTest,
       ShouldReturnErrorWhenReducingOtherUserOrder) {
  TestShouldReturnErrorWhenReducingOtherUserOrder();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldMoveOrderExistingBucket) {
  TestShouldMoveOrderExistingBucket();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldMoveOrderNewBucket) {
  TestShouldMoveOrderNewBucket();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldMatchIocOrderPartialBBO) {
  TestShouldMatchIocOrderPartialBBO();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldMatchIocOrderFullBBO) {
  TestShouldMatchIocOrderFullBBO();
}

TEST_F(OrderBookDirectImplMarginTest,
       ShouldMatchIocOrderWithTwoLimitOrdersPartial) {
  TestShouldMatchIocOrderWithTwoLimitOrdersPartial();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldMatchIocOrderFullLiquidity) {
  TestShouldMatchIocOrderFullLiquidity();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldMatchIocOrderWithRejection) {
  TestShouldMatchIocOrderWithRejection();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldRejectFokBidOrderOutOfBudget) {
  TestShouldRejectFokBidOrderOutOfBudget();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldMatchFokBidOrderExactBudget) {
  TestShouldMatchFokBidOrderExactBudget();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldMatchFokBidOrderExtraBudget) {
  TestShouldMatchFokBidOrderExtraBudget();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldRejectFokAskOrderBelowExpectation) {
  TestShouldRejectFokAskOrderBelowExpectation();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldMatchFokAskOrderExactExpectation) {
  TestShouldMatchFokAskOrderExactExpectation();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldMatchFokAskOrderExtraBudget) {
  TestShouldMatchFokAskOrderExtraBudget();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldFullyMatchMarketableGtcOrder) {
  TestShouldFullyMatchMarketableGtcOrder();
}

TEST_F(OrderBookDirectImplMarginTest,
       ShouldPartiallyMatchMarketableGtcOrderAndPlace) {
  TestShouldPartiallyMatchMarketableGtcOrderAndPlace();
}

TEST_F(OrderBookDirectImplMarginTest,
       ShouldFullyMatchMarketableGtcOrder2Prices) {
  TestShouldFullyMatchMarketableGtcOrder2Prices();
}

TEST_F(OrderBookDirectImplMarginTest,
       ShouldFullyMatchMarketableGtcOrderWithAllLiquidity) {
  TestShouldFullyMatchMarketableGtcOrderWithAllLiquidity();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldMoveOrderFullyMatchAsMarketable) {
  TestShouldMoveOrderFullyMatchAsMarketable();
}

TEST_F(OrderBookDirectImplMarginTest,
       ShouldMoveOrderFullyMatchAsMarketable2Prices) {
  TestShouldMoveOrderFullyMatchAsMarketable2Prices();
}

TEST_F(OrderBookDirectImplMarginTest, ShouldMoveOrderMatchesAllLiquidity) {
  TestShouldMoveOrderMatchesAllLiquidity();
}

TEST_F(OrderBookDirectImplMarginTest, SequentialAsksTest) {
  TestSequentialAsks();
}

TEST_F(OrderBookDirectImplMarginTest, SequentialBidsTest) {
  TestSequentialBids();
}

} // namespace orderbook
} // namespace core
} // namespace core2
} // namespace exchange
