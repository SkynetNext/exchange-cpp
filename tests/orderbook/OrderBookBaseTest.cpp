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

#include "OrderBookBaseTest.h"
#include "../util/TestOrdersGenerator.h"
#include <numeric>

using namespace exchange::core::common;
using namespace exchange::core::common::cmd;
using namespace exchange::core::orderbook;
using namespace exchange::core2::tests::util;

namespace exchange {
namespace core2 {
namespace core {
namespace orderbook {

void OrderBookBaseTest::SetUp() {
  // Store symbol spec before creating order book
  symbolSpec_ = GetCoreSymbolSpec();
  orderBook_ = CreateNewOrderBook();
  orderBook_->ValidateInternalState();

  auto cmd0 = OrderCommand::NewOrder(OrderType::GTC, 0L, UID_2, INITIAL_PRICE,
                                     0L, 13L, OrderAction::ASK);
  ProcessAndValidate(cmd0, CommandResultCode::SUCCESS);

  auto cmdCancel0 = OrderCommand::Cancel(0L, UID_2);
  ProcessAndValidate(cmdCancel0, CommandResultCode::SUCCESS);

  auto cmd1 = OrderCommand::NewOrder(OrderType::GTC, 1L, UID_1, 81600L, 0L,
                                     100L, OrderAction::ASK);
  ProcessAndValidate(cmd1, CommandResultCode::SUCCESS);

  auto cmd2 = OrderCommand::NewOrder(OrderType::GTC, 2L, UID_1, 81599L, 0L, 50L,
                                     OrderAction::ASK);
  ProcessAndValidate(cmd2, CommandResultCode::SUCCESS);

  auto cmd3 = OrderCommand::NewOrder(OrderType::GTC, 3L, UID_1, 81599L, 0L, 25L,
                                     OrderAction::ASK);
  ProcessAndValidate(cmd3, CommandResultCode::SUCCESS);

  auto cmd8 = OrderCommand::NewOrder(OrderType::GTC, 8L, UID_1, 201000L, 0L,
                                     28L, OrderAction::ASK);
  ProcessAndValidate(cmd8, CommandResultCode::SUCCESS);

  auto cmd9 = OrderCommand::NewOrder(OrderType::GTC, 9L, UID_1, 201000L, 0L,
                                     32L, OrderAction::ASK);
  ProcessAndValidate(cmd9, CommandResultCode::SUCCESS);

  auto cmd10 = OrderCommand::NewOrder(OrderType::GTC, 10L, UID_1, 200954L, 0L,
                                      10L, OrderAction::ASK);
  ProcessAndValidate(cmd10, CommandResultCode::SUCCESS);

  auto cmd4 = OrderCommand::NewOrder(OrderType::GTC, 4L, UID_1, 81593L, 82000L,
                                     40L, OrderAction::BID);
  ProcessAndValidate(cmd4, CommandResultCode::SUCCESS);

  auto cmd5 = OrderCommand::NewOrder(OrderType::GTC, 5L, UID_1, 81590L, 82000L,
                                     20L, OrderAction::BID);
  ProcessAndValidate(cmd5, CommandResultCode::SUCCESS);

  auto cmd6 = OrderCommand::NewOrder(OrderType::GTC, 6L, UID_1, 81590L, 82000L,
                                     1L, OrderAction::BID);
  ProcessAndValidate(cmd6, CommandResultCode::SUCCESS);

  auto cmd7 = OrderCommand::NewOrder(OrderType::GTC, 7L, UID_1, 81200L, 82000L,
                                     20L, OrderAction::BID);
  ProcessAndValidate(cmd7, CommandResultCode::SUCCESS);

  auto cmd11 = OrderCommand::NewOrder(OrderType::GTC, 11L, UID_1, 10000L,
                                      12000L, 12L, OrderAction::BID);
  ProcessAndValidate(cmd11, CommandResultCode::SUCCESS);

  auto cmd12 = OrderCommand::NewOrder(OrderType::GTC, 12L, UID_1, 10000L,
                                      12000L, 1L, OrderAction::BID);
  ProcessAndValidate(cmd12, CommandResultCode::SUCCESS);

  auto cmd13 = OrderCommand::NewOrder(OrderType::GTC, 13L, UID_1, 9136L, 12000L,
                                      2L, OrderAction::BID);
  ProcessAndValidate(cmd13, CommandResultCode::SUCCESS);

  // Create expected state
  std::vector<int64_t> askPrices = {81599, 81600, 200954, 201000};
  std::vector<int64_t> askVolumes = {75, 100, 10, 60};
  std::vector<int64_t> askOrders = {2, 1, 1, 2};
  std::vector<int64_t> bidPrices = {81593, 81590, 81200, 10000, 9136};
  std::vector<int64_t> bidVolumes = {40, 21, 20, 13, 2};
  std::vector<int64_t> bidOrders = {1, 2, 1, 2, 1};

  auto initialL2 = std::make_unique<L2MarketData>(
      askPrices, askVolumes, askOrders, bidPrices, bidVolumes, bidOrders);
  expectedState_ = std::make_unique<L2MarketDataHelper>(*initialL2);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(25);
  ASSERT_EQ(*expectedState_->Build(), *snapshot);
}

void OrderBookBaseTest::TearDown() { ClearOrderBook(); }

void OrderBookBaseTest::ClearOrderBook() {
  orderBook_->ValidateInternalState();
  auto snapshot = orderBook_->GetL2MarketDataSnapshot(INT32_MAX);

  // Match all asks
  int64_t askSum =
      std::accumulate(snapshot->askVolumes.begin(),
                      snapshot->askVolumes.begin() + snapshot->askSize, 0LL);
  auto cmdAsk =
      OrderCommand::NewOrder(OrderType::IOC, 100000000000L, -1, MAX_PRICE,
                             MAX_PRICE, askSum, OrderAction::BID);
  IOrderBook::ProcessCommand(orderBook_.get(), &cmdAsk);

  orderBook_->ValidateInternalState();

  // Match all bids
  int64_t bidSum =
      std::accumulate(snapshot->bidVolumes.begin(),
                      snapshot->bidVolumes.begin() + snapshot->bidSize, 0LL);
  auto cmdBid = OrderCommand::NewOrder(OrderType::IOC, 100000000001L, -2, 1, 0,
                                       bidSum, OrderAction::ASK);
  IOrderBook::ProcessCommand(orderBook_.get(), &cmdBid);

  auto finalSnapshot = orderBook_->GetL2MarketDataSnapshot(INT32_MAX);
  ASSERT_EQ(finalSnapshot->askSize, 0);
  ASSERT_EQ(finalSnapshot->bidSize, 0);

  orderBook_->ValidateInternalState();
}

void OrderBookBaseTest::ProcessAndValidate(OrderCommand &cmd,
                                           CommandResultCode expectedCmdState) {
  CommandResultCode resultCode =
      IOrderBook::ProcessCommand(orderBook_.get(), &cmd);
  ASSERT_EQ(resultCode, expectedCmdState);
  orderBook_->ValidateInternalState();
}

void OrderBookBaseTest::CheckEventTrade(const MatcherTradeEvent *event,
                                        int64_t matchedId, int64_t price,
                                        int64_t size) {
  ASSERT_EQ(event->eventType, MatcherEventType::TRADE);
  ASSERT_EQ(event->matchedOrderId, matchedId);
  ASSERT_EQ(event->price, price);
  ASSERT_EQ(event->size, size);
  // TODO add more checks for MatcherTradeEvent
}

void OrderBookBaseTest::CheckEventRejection(const MatcherTradeEvent *event,
                                            int64_t size, int64_t price,
                                            int64_t *bidderHoldPrice) {
  ASSERT_EQ(event->eventType, MatcherEventType::REJECT);
  ASSERT_EQ(event->size, size);
  ASSERT_EQ(event->price, price);
  ASSERT_TRUE(event->activeOrderCompleted);
  if (bidderHoldPrice != nullptr) {
    ASSERT_EQ(event->bidderHoldPrice, *bidderHoldPrice);
  }
}

void OrderBookBaseTest::CheckEventReduce(const MatcherTradeEvent *event,
                                         int64_t reduceSize, int64_t price,
                                         bool completed,
                                         int64_t *bidderHoldPrice) {
  ASSERT_EQ(event->eventType, MatcherEventType::REDUCE);
  ASSERT_EQ(event->size, reduceSize);
  ASSERT_EQ(event->price, price);
  ASSERT_EQ(event->activeOrderCompleted, completed);
  ASSERT_EQ(event->nextEvent, nullptr);
  if (bidderHoldPrice != nullptr) {
    ASSERT_EQ(event->bidderHoldPrice, *bidderHoldPrice);
  }
}

// Test method implementations
void OrderBookBaseTest::TestShouldInitializeWithoutErrors() {
  // Empty test - just verifies setup works
}

void OrderBookBaseTest::TestShouldAddGtcOrders() {
  auto cmd93 = OrderCommand::NewOrder(OrderType::GTC, 93, UID_1, 81598, 0, 1,
                                      OrderAction::ASK);
  IOrderBook::ProcessCommand(orderBook_.get(), &cmd93);
  expectedState_->InsertAsk(0, 81598, 1);

  auto cmd94 =
      OrderCommand::NewOrder(OrderType::GTC, 94, UID_1, 81594, MAX_PRICE,
                             9'000'000'000L, OrderAction::BID);
  IOrderBook::ProcessCommand(orderBook_.get(), &cmd94);
  expectedState_->InsertBid(0, 81594, 9'000'000'000L);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(25);
  ASSERT_EQ(*expectedState_->Build(), *snapshot);
  orderBook_->ValidateInternalState();

  auto cmd95 = OrderCommand::NewOrder(OrderType::GTC, 95, UID_1, 130000, 0,
                                      13'000'000'000L, OrderAction::ASK);
  IOrderBook::ProcessCommand(orderBook_.get(), &cmd95);
  expectedState_->InsertAsk(3, 130000, 13'000'000'000L);

  auto cmd96 = OrderCommand::NewOrder(OrderType::GTC, 96, UID_1, 1000,
                                      MAX_PRICE, 4, OrderAction::BID);
  IOrderBook::ProcessCommand(orderBook_.get(), &cmd96);
  expectedState_->InsertBid(6, 1000, 4);

  snapshot = orderBook_->GetL2MarketDataSnapshot(25);
  ASSERT_EQ(*expectedState_->Build(), *snapshot);
  orderBook_->ValidateInternalState();
}

void OrderBookBaseTest::TestShouldIgnoredDuplicateOrder() {
  auto orderCommand = OrderCommand::NewOrder(OrderType::GTC, 1, UID_1, 81600, 0,
                                             100, OrderAction::ASK);
  ProcessAndValidate(orderCommand, CommandResultCode::SUCCESS);
  auto events = orderCommand.ExtractEvents();
  ASSERT_EQ(events.size(), 1U);
}

void OrderBookBaseTest::TestShouldRemoveBidOrder() {
  auto cmd = OrderCommand::Cancel(5, UID_1);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  expectedState_->SetBidVolume(1, 1).DecrementBidOrdersNum(1);
  auto snapshot = orderBook_->GetL2MarketDataSnapshot(25);
  ASSERT_EQ(*expectedState_->Build(), *snapshot);

  ASSERT_EQ(cmd.action, OrderAction::BID);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 1U);
  CheckEventReduce(events[0], 20L, 81590, true, nullptr);
}

void OrderBookBaseTest::TestShouldRemoveAskOrder() {
  auto cmd = OrderCommand::Cancel(2, UID_1);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  expectedState_->SetAskVolume(0, 25).DecrementAskOrdersNum(0);
  auto snapshot = orderBook_->GetL2MarketDataSnapshot(25);
  ASSERT_EQ(*expectedState_->Build(), *snapshot);

  ASSERT_EQ(cmd.action, OrderAction::ASK);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 1U);
  CheckEventReduce(events[0], 50L, 81599L, true, nullptr);
}

void OrderBookBaseTest::TestShouldReduceBidOrder() {
  auto cmd = OrderCommand::Reduce(5, UID_1, 3);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  expectedState_->DecrementBidVolume(1, 3);
  auto snapshot = orderBook_->GetL2MarketDataSnapshot();
  ASSERT_EQ(*expectedState_->Build(), *snapshot);

  ASSERT_EQ(cmd.action, OrderAction::BID);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 1U);
  CheckEventReduce(events[0], 3L, 81590L, false, nullptr);
}

void OrderBookBaseTest::TestShouldReduceAskOrder() {
  auto cmd = OrderCommand::Reduce(1, UID_1, 300);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  expectedState_->RemoveAsk(1);
  auto snapshot = orderBook_->GetL2MarketDataSnapshot();
  ASSERT_EQ(*expectedState_->Build(), *snapshot);

  ASSERT_EQ(cmd.action, OrderAction::ASK);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 1U);
  CheckEventReduce(events[0], 100L, 81600L, true, nullptr);
}

void OrderBookBaseTest::TestShouldRemoveOrderAndEmptyBucket() {
  auto cmdCancel2 = OrderCommand::Cancel(2, UID_1);
  ProcessAndValidate(cmdCancel2, CommandResultCode::SUCCESS);

  ASSERT_EQ(cmdCancel2.action, OrderAction::ASK);

  auto events = cmdCancel2.ExtractEvents();
  ASSERT_EQ(events.size(), 1U);
  CheckEventReduce(events[0], 50L, 81599L, true, nullptr);

  auto cmdCancel3 = OrderCommand::Cancel(3, UID_1);
  ProcessAndValidate(cmdCancel3, CommandResultCode::SUCCESS);

  ASSERT_EQ(cmdCancel3.action, OrderAction::ASK);

  auto expected = expectedState_->RemoveAsk(0).Build();
  auto snapshot = orderBook_->GetL2MarketDataSnapshot();
  ASSERT_EQ(*expected, *snapshot);

  events = cmdCancel3.ExtractEvents();
  ASSERT_EQ(events.size(), 1U);
  CheckEventReduce(events[0], 25L, 81599L, true, nullptr);
}

void OrderBookBaseTest::TestShouldReturnErrorWhenDeletingUnknownOrder() {
  auto cmd = OrderCommand::Cancel(5291, UID_1);
  ProcessAndValidate(cmd, CommandResultCode::MATCHING_UNKNOWN_ORDER_ID);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot();
  ASSERT_EQ(*expectedState_->Build(), *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 0U);
}

void OrderBookBaseTest::TestShouldReturnErrorWhenDeletingOtherUserOrder() {
  auto cmd = OrderCommand::Cancel(3, UID_2);
  ProcessAndValidate(cmd, CommandResultCode::MATCHING_UNKNOWN_ORDER_ID);
  ASSERT_EQ(cmd.matcherEvent, nullptr);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot();
  ASSERT_EQ(*expectedState_->Build(), *snapshot);
}

void OrderBookBaseTest::TestShouldReturnErrorWhenUpdatingOtherUserOrder() {
  auto cmd = OrderCommand::Update(2, UID_2, 100);
  ProcessAndValidate(cmd, CommandResultCode::MATCHING_UNKNOWN_ORDER_ID);
  ASSERT_EQ(cmd.matcherEvent, nullptr);

  cmd = OrderCommand::Update(8, UID_2, 100);
  ProcessAndValidate(cmd, CommandResultCode::MATCHING_UNKNOWN_ORDER_ID);
  ASSERT_EQ(cmd.matcherEvent, nullptr);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot();
  ASSERT_EQ(*expectedState_->Build(), *snapshot);
}

void OrderBookBaseTest::TestShouldReturnErrorWhenUpdatingUnknownOrder() {
  auto cmd = OrderCommand::Update(2433, UID_1, 300);
  ProcessAndValidate(cmd, CommandResultCode::MATCHING_UNKNOWN_ORDER_ID);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  ASSERT_EQ(*expectedState_->Build(), *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 0U);
}

void OrderBookBaseTest::TestShouldReturnErrorWhenReducingUnknownOrder() {
  auto cmd = OrderCommand::Reduce(3, UID_2, 1);
  ProcessAndValidate(cmd, CommandResultCode::MATCHING_UNKNOWN_ORDER_ID);
  ASSERT_EQ(cmd.matcherEvent, nullptr);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot();
  ASSERT_EQ(*expectedState_->Build(), *snapshot);
}

void OrderBookBaseTest::
    TestShouldReturnErrorWhenReducingByZeroOrNegativeSize() {
  auto cmd = OrderCommand::Reduce(4, UID_1, 0);
  ProcessAndValidate(cmd, CommandResultCode::MATCHING_REDUCE_FAILED_WRONG_SIZE);
  ASSERT_EQ(cmd.matcherEvent, nullptr);

  cmd = OrderCommand::Reduce(8, UID_1, -1);
  ProcessAndValidate(cmd, CommandResultCode::MATCHING_REDUCE_FAILED_WRONG_SIZE);
  ASSERT_EQ(cmd.matcherEvent, nullptr);

  cmd = OrderCommand::Reduce(8, UID_1, INT64_MIN);
  ProcessAndValidate(cmd, CommandResultCode::MATCHING_REDUCE_FAILED_WRONG_SIZE);
  ASSERT_EQ(cmd.matcherEvent, nullptr);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot();
  ASSERT_EQ(*expectedState_->Build(), *snapshot);
}

void OrderBookBaseTest::TestShouldReturnErrorWhenReducingOtherUserOrder() {
  auto cmd = OrderCommand::Reduce(8, UID_2, 3);
  ProcessAndValidate(cmd, CommandResultCode::MATCHING_UNKNOWN_ORDER_ID);
  ASSERT_EQ(cmd.matcherEvent, nullptr);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot();
  ASSERT_EQ(*expectedState_->Build(), *snapshot);
}

void OrderBookBaseTest::TestShouldMoveOrderExistingBucket() {
  auto cmd = OrderCommand::Update(7, UID_1, 81590);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);

