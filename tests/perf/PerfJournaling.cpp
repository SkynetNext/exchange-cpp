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

#include "PerfJournaling.h"
#include "../util/JournalingTestsModule.h"
#include "../util/TestConstants.h"
#include "../util/TestDataParameters.h"
#include "../util/TestOrdersGeneratorConfig.h"
#include <exchange/core/common/config/PerformanceConfiguration.h>

using namespace exchange::core::tests::util;

namespace exchange {
namespace core {
namespace tests {
namespace perf {

void PerfJournaling::TestJournalingMargin() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.matchingEnginesNum = 1;
  perfCfg.riskEnginesNum = 1;

  auto testParams = TestDataParameters::SinglePairMargin();
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER_PLUS_QUARTER;

  JournalingTestsModule::JournalingTestImpl(perfCfg, testParams, 10);
}

void PerfJournaling::TestJournalingExchange() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.matchingEnginesNum = 1;
  perfCfg.riskEnginesNum = 1;

  auto testParams = TestDataParameters::SinglePairExchange();
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER_PLUS_QUARTER;

  JournalingTestsModule::JournalingTestImpl(perfCfg, testParams, 10);
}

void PerfJournaling::TestJournalingMultiSymbolSmall() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.matchingEnginesNum = 2;
  perfCfg.riskEnginesNum = 2;

  TestDataParameters testParams;
  testParams.totalTransactionsNumber = 3'000'000;
  testParams.targetOrderBookOrdersTotal = 50'000;
  testParams.numAccounts = 100'000;
  testParams.currenciesAllowed = TestConstants::GetAllCurrencies();
  testParams.numSymbols = 1'000;
  testParams.allowedSymbolTypes = AllowedSymbolTypes::BOTH;
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER_PLUS_QUARTER;
  testParams.avalancheIOC = false;

  JournalingTestsModule::JournalingTestImpl(perfCfg, testParams, 25);
}

void PerfJournaling::TestJournalingMultiSymbolMedium() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 2;

  auto testParams = TestDataParameters::Medium();
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER_PLUS_QUARTER;

  JournalingTestsModule::JournalingTestImpl(perfCfg, testParams, 25);
}

void PerfJournaling::TestJournalingMultiSymbolLarge() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 4;

  auto testParams = TestDataParameters::Large();
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER_PLUS_QUARTER;

  JournalingTestsModule::JournalingTestImpl(perfCfg, testParams, 25);
}

void PerfJournaling::TestJournalingMultiSymbolHuge() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.ringBufferSize = 128 * 1024;
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 4;
  perfCfg.msgsInGroupLimit = 1024;

  auto testParams = TestDataParameters::Huge();
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER_PLUS_QUARTER;

  JournalingTestsModule::JournalingTestImpl(perfCfg, testParams, 10);
}

// Register tests
TEST_F(PerfJournaling, TestJournalingMargin) { TestJournalingMargin(); }
TEST_F(PerfJournaling, TestJournalingExchange) { TestJournalingExchange(); }
TEST_F(PerfJournaling, TestJournalingMultiSymbolSmall) {
  TestJournalingMultiSymbolSmall();
}
TEST_F(PerfJournaling, TestJournalingMultiSymbolMedium) {
  TestJournalingMultiSymbolMedium();
}
TEST_F(PerfJournaling, TestJournalingMultiSymbolLarge) {
  TestJournalingMultiSymbolLarge();
}
// Disabled by default - requires 12+ threads CPU, 32GB RAM, and takes hours to
// complete Run with: --gtest_also_run_disabled_tests to enable
TEST_F(PerfJournaling, DISABLED_TestJournalingMultiSymbolHuge) {
  TestJournalingMultiSymbolHuge();
}

} // namespace perf
} // namespace tests
} // namespace core
} // namespace exchange
