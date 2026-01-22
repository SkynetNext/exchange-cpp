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

#include <exchange/core/common/config/InitialStateConfiguration.h>
#include <exchange/core/common/config/PerformanceConfiguration.h>
#include <exchange/core/common/config/SerializationConfiguration.h>
#include "TestDataParameters.h"

namespace exchange::core::tests::util {

/**
 * ThroughputTestsModule - module for throughput performance testing
 */
class ThroughputTestsModule {
public:
  /**
   * Run throughput test implementation
   * @param performanceCfg - performance configuration
   * @param testDataParameters - test data parameters
   * @param initialStateCfg - initial state configuration
   * @param serializationCfg - serialization configuration
   * @param iterations - number of test iterations
   */
  static void ThroughputTestImpl(
    const exchange::core::common::config::PerformanceConfiguration& performanceCfg,
    const TestDataParameters& testDataParameters,
    const exchange::core::common::config::InitialStateConfiguration& initialStateCfg,
    const exchange::core::common::config::SerializationConfiguration& serializationCfg,
    int iterations);
};

}  // namespace exchange::core::tests::util
