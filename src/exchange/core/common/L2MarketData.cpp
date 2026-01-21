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

#include <exchange/core/common/L2MarketData.h>

namespace exchange::core::common {

L2MarketData::L2MarketData(const std::vector<int64_t>& askPrices,
                           const std::vector<int64_t>& askVolumes,
                           const std::vector<int64_t>& askOrders,
                           const std::vector<int64_t>& bidPrices,
                           const std::vector<int64_t>& bidVolumes,
                           const std::vector<int64_t>& bidOrders)
  : askPrices(askPrices)
  , askVolumes(askVolumes)
  , askOrders(askOrders)
  , bidPrices(bidPrices)
  , bidVolumes(bidVolumes)
  , bidOrders(bidOrders)
  , askSize(static_cast<int32_t>(askPrices.size()))
  , bidSize(static_cast<int32_t>(bidPrices.size())) {}

L2MarketData::L2MarketData(int32_t askSize, int32_t bidSize)
  : askSize(askSize)
  , bidSize(bidSize)
  , askPrices(askSize)
  , bidPrices(bidSize)
  , askVolumes(askSize)
  , bidVolumes(bidSize)
  , askOrders(askSize)
  , bidOrders(bidSize) {}

std::vector<int64_t> L2MarketData::GetAskPricesCopy() const {
  return std::vector<int64_t>(askPrices.begin(), askPrices.begin() + askSize);
}

std::vector<int64_t> L2MarketData::GetAskVolumesCopy() const {
  return std::vector<int64_t>(askVolumes.begin(), askVolumes.begin() + askSize);
}

std::vector<int64_t> L2MarketData::GetAskOrdersCopy() const {
  return std::vector<int64_t>(askOrders.begin(), askOrders.begin() + askSize);
}

std::vector<int64_t> L2MarketData::GetBidPricesCopy() const {
  return std::vector<int64_t>(bidPrices.begin(), bidPrices.begin() + bidSize);
}

std::vector<int64_t> L2MarketData::GetBidVolumesCopy() const {
  return std::vector<int64_t>(bidVolumes.begin(), bidVolumes.begin() + bidSize);
}

std::vector<int64_t> L2MarketData::GetBidOrdersCopy() const {
  return std::vector<int64_t>(bidOrders.begin(), bidOrders.begin() + bidSize);
}

int64_t L2MarketData::TotalOrderBookVolumeAsk() const {
  int64_t totalVolume = 0;
  for (int32_t i = 0; i < askSize; i++) {
    totalVolume += askVolumes[i];
  }
  return totalVolume;
}

int64_t L2MarketData::TotalOrderBookVolumeBid() const {
  int64_t totalVolume = 0;
  for (int32_t i = 0; i < bidSize; i++) {
    totalVolume += bidVolumes[i];
  }
  return totalVolume;
}

std::unique_ptr<L2MarketData> L2MarketData::Copy() const {
  return std::make_unique<L2MarketData>(GetAskPricesCopy(), GetAskVolumesCopy(), GetAskOrdersCopy(),
                                        GetBidPricesCopy(), GetBidVolumesCopy(),
                                        GetBidOrdersCopy());
}

bool L2MarketData::operator==(const L2MarketData& other) const {
  if (askSize != other.askSize || bidSize != other.bidSize) {
    return false;
  }

  // Add bounds checking to prevent array out-of-bounds access
  if (askSize > 0) {
    // Check that vectors have enough capacity
    if (static_cast<size_t>(askSize) > askPrices.size()
        || static_cast<size_t>(askSize) > askVolumes.size()
        || static_cast<size_t>(askSize) > askOrders.size()
        || static_cast<size_t>(askSize) > other.askPrices.size()
        || static_cast<size_t>(askSize) > other.askVolumes.size()
        || static_cast<size_t>(askSize) > other.askOrders.size()) {
      return false;
    }

    for (int32_t i = 0; i < askSize; i++) {
      if (askPrices[i] != other.askPrices[i] || askVolumes[i] != other.askVolumes[i]
          || askOrders[i] != other.askOrders[i]) {
        return false;
      }
    }
  }

  if (bidSize > 0) {
    // Check that vectors have enough capacity
    if (static_cast<size_t>(bidSize) > bidPrices.size()
        || static_cast<size_t>(bidSize) > bidVolumes.size()
        || static_cast<size_t>(bidSize) > bidOrders.size()
        || static_cast<size_t>(bidSize) > other.bidPrices.size()
        || static_cast<size_t>(bidSize) > other.bidVolumes.size()
        || static_cast<size_t>(bidSize) > other.bidOrders.size()) {
      return false;
    }

    for (int32_t i = 0; i < bidSize; i++) {
      if (bidPrices[i] != other.bidPrices[i] || bidVolumes[i] != other.bidVolumes[i]
          || bidOrders[i] != other.bidOrders[i]) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace exchange::core::common
