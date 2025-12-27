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
#include "ExecutionTime.h"
#include "TestConstants.h"
#include "TestOrdersGeneratorConfig.h"
#include "TestOrdersGeneratorSession.h"
#include "UserCurrencyAccountsGenerator.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cmath>
#include <exchange/core/common/MatcherEventType.h>
#include <exchange/core/common/OrderAction.h>
#include <exchange/core/common/OrderType.h>
#include <exchange/core/common/api/ApiCancelOrder.h>
#include <exchange/core/common/api/ApiCommand.h>
#include <exchange/core/common/api/ApiMoveOrder.h>
#include <exchange/core/common/api/ApiPlaceOrder.h>
#include <exchange/core/common/api/ApiReduceOrder.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <exchange/core/common/config/LoggingConfiguration.h>
#include <exchange/core/orderbook/IOrderBook.h>
#include <exchange/core/orderbook/OrderBookNaiveImpl.h>
#include <exchange/core/utils/Logger.h>
#include <future>
#include <memory>
#include <unordered_map>

using namespace exchange::core::common;
using namespace exchange::core::common::cmd;
using namespace exchange::core::orderbook;

namespace exchange {
namespace core {
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
  // Progress logger that logs every 5 seconds (matching Java implementation)
  constexpr int64_t progressLogIntervalNs =
      5'000'000'000LL; // 5 seconds in nanoseconds
  auto nextUpdateTime = std::make_shared<std::atomic<int64_t>>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count() +
      progressLogIntervalNs);
  auto progress = std::make_shared<std::atomic<int64_t>>(0);

  return [total, nextUpdateTime, progress,
          progressLogIntervalNs](int64_t processed) {
    progress->fetch_add(processed);
    int64_t whenLogNext = nextUpdateTime->load();
    const int64_t timeNow =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    if (timeNow > whenLogNext) {
      int64_t expected = whenLogNext;
      if (nextUpdateTime->compare_exchange_strong(
              expected, timeNow + progressLogIntervalNs)) {
        // Whichever thread won - it should print progress
        const int64_t done = progress->load();
        const float progressPercent = (done * 100.0f) / total;
        LOG_DEBUG("Generating commands progress: {:.1f}% done ({} of {})...",
                  progressPercent, done, total);
      }
    }
  };
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
  // Match Java: rand.nextInt(4)
  OrderAction action = (session->rand.nextInt(4) + session->priceDirection >= 2)
                           ? OrderAction::BID
                           : OrderAction::ASK;

  // Match Java: rand.nextInt(session.numUsers)
  int32_t uid = session->uidMapper(session->rand.nextInt(session->numUsers));
  int32_t newOrderId = session->seq;

  // Match Java: Math.pow(rand.nextDouble(), 2)
  int32_t dev =
      1 + static_cast<int32_t>(std::pow(session->rand.nextDouble(), 2) *
                               session->priceDeviation);
  if (dev <= 0) {
    dev = 1; // Ensure dev is at least 1
  }

  int64_t p = 0;
  const int x = 4;
  // Match Java: rand.nextInt(dev)
  for (int i = 0; i < x; i++) {
    p += session->rand.nextInt(dev);
  }
  p = p / x * 2 - dev;
  if ((p > 0) ^ (action == OrderAction::ASK)) {
    p = -p;
  }

  int64_t price = session->lastTradePrice + p;

  // Match Java: rand.nextInt(6) * rand.nextInt(6) * rand.nextInt(6)
  int32_t size = 1 + session->rand.nextInt(6) * session->rand.nextInt(6) *
                         session->rand.nextInt(6);

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
  // Match Java: rand.nextInt(4)
  OrderAction action = (session->rand.nextInt(4) + session->priceDirection >= 2)
                           ? OrderAction::BID
                           : OrderAction::ASK;

  // Match Java: rand.nextInt(session.numUsers)
  int32_t uid = session->uidMapper(session->rand.nextInt(session->numUsers));

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
    // Match Java: bigRand = rand.nextLong(); bigRand = bigRand < 0 ? -1 -
    // bigRand : bigRand;
    int64_t bigRand = session->rand.nextLong();
    bigRand = bigRand < 0 ? -1 - bigRand : bigRand;
    size = 1 + static_cast<int64_t>(bigRand % (availableVolume + 1));

