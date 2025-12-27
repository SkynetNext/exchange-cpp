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
#include <map>
#include <string>
#include <unordered_map>

namespace exchange {
namespace core {
namespace tests {
namespace nasdaq {

/**
 * StockDescr - stock description
 */
struct StockDescr {
  std::string name;
  uint8_t etpFlag;
  int64_t etpLeverageFactor;

  std::string ToString() const;
};

/**
 * StockStat - stock statistics
 */
struct StockStat {
  int32_t stockLocate;

  int32_t minPrice = INT32_MAX;
  int32_t maxPrice = INT32_MIN;
  int32_t priceStep = 1'000'000'000;
  float priceAvg = NAN;
  int32_t counter = 0;
  bool priceSample = false;
  std::unordered_map<uint8_t, int32_t> counters;

  StockStat(int32_t locate) : stockLocate(locate) {}

  std::string ToString() const;
};

/**
 * ITCH50StatListener - listener for ITCH50 messages
 * Collects statistics about NASDAQ ITCH50 feed
 * Note: Requires external library (paritytrading juncture) for ITCH50Listener
 * interface
 */
class ITCH50StatListener {
public:
  /**
   * Print statistics for symbols with counter > 500000
   */
  void PrintStat() const;

  /**
   * Get symbol statistics map
   */
  const std::map<int32_t, StockStat> &GetSymbolStat() const {
    return symbolStat_;
  }

  // Note: ITCH50Listener interface methods would be implemented here
  // when the external library is available

private:
  std::map<int32_t, StockStat> symbolStat_;
  std::unordered_map<int32_t, StockDescr> symbolDescr_;

  void UpdateStat(uint8_t msgType, int32_t stockLocate);
  void UpdateStat(uint8_t msgType, int32_t stockLocate, int64_t longPrice);
};

} // namespace nasdaq
} // namespace tests
} // namespace core
} // namespace exchange
