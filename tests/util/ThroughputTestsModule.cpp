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

#include "ThroughputTestsModule.h"
#include <algorithm>
#include <cstdio>
#include <numeric>
#include <vector>

namespace exchange {
namespace core {
namespace tests {
namespace util {

void ThroughputTestsModule::ThroughputTestImpl(
    const exchange::core::common::config::PerformanceConfiguration
        &performanceCfg,
    const TestDataParameters &testDataParameters,
    const exchange::core::common::config::InitialStateConfiguration
        &initialStateCfg,
    const exchange::core::common::config::SerializationConfiguration
        &serializationCfg,
    int iterations) {

  // Note: This implementation requires ExchangeTestContainer.cpp to be
  // implemented
  // For now, this is a placeholder that will be completed when
  // ExchangeTestContainer is ready

  auto testDataFutures =
      ExchangeTestContainer::PrepareTestDataAsync(testDataParameters, 1);

  auto container = ExchangeTestContainer::Create(performanceCfg, initialStateCfg,
                                                  serializationCfg);

  std::vector<float> perfResults;
  perfResults.reserve(iterations);

  for (int j = 0; j < iterations; j++) {
    container->LoadSymbolsUsersAndPrefillOrdersNoLog(testDataFutures);

    // Benchmark throughput
    auto genResult = testDataFutures.genResult.get();
    auto benchmarkCommandsFuture = genResult.GetApiCommandsBenchmark();
    auto benchmarkCommands = benchmarkCommandsFuture.get();
    
    auto tStart = std::chrono::steady_clock::now();
    if (!benchmarkCommands.empty()) {
      container->GetApi()->SubmitCommandsSync(benchmarkCommands);
    }
    auto tDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - tStart).count();
    
    float perfMt = benchmarkCommands.size() / static_cast<float>(tDuration) / 1000.0f;
    perfResults.push_back(perfMt);

    // Verify balances
    auto balanceReport = container->TotalBalanceReport();
    if (balanceReport) {
      // Check all balances are zero
      bool allZero = true;
      if (balanceReport->accountBalances) {
        for (const auto &pair : *balanceReport->accountBalances) {
          if (pair.second != 0) {
            allZero = false;
            break;
          }
        }
      }
      if (allZero && balanceReport->fees) {
        for (const auto &pair : *balanceReport->fees) {
          if (pair.second != 0) {
            allZero = false;
            break;
          }
        }
      }
      if (allZero && balanceReport->ordersBalances) {
        for (const auto &pair : *balanceReport->ordersBalances) {
          if (pair.second != 0) {
            allZero = false;
            break;
          }
        }
      }
      if (!allZero) {
        throw std::runtime_error("Total balance report is not zero");
      }
    }

    // Verify order book state
    auto coreSymbolSpecs = testDataFutures.coreSymbolSpecifications.get();
    for (const auto &symbol : coreSymbolSpecs) {
      auto expected = genResult.genResults.find(symbol.symbolId);
      if (expected != genResult.genResults.end() && expected->second.finalOrderBookSnapshot) {
        auto actual = container->RequestCurrentOrderBook(symbol.symbolId);
        if (actual) {
          // Compare order book snapshots
          // Note: Full comparison logic can be added here
        }
      }
    }

    container->ResetExchangeCore();
  }

  // Calculate average
  if (!perfResults.empty()) {
    float avgMt = std::accumulate(perfResults.begin(), perfResults.end(), 0.0f) /
                  perfResults.size();
    printf("Average: %.3f MT/s\n", avgMt);
  }
}

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange

