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

#include "PerfLatency.h"
#include "../util/LatencyTestsModule.h"
#include "../util/TestDataParameters.h"
#include <exchange/core/common/config/InitialStateConfiguration.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <exchange/core/common/config/SerializationConfiguration.h>

using namespace exchange::core::tests::util;

namespace exchange {
namespace core {
namespace tests {
namespace perf {

void PerfLatency::TestLatencyMargin() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      LatencyPerformanceBuilder();
  perfCfg.ringBufferSize = 2 * 1024;
  perfCfg.matchingEnginesNum = 1;
  perfCfg.riskEnginesNum = 1;
  perfCfg.msgsInGroupLimit = 256;

  auto testParams = TestDataParameters::SinglePairMargin();

  LatencyTestsModule::LatencyTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(),
      16);
}

void PerfLatency::TestLatencyExchange() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      LatencyPerformanceBuilder();
  perfCfg.ringBufferSize = 2 * 1024;
  perfCfg.matchingEnginesNum = 1;
  perfCfg.riskEnginesNum = 1;
  perfCfg.msgsInGroupLimit = 256;

  auto testParams = TestDataParameters::SinglePairExchange();

  LatencyTestsModule::LatencyTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(),
      16);
}

void PerfLatency::TestLatencyMultiSymbolMedium() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      LatencyPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 2;
  perfCfg.msgsInGroupLimit = 256;

  auto testParams = TestDataParameters::Medium();

  LatencyTestsModule::LatencyTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(), 8);
}

void PerfLatency::TestLatencyMultiSymbolLarge() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      LatencyPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 2;
  perfCfg.msgsInGroupLimit = 256;

  auto testParams = TestDataParameters::Large();

  LatencyTestsModule::LatencyTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(), 4);
}

void PerfLatency::TestLatencyMultiSymbolHuge() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      LatencyPerformanceBuilder();
  perfCfg.ringBufferSize = 64 * 1024;
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 2;
  perfCfg.msgsInGroupLimit = 256;

  auto testParams = TestDataParameters::Huge();

  LatencyTestsModule::LatencyTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(), 2);
}

void PerfLatency::TestLatencyMarginFixed8M() {
  auto perfCfg = exchange::core::common::config::PerformanceConfiguration::
      LatencyPerformanceBuilder();
  perfCfg.ringBufferSize = 2 * 1024;
  perfCfg.matchingEnginesNum = 1;
  perfCfg.riskEnginesNum = 1;
  perfCfg.msgsInGroupLimit = 256;

  auto testParams = TestDataParameters::SinglePairMargin();

  LatencyTestsModule::LatencyTestFixedTps(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(),
      4'000'000, // Fixed 6M TPS
      16);       // 16 warmup cycles
}

// Register tests
TEST_F(PerfLatency, TestLatencyMargin) { TestLatencyMargin(); }
TEST_F(PerfLatency, TestLatencyExchange) { TestLatencyExchange(); }
TEST_F(PerfLatency, TestLatencyMultiSymbolMedium) {
  TestLatencyMultiSymbolMedium();
}
TEST_F(PerfLatency, TestLatencyMultiSymbolLarge) {
  TestLatencyMultiSymbolLarge();
}
// Disabled by default - requires 12+ threads CPU, 32GB RAM, and takes hours to
// complete Run with: --gtest_also_run_disabled_tests to enable
TEST_F(PerfLatency, DISABLED_TestLatencyMultiSymbolHuge) {
  TestLatencyMultiSymbolHuge();
}

// Fixed TPS test for accurate flame graph analysis
TEST_F(PerfLatency, TestLatencyMarginFixed8M) { TestLatencyMarginFixed8M(); }

} // namespace perf
} // namespace tests
} // namespace core
} // namespace exchange
