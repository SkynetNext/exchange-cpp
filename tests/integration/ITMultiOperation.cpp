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

#include <exchange/core/common/config/InitialStateConfiguration.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <exchange/core/common/config/SerializationConfiguration.h>
#include <gtest/gtest.h>
#include "../util/TestConstants.h"
#include "../util/TestDataParameters.h"
#include "../util/TestOrdersGeneratorConfig.h"
#include "../util/ThroughputTestsModule.h"

using namespace exchange::core::tests::util;

namespace exchange {
namespace core {
namespace tests {
namespace integration {

// Helper function to create throughput performance configuration
static exchange::core::common::config::PerformanceConfiguration
CreateThroughputPerfCfg(int matchingEnginesNum, int riskEnginesNum) {
  // Use ThroughputPerformanceBuilder() to get AffinityThreadFactory (matches
  // Java version)
  auto cfg =
    exchange::core::common::config::PerformanceConfiguration::ThroughputPerformanceBuilder();
  cfg.matchingEnginesNum = matchingEnginesNum;
  cfg.riskEnginesNum = riskEnginesNum;
  return cfg;
}

TEST(ITMultiOperation, ShouldPerformMarginOperations) {
  auto perfCfg = CreateThroughputPerfCfg(1, 1);

  // Match Java: TestDataParameters.builder()
  //   .totalTransactionsNumber(1_000_000)
  //   .targetOrderBookOrdersTotal(1000)
  //   .numAccounts(2000)
  //   .currenciesAllowed(TestConstants.CURRENCIES_FUTURES)
  //   .numSymbols(1)
  //   .allowedSymbolTypes(ExchangeTestContainer.AllowedSymbolTypes.FUTURES_CONTRACT)
  //   .preFillMode(TestOrdersGeneratorConfig.PreFillMode.ORDERS_NUMBER)
  //   .build()
  TestDataParameters testParams;
  testParams.totalTransactionsNumber = 1'000'000;
  testParams.targetOrderBookOrdersTotal = 1000;
  testParams.numAccounts = 2000;
  testParams.currenciesAllowed = TestConstants::GetCurrenciesFutures();
  testParams.numSymbols = 1;
  testParams.allowedSymbolTypes = AllowedSymbolTypes::FUTURES_CONTRACT;
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER;
  testParams.avalancheIOC = false;

  ThroughputTestsModule::ThroughputTestImpl(
    perfCfg, testParams, exchange::core::common::config::InitialStateConfiguration::CleanTest(),
    exchange::core::common::config::SerializationConfiguration::Default(), 2);
}

TEST(ITMultiOperation, ShouldPerformExchangeOperations) {
  auto perfCfg = CreateThroughputPerfCfg(1, 1);

  // Match Java: TestDataParameters.builder()
  //   .totalTransactionsNumber(1_000_000)
  //   .targetOrderBookOrdersTotal(1000)
  //   .numAccounts(2000)
  //   .currenciesAllowed(TestConstants.CURRENCIES_EXCHANGE)
  //   .numSymbols(1)
  //   .allowedSymbolTypes(ExchangeTestContainer.AllowedSymbolTypes.CURRENCY_EXCHANGE_PAIR)
  //   .preFillMode(TestOrdersGeneratorConfig.PreFillMode.ORDERS_NUMBER)
  //   .build()
  TestDataParameters testParams;
  testParams.totalTransactionsNumber = 1'000'000;
  testParams.targetOrderBookOrdersTotal = 1000;
  testParams.numAccounts = 2000;
  testParams.currenciesAllowed = TestConstants::GetCurrenciesExchange();
  testParams.numSymbols = 1;
  testParams.allowedSymbolTypes = AllowedSymbolTypes::CURRENCY_EXCHANGE_PAIR;
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER;
  testParams.avalancheIOC = false;

  ThroughputTestsModule::ThroughputTestImpl(
    perfCfg, testParams, exchange::core::common::config::InitialStateConfiguration::CleanTest(),
    exchange::core::common::config::SerializationConfiguration::Default(), 2);
}

TEST(ITMultiOperation, ShouldPerformSharded) {
  auto perfCfg = CreateThroughputPerfCfg(2, 2);

  // Match Java: TestDataParameters.builder()
  //   .totalTransactionsNumber(1_000_000)
  //   .targetOrderBookOrdersTotal(1000)
  //   .numAccounts(2000)
  //   .currenciesAllowed(TestConstants.CURRENCIES_EXCHANGE)
  //   .numSymbols(32)
  //   .allowedSymbolTypes(ExchangeTestContainer.AllowedSymbolTypes.BOTH)
  //   .preFillMode(TestOrdersGeneratorConfig.PreFillMode.ORDERS_NUMBER)
  //   .build()
  TestDataParameters testParams;
  testParams.totalTransactionsNumber = 1'000'000;
  testParams.targetOrderBookOrdersTotal = 1000;
  testParams.numAccounts = 2000;
  testParams.currenciesAllowed = TestConstants::GetCurrenciesExchange();
  testParams.numSymbols = 32;
  testParams.allowedSymbolTypes = AllowedSymbolTypes::BOTH;
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER;
  testParams.avalancheIOC = false;

  ThroughputTestsModule::ThroughputTestImpl(
    perfCfg, testParams, exchange::core::common::config::InitialStateConfiguration::CleanTest(),
    exchange::core::common::config::SerializationConfiguration::Default(), 2);
}

}  // namespace integration
}  // namespace tests
}  // namespace core
}  // namespace exchange
