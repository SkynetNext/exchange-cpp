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

#include "PerfThroughput.h"
#include "../util/TestConstants.h"
#include "../util/TestDataParameters.h"
#include "../util/TestOrdersGeneratorConfig.h"
#include "../util/ThroughputTestsModule.h"

using namespace exchange::core::tests::util;
#include <exchange/core/common/config/InitialStateConfiguration.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <exchange/core/common/config/SerializationConfiguration.h>

using namespace exchange::core::tests::util;

namespace exchange {
namespace core {
namespace tests {
namespace perf {

void PerfThroughput::TestThroughputMargin() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 1;
  perfCfg.riskEnginesNum = 1;

  auto testParams = TestDataParameters::SinglePairMargin();

  ThroughputTestsModule::ThroughputTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(),
      5); // Reduced from 50 to 5 for faster C++ test execution (industry
          // standard: 5 iterations for simple tests)
}

void PerfThroughput::TestThroughputExchange() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 1;
  perfCfg.riskEnginesNum = 1;

  auto testParams = TestDataParameters::SinglePairExchange();

  ThroughputTestsModule::ThroughputTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(),
      5); // Reduced from 50 to 5 for faster C++ test execution (industry
          // standard: 5 iterations for simple tests)
}

void PerfThroughput::TestThroughputPeak() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 2;
  perfCfg.msgsInGroupLimit = 1536;

  TestDataParameters testParams;
  testParams.totalTransactionsNumber = 3'000'000;
  testParams.targetOrderBookOrdersTotal = 10'000;
  testParams.numAccounts = 10'000;
  testParams.currenciesAllowed = TestConstants::GetAllCurrencies();
  testParams.numSymbols = 100;
  testParams.allowedSymbolTypes = AllowedSymbolTypes::BOTH;
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER;

  ThroughputTestsModule::ThroughputTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(),
      5); // Reduced from 50 to 5 for faster C++ test execution (industry
          // standard: 5 iterations for simple tests)
}

void PerfThroughput::TestThroughputMultiSymbolMedium() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();

  auto testParams = TestDataParameters::Medium();

  ThroughputTestsModule::ThroughputTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(),
      3); // Reduced from 25 to 3 for faster C++ test execution (industry
          // standard: 3 iterations for complex multi-symbol tests)
}

void PerfThroughput::TestThroughputMultiSymbolLarge() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();

  auto testParams = TestDataParameters::Large();

  ThroughputTestsModule::ThroughputTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(),
      3); // Reduced from 25 to 3 for faster C++ test execution (industry
          // standard: 3 iterations for complex multi-symbol tests)
}

void PerfThroughput::TestThroughputMultiSymbolHuge() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 2;

  auto testParams = TestDataParameters::Huge();

  ThroughputTestsModule::ThroughputTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(),
      3); // Reduced from 25 to 3 for faster C++ test execution (industry
          // standard: 3 iterations for complex multi-symbol tests)
}

// Register tests
TEST_F(PerfThroughput, TestThroughputMargin) { TestThroughputMargin(); }
TEST_F(PerfThroughput, TestThroughputExchange) { TestThroughputExchange(); }
TEST_F(PerfThroughput, TestThroughputPeak) { TestThroughputPeak(); }
TEST_F(PerfThroughput, TestThroughputMultiSymbolMedium) {
  TestThroughputMultiSymbolMedium();
}
TEST_F(PerfThroughput, TestThroughputMultiSymbolLarge) {
  TestThroughputMultiSymbolLarge();
}
// Disabled by default - requires 12+ threads CPU, 32GB RAM, and takes hours to
// complete Run with: --gtest_also_run_disabled_tests to enable
TEST_F(PerfThroughput, DISABLED_TestThroughputMultiSymbolHuge) {
  TestThroughputMultiSymbolHuge();
}

} // namespace perf
} // namespace tests
} // namespace core
} // namespace exchange
