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

#include "PerfLatencyJournaling.h"
#include <exchange/core/common/config/InitialStateConfiguration.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <exchange/core/common/config/SerializationConfiguration.h>
#include "../util/ExchangeTestContainer.h"
#include "../util/LatencyTestsModule.h"
#include "../util/TestDataParameters.h"

using namespace exchange::core::tests::util;

namespace exchange::core::tests::perf {

void PerfLatencyJournaling::TestLatencyMarginJournaling() {
  auto perfCfg =
    exchange::core::common::config::PerformanceConfiguration::LatencyPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 1;
  perfCfg.riskEnginesNum = 1;
  perfCfg.msgsInGroupLimit = 256;

  auto testParams = TestDataParameters::SinglePairMargin();

  LatencyTestsModule::LatencyTestImpl(
    perfCfg, testParams,
    exchange::core::common::config::InitialStateConfiguration::CleanStartJournaling(
      ExchangeTestContainer::TimeBasedExchangeId()),
    exchange::core::common::config::SerializationConfiguration::DiskJournaling(), 6);
}

void PerfLatencyJournaling::TestLatencyExchangeJournaling() {
  auto perfCfg =
    exchange::core::common::config::PerformanceConfiguration::LatencyPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 1;
  perfCfg.riskEnginesNum = 1;
  perfCfg.msgsInGroupLimit = 256;

  auto testParams = TestDataParameters::SinglePairExchange();

  LatencyTestsModule::LatencyTestImpl(
    perfCfg, testParams,
    exchange::core::common::config::InitialStateConfiguration::CleanStartJournaling(
      ExchangeTestContainer::TimeBasedExchangeId()),
    exchange::core::common::config::SerializationConfiguration::DiskJournaling(), 6);
}

void PerfLatencyJournaling::TestLatencyMultiSymbolMediumJournaling() {
  auto perfCfg =
    exchange::core::common::config::PerformanceConfiguration::LatencyPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 2;
  perfCfg.msgsInGroupLimit = 256;

  auto testParams = TestDataParameters::Medium();

  LatencyTestsModule::LatencyTestImpl(
    perfCfg, testParams,
    exchange::core::common::config::InitialStateConfiguration::CleanStartJournaling(
      ExchangeTestContainer::TimeBasedExchangeId()),
    exchange::core::common::config::SerializationConfiguration::DiskJournaling(), 3);
}

void PerfLatencyJournaling::TestLatencyMultiSymbolLargeJournaling() {
  auto perfCfg =
    exchange::core::common::config::PerformanceConfiguration::LatencyPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 2;
  perfCfg.msgsInGroupLimit = 256;

  auto testParams = TestDataParameters::Large();

  LatencyTestsModule::LatencyTestImpl(
    perfCfg, testParams,
    exchange::core::common::config::InitialStateConfiguration::CleanStartJournaling(
      ExchangeTestContainer::TimeBasedExchangeId()),
    exchange::core::common::config::SerializationConfiguration::DiskJournaling(), 3);
}

void PerfLatencyJournaling::TestLatencyMultiSymbolHugeJournaling() {
  auto perfCfg =
    exchange::core::common::config::PerformanceConfiguration::LatencyPerformanceBuilder();
  perfCfg.ringBufferSize = 64 * 1024;
  perfCfg.matchingEnginesNum = 4;
  perfCfg.riskEnginesNum = 2;
  perfCfg.msgsInGroupLimit = 256;

  auto testParams = TestDataParameters::Huge();

  LatencyTestsModule::LatencyTestImpl(
    perfCfg, testParams,
    exchange::core::common::config::InitialStateConfiguration::CleanStartJournaling(
      ExchangeTestContainer::TimeBasedExchangeId()),
    exchange::core::common::config::SerializationConfiguration::DiskJournaling(), 2);
}

// Register tests
TEST_F(PerfLatencyJournaling, TestLatencyMarginJournaling) {
  TestLatencyMarginJournaling();
}

TEST_F(PerfLatencyJournaling, TestLatencyExchangeJournaling) {
  TestLatencyExchangeJournaling();
}

TEST_F(PerfLatencyJournaling, TestLatencyMultiSymbolMediumJournaling) {
  TestLatencyMultiSymbolMediumJournaling();
}

TEST_F(PerfLatencyJournaling, TestLatencyMultiSymbolLargeJournaling) {
  TestLatencyMultiSymbolLargeJournaling();
}

// Disabled by default - requires 12+ threads CPU, 32GB RAM, and takes hours to
// complete Run with: --gtest_also_run_disabled_tests to enable
TEST_F(PerfLatencyJournaling, DISABLED_TestLatencyMultiSymbolHugeJournaling) {
  TestLatencyMultiSymbolHugeJournaling();
}

}  // namespace exchange::core::tests::perf
