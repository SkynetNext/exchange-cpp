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

#include "../util/TestDataParameters.h"
#include "../util/ThroughputTestsModule.h"
#include <exchange/core/common/config/InitialStateConfiguration.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <exchange/core/common/config/SerializationConfiguration.h>
#include <gtest/gtest.h>

using namespace exchange::core2::tests::util;

namespace exchange {
namespace core2 {
namespace tests {
namespace integration {

// Helper function to create throughput performance configuration
static exchange::core::common::config::PerformanceConfiguration
CreateThroughputPerfCfg(int matchingEnginesNum, int riskEnginesNum) {
  // Note: C++ version doesn't have builder pattern, so we use Default() and
  // modify fields
  auto cfg =
      exchange::core::common::config::PerformanceConfiguration::Default();
  cfg.matchingEnginesNum = matchingEnginesNum;
  cfg.riskEnginesNum = riskEnginesNum;
  return cfg;
}

TEST(ITMultiOperation, ShouldPerformMarginOperations) {
  auto perfCfg = CreateThroughputPerfCfg(1, 1);

  auto testParams = TestDataParameters::SinglePairMargin();

  ThroughputTestsModule::ThroughputTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(), 2);
}

TEST(ITMultiOperation, ShouldPerformExchangeOperations) {
  auto perfCfg = CreateThroughputPerfCfg(1, 1);

  auto testParams = TestDataParameters::SinglePairExchange();

  ThroughputTestsModule::ThroughputTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(), 2);
}

TEST(ITMultiOperation, ShouldPerformSharded) {
  auto perfCfg = CreateThroughputPerfCfg(2, 2);

  auto testParams = TestDataParameters::SinglePairExchange();
  testParams.numSymbols = 32;
  testParams.allowedSymbolTypes = AllowedSymbolTypes::BOTH;

  ThroughputTestsModule::ThroughputTestImpl(
      perfCfg, testParams,
      exchange::core::common::config::InitialStateConfiguration::CleanTest(),
      exchange::core::common::config::SerializationConfiguration::Default(), 2);
}

} // namespace integration
} // namespace tests
} // namespace core2
} // namespace exchange
