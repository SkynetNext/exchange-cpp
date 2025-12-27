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

class PerfLatency : public ::testing::Test {
public:
  /**
   * This is latency test for simplified conditions
   * - one symbol (margin mode)
   * - ~1K active users (2K currency accounts)
   * - 1K pending limit-orders (in one order book)
   * 6-threads CPU can run this test
   */
  void TestLatencyMargin();

  /**
   * This is latency test for simplified conditions
   * - one symbol (exchange mode)
   * - ~1K active users (2K currency accounts)
   * - 1K pending limit-orders (in one order book)
   * 6-threads CPU can run this test
   */
  void TestLatencyExchange();

  /**
   * This is medium load latency test for verifying "triple million" capability:
   * - 1M active users (3M currency accounts)
   * - 1M pending limit-orders
   * - 1M+ messages per second throughput
   * - 10K symbols
   * - less than 1 millisecond 99.99% latency
   * 12-threads CPU and 32GiB RAM is required for running this test in 2+4
   * configuration.
   */
  void TestLatencyMultiSymbolMedium();

  /**
   * This is high load latency test for verifying exchange core scalability:
   * - 3M active users (10M currency accounts)
   * - 3M pending limit-orders
   * - 1M+ messages per second throughput
   * - 100K symbols
   * - less than 1 millisecond 99.99% latency
   * 12-threads CPU and 32GiB RAM is required for running this test in 2+4
   * configuration.
   */
  void TestLatencyMultiSymbolLarge();

  /**
   * This is high load latency test for verifying exchange core scalability:
   * - 10M active users (33M currency accounts)
   * - 30M pending limit-orders
   * - 200K symbols
   * - 1M+ messages per second throughput
   * 12-threads CPU and 32GiB RAM is required for running this test in 2+4
   * configuration.
   */
  void TestLatencyMultiSymbolHuge();
};

} // namespace perf
} // namespace tests
} // namespace core
} // namespace exchange
