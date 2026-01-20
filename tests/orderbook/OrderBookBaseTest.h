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

#pragma once

#include <exchange/core/common/CoreSymbolSpecification.h>
#include <exchange/core/common/L2MarketData.h>
#include <exchange/core/common/MatcherEventType.h>
#include <exchange/core/common/MatcherTradeEvent.h>
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/orderbook/IOrderBook.h>
#include <gtest/gtest.h>
#include <memory>
#include "../util/L2MarketDataHelper.h"

namespace exchange::core::tests::orderbook {

/**
 * TODO tests where IOC order is not fully matched because of limit price
 * (similar to GTC tests)
 * TODO tests where GTC order has duplicate id - rejection event should be sent
 * TODO add tests for exchange mode (moves)
 * TODO test reserve price validation for BID orders in exchange mode
 */
class OrderBookBaseTest : public ::testing::Test {
 protected:
    std::unique_ptr<exchange::core::orderbook::IOrderBook> orderBook_;
    std::unique_ptr<exchange::core::tests::util::L2MarketDataHelper> expectedState_;
    // Store symbol spec to prevent it from being destroyed
    exchange::core::common::CoreSymbolSpecification symbolSpec_;

    static constexpr int64_t INITIAL_PRICE = 81600L;
    static constexpr int64_t MAX_PRICE = 400000L;
    static constexpr int64_t UID_1 = 412L;
    static constexpr int64_t UID_2 = 413L;

    virtual std::unique_ptr<exchange::core::orderbook::IOrderBook> CreateNewOrderBook() = 0;

    virtual exchange::core::common::CoreSymbolSpecification GetCoreSymbolSpec() = 0;

    void SetUp() override;
    void TearDown() override;

    void ClearOrderBook();

    // Utility methods
    void ProcessAndValidate(exchange::core::common::cmd::OrderCommand& cmd,
                            exchange::core::common::cmd::CommandResultCode expectedCmdState);

    void CheckEventTrade(const exchange::core::common::MatcherTradeEvent* event,
                         int64_t matchedId,
                         int64_t price,
                         int64_t size);

    void CheckEventRejection(const exchange::core::common::MatcherTradeEvent* event,
                             int64_t size,
                             int64_t price,
                             int64_t* bidderHoldPrice = nullptr);

    void CheckEventReduce(const exchange::core::common::MatcherTradeEvent* event,
                          int64_t reduceSize,
                          int64_t price,
                          bool completed,
                          int64_t* bidderHoldPrice = nullptr);

    // Test methods - to be implemented in derived classes or as template methods
    // These are declared here but can be overridden in derived classes
    void TestShouldInitializeWithoutErrors();
    void TestShouldAddGtcOrders();
    void TestShouldIgnoredDuplicateOrder();
    void TestShouldRemoveBidOrder();
    void TestShouldRemoveAskOrder();
    void TestShouldReduceBidOrder();
    void TestShouldReduceAskOrder();
    void TestShouldRemoveOrderAndEmptyBucket();
    void TestShouldReturnErrorWhenDeletingUnknownOrder();
    void TestShouldReturnErrorWhenDeletingOtherUserOrder();
    void TestShouldReturnErrorWhenUpdatingOtherUserOrder();
    void TestShouldReturnErrorWhenUpdatingUnknownOrder();
    void TestShouldReturnErrorWhenReducingUnknownOrder();
    void TestShouldReturnErrorWhenReducingByZeroOrNegativeSize();
    void TestShouldReturnErrorWhenReducingOtherUserOrder();
    void TestShouldMoveOrderExistingBucket();
    void TestShouldMoveOrderNewBucket();
    void TestShouldMatchIocOrderPartialBBO();
    void TestShouldMatchIocOrderFullBBO();
    void TestShouldMatchIocOrderWithTwoLimitOrdersPartial();
    void TestShouldMatchIocOrderFullLiquidity();
    void TestShouldMatchIocOrderWithRejection();
    void TestShouldRejectFokBidOrderOutOfBudget();
    void TestShouldMatchFokBidOrderExactBudget();
    void TestShouldMatchFokBidOrderExtraBudget();
    void TestShouldRejectFokAskOrderBelowExpectation();
    void TestShouldMatchFokAskOrderExactExpectation();
    void TestShouldMatchFokAskOrderExtraBudget();
    void TestShouldFullyMatchMarketableGtcOrder();
    void TestShouldPartiallyMatchMarketableGtcOrderAndPlace();
    void TestShouldFullyMatchMarketableGtcOrder2Prices();
    void TestShouldFullyMatchMarketableGtcOrderWithAllLiquidity();
    void TestShouldMoveOrderFullyMatchAsMarketable();
    void TestShouldMoveOrderFullyMatchAsMarketable2Prices();
    void TestShouldMoveOrderMatchesAllLiquidity();
    void TestMultipleCommandsKeepInternalState();

    // Debug helper methods for comparing implementations
    void PrintOrderBookState(const std::string& label,
                             exchange::core::orderbook::IOrderBook* orderBook);
    void PrintOrderBookComparison(exchange::core::orderbook::IOrderBook* orderBook1,
                                  exchange::core::orderbook::IOrderBook* orderBook2,
                                  const std::string& label1 = "OrderBook1",
                                  const std::string& label2 = "OrderBook2");
};

}  // namespace exchange::core::tests::orderbook