  auto expected = expectedState_->SetBidVolume(1, 41)
                      .IncrementBidOrdersNum(1)
                      .RemoveBid(2)
                      .Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 0U);
}

void OrderBookBaseTest::TestShouldMoveOrderNewBucket() {
  auto cmd = OrderCommand::Update(7, UID_1, 81594);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);

  auto expected = expectedState_->RemoveBid(2).InsertBid(0, 81594, 20).Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 0U);
}

void OrderBookBaseTest::TestShouldMatchIocOrderPartialBBO() {
  auto cmd = OrderCommand::NewOrder(OrderType::IOC, 123, UID_2, 1, 0, 10,
                                    OrderAction::ASK);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  auto expected = expectedState_->SetBidVolume(0, 30).Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 1U);
  CheckEventTrade(events[0], 4L, 81593, 10L);
}

void OrderBookBaseTest::TestShouldMatchIocOrderFullBBO() {
  auto cmd = OrderCommand::NewOrder(OrderType::IOC, 123, UID_2, 1, 0, 40,
                                    OrderAction::ASK);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  auto expected = expectedState_->RemoveBid(0).Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 1U);
  CheckEventTrade(events[0], 4L, 81593, 40L);
}

void OrderBookBaseTest::TestShouldMatchIocOrderWithTwoLimitOrdersPartial() {
  auto cmd = OrderCommand::NewOrder(OrderType::IOC, 123, UID_2, 1, 0, 41,
                                    OrderAction::ASK);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  auto expected = expectedState_->RemoveBid(0).SetBidVolume(0, 20).Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 2U);
  CheckEventTrade(events[0], 4L, 81593, 40L);
  CheckEventTrade(events[1], 5L, 81590, 1L);

  ASSERT_EQ(orderBook_->GetOrderById(4L), nullptr);
  ASSERT_NE(orderBook_->GetOrderById(5L), nullptr);
}

