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

#include "L2MarketDataHelper.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace exchange {
namespace core {
namespace tests {
namespace util {

L2MarketDataHelper::L2MarketDataHelper() = default;

L2MarketDataHelper::L2MarketDataHelper(
    const exchange::core::common::L2MarketData &l2) {
  askPrices_ = l2.GetAskPricesCopy();
  askVolumes_ = l2.GetAskVolumesCopy();
  askOrders_ = l2.GetAskOrdersCopy();
  bidPrices_ = l2.GetBidPricesCopy();
  bidVolumes_ = l2.GetBidVolumesCopy();
  bidOrders_ = l2.GetBidOrdersCopy();
}

std::unique_ptr<exchange::core::common::L2MarketData>
L2MarketDataHelper::Build() const {
  return std::make_unique<exchange::core::common::L2MarketData>(
      askPrices_, askVolumes_, askOrders_, bidPrices_, bidVolumes_, bidOrders_);
}

int64_t L2MarketDataHelper::AggregateBuyBudget(int64_t size) const {
  int64_t budget = 0;
  for (size_t i = 0; i < askPrices_.size(); i++) {
    int64_t v = askVolumes_[i];
    int64_t p = askPrices_[i];
    if (v < size) {
      budget += v * p;
      size -= v;
    } else {
      return budget + size * p;
    }
  }
  throw std::invalid_argument("Can not collect size " + std::to_string(size));
}

int64_t L2MarketDataHelper::AggregateSellExpectation(int64_t size) const {
  int64_t expectation = 0;
  for (size_t i = 0; i < bidPrices_.size(); i++) {
    int64_t v = bidVolumes_[i];
    int64_t p = bidPrices_[i];
    if (v < size) {
      expectation += v * p;
      size -= v;
    } else {
      return expectation + size * p;
    }
  }
  throw std::invalid_argument("Can not collect size " + std::to_string(size));
}

L2MarketDataHelper &L2MarketDataHelper::SetAskPrice(int pos, int64_t askPrice) {
  askPrices_[pos] = askPrice;
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::SetBidPrice(int pos, int64_t bidPrice) {
  bidPrices_[pos] = bidPrice;
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::SetAskVolume(int pos,
                                                     int64_t askVolume) {
  askVolumes_[pos] = askVolume;
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::SetBidVolume(int pos,
                                                     int64_t bidVolume) {
  bidVolumes_[pos] = bidVolume;
  return *this;
}

L2MarketDataHelper &
L2MarketDataHelper::DecrementAskVolume(int pos, int64_t askVolumeDiff) {
  askVolumes_[pos] -= askVolumeDiff;
  return *this;
}

L2MarketDataHelper &
L2MarketDataHelper::DecrementBidVolume(int pos, int64_t bidVolumeDiff) {
  bidVolumes_[pos] -= bidVolumeDiff;
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::SetAskPriceVolume(int pos,
                                                          int64_t askPrice,
                                                          int64_t askVolume) {
  askVolumes_[pos] = askVolume;
  askPrices_[pos] = askPrice;
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::SetBidPriceVolume(int pos,
                                                          int64_t bidPrice,
                                                          int64_t bidVolume) {
  bidVolumes_[pos] = bidVolume;
  bidPrices_[pos] = bidPrice;
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::DecrementAskOrdersNum(int pos) {
  askOrders_[pos]--;
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::DecrementBidOrdersNum(int pos) {
  bidOrders_[pos]--;
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::IncrementAskOrdersNum(int pos) {
  askOrders_[pos]++;
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::IncrementBidOrdersNum(int pos) {
  bidOrders_[pos]++;
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::RemoveAsk(int pos) {
  askPrices_.erase(askPrices_.begin() + pos);
  askVolumes_.erase(askVolumes_.begin() + pos);
  askOrders_.erase(askOrders_.begin() + pos);
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::RemoveAllAsks() {
  askPrices_.clear();
  askVolumes_.clear();
  askOrders_.clear();
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::RemoveBid(int pos) {
  bidPrices_.erase(bidPrices_.begin() + pos);
  bidVolumes_.erase(bidVolumes_.begin() + pos);
  bidOrders_.erase(bidOrders_.begin() + pos);
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::RemoveAllBids() {
  bidPrices_.clear();
  bidVolumes_.clear();
  bidOrders_.clear();
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::InsertAsk(int pos, int64_t price,
                                                  int64_t volume) {
  askPrices_.insert(askPrices_.begin() + pos, price);
  askVolumes_.insert(askVolumes_.begin() + pos, volume);
  askOrders_.insert(askOrders_.begin() + pos, 1);
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::InsertBid(int pos, int64_t price,
                                                  int64_t volume) {
  bidPrices_.insert(bidPrices_.begin() + pos, price);
  bidVolumes_.insert(bidVolumes_.begin() + pos, volume);
  bidOrders_.insert(bidOrders_.begin() + pos, 1);
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::AddAsk(int64_t price, int64_t volume) {
  askPrices_.push_back(price);
  askVolumes_.push_back(volume);
  askOrders_.push_back(1);
  return *this;
}

L2MarketDataHelper &L2MarketDataHelper::AddBid(int64_t price, int64_t volume) {
  bidPrices_.push_back(price);
  bidVolumes_.push_back(volume);
  bidOrders_.push_back(1);
  return *this;
}

std::string L2MarketDataHelper::DumpOrderBook(
    const exchange::core::common::L2MarketData &l2MarketData) const {
  int32_t askSize = l2MarketData.askSize;
  int32_t bidSize = l2MarketData.bidSize;

  auto askPrices = l2MarketData.GetAskPricesCopy();
  auto askVolumes = l2MarketData.GetAskVolumesCopy();
  auto askOrders = l2MarketData.GetAskOrdersCopy();
  auto bidPrices = l2MarketData.GetBidPricesCopy();
  auto bidVolumes = l2MarketData.GetBidVolumesCopy();
  auto bidOrders = l2MarketData.GetBidOrdersCopy();

  // Resize to actual sizes
  askPrices.resize(askSize);
  askVolumes.resize(askSize);
  askOrders.resize(askSize);
  bidPrices.resize(bidSize);
  bidVolumes.resize(bidSize);
  bidOrders.resize(bidSize);

  int priceWidth = MaxWidth(2, askPrices, bidPrices);
  int volWidth = MaxWidth(2, askVolumes, bidVolumes);
  int ordWidth = MaxWidth(2, askOrders, bidOrders);

  std::ostringstream s;
  s << "Order book:\n";
  s << "." << std::string(priceWidth - 2, '-') << "ASKS"
    << std::string(volWidth - 1, '-') << ".\n";
  for (int i = askSize - 1; i >= 0; i--) {
    s << "|" << std::setw(priceWidth) << askPrices[i] << "|"
      << std::setw(volWidth) << askVolumes[i] << "|" << std::setw(ordWidth)
      << askOrders[i] << "|\n";
  }
  s << "|" << std::string(priceWidth, '-') << "+" << std::string(volWidth, '-')
    << "|\n";
  for (int i = 0; i < bidSize; i++) {
    s << "|" << std::setw(priceWidth) << bidPrices[i] << "|"
      << std::setw(volWidth) << bidVolumes[i] << "|" << std::setw(ordWidth)
      << bidOrders[i] << "|\n";
  }
  s << "'" << std::string(priceWidth - 2, '-') << "BIDS"
    << std::string(volWidth - 1, '-') << "'\n";
  return s.str();
}

int L2MarketDataHelper::MaxWidth(int minWidth, const std::vector<int64_t> &arr1,
                                 const std::vector<int64_t> &arr2) {
  int maxLen = minWidth;
  for (int64_t val : arr1) {
    int len = static_cast<int>(std::to_string(val).length());
    if (len > maxLen)
      maxLen = len;
  }
  for (int64_t val : arr2) {
    int len = static_cast<int>(std::to_string(val).length());
    if (len > maxLen)
      maxLen = len;
  }
  return maxLen;
}

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
