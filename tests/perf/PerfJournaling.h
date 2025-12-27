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

class PerfJournaling : public ::testing::Test {
public:
  /**
   * Journaling test for Margin mode
   * - one symbol (margin mode)
   * - ~1K active users (2K currency accounts)
   * - 1K pending limit-orders (in one order book)
   * 6-threads CPU can run this test
   */
  void TestJournalingMargin();

  /**
   * Journaling test for Exchange mode
   * - one symbol (exchange mode)
   * - ~1K active users (2K currency accounts)
   * - 1K pending limit-orders (in one order book)
   * 6-threads CPU can run this test
   */
  void TestJournalingExchange();

  /**
   * Journaling test for small multi-symbol configuration
   * - 1K symbols
   * - 100K active users
   * - 3M transactions
   */
  void TestJournalingMultiSymbolSmall();

  /**
   * Journaling test for medium multi-symbol configuration
   * - 10K symbols
   * - 1M active users (3M currency accounts)
   * - 1M pending limit-orders
   */
  void TestJournalingMultiSymbolMedium();

  /**
   * Journaling test for large multi-symbol configuration
   * - 50K symbols
   * - 3M active users (10M currency accounts)
   * - 3M pending limit-orders
   */
  void TestJournalingMultiSymbolLarge();

  /**
   * Journaling test for huge multi-symbol configuration
   * - 100K symbols
   * - 10M active users (33M currency accounts)
   * - 30M pending limit-orders
   */
  void TestJournalingMultiSymbolHuge();
};

} // namespace perf
} // namespace tests
} // namespace core
} // namespace exchange
