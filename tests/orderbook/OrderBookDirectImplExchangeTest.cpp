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

#include <exchange/core/collections/objpool/ObjectsPool.h>
#include <exchange/core/common/config/LoggingConfiguration.h>
#include <exchange/core/orderbook/OrderBookDirectImpl.h>
#include <exchange/core/orderbook/OrderBookEventsHelper.h>
#include "../util/TestConstants.h"
#include "OrderBookDirectImplTest.h"

using namespace exchange::core::common;
using namespace exchange::core::collections::objpool;
using namespace exchange::core::orderbook;
using namespace exchange::core::tests::util;

namespace exchange {
namespace core {
namespace tests {
namespace orderbook {

class OrderBookDirectImplExchangeTest : public OrderBookDirectImplTest {
protected:
  std::unique_ptr<IOrderBook> CreateNewOrderBook() override {
    // Use member variable symbolSpec_ from base class
    auto pool = ObjectsPool::CreateDefaultTestPool();
    auto eventsHelper = OrderBookEventsHelper::NonPooledEventsHelper();
    return std::make_unique<OrderBookDirectImpl>(&symbolSpec_, pool, eventsHelper, nullptr);
  }

  CoreSymbolSpecification GetCoreSymbolSpec() override {
    return TestConstants::CreateSymbolSpecFeeXbtLtc();
  }
};

// Register all tests from base class
TEST_F(OrderBookDirectImplExchangeTest, ShouldInitializeWithoutErrors) {
  TestShouldInitializeWithoutErrors();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldAddGtcOrders) {
  TestShouldAddGtcOrders();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldIgnoredDuplicateOrder) {
  TestShouldIgnoredDuplicateOrder();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldRemoveBidOrder) {
  TestShouldRemoveBidOrder();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldRemoveAskOrder) {
  TestShouldRemoveAskOrder();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldReduceBidOrder) {
  TestShouldReduceBidOrder();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldReduceAskOrder) {
  TestShouldReduceAskOrder();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldRemoveOrderAndEmptyBucket) {
  TestShouldRemoveOrderAndEmptyBucket();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldReturnErrorWhenDeletingUnknownOrder) {
  TestShouldReturnErrorWhenDeletingUnknownOrder();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldReturnErrorWhenDeletingOtherUserOrder) {
  TestShouldReturnErrorWhenDeletingOtherUserOrder();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldReturnErrorWhenUpdatingOtherUserOrder) {
  TestShouldReturnErrorWhenUpdatingOtherUserOrder();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldReturnErrorWhenUpdatingUnknownOrder) {
  TestShouldReturnErrorWhenUpdatingUnknownOrder();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldReturnErrorWhenReducingUnknownOrder) {
  TestShouldReturnErrorWhenReducingUnknownOrder();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldReturnErrorWhenReducingByZeroOrNegativeSize) {
  TestShouldReturnErrorWhenReducingByZeroOrNegativeSize();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldReturnErrorWhenReducingOtherUserOrder) {
  TestShouldReturnErrorWhenReducingOtherUserOrder();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMoveOrderExistingBucket) {
  TestShouldMoveOrderExistingBucket();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMoveOrderNewBucket) {
  TestShouldMoveOrderNewBucket();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMatchIocOrderPartialBBO) {
  TestShouldMatchIocOrderPartialBBO();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMatchIocOrderFullBBO) {
  TestShouldMatchIocOrderFullBBO();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMatchIocOrderWithTwoLimitOrdersPartial) {
  TestShouldMatchIocOrderWithTwoLimitOrdersPartial();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMatchIocOrderFullLiquidity) {
  TestShouldMatchIocOrderFullLiquidity();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMatchIocOrderWithRejection) {
  TestShouldMatchIocOrderWithRejection();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldRejectFokBidOrderOutOfBudget) {
  TestShouldRejectFokBidOrderOutOfBudget();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMatchFokBidOrderExactBudget) {
  TestShouldMatchFokBidOrderExactBudget();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMatchFokBidOrderExtraBudget) {
  TestShouldMatchFokBidOrderExtraBudget();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldRejectFokAskOrderBelowExpectation) {
  TestShouldRejectFokAskOrderBelowExpectation();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMatchFokAskOrderExactExpectation) {
  TestShouldMatchFokAskOrderExactExpectation();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMatchFokAskOrderExtraBudget) {
  TestShouldMatchFokAskOrderExtraBudget();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldFullyMatchMarketableGtcOrder) {
  TestShouldFullyMatchMarketableGtcOrder();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldPartiallyMatchMarketableGtcOrderAndPlace) {
  TestShouldPartiallyMatchMarketableGtcOrderAndPlace();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldFullyMatchMarketableGtcOrder2Prices) {
  TestShouldFullyMatchMarketableGtcOrder2Prices();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldFullyMatchMarketableGtcOrderWithAllLiquidity) {
  TestShouldFullyMatchMarketableGtcOrderWithAllLiquidity();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMoveOrderFullyMatchAsMarketable) {
  TestShouldMoveOrderFullyMatchAsMarketable();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMoveOrderFullyMatchAsMarketable2Prices) {
  TestShouldMoveOrderFullyMatchAsMarketable2Prices();
}

TEST_F(OrderBookDirectImplExchangeTest, ShouldMoveOrderMatchesAllLiquidity) {
  TestShouldMoveOrderMatchesAllLiquidity();
}

TEST_F(OrderBookDirectImplExchangeTest, SequentialAsksTest) {
  TestSequentialAsks();
}

TEST_F(OrderBookDirectImplExchangeTest, SequentialBidsTest) {
  TestSequentialBids();
}

TEST_F(OrderBookDirectImplExchangeTest, MultipleCommandsCompareTest) {
  TestMultipleCommandsCompare();
}

}  // namespace orderbook
}  // namespace tests
}  // namespace core
}  // namespace exchange
