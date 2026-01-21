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

#include <cstdint>
#include <memory>
#include <vector>

namespace exchange {
namespace core {
namespace common {

/**
 * L2 Market Data carrier object
 * NOTE: Can have dirty data, askSize and bidSize are important!
 */
class L2MarketData {
public:
  static constexpr int32_t L2_SIZE = 32;

  int32_t askSize = 0;
  int32_t bidSize = 0;

  std::vector<int64_t> askPrices;
  std::vector<int64_t> askVolumes;
  std::vector<int64_t> askOrders;
  std::vector<int64_t> bidPrices;
  std::vector<int64_t> bidVolumes;
  std::vector<int64_t> bidOrders;

  // when published
  int64_t timestamp = 0;
  int64_t referenceSeq = 0;

  L2MarketData() = default;

  L2MarketData(const std::vector<int64_t>& askPrices,
               const std::vector<int64_t>& askVolumes,
               const std::vector<int64_t>& askOrders,
               const std::vector<int64_t>& bidPrices,
               const std::vector<int64_t>& bidVolumes,
               const std::vector<int64_t>& bidOrders);

  L2MarketData(int32_t askSize, int32_t bidSize);

  std::vector<int64_t> GetAskPricesCopy() const;
  std::vector<int64_t> GetAskVolumesCopy() const;
  std::vector<int64_t> GetAskOrdersCopy() const;
  std::vector<int64_t> GetBidPricesCopy() const;
  std::vector<int64_t> GetBidVolumesCopy() const;
  std::vector<int64_t> GetBidOrdersCopy() const;

  int64_t TotalOrderBookVolumeAsk() const;
  int64_t TotalOrderBookVolumeBid() const;

  std::unique_ptr<L2MarketData> Copy() const;

  bool operator==(const L2MarketData& other) const;
};

}  // namespace common
}  // namespace core
}  // namespace exchange
