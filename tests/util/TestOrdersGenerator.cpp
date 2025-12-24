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

#include "TestOrdersGenerator.h"
#include "TestConstants.h"
#include "TestOrdersGeneratorSession.h"
#include <algorithm>
#include <cmath>
#include <exchange/core/common/MatcherEventType.h>
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/config/LoggingConfiguration.h>
#include <exchange/core/orderbook/IOrderBook.h>
#include <exchange/core/orderbook/OrderBookNaiveImpl.h>
#include <random>
#include <unordered_map>

using namespace exchange::core::common;
using namespace exchange::core::common::cmd;
using namespace exchange::core::orderbook;

namespace exchange {
namespace core2 {
namespace tests {
namespace util {

// Constants matching Java implementation
static constexpr double CENTRAL_MOVE_ALPHA = 0.01;
static constexpr int32_t CHECK_ORDERBOOK_STAT_EVERY_NTH_COMMAND = 512;

// UID mapper: i -> i + 1
std::function<int32_t(int32_t)> TestOrdersGenerator::UID_PLAIN_MAPPER =
    [](int32_t i) { return i + 1; };

// Store combined commands for GetCommands() to return reference
// Using thread_local to avoid copying OrderCommand (which contains unique_ptr)
static thread_local std::vector<OrderCommand> g_combinedCommands;

std::vector<OrderCommand> &TestOrdersGenerator::GenResult::GetCommands() const {
  g_combinedCommands.clear();
  g_combinedCommands.reserve(commandsFill.size() + commandsBenchmark.size());

  // Use Copy() method to create copies of OrderCommand (which contains
  // unique_ptr)
  for (const auto &cmd : commandsFill) {
    g_combinedCommands.push_back(cmd.Copy());
  }
  for (const auto &cmd : commandsBenchmark) {
    g_combinedCommands.push_back(cmd.Copy());
  }

  return g_combinedCommands;
}

size_t TestOrdersGenerator::GenResult::Size() const {
  return commandsFill.size() + commandsBenchmark.size();
}

std::function<void(int64_t)>
TestOrdersGenerator::CreateAsyncProgressLogger(int64_t total) {
  // Simple no-op progress logger for now
  return [](int64_t) {};
}

// Forward declarations for helper functions
static void MatcherTradeEventEventHandler(TestOrdersGeneratorSession *session,
                                          const MatcherTradeEvent *ev,
                                          const OrderCommand *orderCommand);
static void UpdateOrderBookSizeStat(TestOrdersGeneratorSession *session);
static OrderCommand GenerateRandomGtcOrder(TestOrdersGeneratorSession *session);
static OrderCommand GenerateRandomOrder(TestOrdersGeneratorSession *session);
static OrderCommand
GenerateRandomInstantOrder(TestOrdersGeneratorSession *session);

// Implementation of helper functions
static void MatcherTradeEventEventHandler(TestOrdersGeneratorSession *session,
                                          const MatcherTradeEvent *ev,
                                          const OrderCommand *orderCommand) {
  if (!session || !ev || !orderCommand) {
    return; // Safety check
  }

  int32_t activeOrderId = static_cast<int32_t>(orderCommand->orderId);

  if (ev->eventType == MatcherEventType::TRADE) {
    if (ev->activeOrderCompleted) {
      session->numCompleted++;
    }
    if (ev->matchedOrderCompleted) {
      session->orderUids.erase(static_cast<int32_t>(ev->matchedOrderId));
      session->numCompleted++;
    }

    // decrease size (important for reduce operation)
    auto it =
        session->orderSizes.find(static_cast<int32_t>(ev->matchedOrderId));
    if (it != session->orderSizes.end()) {
      it->second -= static_cast<int32_t>(ev->size);
      if (it->second < 0) {
        throw std::runtime_error("Negative order size");
      }
    }

    session->lastTradePrice =
        std::min(session->maxPrice, std::max(session->minPrice, ev->price));

    if (ev->price <= session->minPrice) {
      session->priceDirection = 1;
    } else if (ev->price >= session->maxPrice) {
      session->priceDirection = -1;
    }

  } else if (ev->eventType == MatcherEventType::REJECT) {
    session->numRejected++;
    // update order book stat if order get rejected
    // that will trigger generator to issue more limit orders
    UpdateOrderBookSizeStat(session);

  } else if (ev->eventType == MatcherEventType::REDUCE) {
    session->numReduced++;
  } else {
    return;
  }

  // decrease size (important for reduce operation)
  auto it = session->orderSizes.find(activeOrderId);
  if (it != session->orderSizes.end()) {
    it->second -= static_cast<int32_t>(ev->size);
    if (it->second < 0) {
      throw std::runtime_error("Incorrect filled size for order " +
                               std::to_string(activeOrderId));
    }
  }

  if (ev->activeOrderCompleted) {
    session->orderUids.erase(activeOrderId);
  }
}

static void UpdateOrderBookSizeStat(TestOrdersGeneratorSession *session) {
  int32_t ordersNumAsk = session->orderBook->GetOrdersNum(OrderAction::ASK);
  int32_t ordersNumBid = session->orderBook->GetOrdersNum(OrderAction::BID);

  // regulating OB size
  session->lastOrderBookOrdersSizeAsk = ordersNumAsk;
  session->lastOrderBookOrdersSizeBid = ordersNumBid;

  if (session->initialOrdersPlaced || session->avalancheIOC) {
    auto l2MarketDataSnapshot =
        session->orderBook->GetL2MarketDataSnapshot(INT32_MAX);

    if (session->avalancheIOC) {
      session->lastTotalVolumeAsk =
          l2MarketDataSnapshot->TotalOrderBookVolumeAsk();
      session->lastTotalVolumeBid =
          l2MarketDataSnapshot->TotalOrderBookVolumeBid();
    }

    if (session->initialOrdersPlaced) {
      session->orderBookSizeAskStat.push_back(l2MarketDataSnapshot->askSize);
      session->orderBookSizeBidStat.push_back(l2MarketDataSnapshot->bidSize);
      session->orderBookNumOrdersAskStat.push_back(ordersNumAsk);
      session->orderBookNumOrdersBidStat.push_back(ordersNumBid);
    }
  }
}

static OrderCommand
GenerateRandomGtcOrder(TestOrdersGeneratorSession *session) {
  std::uniform_int_distribution<int> dist(0, 3);
  OrderAction action = (dist(session->rand) + session->priceDirection >= 2)
                           ? OrderAction::BID
                           : OrderAction::ASK;

  std::uniform_int_distribution<int> userDist(0, session->numUsers - 1);
  int32_t uid = session->uidMapper(userDist(session->rand));
  int32_t newOrderId = session->seq;

  std::uniform_real_distribution<double> doubleDist(0.0, 1.0);
  int32_t dev =
      1 + static_cast<int32_t>(std::pow(doubleDist(session->rand), 2) *
                               session->priceDeviation);
  if (dev <= 0) {
    dev = 1; // Ensure dev is at least 1
  }

  int64_t p = 0;
  const int x = 4;
  std::uniform_int_distribution<int> devDist(0, dev - 1);
  for (int i = 0; i < x; i++) {
    p += devDist(session->rand);
  }
  p = p / x * 2 - dev;
  if ((p > 0) ^ (action == OrderAction::ASK)) {
    p = -p;
  }

  int64_t price = session->lastTradePrice + p;

  std::uniform_int_distribution<int> sizeDist1(0, 5);
  std::uniform_int_distribution<int> sizeDist2(0, 5);
  std::uniform_int_distribution<int> sizeDist3(0, 5);
  int32_t size = 1 + sizeDist1(session->rand) * sizeDist2(session->rand) *
                         sizeDist3(session->rand);

  session->orderPrices[newOrderId] = price;
  session->orderSizes[newOrderId] = size;
  session->orderUids[newOrderId] = uid;
  session->counterPlaceLimit++;
  session->seq++;

  return OrderCommand::NewOrder(
      OrderType::GTC, newOrderId, uid, price,
      action == OrderAction::BID ? session->maxPrice : 0, size, action);
}

static OrderCommand
GenerateRandomInstantOrder(TestOrdersGeneratorSession *session) {
  std::uniform_int_distribution<int> dist(0, 3);
  OrderAction action = (dist(session->rand) + session->priceDirection >= 2)
                           ? OrderAction::BID
                           : OrderAction::ASK;

  std::uniform_int_distribution<int> userDist(0, session->numUsers - 1);
  int32_t uid = session->uidMapper(userDist(session->rand));

  int32_t newOrderId = session->seq;

  int64_t priceLimit =
      action == OrderAction::BID ? session->maxPrice : session->minPrice;

  int64_t size;
  OrderType orderType;
  int64_t priceOrBudget;
  int64_t reserveBidPrice;

  if (session->avalancheIOC) {
    // just match with available liquidity
    orderType = OrderType::IOC;
    priceOrBudget = priceLimit;
    reserveBidPrice = action == OrderAction::BID ? session->maxPrice : 0;
    int64_t availableVolume = action == OrderAction::ASK
                                  ? session->lastTotalVolumeAsk
                                  : session->lastTotalVolumeBid;

    // Ensure availableVolume is non-negative
    if (availableVolume < 0) {
      availableVolume = 0;
    }
    std::uniform_int_distribution<int64_t> bigRandDist(0, availableVolume);
    size = 1 + bigRandDist(session->rand);

    if (action == OrderAction::ASK) {
      session->lastTotalVolumeAsk =
          std::max(session->lastTotalVolumeAsk - size, 0LL);
    } else {
      session->lastTotalVolumeBid =
          std::max(session->lastTotalVolumeBid - size, 0LL);
    }

  } else {
    std::uniform_int_distribution<int> fokDist(0, 31);
    if (fokDist(session->rand) == 0) {
      // IOC:FOKB = 31:1
      orderType = OrderType::FOK_BUDGET;
      std::uniform_int_distribution<int> sizeDist1(0, 7);
      std::uniform_int_distribution<int> sizeDist2(0, 7);
      std::uniform_int_distribution<int> sizeDist3(0, 7);
      size = 1 + sizeDist1(session->rand) * sizeDist2(session->rand) *
                     sizeDist3(session->rand);
      priceOrBudget = size * priceLimit;
      reserveBidPrice = priceOrBudget;
    } else {
      orderType = OrderType::IOC;
      priceOrBudget = priceLimit;
      reserveBidPrice = action == OrderAction::BID ? session->maxPrice : 0;
      std::uniform_int_distribution<int> sizeDist1(0, 5);
      std::uniform_int_distribution<int> sizeDist2(0, 5);
      std::uniform_int_distribution<int> sizeDist3(0, 5);
      size = 1 + sizeDist1(session->rand) * sizeDist2(session->rand) *
                     sizeDist3(session->rand);
    }
  }

  session->orderSizes[newOrderId] = static_cast<int32_t>(size);
  session->counterPlaceMarket++;
  session->seq++;

  return OrderCommand::NewOrder(orderType, newOrderId, uid, priceOrBudget,
                                reserveBidPrice, size, action);
}

static OrderCommand GenerateRandomOrder(TestOrdersGeneratorSession *session) {
  // TODO move to lastOrderBookOrdersSize writer method
  int32_t lackOfOrdersAsk =
      session->targetOrderBookOrdersHalf - session->lastOrderBookOrdersSizeAsk;
  int32_t lackOfOrdersBid =
      session->targetOrderBookOrdersHalf - session->lastOrderBookOrdersSizeBid;

  if (!session->initialOrdersPlaced && lackOfOrdersAsk <= 0 &&
      lackOfOrdersBid <= 0) {
    session->initialOrdersPlaced = true;

    session->counterPlaceMarket = 0;
    session->counterPlaceLimit = 0;
    session->counterCancel = 0;
    session->counterMove = 0;
    session->counterReduce = 0;
  }

  std::uniform_int_distribution<int> actionDist(0, 3);
  OrderAction action =
      (actionDist(session->rand) + session->priceDirection >= 2)
          ? OrderAction::BID
          : OrderAction::ASK;

  int32_t lackOfOrders =
      (action == OrderAction::ASK) ? lackOfOrdersAsk : lackOfOrdersBid;

  bool requireFastFill = session->filledAtSeq == -1 ||
                         lackOfOrders > session->lackOrOrdersFastFillThreshold;

  bool growOrders = lackOfOrders > 0;

  if (session->filledAtSeq == -1 && !growOrders) {
    session->filledAtSeq = session->seq;
  }

  int32_t q;
  if (growOrders) {
    std::uniform_int_distribution<int> qDist(0, requireFastFill ? 1 : 9);
    q = qDist(session->rand);
  } else {
    std::uniform_int_distribution<int> qDist(0, 39);
    q = qDist(session->rand);
  }

  if (q < 2 || session->orderUids.empty()) {
    if (growOrders) {
      return GenerateRandomGtcOrder(session);
    } else {
      return GenerateRandomInstantOrder(session);
    }
  }

  // TODO improve random picking performance
  int32_t size = std::min(static_cast<int32_t>(session->orderUids.size()), 512);
  if (size <= 0) {
    // Fallback: generate new order if map is empty
    if (growOrders) {
      return GenerateRandomGtcOrder(session);
    } else {
      return GenerateRandomInstantOrder(session);
    }
  }

  std::uniform_int_distribution<int32_t> randPosDist(0, size - 1);
  int32_t randPos = randPosDist(session->rand);

  // Safely advance iterator
  auto it = session->orderUids.begin();
  for (int32_t i = 0; i < randPos && it != session->orderUids.end(); ++i) {
    ++it;
  }

  if (it == session->orderUids.end()) {
    // Fallback if iterator went out of bounds
    if (growOrders) {
      return GenerateRandomGtcOrder(session);
    } else {
      return GenerateRandomInstantOrder(session);
    }
  }

  int32_t orderId = it->first;
  int32_t uid = it->second;

  if (uid == 0) {
    throw std::runtime_error("Invalid uid");
  }

  if (q == 2) {
    session->orderUids.erase(orderId);
    session->counterCancel++;
    return OrderCommand::Cancel(orderId, uid);

  } else if (q == 3) {
    session->counterReduce++;

    auto sizeIt = session->orderSizes.find(orderId);
    if (sizeIt == session->orderSizes.end()) {
      throw std::runtime_error("Order size not found");
    }
    int32_t prevSize = sizeIt->second;
    std::uniform_int_distribution<int32_t> reduceDist(1, prevSize);
    int32_t reduceBy = reduceDist(session->rand);
    return OrderCommand::Reduce(orderId, uid, reduceBy);

  } else {
    auto priceIt = session->orderPrices.find(orderId);
    if (priceIt == session->orderPrices.end() || priceIt->second == 0) {
      throw std::runtime_error("Order price not found");
    }
    int64_t prevPrice = priceIt->second;

    double priceMove =
        (session->lastTradePrice - prevPrice) * CENTRAL_MOVE_ALPHA;
    int64_t priceMoveRounded;
    if (prevPrice > session->lastTradePrice) {
      priceMoveRounded = static_cast<int64_t>(std::floor(priceMove));
    } else if (prevPrice < session->lastTradePrice) {
      priceMoveRounded = static_cast<int64_t>(std::ceil(priceMove));
    } else {
      std::uniform_int_distribution<int> dirDist(0, 1);
      priceMoveRounded = dirDist(session->rand) * 2 - 1;
    }

    int64_t newPrice =
        std::min(prevPrice + priceMoveRounded, session->maxPrice);

    session->counterMove++;
    session->orderPrices[orderId] = newPrice;

    return OrderCommand::Update(orderId, uid, newPrice);
  }
}

// Main GenerateCommands implementation
TestOrdersGenerator::GenResult TestOrdersGenerator::GenerateCommands(
    int benchmarkTransactionsNumber, int targetOrderBookOrders, int numUsers,
    std::function<int32_t(int32_t)> uidMapper, int32_t symbol,
    bool enableSlidingPrice, bool avalancheIOC,
    std::function<void(int64_t)> asyncProgressConsumer, int seed) {

  GenResult result;
  result.commandsFill.reserve(targetOrderBookOrders);
  result.commandsBenchmark.reserve(benchmarkTransactionsNumber);

  // Create a reference orderbook to simulate order generation
  auto symbolSpec = TestConstants::CreateSymbolSpecEurUsd();
  std::unique_ptr<IOrderBook> orderBook =
      std::make_unique<OrderBookNaiveImpl>(&symbolSpec, nullptr, nullptr);

  // Create session
  TestOrdersGeneratorSession session(
      orderBook.get(), benchmarkTransactionsNumber,
      targetOrderBookOrders / 2, // asks + bids
      avalancheIOC, numUsers, uidMapper, symbol, enableSlidingPrice, seed);

  int32_t nextSizeCheck = std::min(CHECK_ORDERBOOK_STAT_EVERY_NTH_COMMAND,
                                   targetOrderBookOrders + 1);

  int32_t totalCommandsNumber =
      benchmarkTransactionsNumber + targetOrderBookOrders;

  int32_t lastProgressReported = 0;

  for (int32_t i = 0; i < totalCommandsNumber; i++) {
    bool fillInProgress = i < targetOrderBookOrders;

    OrderCommand cmd;

    if (fillInProgress) {
      cmd = GenerateRandomGtcOrder(&session);
      result.commandsFill.push_back(cmd.Copy());
    } else {
      cmd = GenerateRandomOrder(&session);
      result.commandsBenchmark.push_back(cmd.Copy());
    }

    cmd.resultCode = CommandResultCode::VALID_FOR_MATCHING_ENGINE;
    cmd.symbol = session.symbol;

    CommandResultCode resultCode =
        IOrderBook::ProcessCommand(orderBook.get(), &cmd);
    if (resultCode != CommandResultCode::SUCCESS) {
      throw std::runtime_error("Unsuccessful result code: " +
                               std::to_string(static_cast<int>(resultCode)) +
                               " for command");
    }

    // process and cleanup matcher events
    if (cmd.matcherEvent != nullptr) {
      cmd.ProcessMatcherEvents([&session, &cmd](const MatcherTradeEvent *ev) {
        if (ev != nullptr) {
          MatcherTradeEventEventHandler(&session, ev, &cmd);
        }
      });
      cmd.matcherEvent = nullptr;
    }

    if (i >= nextSizeCheck) {
      nextSizeCheck += std::min(CHECK_ORDERBOOK_STAT_EVERY_NTH_COMMAND,
                                targetOrderBookOrders + 1);
      UpdateOrderBookSizeStat(&session);
    }

    if (i % 10000 == 9999) {
      if (asyncProgressConsumer) {
        asyncProgressConsumer(i - lastProgressReported);
      }
      lastProgressReported = i;
    }
  }

  if (asyncProgressConsumer) {
    asyncProgressConsumer(totalCommandsNumber - lastProgressReported);
  }

  UpdateOrderBookSizeStat(&session);

  result.finalOrderBookSnapshot = orderBook->GetL2MarketDataSnapshot(INT32_MAX);
  result.finalOrderbookHash = orderBook->GetStateHash();

  return result;
}

} // namespace util
} // namespace tests
} // namespace core2
} // namespace exchange
