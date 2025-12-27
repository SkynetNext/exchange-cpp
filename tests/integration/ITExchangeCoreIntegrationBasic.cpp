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

#include "ITExchangeCoreIntegrationBasic.h"
#include "../util/TestConstants.h"
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <gtest/gtest.h>

using namespace exchange::core::tests::util;

namespace exchange {
namespace core {
namespace tests {
namespace integration {

ITExchangeCoreIntegrationBasic::ITExchangeCoreIntegrationBasic()
    : ITExchangeCoreIntegration() {}

exchange::core::common::config::PerformanceConfiguration
ITExchangeCoreIntegrationBasic::GetPerformanceConfiguration() {
  return exchange::core::common::config::PerformanceConfiguration::Default();
}

// Register tests
TEST_F(ITExchangeCoreIntegrationBasic, BasicFullCycleTestMargin) {
  BasicFullCycleTest(TestConstants::SYMBOLSPEC_EUR_USD());
}

TEST_F(ITExchangeCoreIntegrationBasic, BasicFullCycleTestExchange) {
  BasicFullCycleTest(TestConstants::SYMBOLSPEC_ETH_XBT());
}

TEST_F(ITExchangeCoreIntegrationBasic, ShouldInitSymbols) {
  ShouldInitSymbols();
}

TEST_F(ITExchangeCoreIntegrationBasic, ShouldInitUsers) { ShouldInitUsers(); }

TEST_F(ITExchangeCoreIntegrationBasic, ExchangeRiskBasicTest) {
  ExchangeRiskBasicTest();
}

TEST_F(ITExchangeCoreIntegrationBasic, ExchangeCancelBid) {
  ExchangeCancelBid();
}

TEST_F(ITExchangeCoreIntegrationBasic, ExchangeRiskMoveTest) {
  ExchangeRiskMoveTest();
}

} // namespace integration
} // namespace tests
} // namespace core
} // namespace exchange
