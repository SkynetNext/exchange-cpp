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

#include "ITExchangeCoreIntegrationRejection.h"
#include "../util/ExchangeTestContainer.h"
#include "../util/TestConstants.h"
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/cmd/CommandResultCode.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace exchange::core::tests::util;
using namespace exchange::core::common;
using namespace exchange::core::common::cmd;

namespace exchange {
namespace core {
namespace tests {
namespace integration {

using namespace exchange::core::common;
using namespace exchange::core::common::cmd;
using namespace exchange::core::tests::util;

ITExchangeCoreIntegrationRejection::ITExchangeCoreIntegrationRejection()
    : processor_(std::make_unique<exchange::core::SimpleEventsProcessor>(
          &mockHandler_)) {}

void ITExchangeCoreIntegrationRejection::SetUp() {
  processor_ =
      std::make_unique<exchange::core::SimpleEventsProcessor>(&mockHandler_);
}

exchange::core::common::api::ApiPlaceOrder
ITExchangeCoreIntegrationRejection::BuilderPlace(
    int32_t symbolId, int64_t uid, exchange::core::common::OrderAction action,
    exchange::core::common::OrderType type) {
  // Helper method to create ApiPlaceOrder builder pattern equivalent
  // Returns a placeholder order - actual values should be set by caller
  return exchange::core::common::api::ApiPlaceOrder(0, 0, 0, action, type, uid,
                                                    symbolId, 0, 0);
}

void ITExchangeCoreIntegrationRejection::TestMultiBuy(
    const exchange::core::common::CoreSymbolSpecification &symbolSpec,
    exchange::core::common::OrderType orderType,
    RejectionCause rejectionCause) {
  const int32_t symbolId = symbolSpec.symbolId;
  const int64_t size =
      40L + (rejectionCause == RejectionCause::REJECTION_BY_SIZE ? 1 : 0);

  // Set up mock expectations BEFORE executing commands
  using ::testing::_;

  EXPECT_CALL(mockHandler_, CommandResult(_)).Times(5);
  EXPECT_CALL(mockHandler_, ReduceEvent(_)).Times(0);
  // OrderBook may be called if market data is generated, allow any number of
  // calls
  EXPECT_CALL(mockHandler_, OrderBook(_)).Times(::testing::AnyNumber());

  if (orderType == OrderType::FOK_BUDGET &&
      rejectionCause != RejectionCause::NO_REJECTION) {
    // no trades for FoK with rejection
    EXPECT_CALL(mockHandler_, TradeEvent(_)).Times(0);
  } else {
    // Verify trade event - use WillOnce with lambda to verify struct members
    EXPECT_CALL(mockHandler_, TradeEvent(_))
        .WillOnce(
            [symbolId, rejectionCause](const exchange::core::TradeEvent &evt) {
              EXPECT_EQ(evt.symbol, symbolId);
              EXPECT_EQ(evt.totalVolume, 40L);
              EXPECT_EQ(evt.takerOrderId, 405L);
              EXPECT_EQ(evt.takerUid, TestConstants::UID_4);
              EXPECT_EQ(evt.takerAction, OrderAction::BID);
              EXPECT_EQ(evt.takeOrderCompleted,
                        rejectionCause == RejectionCause::NO_REJECTION);
            });
  }

  if (rejectionCause != RejectionCause::NO_REJECTION &&
      orderType != OrderType::GTC) {
    // Verify reject event - use WillOnce with lambda to verify struct members
    int64_t expectedRejectedVolume =
        (orderType == OrderType::FOK_BUDGET) ? size : 1L;
    EXPECT_CALL(mockHandler_, RejectEvent(_))
        .WillOnce([symbolId, expectedRejectedVolume](
                      const exchange::core::RejectEvent &evt) {
          EXPECT_EQ(evt.symbol, symbolId);
          EXPECT_EQ(evt.rejectedVolume, expectedRejectedVolume);
          EXPECT_EQ(evt.orderId, 405L);
          EXPECT_EQ(evt.uid, TestConstants::UID_4);
        });
  } else {
    EXPECT_CALL(mockHandler_, RejectEvent(_)).Times(0);
  }

  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  container->InitFeeSymbols();
  container->InitFeeUsers();

  // Set consumer to processor
  container->SetConsumer(
      [this](exchange::core::common::cmd::OrderCommand *cmd, int64_t seq) {
        if (processor_) {
          processor_->Accept(cmd, seq);
        }
      });

  // Submit multiple ASK orders
  auto order101 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
      160000L, 7L, 101L, OrderAction::ASK, OrderType::GTC, TestConstants::UID_1,
      symbolId, 0, 0);
  container->SubmitCommandSync(std::move(order101), CommandResultCode::SUCCESS);

  auto order202 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
      159900L, 10L, 202L, OrderAction::ASK, OrderType::GTC,
      TestConstants::UID_2, symbolId, 0, 0);
  container->SubmitCommandSync(std::move(order202), CommandResultCode::SUCCESS);

  auto order303 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
      160000L, 3L, 303L, OrderAction::ASK, OrderType::GTC, TestConstants::UID_3,
      symbolId, 0, 0);
  container->SubmitCommandSync(std::move(order303), CommandResultCode::SUCCESS);

  auto order304 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
      160500L, 20L, 304L, OrderAction::ASK, OrderType::GTC,
      TestConstants::UID_3, symbolId, 0, 0);
  container->SubmitCommandSync(std::move(order304), CommandResultCode::SUCCESS);