void OrderBookBaseTest::TestShouldMatchIocOrderFullLiquidity() {
  auto cmd = OrderCommand::NewOrder(OrderType::IOC, 123, UID_2, MAX_PRICE,
                                    MAX_PRICE, 175, OrderAction::BID);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  auto expected = expectedState_->RemoveAsk(0).RemoveAsk(0).Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 3U);
  CheckEventTrade(events[0], 2L, 81599L, 50L);
  CheckEventTrade(events[1], 3L, 81599L, 25L);
  CheckEventTrade(events[2], 1L, 81600L, 100L);

  ASSERT_EQ(orderBook_->GetOrderById(1L), nullptr);
  ASSERT_EQ(orderBook_->GetOrderById(2L), nullptr);
  ASSERT_EQ(orderBook_->GetOrderById(3L), nullptr);
}

void OrderBookBaseTest::TestShouldMatchIocOrderWithRejection() {
  auto cmd = OrderCommand::NewOrder(OrderType::IOC, 123, UID_2, MAX_PRICE,
                                    MAX_PRICE + 1, 270, OrderAction::BID);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  auto expected = expectedState_->RemoveAllAsks().Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 7U);

  int64_t bidderHoldPrice = MAX_PRICE + 1;
  CheckEventRejection(events[0], 25L, 400000L, &bidderHoldPrice);
}

