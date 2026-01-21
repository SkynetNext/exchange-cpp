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

#include "TestOrdersGeneratorSession.h"
#include <algorithm>
#include <cmath>

namespace exchange {
namespace core {
namespace tests {
namespace util {

TestOrdersGeneratorSession::TestOrdersGeneratorSession(core::orderbook::IOrderBook* orderBook,
                                                       int transactionsNumber,
                                                       int targetOrderBookOrdersHalf,
                                                       bool avalancheIOC,
                                                       int numUsers,
                                                       std::function<int32_t(int32_t)> uidMapper,
                                                       int32_t symbol,
                                                       bool enableSlidingPrice,
                                                       int seed)
  : orderBook(orderBook)
  , transactionsNumber(transactionsNumber)
  , targetOrderBookOrdersHalf(targetOrderBookOrdersHalf)
  , priceDeviation(0)
  ,  // Will be set via const_cast below
  avalancheIOC(avalancheIOC)
  , numUsers(numUsers)
  , uidMapper(uidMapper)
  ,
  // Match Java: Objects.hash(symbol * -177277, seed * 10037 + 198267)
  // Objects.hash for 2 args: 31 * (31 * 1 + value1) + value2 = 961 + 31 *
  // value1 + value2
  symbol(symbol)
  , rand(961 + 31 * (symbol * -177277) + (seed * 10037 + 198267))
  , minPrice(0)
  , maxPrice(0)
  ,                                   // Will be set via const_cast below
  lackOrOrdersFastFillThreshold(0) {  // Will be set via const_cast below
  // Generate initial price: 10^(3.3 + rand*1.5 + rand*1.5)
  // Match Java: int price = (int) Math.pow(10, 3.3 + rand.nextDouble() * 1.5 +
  // rand.nextDouble() * 1.5);
  double rand1 = rand.nextDouble();
  double rand2 = rand.nextDouble();
  int64_t price = static_cast<int64_t>(std::pow(10.0, 3.3 + rand1 * 1.5 + rand2 * 1.5));

  this->lastTradePrice = price;
  int64_t calculatedPriceDeviation =
    std::min(static_cast<int64_t>(price * 0.05), static_cast<int64_t>(10000));
  int64_t calculatedMinPrice = price - calculatedPriceDeviation * 5;
  int64_t calculatedMaxPrice = price + calculatedPriceDeviation * 5;

  // Use const_cast to initialize const members (necessary for computed values)
  const_cast<int64_t&>(this->priceDeviation) = calculatedPriceDeviation;
  const_cast<int64_t&>(this->minPrice) = calculatedMinPrice;
  const_cast<int64_t&>(this->maxPrice) = calculatedMaxPrice;

  this->priceDirection = enableSlidingPrice ? 1 : 0;

  const int CHECK_ORDERBOOK_STAT_EVERY_NTH_COMMAND = 512;
  const_cast<int&>(this->lackOrOrdersFastFillThreshold) =
    std::min(CHECK_ORDERBOOK_STAT_EVERY_NTH_COMMAND, targetOrderBookOrdersHalf * 3 / 4);
}

}  // namespace util
}  // namespace tests
}  // namespace core
}  // namespace exchange
