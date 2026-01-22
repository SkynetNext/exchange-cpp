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

#include <exchange/core/IEventsHandler.h>
#include <exchange/core/SimpleEventsProcessor.h>
#include <exchange/core/common/MatcherEventType.h>
#include <exchange/core/common/MatcherTradeEvent.h>
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/api/ApiCancelOrder.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/api/ApiReduceOrder.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/cmd/OrderCommandType.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace exchange::core;
using namespace exchange::core::common;
using namespace exchange::core::common::cmd;
using namespace exchange::core::common::api;
using ::testing::_;
using ::testing::SaveArg;
using ::testing::StrictMock;

// Mock class for IEventsHandler
class MockEventsHandler : public IEventsHandler {
public:
  MOCK_METHOD(void, CommandResult, (const ApiCommandResult&), (override));
  MOCK_METHOD(void, TradeEvent, (const exchange::core::TradeEvent&), (override));
  MOCK_METHOD(void, RejectEvent, (const exchange::core::RejectEvent&), (override));
  MOCK_METHOD(void, ReduceEvent, (const exchange::core::ReduceEvent&), (override));
  MOCK_METHOD(void, OrderBook, (const exchange::core::OrderBook&), (override));
};

class SimpleEventsProcessorTest : public ::testing::Test {
protected:
  void SetUp() override {
    mockHandler_ = std::make_unique<StrictMock<MockEventsHandler>>();
    processor_ = std::make_unique<SimpleEventsProcessor>(mockHandler_.get());
  }

  void TearDown() override {
    processor_.reset();
    mockHandler_.reset();
  }

  OrderCommand SampleCancelCommand() {
    OrderCommand cmd;
    cmd.command = OrderCommandType::CANCEL_ORDER;
    cmd.orderId = 123L;
    cmd.symbol = 3;
    cmd.price = 12800L;
    cmd.size = 3L;
    cmd.reserveBidPrice = 12800L;
    cmd.action = OrderAction::BID;
    cmd.orderType = OrderType::GTC;
    cmd.uid = 29851L;
    cmd.timestamp = 1578930983745201L;
    cmd.userCookie = 44188;
    cmd.resultCode = CommandResultCode::MATCHING_INVALID_ORDER_BOOK_ID;
    cmd.matcherEvent = nullptr;
    cmd.marketData = nullptr;
    return cmd;
  }

  OrderCommand SampleReduceCommand() {
    OrderCommand cmd;
    cmd.command = OrderCommandType::REDUCE_ORDER;
    cmd.orderId = 123L;
    cmd.symbol = 3;
    cmd.price = 52200L;
    cmd.size = 3200L;
    cmd.reserveBidPrice = 12800L;
    cmd.action = OrderAction::BID;
    cmd.orderType = OrderType::GTC;
    cmd.uid = 29851L;
    cmd.timestamp = 1578930983745201L;
    cmd.userCookie = 44188;
    cmd.resultCode = CommandResultCode::SUCCESS;
    cmd.matcherEvent = nullptr;
    cmd.marketData = nullptr;
    return cmd;
  }

  OrderCommand SamplePlaceOrderCommand() {
    OrderCommand cmd;
    cmd.command = OrderCommandType::PLACE_ORDER;
    cmd.orderId = 123L;
    cmd.symbol = 3;
    cmd.price = 52200L;
    cmd.size = 3200L;
    cmd.reserveBidPrice = 12800L;
    cmd.action = OrderAction::BID;
    cmd.orderType = OrderType::IOC;
    cmd.uid = 29851L;
    cmd.timestamp = 1578930983745201L;
    cmd.userCookie = 44188;
    cmd.resultCode = CommandResultCode::SUCCESS;
    cmd.matcherEvent = nullptr;
    cmd.marketData = nullptr;
    return cmd;
  }