void OrderBookBaseTest::TestShouldRejectFokBidOrderOutOfBudget() {
  int64_t size = 180L;
  int64_t buyBudget = expectedState_->AggregateBuyBudget(size) - 1;
  ASSERT_EQ(buyBudget, 81599L * 75L + 81600L * 100L + 200954L * 5L - 1);

  auto cmd =
      OrderCommand::NewOrder(OrderType::FOK_BUDGET, 123L, UID_2, buyBudget,
                             buyBudget, size, OrderAction::BID);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  ASSERT_EQ(*expectedState_->Build(), *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 1U);

  CheckEventRejection(events[0], size, buyBudget, &buyBudget);
}

void OrderBookBaseTest::TestShouldMatchFokBidOrderExactBudget() {
  int64_t size = 180L;
  int64_t buyBudget = expectedState_->AggregateBuyBudget(size);
  ASSERT_EQ(buyBudget, 81599L * 75L + 81600L * 100L + 200954L * 5L);

  auto cmd =
      OrderCommand::NewOrder(OrderType::FOK_BUDGET, 123L, UID_2, buyBudget,
                             buyBudget, size, OrderAction::BID);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  auto expected =
      expectedState_->RemoveAsk(0).RemoveAsk(0).SetAskVolume(0, 5).Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 4U);
  CheckEventTrade(events[0], 2L, 81599, 50L);
  CheckEventTrade(events[1], 3L, 81599, 25L);
  CheckEventTrade(events[2], 1L, 81600L, 100L);
  CheckEventTrade(events[3], 10L, 200954L, 5L);
}

