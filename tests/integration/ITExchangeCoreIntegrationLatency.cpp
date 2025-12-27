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

#include "ITExchangeCoreIntegrationLatency.h"
#include "../util/TestConstants.h"
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <gtest/gtest.h>

using namespace exchange::core::tests::util;

namespace exchange {
namespace core {
namespace tests {
namespace integration {

ITExchangeCoreIntegrationLatency::ITExchangeCoreIntegrationLatency()
    : ITExchangeCoreIntegration() {}

exchange::core::common::config::PerformanceConfiguration
ITExchangeCoreIntegrationLatency::GetPerformanceConfiguration() {
  // Match Java: PerformanceConfiguration.latencyPerformanceBuilder().build()
  return exchange::core::common::config::PerformanceConfiguration::
      LatencyPerformanceBuilder();
}

// Register tests (same as Basic version but with latency configuration)
TEST_F(ITExchangeCoreIntegrationLatency, BasicFullCycleTestMargin) {
  BasicFullCycleTest(TestConstants::SYMBOLSPEC_EUR_USD());
}

TEST_F(ITExchangeCoreIntegrationLatency, BasicFullCycleTestExchange) {
  BasicFullCycleTest(TestConstants::SYMBOLSPEC_ETH_XBT());
}

TEST_F(ITExchangeCoreIntegrationLatency, ShouldInitSymbols) {
  ShouldInitSymbols();
}

TEST_F(ITExchangeCoreIntegrationLatency, ShouldInitUsers) { ShouldInitUsers(); }

TEST_F(ITExchangeCoreIntegrationLatency, ExchangeRiskBasicTest) {
  ExchangeRiskBasicTest();
}

TEST_F(ITExchangeCoreIntegrationLatency, ExchangeCancelBid) {
  ExchangeCancelBid();
}

TEST_F(ITExchangeCoreIntegrationLatency, ExchangeRiskMoveTest) {
  ExchangeRiskMoveTest();
}

} // namespace integration
} // namespace tests
} // namespace core
} // namespace exchange
