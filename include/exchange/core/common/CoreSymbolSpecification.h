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
#include <string>
#include "BytesIn.h"
#include "StateHash.h"
#include "SymbolType.h"
#include "WriteBytesMarshallable.h"

namespace exchange {
namespace core {
namespace common {

class CoreSymbolSpecification : public StateHash, public WriteBytesMarshallable {
public:
  int32_t symbolId = 0;
  SymbolType type = SymbolType::CURRENCY_EXCHANGE_PAIR;

  // currency pair specification
  int32_t baseCurrency = 0;   // base currency
  int32_t quoteCurrency = 0;  // quote/counter currency (OR futures contract currency)
  int64_t baseScaleK = 0;     // base currency amount multiplier (lot size in base currency units)
  int64_t quoteScaleK = 0;  // quote currency amount multiplier (step size in quote currency units)

  // fees per lot in quote currency units
  int64_t takerFee = 0;  // TODO check invariant: taker fee is not less than maker fee
  int64_t makerFee = 0;

  // margin settings (for type=FUTURES_CONTRACT only)
  int64_t marginBuy = 0;   // buy margin (quote currency)
  int64_t marginSell = 0;  // sell margin (quote currency)

  CoreSymbolSpecification() = default;

  CoreSymbolSpecification(int32_t symbolId,
                          SymbolType type,
                          int32_t baseCurrency,
                          int32_t quoteCurrency,
                          int64_t baseScaleK,
                          int64_t quoteScaleK,
                          int64_t takerFee,
                          int64_t makerFee,
                          int64_t marginBuy,
                          int64_t marginSell);

  /**
   * Constructor from BytesIn (deserialization)
   */
  CoreSymbolSpecification(BytesIn& bytes);

  // StateHash interface implementation
  int32_t GetStateHash() const override;

  // WriteBytesMarshallable interface
  void WriteMarshallable(BytesOut& bytes) const override;

  std::string ToString() const;
};

}  // namespace common
}  // namespace core
}  // namespace exchange
