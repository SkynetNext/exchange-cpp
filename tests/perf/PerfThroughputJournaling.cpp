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

#include "PerfThroughputJournaling.h"
#include <exchange/core/common/config/InitialStateConfiguration.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <exchange/core/common/config/SerializationConfiguration.h>
#include "../util/ExchangeTestContainer.h"
#include "../util/TestDataParameters.h"
#include "../util/ThroughputTestsModule.h"

using namespace exchange::core::tests::util;

namespace exchange {
namespace core {
namespace tests {
namespace perf {

void PerfThroughputJournaling::TestThroughputMargin() {
  auto perfCfg =
    exchange::core::common::config::PerformanceConfiguration::ThroughputPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 1;
  perfCfg.riskEnginesNum = 1;
  perfCfg.msgsInGroupLimit = 1536;

  auto testParams = TestDataParameters::SinglePairMargin();

  ThroughputTestsModule::ThroughputTestImpl(
    perfCfg, testParams,
    exchange::core::common::config::InitialStateConfiguration::CleanStartJournaling(
      ExchangeTestContainer::TimeBasedExchangeId()),
    exchange::core::common::config::SerializationConfiguration::DiskJournaling(), 50);
}

void PerfThroughputJournaling::TestThroughputExchange() {
  auto perfCfg =
    exchange::core::common::config::PerformanceConfiguration::ThroughputPerformanceBuilder();
  perfCfg.ringBufferSize = 32 * 1024;
  perfCfg.matchingEnginesNum = 1;
  perfCfg.riskEnginesNum = 1;
  perfCfg.msgsInGroupLimit = 1536;

  auto testParams = TestDataParameters::SinglePairExchange();

  ThroughputTestsModule::ThroughputTestImpl(
    perfCfg, testParams,
    exchange::core::common::config::InitialStateConfiguration::CleanStartJournaling(
      ExchangeTestContainer::TimeBasedExchangeId()),
    exchange::core::common::config::SerializationConfiguration::DiskJournaling(), 50);
}

void PerfThroughputJournaling::TestThroughputMultiSymbolMedium() {
  auto perfCfg =
    exchange::core::common::config::PerformanceConfiguration::ThroughputPerformanceBuilder();

  auto testParams = TestDataParameters::Medium();

  ThroughputTestsModule::ThroughputTestImpl(
    perfCfg, testParams,
    exchange::core::common::config::InitialStateConfiguration::CleanStartJournaling(
      ExchangeTestContainer::TimeBasedExchangeId()),
    exchange::core::common::config::SerializationConfiguration::DiskJournaling(), 25);
}

void PerfThroughputJournaling::TestThroughputMultiSymbolLarge() {
  auto perfCfg =
    exchange::core::common::config::PerformanceConfiguration::ThroughputPerformanceBuilder();

  auto testParams = TestDataParameters::Large();

  ThroughputTestsModule::ThroughputTestImpl(
    perfCfg, testParams,
    exchange::core::common::config::InitialStateConfiguration::CleanStartJournaling(
      ExchangeTestContainer::TimeBasedExchangeId()),
    exchange::core::common::config::SerializationConfiguration::DiskJournaling(), 25);
}

void PerfThroughputJournaling::TestThroughputMultiSymbolHuge() {
  auto perfCfg =
    exchange::core::common::config::PerformanceConfiguration::ThroughputPerformanceBuilder();

  auto testParams = TestDataParameters::Huge();

  ThroughputTestsModule::ThroughputTestImpl(
    perfCfg, testParams,
    exchange::core::common::config::InitialStateConfiguration::CleanStartJournaling(
      ExchangeTestContainer::TimeBasedExchangeId()),
    exchange::core::common::config::SerializationConfiguration::DiskJournaling(), 25);
}

// Register tests
TEST_F(PerfThroughputJournaling, TestThroughputMargin) {
  TestThroughputMargin();
}

TEST_F(PerfThroughputJournaling, TestThroughputExchange) {
  TestThroughputExchange();
}

TEST_F(PerfThroughputJournaling, TestThroughputMultiSymbolMedium) {
  TestThroughputMultiSymbolMedium();
}

TEST_F(PerfThroughputJournaling, TestThroughputMultiSymbolLarge) {
  TestThroughputMultiSymbolLarge();
}

// Disabled by default - requires 12+ threads CPU, 32GB RAM, and takes hours to
// complete Run with: --gtest_also_run_disabled_tests to enable
TEST_F(PerfThroughputJournaling, DISABLED_TestThroughputMultiSymbolHuge) {
  TestThroughputMultiSymbolHuge();
}

}  // namespace perf
}  // namespace tests
}  // namespace core
}  // namespace exchange
