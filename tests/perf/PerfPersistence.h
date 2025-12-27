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

#include <gtest/gtest.h>

namespace exchange {
namespace core {
namespace tests {
namespace perf {

class PerfPersistence : public ::testing::Test {
public:
  /**
   * Persistence test for Margin mode
   * This is serialization test for simplified conditions:
   * - one symbol
   * - ~1K active users (2K currency accounts)
   * - 1K pending limit-orders (in one order book)
   * 6-threads CPU can run this test
   */
  void TestPersistenceMargin();

  /**
   * Persistence test for Exchange mode
   * This is serialization test for simplified conditions:
   * - one symbol
   * - ~1K active users (2K currency accounts)
   * - 1K pending limit-orders (in one order book)
   * 6-threads CPU can run this test
   */
  void TestPersistenceExchange();

  /**
   * Persistence test for medium multi-symbol configuration
   * This is serialization test for verifying "triple million" capability.
   * This test requires 10+ GiB free disk space, 16+ GiB of RAM and 12-threads
   * CPU
   */
  void TestPersistenceMultiSymbolMedium();

  /**
   * Persistence test for large multi-symbol configuration
   */
  void TestPersistenceMultiSymbolLarge();

  /**
   * Persistence test for huge multi-symbol configuration
   */
  void TestPersistenceMultiSymbolHuge();
};

} // namespace perf
} // namespace tests
} // namespace core
} // namespace exchange
