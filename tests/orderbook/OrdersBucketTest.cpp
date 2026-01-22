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

#include "OrdersBucketTest.h"
#include <exchange/core/common/MatcherTradeEvent.h>
#include <algorithm>
#include <random>

using namespace exchange::core::common;
using namespace exchange::core::common::cmd;
using namespace exchange::core::orderbook;
using namespace exchange::core::tests::orderbook;

namespace exchange {
namespace core {
namespace tests {
namespace orderbook {

void OrdersBucketTest::SetUp() {
  eventsHelper_ = std::make_unique<OrderBookEventsHelper>([]() { return new MatcherTradeEvent(); });

  bucket_ = std::make_unique<OrdersBucket>(PRICE);

  auto order1 = CreateOrder(1, UID_1, 100);
  bucket_->Put(order1);
  ASSERT_EQ(bucket_->GetNumOrders(), 1);
  ASSERT_EQ(bucket_->GetTotalVolume(), 100L);

  bucket_->Validate();

  auto order2 = CreateOrder(2, UID_2, 40);
  bucket_->Put(order2);
  ASSERT_EQ(bucket_->GetNumOrders(), 2);
  ASSERT_EQ(bucket_->GetTotalVolume(), 140L);

  bucket_->Validate();

  auto order3 = CreateOrder(3, UID_1, 1);
  bucket_->Put(order3);
  ASSERT_EQ(bucket_->GetNumOrders(), 3);
  ASSERT_EQ(bucket_->GetTotalVolume(), 141L);

  bucket_->Validate();

  auto removed = bucket_->Remove(2, UID_2);
  ASSERT_NE(removed, nullptr);
  delete removed;
  ASSERT_EQ(bucket_->GetNumOrders(), 2);
  ASSERT_EQ(bucket_->GetTotalVolume(), 101L);

  bucket_->Validate();

  auto order4 = CreateOrder(4, UID_1, 200);
  bucket_->Put(order4);
  ASSERT_EQ(bucket_->GetNumOrders(), 3);
  ASSERT_EQ(bucket_->GetTotalVolume(), 301L);
}

void OrdersBucketTest::TearDown() {
  // OrdersBucket::~OrdersBucket() is responsible for deleting all orders
  // it contains, so we only need to reset the bucket here.
  // DO NOT manually delete orders - that causes double-free!
  bucket_.reset();
  eventsHelper_.reset();
}

exchange::core::common::Order*
OrdersBucketTest::CreateOrder(int64_t orderId, int64_t uid, int64_t size) {
  return new Order(orderId, PRICE, size, 0, 0, OrderAction::ASK, uid, 0);
}

std::vector<exchange::core::common::MatcherTradeEvent*>
OrdersBucketTest::EventChainToList(exchange::core::common::MatcherTradeEvent* head) {
  return MatcherTradeEvent::AsList(head);
}

TEST_F(OrdersBucketTest, ShouldAddOrder) {
  auto order5 = CreateOrder(5, UID_2, 240);
  bucket_->Put(order5);

  ASSERT_EQ(bucket_->GetNumOrders(), 4);
  ASSERT_EQ(bucket_->GetTotalVolume(), 541L);
}

TEST_F(OrdersBucketTest, ShouldRemoveOrders) {
  auto removed = bucket_->Remove(1, UID_1);
  ASSERT_NE(removed, nullptr);
  delete removed;
  ASSERT_EQ(bucket_->GetNumOrders(), 2);
  ASSERT_EQ(bucket_->GetTotalVolume(), 201L);

  removed = bucket_->Remove(4, UID_1);
  ASSERT_NE(removed, nullptr);
  delete removed;
  ASSERT_EQ(bucket_->GetNumOrders(), 1);
  ASSERT_EQ(bucket_->GetTotalVolume(), 1L);

  // can not remove existing order
  removed = bucket_->Remove(4, UID_1);
  ASSERT_EQ(removed, nullptr);
  ASSERT_EQ(bucket_->GetNumOrders(), 1);
  ASSERT_EQ(bucket_->GetTotalVolume(), 1L);

  removed = bucket_->Remove(3, UID_1);
  ASSERT_NE(removed, nullptr);
  delete removed;
  ASSERT_EQ(bucket_->GetNumOrders(), 0);
  ASSERT_EQ(bucket_->GetTotalVolume(), 0L);
}

TEST_F(OrdersBucketTest, ShouldAddManyOrders) {
  int numOrdersToAdd = 100000;
  int64_t expectedVolume = bucket_->GetTotalVolume();
  int expectedNumOrders = bucket_->GetNumOrders() + numOrdersToAdd;
  for (int i = 0; i < numOrdersToAdd; i++) {
    auto order = CreateOrder(i + 5, UID_2, i);
    bucket_->Put(order);
    expectedVolume += i;
  }

  ASSERT_EQ(bucket_->GetNumOrders(), expectedNumOrders);
  ASSERT_EQ(bucket_->GetTotalVolume(), expectedVolume);
}

TEST_F(OrdersBucketTest, ShouldAddAndRemoveManyOrders) {
  int numOrdersToAdd = 100;
  int64_t expectedVolume = bucket_->GetTotalVolume();
  int expectedNumOrders = bucket_->GetNumOrders() + numOrdersToAdd;

  std::vector<Order*> orders;
  orders.reserve(numOrdersToAdd);
  for (int i = 0; i < numOrdersToAdd; i++) {
    auto order = CreateOrder(i + 5, UID_2, i);
    orders.push_back(order);
    bucket_->Put(order);
    expectedVolume += i;
  }

  ASSERT_EQ(bucket_->GetNumOrders(), expectedNumOrders);
  ASSERT_EQ(bucket_->GetTotalVolume(), expectedVolume);

  std::mt19937 rng(1);
  std::shuffle(orders.begin(), orders.end(), rng);

  for (auto* order : orders) {
    auto removed = bucket_->Remove(order->orderId, UID_2);
    ASSERT_NE(removed, nullptr);
    int64_t orderSize = removed->size;
    delete removed;
    expectedNumOrders--;
    expectedVolume -= orderSize;
    ASSERT_EQ(bucket_->GetNumOrders(), expectedNumOrders);
    ASSERT_EQ(bucket_->GetTotalVolume(), expectedVolume);
  }

  // All orders have been removed and deleted, no cleanup needed
}

TEST_F(OrdersBucketTest, ShouldMatchAllOrders) {
  int numOrdersToAdd = 100;
  int64_t expectedVolume = bucket_->GetTotalVolume();
  int expectedNumOrders = bucket_->GetNumOrders() + numOrdersToAdd;

  int orderId = 5;

  std::vector<Order*> orders;
  orders.reserve(numOrdersToAdd);
  for (int i = 0; i < numOrdersToAdd; i++) {
    auto order = CreateOrder(orderId++, UID_2, i);
    orders.push_back(order);
    bucket_->Put(order);
    expectedVolume += i;
  }

  ASSERT_EQ(bucket_->GetNumOrders(), expectedNumOrders);
  ASSERT_EQ(bucket_->GetTotalVolume(), expectedVolume);

  std::mt19937 rng(1);
  std::shuffle(orders.begin(), orders.end(), rng);

  // Remove first 80 orders
  for (int i = 0; i < 80; i++) {
    Order* order = orders[i];
    auto removed = bucket_->Remove(order->orderId, UID_2);
    ASSERT_NE(removed, nullptr);
    int64_t orderSize = removed->size;
    delete removed;
    expectedNumOrders--;
    expectedVolume -= orderSize;
    ASSERT_EQ(bucket_->GetNumOrders(), expectedNumOrders);
    ASSERT_EQ(bucket_->GetTotalVolume(), expectedVolume);
  }

  auto triggerOrd = OrderCommand::Update(8182, UID_9, 1000);
  auto matcherResult = bucket_->Match(expectedVolume, &triggerOrd, eventsHelper_.get());

  auto events = EventChainToList(matcherResult.eventsChainHead);
  ASSERT_EQ(events.size(), static_cast<size_t>(expectedNumOrders));

  ASSERT_EQ(bucket_->GetNumOrders(), 0);
  ASSERT_EQ(bucket_->GetTotalVolume(), 0L);

  // NOTE: Orders that were fully matched are already deleted by Match().
  // Orders that were removed via Remove() were already deleted above.
  // Orders still in bucket (partially matched) will be deleted by TearDown().
  // DO NOT manually delete any orders here - it causes double-free!
}

TEST_F(OrdersBucketTest, ShouldMatchAllOrders2) {
  int numOrdersToAdd = 1000;
  int64_t expectedVolume = bucket_->GetTotalVolume();
  int expectedNumOrders = bucket_->GetNumOrders();

  bucket_->Validate();
  int orderId = 5;

  for (int j = 0; j < 100; j++) {
    std::vector<Order*> orders;
    orders.reserve(numOrdersToAdd);
    for (int i = 0; i < numOrdersToAdd; i++) {
      auto order = CreateOrder(orderId++, UID_2, i);
      orders.push_back(order);

      bucket_->Put(order);
      expectedNumOrders++;
      expectedVolume += i;

      bucket_->Validate();
    }

    ASSERT_EQ(bucket_->GetNumOrders(), expectedNumOrders);
    ASSERT_EQ(bucket_->GetTotalVolume(), expectedVolume);

    std::mt19937 rng(1);
    std::shuffle(orders.begin(), orders.end(), rng);

    // Remove first 900 orders
    for (int i = 0; i < 900; i++) {
      Order* order = orders[i];
      auto removed = bucket_->Remove(order->orderId, UID_2);
      ASSERT_NE(removed, nullptr);
      int64_t orderSize = removed->size;
      delete removed;
      expectedNumOrders--;
      expectedVolume -= orderSize;
      ASSERT_EQ(bucket_->GetNumOrders(), expectedNumOrders);
      ASSERT_EQ(bucket_->GetTotalVolume(), expectedVolume);

      bucket_->Validate();
    }

    int64_t toMatch = expectedVolume / 2;

    auto triggerOrd = OrderCommand::Update(119283900, UID_9, 1000);

    auto matcherResult = bucket_->Match(toMatch, &triggerOrd, eventsHelper_.get());
    int64_t totalVolume = matcherResult.volume;
    ASSERT_EQ(totalVolume, toMatch);
    expectedVolume -= totalVolume;
    ASSERT_EQ(bucket_->GetTotalVolume(), expectedVolume);
    expectedNumOrders = bucket_->GetNumOrders();

    bucket_->Validate();

    // NOTE: Orders that were fully matched are already deleted by Match().
    // Orders that were removed via Remove() were already deleted above.
    // Orders still in bucket (partially matched) will be deleted by TearDown().
    // DO NOT manually delete any orders here - it causes double-free!
  }

  auto triggerOrd = OrderCommand::Update(1238729387, UID_9, 1000);

  auto matcherResult = bucket_->Match(expectedVolume, &triggerOrd, eventsHelper_.get());

  auto events = EventChainToList(matcherResult.eventsChainHead);
  ASSERT_EQ(events.size(), static_cast<size_t>(expectedNumOrders));

  ASSERT_EQ(bucket_->GetNumOrders(), 0);
  ASSERT_EQ(bucket_->GetTotalVolume(), 0L);
}

}  // namespace orderbook
}  // namespace tests
}  // namespace core
}  // namespace exchange