  MatcherTradeEvent* CreateMatcherTradeEvent(MatcherEventType eventType,
                                             bool activeOrderCompleted,
                                             int64_t matchedOrderId,
                                             int64_t matchedOrderUid,
                                             bool matchedOrderCompleted,
                                             int64_t price,
                                             int64_t size) {
    MatcherTradeEvent* event = new MatcherTradeEvent();
    event->eventType = eventType;
    event->activeOrderCompleted = activeOrderCompleted;
    event->matchedOrderId = matchedOrderId;
    event->matchedOrderUid = matchedOrderUid;
    event->matchedOrderCompleted = matchedOrderCompleted;
    event->price = price;
    event->size = size;
    event->nextEvent = nullptr;
    return event;
  }

  std::unique_ptr<StrictMock<MockEventsHandler>> mockHandler_;
  std::unique_ptr<SimpleEventsProcessor> processor_;
};

TEST_F(SimpleEventsProcessorTest, ShouldHandleSimpleCommand) {
  OrderCommand cmd = SampleCancelCommand();

  EXPECT_CALL(*mockHandler_, CommandResult(_)).WillOnce([=](const ApiCommandResult& result) {
    // Verify immediately since SimpleEventsProcessor deletes the command
    ASSERT_NE(result.command, nullptr);
    const auto* cancelOrder = dynamic_cast<const ApiCancelOrder*>(result.command);
    ASSERT_NE(cancelOrder, nullptr);
    EXPECT_EQ(cancelOrder->orderId, 123L);
    EXPECT_EQ(cancelOrder->symbol, 3);
    EXPECT_EQ(cancelOrder->uid, 29851L);
  });
  EXPECT_CALL(*mockHandler_, TradeEvent(_)).Times(0);
  EXPECT_CALL(*mockHandler_, RejectEvent(_)).Times(0);
  EXPECT_CALL(*mockHandler_, ReduceEvent(_)).Times(0);

  processor_->Accept(&cmd, 192837L);
}

TEST_F(SimpleEventsProcessorTest, ShouldHandleWithReduceCommand) {
  OrderCommand cmd = SampleReduceCommand();

  MatcherTradeEvent* matcherEvent =
    CreateMatcherTradeEvent(MatcherEventType::REDUCE, true, 0, 0, false, 20100L, 8272L);
  cmd.matcherEvent = matcherEvent;

  ReduceEvent capturedReduceEvent{};
  EXPECT_CALL(*mockHandler_, CommandResult(_)).WillOnce([=](const ApiCommandResult& result) {
    // Verify immediately since SimpleEventsProcessor deletes the command
    ASSERT_NE(result.command, nullptr);
    const auto* reduceOrder = dynamic_cast<const ApiReduceOrder*>(result.command);
    ASSERT_NE(reduceOrder, nullptr);
    EXPECT_EQ(reduceOrder->orderId, 123L);
    EXPECT_EQ(reduceOrder->reduceSize, 3200L);
    EXPECT_EQ(reduceOrder->symbol, 3);
    EXPECT_EQ(reduceOrder->uid, 29851L);
  });
  EXPECT_CALL(*mockHandler_, TradeEvent(_)).Times(0);
  EXPECT_CALL(*mockHandler_, RejectEvent(_)).Times(0);
  EXPECT_CALL(*mockHandler_, ReduceEvent(_)).WillOnce(SaveArg<0>(&capturedReduceEvent));

  processor_->Accept(&cmd, 192837L);

  EXPECT_EQ(capturedReduceEvent.orderId, 123L);
  EXPECT_EQ(capturedReduceEvent.price, 20100L);
  EXPECT_EQ(capturedReduceEvent.reducedVolume, 8272L);
  EXPECT_TRUE(capturedReduceEvent.orderCompleted);

  delete matcherEvent;
}

