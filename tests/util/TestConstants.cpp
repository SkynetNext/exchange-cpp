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
#include <set>
#include <stdexcept>

namespace exchange {
namespace core {
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

// Currency sets
static const std::set<int32_t> &GetCurrenciesFuturesImpl() {
  static const std::set<int32_t> currencies = {TestConstants::CURRENCY_USD,
                                               TestConstants::CURRENCY_EUR};
  return currencies;
}

static const std::set<int32_t> &GetCurrenciesExchangeImpl() {
  static const std::set<int32_t> currencies = {TestConstants::CURRENCY_ETH,
                                               TestConstants::CURRENCY_XBT};
  return currencies;
}

static const std::set<int32_t> &GetAllCurrenciesImpl() {
  static const std::set<int32_t> currencies = {
      TestConstants::CURRENCY_AUD, TestConstants::CURRENCY_BRL,
      TestConstants::CURRENCY_CAD, TestConstants::CURRENCY_CHF,
      TestConstants::CURRENCY_CNY, TestConstants::CURRENCY_CZK,
      TestConstants::CURRENCY_DKK, TestConstants::CURRENCY_EUR,
      TestConstants::CURRENCY_GBP, TestConstants::CURRENCY_HKD,
      TestConstants::CURRENCY_JPY, TestConstants::CURRENCY_KRW,
      TestConstants::CURRENCY_MXN, TestConstants::CURRENCY_MYR,
      TestConstants::CURRENCY_NOK, TestConstants::CURRENCY_NZD,
      TestConstants::CURRENCY_PLN, TestConstants::CURRENCY_RUB,
      TestConstants::CURRENCY_SEK, TestConstants::CURRENCY_SGD,
      TestConstants::CURRENCY_THB, TestConstants::CURRENCY_TRY,
      TestConstants::CURRENCY_UAH, TestConstants::CURRENCY_USD,
      TestConstants::CURRENCY_VND, TestConstants::CURRENCY_XAG,
      TestConstants::CURRENCY_XAU, TestConstants::CURRENCY_ZAR,
      TestConstants::CURRENCY_XBT, TestConstants::CURRENCY_ETH,
      TestConstants::CURRENCY_LTC, TestConstants::CURRENCY_XDG,
      TestConstants::CURRENCY_GRC, TestConstants::CURRENCY_XPM,
      TestConstants::CURRENCY_XRP, TestConstants::CURRENCY_DASH,
      TestConstants::CURRENCY_XMR, TestConstants::CURRENCY_XLM,
      TestConstants::CURRENCY_ETC, TestConstants::CURRENCY_ZEC};
  return currencies;
}

const std::set<int32_t> &TestConstants::GetCurrenciesFutures() {
  return GetCurrenciesFuturesImpl();
}

const std::set<int32_t> &TestConstants::GetCurrenciesExchange() {
  return GetCurrenciesExchangeImpl();
}

const std::set<int32_t> &TestConstants::GetAllCurrencies() {
  return GetAllCurrenciesImpl();
}

// Static symbol specifications
static const exchange::core::common::CoreSymbolSpecification &
GetSymbolSpecEurUsdImpl() {
  static const exchange::core::common::CoreSymbolSpecification spec =
      TestConstants::CreateSymbolSpecEurUsd();
  return spec;
}

static const exchange::core::common::CoreSymbolSpecification &
GetSymbolSpecEthXbtImpl() {
  static const exchange::core::common::CoreSymbolSpecification spec =
      TestConstants::CreateSymbolSpecEthXbt();
  return spec;
}

static const exchange::core::common::CoreSymbolSpecification &
GetSymbolSpecFeeUsdJpyImpl() {
  static const exchange::core::common::CoreSymbolSpecification spec =
      TestConstants::CreateSymbolSpecFeeUsdJpy();
  return spec;
}

static const exchange::core::common::CoreSymbolSpecification &
GetSymbolSpecFeeXbtLtcImpl() {
  static const exchange::core::common::CoreSymbolSpecification spec =
      TestConstants::CreateSymbolSpecFeeXbtLtc();
  return spec;
}

const exchange::core::common::CoreSymbolSpecification &
TestConstants::GetSymbolSpecEurUsd() {
  return GetSymbolSpecEurUsdImpl();
}

const exchange::core::common::CoreSymbolSpecification &
TestConstants::GetSymbolSpecEthXbt() {
  return GetSymbolSpecEthXbtImpl();
}

const exchange::core::common::CoreSymbolSpecification &
TestConstants::GetSymbolSpecFeeUsdJpy() {
  return GetSymbolSpecFeeUsdJpyImpl();
}

const exchange::core::common::CoreSymbolSpecification &
TestConstants::GetSymbolSpecFeeXbtLtc() {
  return GetSymbolSpecFeeXbtLtcImpl();
}

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
