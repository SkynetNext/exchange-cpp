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
namespace core2 {
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

    // Note: benchmarkMtps and getGenResult need to be implemented in
    // ExchangeTestContainer
    // For now, this is a placeholder
    // float perfMt = container->BenchmarkMtps(...);
    // perfResults.push_back(perfMt);

    // Verify balances
    // assertTrue(container->TotalBalanceReport()->IsGlobalBalancesAllZero());

    // Verify order book state
    // auto coreSymbolSpecs = testDataFutures.coreSymbolSpecifications.get();
    // for (const auto& symbol : coreSymbolSpecs) {
    //   auto expected = ...;
    //   auto actual = container->RequestCurrentOrderBook(symbol.symbolId);
    //   assertEquals(expected, actual);
    // }

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
} // namespace core2
} // namespace exchange