TEST_F(SimpleEventsProcessorTest, ShouldHandleWithSingleTrade) {
  OrderCommand cmd = SamplePlaceOrderCommand();

  MatcherTradeEvent* matcherEvent =
    CreateMatcherTradeEvent(MatcherEventType::TRADE, false, 276810L, 10332L, true, 20100L, 8272L);
  cmd.matcherEvent = matcherEvent;

  TradeEvent capturedTradeEvent;
  EXPECT_CALL(*mockHandler_, CommandResult(_)).WillOnce([=](const ApiCommandResult& result) {
    // Verify immediately since SimpleEventsProcessor deletes the command
    ASSERT_NE(result.command, nullptr);
    const auto* placeOrder = dynamic_cast<const ApiPlaceOrder*>(result.command);
    ASSERT_NE(placeOrder, nullptr);
    EXPECT_EQ(placeOrder->orderId, 123L);
    EXPECT_EQ(placeOrder->symbol, 3);
    EXPECT_EQ(placeOrder->price, 52200L);
    EXPECT_EQ(placeOrder->size, 3200L);
    EXPECT_EQ(placeOrder->reservePrice, 12800L);
    EXPECT_EQ(placeOrder->action, OrderAction::BID);
    EXPECT_EQ(placeOrder->orderType, OrderType::IOC);
    EXPECT_EQ(placeOrder->uid, 29851L);
    EXPECT_EQ(placeOrder->userCookie, 44188);
  });
  EXPECT_CALL(*mockHandler_, RejectEvent(_)).Times(0);
  EXPECT_CALL(*mockHandler_, ReduceEvent(_)).Times(0);
  EXPECT_CALL(*mockHandler_, TradeEvent(_)).WillOnce(SaveArg<0>(&capturedTradeEvent));

  processor_->Accept(&cmd, 192837L);

  EXPECT_EQ(capturedTradeEvent.symbol, 3);
  EXPECT_EQ(capturedTradeEvent.totalVolume, 8272L);
  EXPECT_EQ(capturedTradeEvent.takerOrderId, 123L);
  EXPECT_EQ(capturedTradeEvent.takerUid, 29851L);
  EXPECT_EQ(capturedTradeEvent.takerAction, OrderAction::BID);
  EXPECT_FALSE(capturedTradeEvent.takeOrderCompleted);

  ASSERT_EQ(capturedTradeEvent.trades.size(), 1u);
  const Trade& trade = capturedTradeEvent.trades[0];
  EXPECT_EQ(trade.makerOrderId, 276810L);
  EXPECT_EQ(trade.makerUid, 10332L);
  EXPECT_TRUE(trade.makerOrderCompleted);
  EXPECT_EQ(trade.price, 20100L);
  EXPECT_EQ(trade.volume, 8272L);

  delete matcherEvent;
}