    if (action == OrderAction::ASK) {
      session->lastTotalVolumeAsk =
          std::max(session->lastTotalVolumeAsk - size, static_cast<int64_t>(0));
    } else {
      // Match Java bug: should be lastTotalVolumeBid but Java uses
      // lastTotalVolumeAsk
      session->lastTotalVolumeBid =
          std::max(session->lastTotalVolumeAsk - size, static_cast<int64_t>(0));
    }

  } else {
    // Match Java: rand.nextInt(32) == 0
    if (session->rand.nextInt(32) == 0) {
      // IOC:FOKB = 31:1
      orderType = OrderType::FOK_BUDGET;
      // Match Java: rand.nextInt(8) * rand.nextInt(8) * rand.nextInt(8)
      size = 1 + session->rand.nextInt(8) * session->rand.nextInt(8) *
                     session->rand.nextInt(8);
      priceOrBudget = size * priceLimit;
      reserveBidPrice = priceOrBudget;
    } else {
      orderType = OrderType::IOC;
      priceOrBudget = priceLimit;
      reserveBidPrice = action == OrderAction::BID ? session->maxPrice : 0;
      // Match Java: rand.nextInt(6) * rand.nextInt(6) * rand.nextInt(6)
      size = 1 + session->rand.nextInt(6) * session->rand.nextInt(6) *
                     session->rand.nextInt(6);
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

  // Match Java: rand.nextInt(4)
  OrderAction action = (session->rand.nextInt(4) + session->priceDirection >= 2)
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
    // Match Java: rand.nextInt(requireFastFill ? 2 : 10)
    q = session->rand.nextInt(requireFastFill ? 2 : 10);
  } else {
    // Match Java: rand.nextInt(40)
    q = session->rand.nextInt(40);
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

  // Match Java: rand.nextInt(size)
  int32_t randPos = session->rand.nextInt(size);

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
    // Match Java: rand.nextInt(prevSize) + 1
    int32_t reduceBy = session->rand.nextInt(prevSize) + 1;
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
      // Match Java: rand.nextInt(2) * 2 - 1
      priceMoveRounded = session->rand.nextInt(2) * 2 - 1;
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
  // Select symbol specification based on symbol ID to ensure order book
  // snapshot matches actual execution (fixes Java TODO comment)
  exchange::core::common::CoreSymbolSpecification symbolSpec;
  if (symbol == TestConstants::SYMBOL_MARGIN) {
    symbolSpec = TestConstants::CreateSymbolSpecEurUsd();
  } else if (symbol == TestConstants::SYMBOL_EXCHANGE) {
    symbolSpec = TestConstants::CreateSymbolSpecEthXbt();
  } else if (symbol == TestConstants::SYMBOL_EXCHANGE_FEE) {
    symbolSpec = TestConstants::CreateSymbolSpecFeeXbtLtc();
  } else {
    // Fallback to EUR_USD for unknown symbols
    symbolSpec = TestConstants::CreateSymbolSpecEurUsd();
  }
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
    } else {
      cmd = GenerateRandomOrder(&session);
    }

    // Set symbol and resultCode BEFORE copying
    cmd.resultCode = CommandResultCode::VALID_FOR_MATCHING_ENGINE;
    cmd.symbol = session.symbol;

    // Now copy the command with correct symbol
    if (fillInProgress) {
      result.commandsFill.push_back(cmd.Copy());
    } else {
      result.commandsBenchmark.push_back(cmd.Copy());
    }

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

// Generate multiple symbols test commands
TestOrdersGenerator::MultiSymbolGenResult
TestOrdersGenerator::GenerateMultipleSymbols(
    const TestOrdersGeneratorConfig &config) {
  // Use ExecutionTime to log total generation time (matching Java
  // implementation)
  ExecutionTime executionTime([](const std::string &timeStr) {
    LOG_DEBUG("All test commands generated in {}", timeStr);
  });

  MultiSymbolGenResult multiResult;

  // Calculate transactions per symbol
  int transactionsPerSymbol =
      config.totalTransactionsNumber /
      static_cast<int>(config.coreSymbolSpecifications.size());
  if (transactionsPerSymbol == 0) {
    transactionsPerSymbol = 1;
  }

  // Calculate target orders per symbol
  int targetOrdersPerSymbol =
      config.targetOrderBookOrdersTotal /
      static_cast<int>(config.coreSymbolSpecifications.size());
  if (targetOrdersPerSymbol == 0) {
    targetOrdersPerSymbol = 1;
  }

  // Generate commands for each symbol
  for (const auto &symbolSpec : config.coreSymbolSpecifications) {
    // Get users that can trade this symbol
    auto userList = UserCurrencyAccountsGenerator::CreateUserListForSymbol(
        config.usersAccounts, symbolSpec, transactionsPerSymbol);

    if (userList.empty()) {
      continue; // Skip symbols with no eligible users
    }

    // Create UID mapper for this symbol
    std::unordered_map<int32_t, int32_t> userIndexMap;
    for (size_t i = 0; i < userList.size(); i++) {
      userIndexMap[userList[i]] = static_cast<int32_t>(i);
    }

    auto uidMapper = [&userList](int32_t index) -> int32_t {
      if (index >= 0 && index < static_cast<int32_t>(userList.size())) {
        return userList[index];
      }
      return index + 1; // Fallback to plain mapper
    };

    // Generate commands for this symbol
    int readySeq = config.CalculateReadySeq();
    int targetOrders =
        (config.preFillMode == util::PreFillMode::ORDERS_NUMBER_PLUS_QUARTER)
            ? targetOrdersPerSymbol * 5 / 4
            : targetOrdersPerSymbol;

    auto genResult = GenerateCommands(
        transactionsPerSymbol, targetOrders, static_cast<int>(userList.size()),
        uidMapper, symbolSpec.symbolId, false, config.avalancheIOC,
        CreateAsyncProgressLogger(transactionsPerSymbol + targetOrders),
        config.seed);

    // Combine commands (use Copy() method for OrderCommand) before moving
    for (const auto &cmd : genResult.commandsFill) {
      multiResult.commandsFill.push_back(cmd.Copy());
    }
    for (const auto &cmd : genResult.commandsBenchmark) {
      multiResult.commandsBenchmark.push_back(cmd.Copy());
    }

    // Store per-symbol result (use try_emplace to avoid copy assignment)
    multiResult.genResults.try_emplace(symbolSpec.symbolId,
                                       std::move(genResult));
  }

  // Log merging commands
  LOG_DEBUG("Merging {} commands for {} symbols (preFill)...",
            multiResult.commandsFill.size(),
            config.coreSymbolSpecifications.size());
  LOG_DEBUG("Merging {} commands for {} symbols (benchmark)...",
            multiResult.commandsBenchmark.size(),
            config.coreSymbolSpecifications.size());

  return multiResult;
}

// Implementation of MultiSymbolGenResult methods
std::future<std::vector<exchange::core::common::api::ApiCommand *>>
TestOrdersGenerator::MultiSymbolGenResult::GetApiCommandsFill() const {
  // Convert to ApiCommand* and return as future (for async compatibility)
  std::promise<std::vector<exchange::core::common::api::ApiCommand *>> promise;
  auto future = promise.get_future();

  ExecutionTime executionTime;
  std::vector<exchange::core::common::api::ApiCommand *> apiCommands;
  apiCommands.reserve(commandsFill.size());

  for (const auto &cmd : commandsFill) {
    exchange::core::common::api::ApiCommand *apiCmd = nullptr;
    if (cmd.command ==
        exchange::core::common::cmd::OrderCommandType::PLACE_ORDER) {
      apiCmd = new exchange::core::common::api::ApiPlaceOrder(
          cmd.price, cmd.size, cmd.orderId, cmd.action, cmd.orderType, cmd.uid,
          cmd.symbol, cmd.userCookie, cmd.reserveBidPrice);
    } else if (cmd.command ==
               exchange::core::common::cmd::OrderCommandType::MOVE_ORDER) {
      apiCmd = new exchange::core::common::api::ApiMoveOrder(
          cmd.orderId, cmd.price, cmd.uid, cmd.symbol);
    } else if (cmd.command ==
               exchange::core::common::cmd::OrderCommandType::CANCEL_ORDER) {
      apiCmd = new exchange::core::common::api::ApiCancelOrder(
          cmd.orderId, cmd.uid, cmd.symbol);
    } else if (cmd.command ==
               exchange::core::common::cmd::OrderCommandType::REDUCE_ORDER) {
      apiCmd = new exchange::core::common::api::ApiReduceOrder(
          cmd.orderId, cmd.uid, cmd.symbol, cmd.size);
    } else {
      // Match Java: throw exception for unsupported command type
      throw std::runtime_error(
          "Unsupported command type in GetApiCommandsFill: " +
          std::to_string(static_cast<int>(cmd.command)));
    }
    if (apiCmd) {
      apiCommands.push_back(apiCmd);
    }
  }

  LOG_DEBUG("Converted {} commands to API commands in: {}", apiCommands.size(),
            executionTime.GetTimeFormatted());
  promise.set_value(std::move(apiCommands));
  return future;
}

std::future<std::vector<exchange::core::common::api::ApiCommand *>>
TestOrdersGenerator::MultiSymbolGenResult::GetApiCommandsBenchmark() const {
  std::promise<std::vector<exchange::core::common::api::ApiCommand *>> promise;
  auto future = promise.get_future();

  ExecutionTime executionTime;
  std::vector<exchange::core::common::api::ApiCommand *> apiCommands;
  apiCommands.reserve(commandsBenchmark.size());

  // Count command types for statistics
  int counterPlaceGTC = 0;
  int counterPlaceIOC = 0;
  int counterPlaceFOKB = 0;
  int counterCancel = 0;
  int counterMove = 0;
  int counterReduce = 0;
  std::unordered_map<int32_t, int> symbolCounters;

  for (const auto &cmd : commandsBenchmark) {
    exchange::core::common::api::ApiCommand *apiCmd = nullptr;
    if (cmd.command ==
        exchange::core::common::cmd::OrderCommandType::PLACE_ORDER) {
      apiCmd = new exchange::core::common::api::ApiPlaceOrder(
          cmd.price, cmd.size, cmd.orderId, cmd.action, cmd.orderType, cmd.uid,
          cmd.symbol, cmd.userCookie, cmd.reserveBidPrice);
      if (cmd.orderType == exchange::core::common::OrderType::GTC) {
        counterPlaceGTC++;
      } else if (cmd.orderType == exchange::core::common::OrderType::IOC) {
        counterPlaceIOC++;
      } else if (cmd.orderType ==
                 exchange::core::common::OrderType::FOK_BUDGET) {
        counterPlaceFOKB++;
      }
    } else if (cmd.command ==
               exchange::core::common::cmd::OrderCommandType::MOVE_ORDER) {
      apiCmd = new exchange::core::common::api::ApiMoveOrder(
          cmd.orderId, cmd.price, cmd.uid, cmd.symbol);
      counterMove++;
    } else if (cmd.command ==
               exchange::core::common::cmd::OrderCommandType::CANCEL_ORDER) {
      apiCmd = new exchange::core::common::api::ApiCancelOrder(
          cmd.orderId, cmd.uid, cmd.symbol);
      counterCancel++;
    } else if (cmd.command ==
               exchange::core::common::cmd::OrderCommandType::REDUCE_ORDER) {
      apiCmd = new exchange::core::common::api::ApiReduceOrder(
          cmd.orderId, cmd.uid, cmd.symbol, cmd.size);
      counterReduce++;
    } else {
      // Match Java: throw exception for unsupported command type
      throw std::runtime_error(
          "Unsupported command type in GetApiCommandsBenchmark: " +
          std::to_string(static_cast<int>(cmd.command)));
    }
    if (apiCmd) {
      apiCommands.push_back(apiCmd);
      symbolCounters[cmd.symbol]++;
    }
  }

  // Log statistics
  int totalCommands = static_cast<int>(commandsBenchmark.size());
  if (totalCommands > 0) {
    float gtcPercent = (counterPlaceGTC * 100.0f) / totalCommands;
    float iocPercent = (counterPlaceIOC * 100.0f) / totalCommands;
    float fokbPercent = (counterPlaceFOKB * 100.0f) / totalCommands;
    float cancelPercent = (counterCancel * 100.0f) / totalCommands;
    float movePercent = (counterMove * 100.0f) / totalCommands;
    float reducePercent = (counterReduce * 100.0f) / totalCommands;

    LOG_INFO("GTC:{:.2f}% IOC:{:.2f}% FOKB:{:.2f}% cancel:{:.2f}% move:{:.2f}% "
             "reduce:{:.2f}%",
             gtcPercent, iocPercent, fokbPercent, cancelPercent, movePercent,
             reducePercent);

    // Calculate per-symbol statistics
    if (!symbolCounters.empty()) {
      int maxCommands = 0, minCommands = INT_MAX, sumCommands = 0;
      for (const auto &pair : symbolCounters) {
        maxCommands = std::max(maxCommands, pair.second);
        minCommands = std::min(minCommands, pair.second);
        sumCommands += pair.second;
      }
      float avgCommands =
          static_cast<float>(sumCommands) / symbolCounters.size();
      float maxPercent = (maxCommands * 100.0f) / totalCommands;
      float avgPercent = (avgCommands * 100.0f) / totalCommands;
      float minPercent = (minCommands * 100.0f) / totalCommands;

      LOG_INFO("commands per symbol: max:{} ({:.2f}%); avg:{:.0f} ({:.2f}%); "
               "min:{} ({:.2f}%)",
               maxCommands, maxPercent, avgCommands, avgPercent, minCommands,
               minPercent);
    }
  }

  LOG_DEBUG("Converted {} commands to API commands in: {}", apiCommands.size(),
            executionTime.GetTimeFormatted());
  promise.set_value(std::move(apiCommands));
  return future;
}

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
