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

#include <algorithm>
#include <exchange/core/common/BytesIn.h>
#include <exchange/core/common/BytesOut.h>
#include <exchange/core/common/CoreSymbolSpecification.h>
#include <exchange/core/common/PositionDirection.h>
#include <exchange/core/common/SymbolPositionRecord.h>
#include <sstream>
#include <stdexcept>

namespace exchange {
namespace core {
namespace common {

SymbolPositionRecord::SymbolPositionRecord(int64_t uid, int32_t symbol,
                                           int32_t currency)
    : uid(uid), symbol(symbol), currency(currency),
      direction(PositionDirection::EMPTY), openVolume(0), openPriceSum(0),
      profit(0), pendingSellSize(0), pendingBuySize(0) {}

SymbolPositionRecord::SymbolPositionRecord(int64_t uid, BytesIn &bytes)
    : uid(uid) {
  symbol = bytes.ReadInt();
  currency = bytes.ReadInt();
  direction = PositionDirectionFromCode(bytes.ReadByte());
  openVolume = bytes.ReadLong();
  openPriceSum = bytes.ReadLong();
  profit = bytes.ReadLong();
  pendingSellSize = bytes.ReadLong();
  pendingBuySize = bytes.ReadLong();
}

void SymbolPositionRecord::Initialize(int64_t uid, int32_t symbol,
                                      int32_t currency) {
  this->uid = uid;
  this->symbol = symbol;
  this->currency = currency;
  this->direction = PositionDirection::EMPTY;
  this->openVolume = 0;
  this->openPriceSum = 0;
  this->profit = 0;
  this->pendingSellSize = 0;
  this->pendingBuySize = 0;
}

bool SymbolPositionRecord::IsEmpty() const {
  return direction == PositionDirection::EMPTY && pendingSellSize == 0 &&
         pendingBuySize == 0;
}

void SymbolPositionRecord::PendingHold(OrderAction orderAction, int64_t size) {
  if (orderAction == OrderAction::ASK) {
    pendingSellSize += size;
  } else {
    pendingBuySize += size;
  }
}

void SymbolPositionRecord::PendingRelease(OrderAction orderAction,
                                          int64_t size) {
  if (orderAction == OrderAction::ASK) {
    pendingSellSize -= size;
  } else {
    pendingBuySize -= size;
  }
}

int64_t SymbolPositionRecord::EstimateProfit(
    const CoreSymbolSpecification &spec,
    const processors::LastPriceCacheRecord *lastPriceCacheRecord) const {
  switch (direction) {
  case PositionDirection::EMPTY:
    return profit;
  case PositionDirection::LONG:
    if (lastPriceCacheRecord != nullptr &&
        lastPriceCacheRecord->bidPrice != 0) {
      return profit +
             (openVolume * lastPriceCacheRecord->bidPrice - openPriceSum);
    } else {
      // unknown price - no liquidity - require extra margin
      return profit + spec.marginBuy * openVolume;
    }
  case PositionDirection::SHORT:
    if (lastPriceCacheRecord != nullptr &&
        lastPriceCacheRecord->askPrice != INT64_MAX) {
      return profit +
             (openPriceSum - openVolume * lastPriceCacheRecord->askPrice);
    } else {
      // unknown price - no liquidity - require extra margin
      return profit + spec.marginSell * openVolume;
    }
  default:
    throw std::runtime_error("Invalid position direction");
  }
}

int64_t SymbolPositionRecord::CalculateRequiredMarginForFutures(
    const CoreSymbolSpecification &spec) const {
  const int64_t specMarginBuy = spec.marginBuy;
  const int64_t specMarginSell = spec.marginSell;

  const int64_t signedPosition = openVolume * GetMultiplier(direction);
  const int64_t currentRiskBuySize = pendingBuySize + signedPosition;
  const int64_t currentRiskSellSize = pendingSellSize - signedPosition;

  const int64_t marginBuy = specMarginBuy * currentRiskBuySize;
  const int64_t marginSell = specMarginSell * currentRiskSellSize;
  // marginBuy or marginSell can be negative, but not both of them
  return std::max(marginBuy, marginSell);
}

int64_t SymbolPositionRecord::CalculateRequiredMarginForOrder(
    const CoreSymbolSpecification &spec, OrderAction action,
    int64_t size) const {
  const int64_t specMarginBuy = spec.marginBuy;
  const int64_t specMarginSell = spec.marginSell;

  const int64_t signedPosition = openVolume * GetMultiplier(direction);
  const int64_t currentRiskBuySize = pendingBuySize + signedPosition;
  const int64_t currentRiskSellSize = pendingSellSize - signedPosition;

  int64_t marginBuy = specMarginBuy * currentRiskBuySize;
  int64_t marginSell = specMarginSell * currentRiskSellSize;
  // either marginBuy or marginSell can be negative (because of signedPosition),
  // but not both of them
  const int64_t currentMargin = std::max(marginBuy, marginSell);

  if (action == OrderAction::BID) {
    marginBuy += spec.marginBuy * size;
  } else {
    marginSell += spec.marginSell * size;
  }

  // marginBuy or marginSell can be negative, but not both of them
  const int64_t newMargin = std::max(marginBuy, marginSell);

  return (newMargin <= currentMargin) ? -1 : newMargin;
}

int64_t SymbolPositionRecord::UpdatePositionForMarginTrade(OrderAction action,
                                                           int64_t size,
                                                           int64_t price) {
  // 1. Un-hold pending size
  PendingRelease(action, size);

  // 2. Reduce opposite position accordingly (if exists)
  const int64_t sizeToOpen = CloseCurrentPositionFutures(action, size, price);

  // 3. Increase forward position accordingly (if size left in the trading
  // event)
  if (sizeToOpen > 0) {
    OpenPositionMargin(action, sizeToOpen, price);
  }
  return sizeToOpen;
}

int64_t SymbolPositionRecord::CloseCurrentPositionFutures(OrderAction action,
                                                          int64_t tradeSize,
                                                          int64_t tradePrice) {
  if (direction == PositionDirection::EMPTY ||
      direction == PositionDirectionFromOrderAction(action)) {
    // nothing to close
    return tradeSize;
  }

  if (openVolume > tradeSize) {
    // current position is bigger than trade size - just reduce position
    // accordingly, don't fix profit
    openVolume -= tradeSize;
    openPriceSum -= tradeSize * tradePrice;
    return 0;
  }

  // current position smaller than trade size, can close completely and
  // calculate profit
  profit += (openVolume * tradePrice - openPriceSum) * GetMultiplier(direction);
  openPriceSum = 0;
  direction = PositionDirection::EMPTY;
  const int64_t sizeToOpen = tradeSize - openVolume;
  openVolume = 0;

  return sizeToOpen;
}

void SymbolPositionRecord::OpenPositionMargin(OrderAction action,
                                              int64_t sizeToOpen,
                                              int64_t tradePrice) {
  openVolume += sizeToOpen;
  openPriceSum += tradePrice * sizeToOpen;
  direction = PositionDirectionFromOrderAction(action);
}

void SymbolPositionRecord::Reset() {
  pendingBuySize = 0;
  pendingSellSize = 0;
  openVolume = 0;
  openPriceSum = 0;
  direction = PositionDirection::EMPTY;
}

void SymbolPositionRecord::ValidateInternalState() const {
  if (direction == PositionDirection::EMPTY &&
      (openVolume != 0 || openPriceSum != 0)) {
    throw std::runtime_error(
        "Invalid state: EMPTY direction but non-zero volume or price sum");
  }
  if (direction != PositionDirection::EMPTY &&
      (openVolume <= 0 || openPriceSum <= 0)) {
    throw std::runtime_error("Invalid state: non-EMPTY direction but zero or "
                             "negative volume or price sum");
  }
  if (pendingSellSize < 0 || pendingBuySize < 0) {
    throw std::runtime_error("Invalid state: negative pending sizes");
  }
}

int32_t SymbolPositionRecord::GetStateHash() const {
  // Match Java Objects.hash(symbol, currency, direction.getMultiplier(),
  // openVolume, openPriceSum, profit, pendingSellSize, pendingBuySize)
  // Objects.hash uses: result = 31 * result + (element == null ? 0 :
  // element.hashCode())
  int32_t result = 1;
  result = 31 * result + symbol;
  result = 31 * result + currency;
  result = 31 * result + static_cast<int32_t>(GetMultiplier(direction));
  result = 31 * result + static_cast<int32_t>(openVolume ^ (openVolume >> 32));
  result =
      31 * result + static_cast<int32_t>(openPriceSum ^ (openPriceSum >> 32));
  result = 31 * result + static_cast<int32_t>(profit ^ (profit >> 32));
  result = 31 * result +
           static_cast<int32_t>(pendingSellSize ^ (pendingSellSize >> 32));
  result = 31 * result +
           static_cast<int32_t>(pendingBuySize ^ (pendingBuySize >> 32));
  return result;
}

std::string SymbolPositionRecord::ToString() const {
  std::ostringstream oss;
  oss << "SPR{u" << uid << " sym" << symbol << " cur" << currency << " pos"
      << static_cast<int>(GetMultiplier(direction)) << " Σv=" << openVolume
      << " Σp=" << openPriceSum << " pnl=" << profit
      << " pendingS=" << pendingSellSize << " pendingB=" << pendingBuySize
      << "}";
  return oss.str();
}

void SymbolPositionRecord::WriteMarshallable(BytesOut &bytes) const {
  bytes.WriteInt(symbol);
  bytes.WriteInt(currency);
  bytes.WriteByte(static_cast<int8_t>(GetMultiplier(direction)));
  bytes.WriteLong(openVolume);
  bytes.WriteLong(openPriceSum);
  bytes.WriteLong(profit);
  bytes.WriteLong(pendingSellSize);
  bytes.WriteLong(pendingBuySize);
}

} // namespace common
} // namespace core
} // namespace exchange
