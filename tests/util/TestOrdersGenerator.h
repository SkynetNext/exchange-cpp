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

#include <exchange/core/common/L2MarketData.h>
#include <exchange/core/common/api/ApiCommand.h>
#include <exchange/core/common/cmd/OrderCommand.h>
#include <functional>
#include <future>
#include <memory>
#include <unordered_map>
#include <vector>

namespace exchange {
namespace core {
namespace tests {
namespace util {
// Forward declaration - will be included in .cpp
struct TestOrdersGeneratorConfig;
} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange

namespace exchange {
namespace core {
namespace tests {
namespace util {

/**
 * TestOrdersGenerator - generates test order commands for orderbook testing
 * Simplified version for basic test support
 */
class TestOrdersGenerator {
public:
  /**
   * Result of command generation
   */
  struct GenResult {
    std::vector<exchange::core::common::cmd::OrderCommand> commandsFill;
    std::vector<exchange::core::common::cmd::OrderCommand> commandsBenchmark;
    std::shared_ptr<exchange::core::common::L2MarketData>
        finalOrderBookSnapshot;
    int64_t finalOrderbookHash;

    /**
     * Get all commands (fill + benchmark)
     * Returns reference to allow modification (matching Java version behavior)
     */
    std::vector<exchange::core::common::cmd::OrderCommand> &GetCommands() const;

    /**
     * Get total number of commands
     */
    size_t Size() const;
  };

  /**
   * UID mapper that maps index to UID (plain: i -> i + 1)
   */
  static std::function<int32_t(int32_t)> UID_PLAIN_MAPPER;

  /**
   * Generate test commands
   * @param benchmarkTransactionsNumber - number of benchmark transactions
   * @param targetOrderBookOrders - target number of orders in orderbook
   * @param numUsers - number of users
   * @param uidMapper - function to map index to UID
   * @param symbol - symbol ID
   * @param enableSlidingPrice - enable sliding price
   * @param avalancheIOC - enable avalanche IOC orders
   * @param asyncProgressConsumer - progress callback (can be nullptr)
   * @param seed - random seed
   * @return generated commands result
   */
  static GenResult
  GenerateCommands(int benchmarkTransactionsNumber, int targetOrderBookOrders,
                   int numUsers, std::function<int32_t(int32_t)> uidMapper,
                   int32_t symbol, bool enableSlidingPrice, bool avalancheIOC,
                   std::function<void(int64_t)> asyncProgressConsumer,
                   int seed);

  /**
   * Create async progress logger
   * @param total - total number of items
   * @return progress consumer function
   */
  static std::function<void(int64_t)> CreateAsyncProgressLogger(int64_t total);

  /**
   * MultiSymbolGenResult - result for multiple symbols generation
   */
  struct MultiSymbolGenResult {
    // Map from symbolId to GenResult
    std::unordered_map<int32_t, GenResult> genResults;

    // Combined benchmark commands (all symbols)
    std::vector<exchange::core::common::cmd::OrderCommand> commandsBenchmark;

    // Combined fill commands (all symbols)
    std::vector<exchange::core::common::cmd::OrderCommand> commandsFill;

    /**
     * Get benchmark commands size
     */
    size_t GetBenchmarkCommandsSize() const { return commandsBenchmark.size(); }

    /**
     * Get fill commands (as future for async compatibility with Java version)
     */
    std::future<std::vector<exchange::core::common::api::ApiCommand *>>
    GetApiCommandsFill() const;

    /**
     * Get benchmark commands (as future for async compatibility with Java
     * version)
     */
    std::future<std::vector<exchange::core::common::api::ApiCommand *>>
    GetApiCommandsBenchmark() const;
  };

  /**
   * Generate multiple symbols test commands
   * @param config - test orders generator configuration
   * @return multi-symbol generation result
   */
  static MultiSymbolGenResult
  GenerateMultipleSymbols(const TestOrdersGeneratorConfig &config);

  /**
   * Create weighted distribution for multiple symbols (Pareto distribution)
   * @param size - number of symbols
   * @param seed - random seed
   * @return normalized distribution array
   */
  static std::vector<double> CreateWeightedDistribution(int size, int seed);
};

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