void OrderBookBaseTest::TestShouldMatchFokBidOrderExtraBudget() {
  int64_t size = 176L;
  int64_t buyBudget = expectedState_->AggregateBuyBudget(size) + 1;
  ASSERT_EQ(buyBudget, 81599L * 75L + 81600L * 100L + 200954L + 1L);

  auto cmd =
      OrderCommand::NewOrder(OrderType::FOK_BUDGET, 123L, UID_2, buyBudget,
                             buyBudget, size, OrderAction::BID);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  auto expected =
      expectedState_->RemoveAsk(0).RemoveAsk(0).SetAskVolume(0, 9).Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 4U);
  CheckEventTrade(events[0], 2L, 81599, 50L);
  CheckEventTrade(events[1], 3L, 81599, 25L);
  CheckEventTrade(events[2], 1L, 81600L, 100L);
  CheckEventTrade(events[3], 10L, 200954L, 1L);
}

void OrderBookBaseTest::TestShouldRejectFokAskOrderBelowExpectation() {
  int64_t size = 60L;
  int64_t sellExpectation = expectedState_->AggregateSellExpectation(size) + 1;
  ASSERT_EQ(sellExpectation, 81593L * 40L + 81590L * 20L + 1);

  auto cmd = OrderCommand::NewOrder(OrderType::FOK_BUDGET, 123L, UID_2,
                                    sellExpectation, sellExpectation, size,
                                    OrderAction::ASK);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  ASSERT_EQ(*expectedState_->Build(), *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 1U);
  CheckEventRejection(events[0], size, sellExpectation, &sellExpectation);
}

