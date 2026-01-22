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

#include <exchange/core/common/L2MarketData.h>
#include <string>
#include <vector>

namespace exchange::core::tests::util {

/**
 * L2MarketDataHelper - helper class for building and manipulating L2MarketData
 * in tests
 */
class L2MarketDataHelper {
public:
  L2MarketDataHelper();
  explicit L2MarketDataHelper(const exchange::core::common::L2MarketData& l2);

  std::unique_ptr<exchange::core::common::L2MarketData> Build() const;

  int64_t AggregateBuyBudget(int64_t size) const;
  int64_t AggregateSellExpectation(int64_t size) const;

  L2MarketDataHelper& SetAskPrice(int pos, int64_t askPrice);
  L2MarketDataHelper& SetBidPrice(int pos, int64_t bidPrice);
  L2MarketDataHelper& SetAskVolume(int pos, int64_t askVolume);
  L2MarketDataHelper& SetBidVolume(int pos, int64_t bidVolume);
  L2MarketDataHelper& DecrementAskVolume(int pos, int64_t askVolumeDiff);
  L2MarketDataHelper& DecrementBidVolume(int pos, int64_t bidVolumeDiff);
  L2MarketDataHelper& SetAskPriceVolume(int pos, int64_t askPrice, int64_t askVolume);
  L2MarketDataHelper& SetBidPriceVolume(int pos, int64_t bidPrice, int64_t bidVolume);
  L2MarketDataHelper& DecrementAskOrdersNum(int pos);
  L2MarketDataHelper& DecrementBidOrdersNum(int pos);
  L2MarketDataHelper& IncrementAskOrdersNum(int pos);
  L2MarketDataHelper& IncrementBidOrdersNum(int pos);
  L2MarketDataHelper& RemoveAsk(int pos);
  L2MarketDataHelper& RemoveAllAsks();
  L2MarketDataHelper& RemoveBid(int pos);
  L2MarketDataHelper& RemoveAllBids();
  L2MarketDataHelper& InsertAsk(int pos, int64_t price, int64_t volume);
  L2MarketDataHelper& InsertBid(int pos, int64_t price, int64_t volume);
  L2MarketDataHelper& AddAsk(int64_t price, int64_t volume);
  L2MarketDataHelper& AddBid(int64_t price, int64_t volume);

  std::string DumpOrderBook(const exchange::core::common::L2MarketData& l2MarketData) const;

private:
  std::vector<int64_t> askPrices_;
  std::vector<int64_t> askVolumes_;
  std::vector<int64_t> askOrders_;
  std::vector<int64_t> bidPrices_;
  std::vector<int64_t> bidVolumes_;
  std::vector<int64_t> bidOrders_;

  static int
  MaxWidth(int minWidth, const std::vector<int64_t>& arr1, const std::vector<int64_t>& arr2);
};

}  // namespace exchange::core::tests::util