TEST_F(SimpleEventsProcessorTest, ShouldHandleWithTwoTrades) {
  OrderCommand cmd = SamplePlaceOrderCommand();

  MatcherTradeEvent* firstTrade =
    CreateMatcherTradeEvent(MatcherEventType::TRADE, false, 276810L, 10332L, true, 20100L, 8272L);
  MatcherTradeEvent* secondTrade =
    CreateMatcherTradeEvent(MatcherEventType::TRADE, true, 100293L, 1982L, false, 20110L, 3121L);
  cmd.matcherEvent = firstTrade;
  firstTrade->nextEvent = secondTrade;

  TradeEvent capturedTradeEvent;
  EXPECT_CALL(*mockHandler_, CommandResult(_)).WillOnce([=](const ApiCommandResult& result) {
    // Verify immediately since SimpleEventsProcessor deletes the command
    ASSERT_NE(result.command, nullptr);
    const auto* placeOrder = dynamic_cast<const ApiPlaceOrder*>(result.command);
    ASSERT_NE(placeOrder, nullptr);
    EXPECT_EQ(placeOrder->orderId, 123L);
    EXPECT_EQ(placeOrder->symbol, 3);
    EXPECT_EQ(placeOrder->price, 52200L);
    EXPECT_EQ(placeOrder->size, 3200L);
    EXPECT_EQ(placeOrder->reservePrice, 12800L);
    EXPECT_EQ(placeOrder->action, OrderAction::BID);
    EXPECT_EQ(placeOrder->orderType, OrderType::IOC);
    EXPECT_EQ(placeOrder->uid, 29851L);
    EXPECT_EQ(placeOrder->userCookie, 44188);
  });
  EXPECT_CALL(*mockHandler_, RejectEvent(_)).Times(0);
  EXPECT_CALL(*mockHandler_, ReduceEvent(_)).Times(0);
  EXPECT_CALL(*mockHandler_, TradeEvent(_)).WillOnce(SaveArg<0>(&capturedTradeEvent));

  processor_->Accept(&cmd, 12981721239L);

  // Validating first event
  EXPECT_EQ(capturedTradeEvent.symbol, 3);
  EXPECT_EQ(capturedTradeEvent.totalVolume, 11393L);  // 8272 + 3121
  EXPECT_EQ(capturedTradeEvent.takerOrderId, 123L);
  EXPECT_EQ(capturedTradeEvent.takerUid, 29851L);
  EXPECT_EQ(capturedTradeEvent.takerAction, OrderAction::BID);
  EXPECT_TRUE(capturedTradeEvent.takeOrderCompleted);

  ASSERT_EQ(capturedTradeEvent.trades.size(), 2u);

  const Trade& trade1 = capturedTradeEvent.trades[0];
  EXPECT_EQ(trade1.makerOrderId, 276810L);
  EXPECT_EQ(trade1.makerUid, 10332L);
  EXPECT_TRUE(trade1.makerOrderCompleted);
  EXPECT_EQ(trade1.price, 20100L);
  EXPECT_EQ(trade1.volume, 8272L);

  const Trade& trade2 = capturedTradeEvent.trades[1];
  EXPECT_EQ(trade2.makerOrderId, 100293L);
  EXPECT_EQ(trade2.makerUid, 1982L);
  EXPECT_FALSE(trade2.makerOrderCompleted);
  EXPECT_EQ(trade2.price, 20110L);
  EXPECT_EQ(trade2.volume, 3121L);

  delete firstTrade;
  delete secondTrade;
}

TEST_F(SimpleEventsProcessorTest, ShouldHandleWithTwoTradesAndReject) {
  OrderCommand cmd = SamplePlaceOrderCommand();

  MatcherTradeEvent* firstTrade =
    CreateMatcherTradeEvent(MatcherEventType::TRADE, false, 276810L, 10332L, true, 20100L, 8272L);
  MatcherTradeEvent* secondTrade =
    CreateMatcherTradeEvent(MatcherEventType::TRADE, true, 100293L, 1982L, false, 20110L, 3121L);
  MatcherTradeEvent* reject =
    CreateMatcherTradeEvent(MatcherEventType::REJECT, true, 0, 0, false, 0, 8272L);
  reject->price = 0;  // REJECT event price is 0

  cmd.matcherEvent = firstTrade;
  firstTrade->nextEvent = secondTrade;
  secondTrade->nextEvent = reject;

  TradeEvent capturedTradeEvent;
  RejectEvent capturedRejectEvent{};
  EXPECT_CALL(*mockHandler_, CommandResult(_)).WillOnce([=](const ApiCommandResult& result) {
    // Verify immediately since SimpleEventsProcessor deletes the command
    ASSERT_NE(result.command, nullptr);
    const auto* placeOrder = dynamic_cast<const ApiPlaceOrder*>(result.command);
    ASSERT_NE(placeOrder, nullptr);
    EXPECT_EQ(placeOrder->orderId, 123L);
    EXPECT_EQ(placeOrder->symbol, 3);
    EXPECT_EQ(placeOrder->price, 52200L);
    EXPECT_EQ(placeOrder->size, 3200L);
    EXPECT_EQ(placeOrder->reservePrice, 12800L);
    EXPECT_EQ(placeOrder->action, OrderAction::BID);
    EXPECT_EQ(placeOrder->orderType, OrderType::IOC);
    EXPECT_EQ(placeOrder->uid, 29851L);
    EXPECT_EQ(placeOrder->userCookie, 44188);
  });
  EXPECT_CALL(*mockHandler_, RejectEvent(_)).WillOnce(SaveArg<0>(&capturedRejectEvent));
  EXPECT_CALL(*mockHandler_, ReduceEvent(_)).Times(0);
  EXPECT_CALL(*mockHandler_, TradeEvent(_)).WillOnce(SaveArg<0>(&capturedTradeEvent));

  processor_->Accept(&cmd, 12981721239L);

  // Validating trade event
  EXPECT_EQ(capturedTradeEvent.symbol, 3);
  EXPECT_EQ(capturedTradeEvent.totalVolume, 11393L);
  EXPECT_EQ(capturedTradeEvent.takerOrderId, 123L);
  EXPECT_EQ(capturedTradeEvent.takerUid, 29851L);
  EXPECT_EQ(capturedTradeEvent.takerAction, OrderAction::BID);
  EXPECT_TRUE(capturedTradeEvent.takeOrderCompleted);

  ASSERT_EQ(capturedTradeEvent.trades.size(), 2u);

  const Trade& trade1 = capturedTradeEvent.trades[0];
  EXPECT_EQ(trade1.makerOrderId, 276810L);
  EXPECT_EQ(trade1.makerUid, 10332L);
  EXPECT_TRUE(trade1.makerOrderCompleted);
  EXPECT_EQ(trade1.price, 20100L);
  EXPECT_EQ(trade1.volume, 8272L);

  const Trade& trade2 = capturedTradeEvent.trades[1];
  EXPECT_EQ(trade2.makerOrderId, 100293L);
  EXPECT_EQ(trade2.makerUid, 1982L);
  EXPECT_FALSE(trade2.makerOrderCompleted);
  EXPECT_EQ(trade2.price, 20110L);
  EXPECT_EQ(trade2.volume, 3121L);

  delete firstTrade;
  delete secondTrade;
  delete reject;
}