void OrderBookBaseTest::TestShouldMatchFokAskOrderExactExpectation() {
  int64_t size = 60L;
  int64_t sellExpectation = expectedState_->AggregateSellExpectation(size);
  ASSERT_EQ(sellExpectation, 81593L * 40L + 81590L * 20L);

  auto cmd = OrderCommand::NewOrder(OrderType::FOK_BUDGET, 123L, UID_2,
                                    sellExpectation, sellExpectation, size,
                                    OrderAction::ASK);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  auto expected = expectedState_->RemoveBid(0)
                      .SetBidVolume(0, 1)
                      .DecrementBidOrdersNum(0)
                      .Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 2U);
  CheckEventTrade(events[0], 4L, 81593L, 40L);
  CheckEventTrade(events[1], 5L, 81590L, 20L);
}

void OrderBookBaseTest::TestShouldMatchFokAskOrderExtraBudget() {
  int64_t size = 61L;
  int64_t sellExpectation = expectedState_->AggregateSellExpectation(size) - 1;
  ASSERT_EQ(sellExpectation, 81593L * 40L + 81590L * 21L - 1);

  auto cmd = OrderCommand::NewOrder(OrderType::FOK_BUDGET, 123L, UID_2,
                                    sellExpectation, sellExpectation, size,
                                    OrderAction::ASK);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  auto expected = expectedState_->RemoveBid(0).RemoveBid(0).Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 3U);
  CheckEventTrade(events[0], 4L, 81593L, 40L);
  CheckEventTrade(events[1], 5L, 81590L, 20L);
  CheckEventTrade(events[2], 6L, 81590L, 1L);
}