  // Calculate price based on order type
  int64_t price = 160500L;
  if (orderType == OrderType::FOK_BUDGET) {
    price = 160000L * 7L + 159900L * 10L + 160000L * 3L + 160500L * 20L +
            (rejectionCause == RejectionCause::REJECTION_BY_BUDGET ? -1 : 0);
  }

  // Submit BID order
  auto order405 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
      price, size, 405L, OrderAction::BID, orderType, TestConstants::UID_4,
      symbolId, 0, price);
  container->SubmitCommandSync(std::move(order405), CommandResultCode::SUCCESS);

  auto totalBal = container->TotalBalanceReport();
  ASSERT_NE(totalBal, nullptr);
  EXPECT_TRUE(totalBal->IsGlobalBalancesAllZero());
}

void ITExchangeCoreIntegrationRejection::TestMultiSell(
    const exchange::core::common::CoreSymbolSpecification &symbolSpec,
    exchange::core::common::OrderType orderType,
    RejectionCause rejectionCause) {
  const int32_t symbolId = symbolSpec.symbolId;
  const int64_t size =
      22L + (rejectionCause == RejectionCause::REJECTION_BY_SIZE ? 1 : 0);

  // Set up mock expectations BEFORE executing commands
  using ::testing::_;

  EXPECT_CALL(mockHandler_, CommandResult(_)).Times(5);
  EXPECT_CALL(mockHandler_, ReduceEvent(_)).Times(0);
  // OrderBook may be called if market data is generated, allow any number of
  // calls
  EXPECT_CALL(mockHandler_, OrderBook(_)).Times(::testing::AnyNumber());

  if (orderType == OrderType::FOK_BUDGET &&
      rejectionCause != RejectionCause::NO_REJECTION) {
    // no trades for FoK with rejection
    EXPECT_CALL(mockHandler_, TradeEvent(_)).Times(0);
  } else {
    // Verify trade event - use WillOnce with lambda to verify struct members
    EXPECT_CALL(mockHandler_, TradeEvent(_))
        .WillOnce(
            [symbolId, rejectionCause](const exchange::core::TradeEvent &evt) {
              EXPECT_EQ(evt.symbol, symbolId);
              EXPECT_EQ(evt.totalVolume, 22L);
              EXPECT_EQ(evt.takerOrderId, 405L);
              EXPECT_EQ(evt.takerUid, TestConstants::UID_4);
              EXPECT_EQ(evt.takerAction, OrderAction::ASK);
              EXPECT_EQ(evt.takeOrderCompleted,
                        rejectionCause == RejectionCause::NO_REJECTION);
            });
  }

  if (rejectionCause != RejectionCause::NO_REJECTION &&
      orderType != OrderType::GTC) {
    // Verify reject event - use WillOnce with lambda to verify struct members
    int64_t expectedRejectedVolume =
        (orderType == OrderType::FOK_BUDGET) ? size : 1L;
    EXPECT_CALL(mockHandler_, RejectEvent(_))
        .WillOnce([symbolId, expectedRejectedVolume](
                      const exchange::core::RejectEvent &evt) {
          EXPECT_EQ(evt.symbol, symbolId);
          EXPECT_EQ(evt.rejectedVolume, expectedRejectedVolume);
          EXPECT_EQ(evt.orderId, 405L);
          EXPECT_EQ(evt.uid, TestConstants::UID_4);
        });
  } else {
    EXPECT_CALL(mockHandler_, RejectEvent(_)).Times(0);
  }

  auto container = ExchangeTestContainer::Create(GetPerformanceConfiguration());
  container->InitFeeSymbols();
  container->InitFeeUsers();

  // Set consumer to processor
  container->SetConsumer(
      [this](exchange::core::common::cmd::OrderCommand *cmd, int64_t seq) {
        if (processor_) {
          processor_->Accept(cmd, seq);
        }
      });

  // Calculate price based on order type
  int64_t price = 159900L;
  if (orderType == OrderType::FOK_BUDGET) {
    price = 160500L + 160000L * 20L + 159900L +
            (rejectionCause == RejectionCause::REJECTION_BY_BUDGET ? 1 : 0);
  }

  // Submit multiple BID orders
  auto order101 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
      160000L, 12L, 101L, OrderAction::BID, OrderType::GTC,
      TestConstants::UID_1, symbolId, 0, 166000L);
  container->SubmitCommandSync(std::move(order101), CommandResultCode::SUCCESS);

  auto order202 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
      159900L, 1L, 202L, OrderAction::BID, OrderType::GTC, TestConstants::UID_2,
      symbolId, 0, 166000L);
  container->SubmitCommandSync(std::move(order202), CommandResultCode::SUCCESS);

  auto order303 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
      160000L, 8L, 303L, OrderAction::BID, OrderType::GTC, TestConstants::UID_3,
      symbolId, 0, 166000L);
  container->SubmitCommandSync(std::move(order303), CommandResultCode::SUCCESS);

  auto order304 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
      160500L, 1L, 304L, OrderAction::BID, OrderType::GTC, TestConstants::UID_3,
      symbolId, 0, 166000L);
  container->SubmitCommandSync(std::move(order304), CommandResultCode::SUCCESS);

  // Submit ASK order
  auto order405 = std::make_unique<exchange::core::common::api::ApiPlaceOrder>(
      price, size, 405L, OrderAction::ASK, orderType, TestConstants::UID_4,
      symbolId, 0, 0);
  container->SubmitCommandSync(std::move(order405), CommandResultCode::SUCCESS);

  auto totalBal = container->TotalBalanceReport();
  ASSERT_NE(totalBal, nullptr);
  EXPECT_TRUE(totalBal->IsGlobalBalancesAllZero());
}

} // namespace integration
} // namespace tests
} // namespace core
} // namespace exchange
