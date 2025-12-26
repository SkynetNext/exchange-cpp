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

#pragma once

#include "TestDataParameters.h"
#include <exchange/core/common/config/PerformanceConfiguration.h>

namespace exchange {
namespace core {
namespace tests {
namespace util {

/**
 * JournalingTestsModule - module for journaling performance testing
 */
class JournalingTestsModule {
public:
  /**
   * Run journaling test implementation
   * @param performanceCfg - performance configuration
   * @param testDataParameters - test data parameters
   * @param iterations - number of test iterations
   */
  static void JournalingTestImpl(
      const exchange::core::common::config::PerformanceConfiguration
          &performanceCfg,
      const TestDataParameters &testDataParameters,
      int iterations);
};

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange

