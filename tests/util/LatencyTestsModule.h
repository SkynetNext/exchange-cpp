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

#include "TestDataParameters.h"
#include <exchange/core/common/config/InitialStateConfiguration.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <exchange/core/common/config/SerializationConfiguration.h>

namespace exchange {
namespace core {
namespace tests {
namespace util {

/**
 * LatencyTestsModule - module for latency performance testing
 */
class LatencyTestsModule {
public:
  /**
   * Run latency test implementation
   * @param performanceCfg - performance configuration
   * @param testDataParameters - test data parameters
   * @param initialStateCfg - initial state configuration
   * @param serializationCfg - serialization configuration
   * @param warmupCycles - number of warmup cycles
   */
  static void LatencyTestImpl(
      const exchange::core::common::config::PerformanceConfiguration
          &performanceCfg,
      const TestDataParameters &testDataParameters,
      const exchange::core::common::config::InitialStateConfiguration
          &initialStateCfg,
      const exchange::core::common::config::SerializationConfiguration
          &serializationCfg,
      int warmupCycles);

  /**
   * Run hiccup test implementation (latency jitter testing)
   * @param performanceCfg - performance configuration
   * @param testDataParameters - test data parameters
   * @param initialStateCfg - initial state configuration
   * @param warmupCycles - number of warmup cycles
   */
  static void
  HiccupTestImpl(const exchange::core::common::config::PerformanceConfiguration
                     &performanceCfg,
                 const TestDataParameters &testDataParameters,
                 const exchange::core::common::config::InitialStateConfiguration
                     &initialStateCfg,
                 int warmupCycles);

  /**
   * Run latency test with fixed TPS (for accurate flame graph analysis)
   * @param performanceCfg - performance configuration
   * @param testDataParameters - test data parameters
   * @param initialStateCfg - initial state configuration
   * @param serializationCfg - serialization configuration
   * @param fixedTps - fixed TPS rate for both warmup and test (e.g., 4'000'000)
   * @param warmupCycles - number of warmup cycles (using same fixed TPS)
   */
  static void LatencyTestFixedTps(
      const exchange::core::common::config::PerformanceConfiguration
          &performanceCfg,
      const TestDataParameters &testDataParameters,
      const exchange::core::common::config::InitialStateConfiguration
          &initialStateCfg,
      const exchange::core::common::config::SerializationConfiguration
          &serializationCfg,
      int fixedTps, int warmupCycles);
};

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
