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

#pragma once

#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <gtest/gtest.h>

namespace exchange {
namespace core {
namespace tests {
namespace integration {

/**
 * ITExchangeCoreIntegration - abstract base class for integration tests
 * Provides common test methods and requires performance configuration from
 * child classes
 */
class ITExchangeCoreIntegration : public ::testing::Test {
public:
  ITExchangeCoreIntegration() = default;
  virtual ~ITExchangeCoreIntegration() = default;

  /**
   * Get performance configuration (must be implemented by child class)
   */
  virtual exchange::core::common::config::PerformanceConfiguration
  GetPerformanceConfiguration() = 0;

  /**
   * Basic full cycle test - tests complete order lifecycle
   */
  void BasicFullCycleTest(
      const exchange::core::common::CoreSymbolSpecification &symbolSpec);

  /**
   * Test initialization of symbols
   */
  void ShouldInitSymbols();

  /**
   * Test initialization of users
   */
  void ShouldInitUsers();

  /**
   * Exchange risk basic test - tests risk management and order rejection
   */
  void ExchangeRiskBasicTest();

  /**
   * Exchange cancel bid test - tests order cancellation
   */
  void ExchangeCancelBid();

  /**
   * Exchange risk move test - tests risk management for order moves
   */
  void ExchangeRiskMoveTest();
};

} // namespace integration
} // namespace tests
} // namespace core
} // namespace exchange
