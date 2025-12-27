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
 * ITExchangeCoreIntegrationStress - abstract base class for stress tests
 * Tests high-load scenarios with multiple symbols and users
 */
class ITExchangeCoreIntegrationStress : public ::testing::Test {
public:
  ITExchangeCoreIntegrationStress() = default;
  virtual ~ITExchangeCoreIntegrationStress() = default;

  /**
   * Get performance configuration (must be implemented by child class)
   */
  virtual exchange::core::common::config::PerformanceConfiguration
  GetPerformanceConfiguration() = 0;

  /**
   * Many operations test (matches Java manyOperations method)
   */
  void ManyOperations(
      const exchange::core::common::CoreSymbolSpecification &symbolSpec);
};

} // namespace integration
} // namespace tests
} // namespace core
} // namespace exchange
