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

#include "ITCH50StatListener.h"
#include <exchange/core/utils/Logger.h>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace exchange::core::tests::nasdaq {

std::string StockDescr::ToString() const {
  std::ostringstream oss;
  oss << name << " ETP=" << static_cast<char>(etpFlag) << " LF" << etpLeverageFactor;
  return oss.str();
}

std::string StockStat::ToString() const {
  std::ostringstream oss;
  oss << stockLocate << " p:" << minPrice << "-" << static_cast<int>(priceAvg) << "-" << maxPrice
      << " s:" << priceStep << " c:" << counter << " ca:";
  for (const auto& [k, v] : counters) {
    oss << static_cast<char>(k) << v << ' ';
  }
  return oss.str();
}

void ITCH50StatListener::PrintStat() const {
  for (const auto& [k, v] : symbolStat_) {
    if (v.counter > 500000) {
      auto it = symbolDescr_.find(k);
      if (it != symbolDescr_.end()) {
        LOG_INFO("{} {}", it->second.ToString(), v.ToString());
      }
    }
  }
}

void ITCH50StatListener::UpdateStat(uint8_t msgType, int32_t stockLocate) {
  UpdateStat(msgType, stockLocate, INT64_MIN);
}

void ITCH50StatListener::UpdateStat(uint8_t msgType, int32_t stockLocate, int64_t longPrice) {
  auto& stockStat = symbolStat_[stockLocate];
  stockStat.counter++;
  stockStat.counters[msgType]++;

  if (longPrice != INT64_MIN) {
    stockStat.priceSample = true;

    if (longPrice > 2'000'000'000) {
      throw std::runtime_error("PRICE is above 2_000_000_000");
    }
    int32_t msgPrice = static_cast<int32_t>(longPrice);

    while (stockStat.priceStep != 1 && msgPrice % stockStat.priceStep != 0) {
      stockStat.priceStep /= 10;
    }

    stockStat.minPrice = std::min(stockStat.minPrice, msgPrice);
    stockStat.maxPrice = std::max(stockStat.maxPrice, msgPrice);

    if (std::isnan(stockStat.priceAvg)) {
      stockStat.priceAvg = msgPrice;
    } else {
      stockStat.priceAvg = (stockStat.priceAvg * stockStat.counter + msgPrice)
                           / static_cast<float>(stockStat.counter + 1);
    }
  }
}

}  // namespace exchange::core::tests::nasdaq
