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

#include "TestDataParameters.h"
#include "TestConstants.h"

namespace exchange::core::tests::util {

TestDataParameters TestDataParameters::SinglePairMargin() {
  TestDataParameters params;
  params.totalTransactionsNumber = 3'000'000;
  params.targetOrderBookOrdersTotal = 1000;
  params.numAccounts = 2000;
  params.currenciesAllowed = TestConstants::GetCurrenciesFutures();
  params.numSymbols = 1;
  params.allowedSymbolTypes = AllowedSymbolTypes::FUTURES_CONTRACT;
  params.preFillMode = PreFillMode::ORDERS_NUMBER;
  params.avalancheIOC = false;
  return params;
}

TestDataParameters TestDataParameters::SinglePairExchange() {
  TestDataParameters params;
  params.totalTransactionsNumber = 3'000'000;  // Match Java: 3_000_000
  params.targetOrderBookOrdersTotal = 1000;
  params.numAccounts = 2000;
  params.currenciesAllowed = TestConstants::GetCurrenciesExchange();
  params.numSymbols = 1;
  params.allowedSymbolTypes = AllowedSymbolTypes::CURRENCY_EXCHANGE_PAIR;
  params.preFillMode = PreFillMode::ORDERS_NUMBER;
  params.avalancheIOC = false;
  return params;
}

TestDataParameters TestDataParameters::Medium() {
  TestDataParameters params;
  params.totalTransactionsNumber = 3'000'000;
  params.targetOrderBookOrdersTotal = 1'000'000;
  params.numAccounts = 3'300'000;
  params.currenciesAllowed = TestConstants::GetAllCurrencies();
  params.numSymbols = 10'000;
  params.allowedSymbolTypes = AllowedSymbolTypes::BOTH;
  params.preFillMode = PreFillMode::ORDERS_NUMBER;
  params.avalancheIOC = false;
  return params;
}

TestDataParameters TestDataParameters::Large() {
  TestDataParameters params;
  params.totalTransactionsNumber = 3'000'000;
  params.targetOrderBookOrdersTotal = 3'000'000;
  params.numAccounts = 10'000'000;
  params.currenciesAllowed = TestConstants::GetAllCurrencies();
  params.numSymbols = 50'000;
  params.allowedSymbolTypes = AllowedSymbolTypes::BOTH;
  params.preFillMode = PreFillMode::ORDERS_NUMBER;
  params.avalancheIOC = false;
  return params;
}

TestDataParameters TestDataParameters::Huge() {
  TestDataParameters params;
  params.totalTransactionsNumber = 10'000'000;
  params.targetOrderBookOrdersTotal = 30'000'000;
  params.numAccounts = 33'000'000;
  params.currenciesAllowed = TestConstants::GetAllCurrencies();
  params.numSymbols = 100'000;
  params.allowedSymbolTypes = AllowedSymbolTypes::BOTH;
  params.preFillMode = PreFillMode::ORDERS_NUMBER;
  params.avalancheIOC = false;
  return params;
}

}  // namespace exchange::core::tests::util