void OrderBookBaseTest::TestShouldFullyMatchMarketableGtcOrder() {
  auto cmd = OrderCommand::NewOrder(OrderType::GTC, 123, UID_2, 81599,
                                    MAX_PRICE, 1, OrderAction::BID);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  auto expected = expectedState_->SetAskVolume(0, 74).Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 1U);
  CheckEventTrade(events[0], 2L, 81599, 1L);
}

void OrderBookBaseTest::TestShouldPartiallyMatchMarketableGtcOrderAndPlace() {
  auto cmd = OrderCommand::NewOrder(OrderType::GTC, 123, UID_2, 81599,
                                    MAX_PRICE, 77, OrderAction::BID);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  auto expected = expectedState_->RemoveAsk(0).InsertBid(0, 81599, 2).Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 2U);

  CheckEventTrade(events[0], 2L, 81599, 50L);
  CheckEventTrade(events[1], 3L, 81599, 25L);
}

void OrderBookBaseTest::TestShouldFullyMatchMarketableGtcOrder2Prices() {
  auto cmd = OrderCommand::NewOrder(OrderType::GTC, 123, UID_2, 81600,
                                    MAX_PRICE, 77, OrderAction::BID);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  auto expected = expectedState_->RemoveAsk(0).SetAskVolume(0, 98).Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 3U);

  CheckEventTrade(events[0], 2L, 81599, 50L);
  CheckEventTrade(events[1], 3L, 81599, 25L);
  CheckEventTrade(events[2], 1L, 81600, 2L);
}

void OrderBookBaseTest::
    TestShouldFullyMatchMarketableGtcOrderWithAllLiquidity() {
  auto cmd = OrderCommand::NewOrder(OrderType::GTC, 123, UID_2, 220000,
                                    MAX_PRICE, 1000, OrderAction::BID);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  auto expected =
      expectedState_->RemoveAllAsks().InsertBid(0, 220000, 755).Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 6U);

  CheckEventTrade(events[0], 2L, 81599, 50L);
  CheckEventTrade(events[1], 3L, 81599, 25L);
  CheckEventTrade(events[2], 1L, 81600, 100L);
  CheckEventTrade(events[3], 10L, 200954, 10L);
  CheckEventTrade(events[4], 8L, 201000, 28L);
  CheckEventTrade(events[5], 9L, 201000, 32L);
}

