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

#include <exchange/core/common/OrderAction.h>
#include <exchange/core/orderbook/IOrderBook.h>
#include "JavaRandom.h"
#include <functional>
#include <unordered_map>
#include <vector>

namespace exchange {
namespace core {
namespace tests {
namespace util {

/**
 * TestOrdersGeneratorSession - session state for generating test orders
 * Matches Java TestOrdersGeneratorSession implementation
 */
class TestOrdersGeneratorSession {
public:
  core::orderbook::IOrderBook *orderBook;

  const int transactionsNumber;
  const int targetOrderBookOrdersHalf;

  const int64_t priceDeviation;

  const bool avalancheIOC;

  const int numUsers;
  std::function<int32_t(int32_t)> uidMapper;

  const int32_t symbol;

  JavaRandom rand;

  std::unordered_map<int32_t, int64_t> orderPrices;
  std::unordered_map<int32_t, int32_t> orderSizes;
  std::unordered_map<int32_t, int32_t> orderUids; // orderId -> uid

  std::vector<int32_t> orderBookSizeAskStat;
  std::vector<int32_t> orderBookSizeBidStat;
  std::vector<int32_t> orderBookNumOrdersAskStat;
  std::vector<int32_t> orderBookNumOrdersBidStat;

  const int64_t minPrice;
  const int64_t maxPrice;

  const int lackOrOrdersFastFillThreshold;

  int64_t lastTradePrice;

  // set to 1 to make price move up and down
  int priceDirection;

  bool initialOrdersPlaced = false;

  int64_t numCompleted = 0;
  int64_t numRejected = 0;
  int64_t numReduced = 0;

  int64_t counterPlaceMarket = 0;
  int64_t counterPlaceLimit = 0;
  int64_t counterCancel = 0;
  int64_t counterMove = 0;
  int64_t counterReduce = 0;

  int32_t seq = 1;

  int32_t filledAtSeq = -1; // -1 means null

  // statistics (updated every 256 orders)
  int32_t lastOrderBookOrdersSizeAsk = 0;
  int32_t lastOrderBookOrdersSizeBid = 0;
  int64_t lastTotalVolumeAsk = 0;
  int64_t lastTotalVolumeBid = 0;

  TestOrdersGeneratorSession(core::orderbook::IOrderBook *orderBook,
                             int transactionsNumber,
                             int targetOrderBookOrdersHalf, bool avalancheIOC,
                             int numUsers,
                             std::function<int32_t(int32_t)> uidMapper,
                             int32_t symbol, bool enableSlidingPrice, int seed);
};

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
