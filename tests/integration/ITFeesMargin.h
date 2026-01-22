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

namespace exchange::core::tests::integration {

/**
 * ITFeesMargin - abstract base class for margin fees tests
 * Tests fee calculation and collection for futures contracts
 */
class ITFeesMargin : public ::testing::Test {
public:
  ITFeesMargin() = default;
  virtual ~ITFeesMargin() = default;

  /**
   * Get performance configuration (must be implemented by child class)
   */
  virtual exchange::core::common::config::PerformanceConfiguration
  GetPerformanceConfiguration() = 0;

  // Note: Test methods will be implemented in .cpp file
  // They require ExchangeTestContainer which needs to be implemented first
};

}  // namespace exchange::core::tests::integration
