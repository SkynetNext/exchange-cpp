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

#pragma once

#include <exchange/core/common/Order.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/orderbook/OrderBookEventsHelper.h>
#include <exchange/core/orderbook/OrdersBucket.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace exchange {
namespace core {
namespace tests {
namespace orderbook {

class OrdersBucketTest : public ::testing::Test {
protected:
  static constexpr int64_t PRICE = 1000;
  static constexpr int64_t UID_1 = 412;
  static constexpr int64_t UID_2 = 413;
  static constexpr int64_t UID_9 = 419;

  std::unique_ptr<exchange::core::orderbook::OrderBookEventsHelper>
      eventsHelper_;
  std::unique_ptr<exchange::core::orderbook::OrdersBucket> bucket_;

  void SetUp() override;
  void TearDown() override;

  // Helper to create Order
  exchange::core::common::Order *CreateOrder(int64_t orderId, int64_t uid,
                                             int64_t size);

  // Helper to convert MatcherTradeEvent chain to vector
  std::vector<exchange::core::common::MatcherTradeEvent *>
  EventChainToList(exchange::core::common::MatcherTradeEvent *head);
};

} // namespace orderbook
} // namespace tests
} // namespace core
} // namespace exchange