TEST_F(SimpleEventsProcessorTest, ShouldHandlerWithSingleReject) {
  OrderCommand cmd = SamplePlaceOrderCommand();

  MatcherTradeEvent* reject =
    CreateMatcherTradeEvent(MatcherEventType::REJECT, true, 0, 0, false, 52201L, 8272L);
  cmd.matcherEvent = reject;

  RejectEvent capturedRejectEvent{};
  EXPECT_CALL(*mockHandler_, CommandResult(_)).WillOnce([=](const ApiCommandResult& result) {
    // Verify immediately since SimpleEventsProcessor deletes the command
    ASSERT_NE(result.command, nullptr);
    const auto* placeOrder = dynamic_cast<const ApiPlaceOrder*>(result.command);
    ASSERT_NE(placeOrder, nullptr);
    EXPECT_EQ(placeOrder->orderId, 123L);
    EXPECT_EQ(placeOrder->symbol, 3);
    EXPECT_EQ(placeOrder->price, 52200L);
    EXPECT_EQ(placeOrder->size, 3200L);
    EXPECT_EQ(placeOrder->reservePrice, 12800L);
    EXPECT_EQ(placeOrder->action, OrderAction::BID);
    EXPECT_EQ(placeOrder->orderType, OrderType::IOC);
    EXPECT_EQ(placeOrder->uid, 29851L);
    EXPECT_EQ(placeOrder->userCookie, 44188);
  });
  EXPECT_CALL(*mockHandler_, TradeEvent(_)).Times(0);
  EXPECT_CALL(*mockHandler_, ReduceEvent(_)).Times(0);
  EXPECT_CALL(*mockHandler_, RejectEvent(_)).WillOnce(SaveArg<0>(&capturedRejectEvent));

  processor_->Accept(&cmd, 192837L);

  EXPECT_EQ(capturedRejectEvent.symbol, 3);
  EXPECT_EQ(capturedRejectEvent.orderId, 123L);
  EXPECT_EQ(capturedRejectEvent.rejectedVolume, 8272L);
  EXPECT_EQ(capturedRejectEvent.price, 52201L);
  EXPECT_EQ(capturedRejectEvent.uid, 29851L);

  delete reject;
}