void OrderBookBaseTest::TestShouldMoveOrderFullyMatchAsMarketable() {
  auto cmd = OrderCommand::NewOrder(OrderType::GTC, 83, UID_2, 81200, MAX_PRICE,
                                    20, OrderAction::BID);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 0U);

  auto expected =
      expectedState_->SetBidVolume(2, 40).IncrementBidOrdersNum(2).Build();
  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  ASSERT_EQ(*expected, *snapshot);

  cmd = OrderCommand::Update(83, UID_2, 81602);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  expected = expectedState_->SetBidVolume(2, 20)
                 .DecrementBidOrdersNum(2)
                 .SetAskVolume(0, 55)
                 .Build();
  snapshot = orderBook_->GetL2MarketDataSnapshot(10);
  ASSERT_EQ(*expected, *snapshot);

  events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 1U);
  CheckEventTrade(events[0], 2L, 81599, 20L);
}

void OrderBookBaseTest::TestShouldMoveOrderFullyMatchAsMarketable2Prices() {
  auto cmd = OrderCommand::NewOrder(OrderType::GTC, 83, UID_2, 81594, MAX_PRICE,
                                    100, OrderAction::BID);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 0U);

  cmd = OrderCommand::Update(83, UID_2, 81600);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);

  auto expected = expectedState_->RemoveAsk(0).SetAskVolume(0, 75).Build();
  ASSERT_EQ(*expected, *snapshot);

  events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 3U);
  CheckEventTrade(events[0], 2L, 81599, 50L);
  CheckEventTrade(events[1], 3L, 81599, 25L);
  CheckEventTrade(events[2], 1L, 81600, 25L);
}

void OrderBookBaseTest::TestShouldMoveOrderMatchesAllLiquidity() {
  auto cmd = OrderCommand::NewOrder(OrderType::GTC, 83, UID_2, 81594, MAX_PRICE,
                                    246, OrderAction::BID);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  cmd = OrderCommand::Update(83, UID_2, 201000);
  ProcessAndValidate(cmd, CommandResultCode::SUCCESS);

  auto snapshot = orderBook_->GetL2MarketDataSnapshot(10);

  auto expected =
      expectedState_->RemoveAllAsks().InsertBid(0, 201000, 1).Build();
  ASSERT_EQ(*expected, *snapshot);

  auto events = cmd.ExtractEvents();
  ASSERT_EQ(events.size(), 6U);
  CheckEventTrade(events[0], 2L, 81599, 50L);
  CheckEventTrade(events[1], 3L, 81599, 25L);
  CheckEventTrade(events[2], 1L, 81600, 100L);
  CheckEventTrade(events[3], 10L, 200954, 10L);
  CheckEventTrade(events[4], 8L, 201000, 28L);
  CheckEventTrade(events[5], 9L, 201000, 32L);
}

void OrderBookBaseTest::TestMultipleCommandsKeepInternalState() {
  const int tranNum = 25000;

  auto localOrderBook = CreateNewOrderBook();
  localOrderBook->ValidateInternalState();

  auto genResult = TestOrdersGenerator::GenerateCommands(
      tranNum, 200, 6, TestOrdersGenerator::UID_PLAIN_MAPPER, 0, false, false,
      TestOrdersGenerator::CreateAsyncProgressLogger(tranNum), 348290254);

  auto &allCommands = genResult.GetCommands();
  for (auto &cmd : allCommands) {
    cmd.orderId += 100; // TODO set start id
    CommandResultCode commandResultCode =
        IOrderBook::ProcessCommand(localOrderBook.get(), &cmd);
    ASSERT_EQ(commandResultCode, CommandResultCode::SUCCESS);
    localOrderBook->ValidateInternalState();
  }
}

} // namespace orderbook
} // namespace core
} // namespace core2
} // namespace exchange
