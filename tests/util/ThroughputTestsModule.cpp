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

#include "ThroughputTestsModule.h"
#include <exchange/core/utils/Logger.h>
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

  auto container = ExchangeTestContainer::Create(
      performanceCfg, initialStateCfg, serializationCfg);

  std::vector<float> perfResults;
  perfResults.reserve(iterations);

  for (int j = 0; j < iterations; j++) {
    container->LoadSymbolsUsersAndPrefillOrdersNoLog(testDataFutures);

    // Benchmark throughput
    auto genResult = testDataFutures.genResult.get();
    auto benchmarkCommandsFuture = genResult->GetApiCommandsBenchmark();
    auto benchmarkCommands = benchmarkCommandsFuture.get();

    // Match Java: System.currentTimeMillis() - use milliseconds precision
    auto tStart = std::chrono::steady_clock::now();
    if (!benchmarkCommands.empty()) {
      container->GetApi()->SubmitCommandsSync(benchmarkCommands);
    }
    auto tEnd = std::chrono::steady_clock::now();
    // Use milliseconds to match Java: System.currentTimeMillis()
    auto tDurationMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart)
            .count();

    // Avoid division by zero - if duration is 0, use 1 millisecond minimum
    if (tDurationMs == 0) {
      tDurationMs = 1;
    }

    // Calculate MT/s: Match Java exactly: size / (float) tDuration / 1000.0f
    // where tDuration is in milliseconds
    // This gives: (commands / ms) / 1000 = commands / (ms * 1000) = commands /
    // s / 1000000 = MT/s
    float perfMt =
        benchmarkCommands.size() / static_cast<float>(tDurationMs) / 1000.0f;
    float tDurationS = tDurationMs / 1000.0f;
    perfResults.push_back(perfMt);

    // Log performance with debug info (matching Java: log.info("{}. {} MT/s",
    // j, ...))
    LOG_INFO("{}. {:.3f} MT/s ({} commands in {:.3f}s = {}ms)", j, perfMt,
             benchmarkCommands.size(), tDurationS,
             static_cast<long long>(tDurationMs));

    // Clean up ApiCommand objects to prevent memory leak
    for (auto *cmd : benchmarkCommands) {
      delete cmd;
    }

    // Verify balances - match Java:
    // container.totalBalanceReport().isGlobalBalancesAllZero()
    auto balanceReport = container->TotalBalanceReport();
    if (balanceReport) {
      if (!balanceReport->IsGlobalBalancesAllZero()) {
        // Log non-zero balances for debugging
        auto globalBalances = balanceReport->GetGlobalBalancesSum();
        std::string errorMsg =
            "Total balance report is not zero. Non-zero balances: ";
        bool first = true;
        for (const auto &pair : globalBalances) {
          if (pair.second != 0) {
            if (!first)
              errorMsg += ", ";
            errorMsg += "currency " + std::to_string(pair.first) + " = " +
                        std::to_string(pair.second);
            first = false;
          }
        }

        // DIAGNOSIS: Log detailed breakdown for non-zero currency
        for (const auto &pair : globalBalances) {
          if (pair.second != 0) {
            int32_t currency = pair.first;
            LOG_DEBUG("Balance breakdown for currency {}:", currency);
            LOG_DEBUG("  GetGlobalBalancesSum() result: {}", pair.second);

            // Manual calculation to verify
            int64_t manualSum = 0;
            if (balanceReport->accountBalances) {
              auto it = balanceReport->accountBalances->find(currency);
              if (it != balanceReport->accountBalances->end()) {
                LOG_DEBUG("  accountBalances: {}", it->second);
                manualSum += it->second;
              }
            }
            if (balanceReport->ordersBalances) {
              auto it = balanceReport->ordersBalances->find(currency);
              if (it != balanceReport->ordersBalances->end()) {
                LOG_DEBUG("  ordersBalances: {}", it->second);
                manualSum += it->second;
              }
            }
            if (balanceReport->fees) {
              auto it = balanceReport->fees->find(currency);
              if (it != balanceReport->fees->end()) {
                LOG_DEBUG("  fees: {}", it->second);
                manualSum += it->second;
              }
            }
            if (balanceReport->adjustments) {
              auto it = balanceReport->adjustments->find(currency);
              if (it != balanceReport->adjustments->end()) {
                LOG_DEBUG("  adjustments: {}", it->second);
                manualSum += it->second;
              }
            }
            if (balanceReport->suspends) {
              auto it = balanceReport->suspends->find(currency);
              if (it != balanceReport->suspends->end()) {
                LOG_DEBUG("  suspends: {}", it->second);
                manualSum += it->second;
              }
            }
            LOG_DEBUG("  Manual sum: {}", manualSum);
            LOG_DEBUG("  Difference (GetGlobalBalancesSum - manual): {}",
                      pair.second - manualSum);
          }
        }

        throw std::runtime_error(errorMsg);
      }
    }

    // Verify order book state - compare final state to make sure all commands
    // executed same way Match Java:
    // testDataFutures.coreSymbolSpecifications.join().forEach(
    //     symbol -> assertEquals(expected, actual))
    auto coreSymbolSpecs = testDataFutures.coreSymbolSpecifications.get();
    for (const auto &symbol : coreSymbolSpecs) {
      auto expectedIt = genResult->genResults.find(symbol.symbolId);
      if (expectedIt != genResult->genResults.end()) {
        const auto &expectedPtr = expectedIt->second.finalOrderBookSnapshot;

        // If expected is null, skip comparison (shouldn't happen, but be safe)
        if (!expectedPtr) {
          continue;
        }

        // Get actual order book - this may return nullptr if symbol doesn't
        // exist
        auto actual = container->RequestCurrentOrderBook(symbol.symbolId);

        // If actual is null, that's an error (Java assertEquals would fail)
        if (!actual) {
          throw std::runtime_error("Failed to get order book for symbol " +
                                   std::to_string(symbol.symbolId));
        }

        // Compare order book snapshots using operator==
        // Match Java assertEquals behavior: both must be equal
        // Java version doesn't print anything if they match, only throws on
        // mismatch
        if (*expectedPtr != *actual) {
          throw std::runtime_error("Order book state mismatch for symbol " +
                                   std::to_string(symbol.symbolId));
        }
      }
    }

    container->ResetExchangeCore();
  }

  // Calculate average
  if (!perfResults.empty()) {
    float avgMt =
        std::accumulate(perfResults.begin(), perfResults.end(), 0.0f) /
        perfResults.size();
    LOG_INFO("Average: {:.3f} MT/s", avgMt);
  }
}

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
