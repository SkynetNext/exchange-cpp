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
#include "OrderAction.h"
#include "PositionDirection.h"
#include "StateHash.h"
#include "WriteBytesMarshallable.h"

namespace exchange {
namespace core {
namespace common {
class BytesIn;
}  // namespace common
}  // namespace core
}  // namespace exchange

#include <string>

namespace exchange {
namespace core {
namespace common {

// Forward declarations
class CoreSymbolSpecification;

namespace processors {
struct LastPriceCacheRecord {
  int64_t askPrice = INT64_MAX;
  int64_t bidPrice = 0;
};
}  // namespace processors

class SymbolPositionRecord : public StateHash, public WriteBytesMarshallable {
public:
  int64_t uid = 0;
  int32_t symbol = 0;
  int32_t currency = 0;

  // open positions state (for margin trades only)
  PositionDirection direction = PositionDirection::EMPTY;
  int64_t openVolume = 0;
  int64_t openPriceSum = 0;
  int64_t profit = 0;

  // pending orders total size
  // increment before sending order to matching engine
  // decrement after receiving trade confirmation from matching engine
  int64_t pendingSellSize = 0;
  int64_t pendingBuySize = 0;

  SymbolPositionRecord() = default;

  SymbolPositionRecord(int64_t uid, int32_t symbol, int32_t currency);

  /**
   * Constructor from BytesIn (deserialization)
   */
  SymbolPositionRecord(int64_t uid, common::BytesIn& bytes);

  void Initialize(int64_t uid, int32_t symbol, int32_t currency);

  /**
   * Check if position is empty (no pending orders, no open trades) - can remove
   * it from hashmap
   */
  bool IsEmpty() const;

  void PendingHold(OrderAction orderAction, int64_t size);
  void PendingRelease(OrderAction orderAction, int64_t size);

  int64_t EstimateProfit(const CoreSymbolSpecification& spec,
                         const processors::LastPriceCacheRecord* lastPriceCacheRecord) const;

  /**
   * Calculate required margin based on specification and current
   * position/orders
   */
  int64_t CalculateRequiredMarginForFutures(const CoreSymbolSpecification& spec) const;

  /**
   * Calculate required margin based on specification and current
   * position/orders considering extra size added to current position (or
   * outstanding orders)
   */
  int64_t CalculateRequiredMarginForOrder(const CoreSymbolSpecification& spec,
                                          OrderAction action,
                                          int64_t size) const;

  /**
   * Update position for one user
   * 1. Un-hold pending size
   * 2. Reduce opposite position accordingly (if exists)
   * 3. Increase forward position accordingly (if size left in the trading
   * event)
   */
  int64_t UpdatePositionForMarginTrade(OrderAction action, int64_t size, int64_t price);

  void Reset();

  void ValidateInternalState() const;

  // StateHash interface implementation
  int32_t GetStateHash() const override;

  // WriteBytesMarshallable interface
  void WriteMarshallable(BytesOut& bytes) const override;

  std::string ToString() const;

private:
  int64_t CloseCurrentPositionFutures(OrderAction action, int64_t tradeSize, int64_t tradePrice);
  void OpenPositionMargin(OrderAction action, int64_t sizeToOpen, int64_t tradePrice);
};

}  // namespace common
}  // namespace core
}  // namespace exchange
