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

#include "PerfPersistence.h"
#include "../util/PersistenceTestsModule.h"
#include "../util/TestDataParameters.h"
#include "../util/TestOrdersGeneratorConfig.h"
#include <exchange/core/common/config/PerformanceConfiguration.h>

using namespace exchange::core::tests::util;

namespace exchange {
namespace core {
namespace tests {
namespace perf {

void PerfPersistence::TestPersistenceMargin() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 1;
  perfCfg.riskEnginesNum = 1;
  perfCfg.msgsInGroupLimit = 512;

  auto testParams = TestDataParameters::SinglePairMargin();
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER_PLUS_QUARTER;

  PersistenceTestsModule::PersistenceTestImpl(perfCfg, testParams, 10);
}

void PerfPersistence::TestPersistenceExchange() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 1;
  perfCfg.riskEnginesNum = 1;
  perfCfg.msgsInGroupLimit = 512;

  auto testParams = TestDataParameters::SinglePairExchange();
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER_PLUS_QUARTER;

  PersistenceTestsModule::PersistenceTestImpl(perfCfg, testParams, 10);
}

void PerfPersistence::TestPersistenceMultiSymbolMedium() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 2;
  perfCfg.msgsInGroupLimit = 1024;

  auto testParams = TestDataParameters::Medium();
  testParams.allowedSymbolTypes = AllowedSymbolTypes::BOTH;
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER_PLUS_QUARTER;

  PersistenceTestsModule::PersistenceTestImpl(perfCfg, testParams, 25);
}

void PerfPersistence::TestPersistenceMultiSymbolLarge() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 4;
  perfCfg.msgsInGroupLimit = 1024;

  auto testParams = TestDataParameters::Large();
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER_PLUS_QUARTER;

  PersistenceTestsModule::PersistenceTestImpl(perfCfg, testParams, 25);
}

void PerfPersistence::TestPersistenceMultiSymbolHuge() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      ThroughputPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 4;
  perfCfg.msgsInGroupLimit = 1024;

  auto testParams = TestDataParameters::Huge();
  testParams.preFillMode = PreFillMode::ORDERS_NUMBER_PLUS_QUARTER;

  PersistenceTestsModule::PersistenceTestImpl(perfCfg, testParams, 25);
}

// Register tests
TEST_F(PerfPersistence, TestPersistenceMargin) { TestPersistenceMargin(); }
TEST_F(PerfPersistence, TestPersistenceExchange) { TestPersistenceExchange(); }
TEST_F(PerfPersistence, TestPersistenceMultiSymbolMedium) {
  TestPersistenceMultiSymbolMedium();
}
TEST_F(PerfPersistence, TestPersistenceMultiSymbolLarge) {
  TestPersistenceMultiSymbolLarge();
}
// Disabled by default - requires 12+ threads CPU, 32GB RAM, and takes hours to
// complete Run with: --gtest_also_run_disabled_tests to enable
TEST_F(PerfPersistence, DISABLED_TestPersistenceMultiSymbolHuge) {
  TestPersistenceMultiSymbolHuge();
}

} // namespace perf
} // namespace tests
} // namespace core
} // namespace exchange
