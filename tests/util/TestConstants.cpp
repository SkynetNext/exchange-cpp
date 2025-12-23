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

#include "TestConstants.h"
#include <stdexcept>

namespace exchange {
namespace core2 {
namespace tests {
namespace util {

exchange::core::common::CoreSymbolSpecification
TestConstants::CreateSymbolSpecEurUsd() {
  exchange::core::common::CoreSymbolSpecification spec;
  spec.symbolId = SYMBOL_MARGIN;
  spec.type = exchange::core::common::SymbolType::FUTURES_CONTRACT;
  spec.baseCurrency = CURRENCY_EUR;
  spec.quoteCurrency = CURRENCY_USD;
  spec.baseScaleK = 1;
  spec.quoteScaleK = 1;
  spec.marginBuy = 2200;
  spec.marginSell = 3210;
  spec.takerFee = 0;
  spec.makerFee = 0;
  return spec;
}

exchange::core::common::CoreSymbolSpecification
TestConstants::CreateSymbolSpecFeeUsdJpy() {
  exchange::core::common::CoreSymbolSpecification spec;
  spec.symbolId = SYMBOL_MARGIN;
  spec.type = exchange::core::common::SymbolType::FUTURES_CONTRACT;
  spec.baseCurrency = CURRENCY_USD;
  spec.quoteCurrency = CURRENCY_JPY;
  spec.baseScaleK = 1'000'00; // 1K USD "micro" lot
  spec.quoteScaleK = 10;      // 10 JPY step
  spec.marginBuy = 5'000;     // effective leverage ~21
  spec.marginSell = 6'000;    // effective leverage ~18
  spec.takerFee = 3;
  spec.makerFee = 2;
  return spec;
}

exchange::core::common::CoreSymbolSpecification
TestConstants::CreateSymbolSpecEthXbt() {
  exchange::core::common::CoreSymbolSpecification spec;
  spec.symbolId = SYMBOL_EXCHANGE;
  spec.type = exchange::core::common::SymbolType::CURRENCY_EXCHANGE_PAIR;
  spec.baseCurrency = CURRENCY_ETH;  // base = szabo
  spec.quoteCurrency = CURRENCY_XBT; // quote = satoshi
  spec.baseScaleK = 100'000;         // 1 lot = 100K szabo (0.1 ETH)
  spec.quoteScaleK = 10;             // 1 step = 10 satoshi
  spec.takerFee = 0;
  spec.makerFee = 0;
  return spec;
}

exchange::core::common::CoreSymbolSpecification
TestConstants::CreateSymbolSpecFeeXbtLtc() {
  exchange::core::common::CoreSymbolSpecification spec;
  spec.symbolId = SYMBOL_EXCHANGE_FEE;
  spec.type = exchange::core::common::SymbolType::CURRENCY_EXCHANGE_PAIR;
  spec.baseCurrency = CURRENCY_XBT;  // base = satoshi
  spec.quoteCurrency = CURRENCY_LTC; // quote = litoshi
  spec.baseScaleK = 1'000'000;       // 1 lot = 1M satoshi (0.01 BTC)
  spec.quoteScaleK = 10'000;         // 1 step = 10K litoshi
  spec.takerFee = 1900;              // taker fee 1900 litoshi per 1 lot
  spec.makerFee = 700;               // maker fee 700 litoshi per 1 lot
  return spec;
}

int32_t TestConstants::GetCurrency(const std::string &currency) {
  if (currency == "USD") {
    return CURRENCY_USD;
  } else if (currency == "XBT") {
    return CURRENCY_XBT;
  } else if (currency == "ETH") {
    return CURRENCY_ETH;
  }
  throw std::runtime_error("Unknown currency [" + currency + "]");
}

} // namespace util
} // namespace tests
} // namespace core2
} // namespace exchange
