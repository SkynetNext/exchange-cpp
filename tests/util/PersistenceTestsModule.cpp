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

#include "PersistenceTestsModule.h"
#include "ExchangeTestContainer.h"
#include <exchange/core/common/api/ApiPersistState.h>
#include <exchange/core/common/config/InitialStateConfiguration.h>
#include <exchange/core/common/config/SerializationConfiguration.h>
#include <chrono>
#include <thread>

namespace exchange {
namespace core {
namespace tests {
namespace util {

void PersistenceTestsModule::PersistenceTestImpl(
    const exchange::core::common::config::PerformanceConfiguration
        &performanceCfg,
    const TestDataParameters &testDataParameters,
    int iterations) {

  for (int iteration = 0; iteration < iterations; iteration++) {
    auto testDataFutures =
        ExchangeTestContainer::PrepareTestDataAsync(testDataParameters, iteration);

    const long stateId = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count() *
                         1000 +
                         iteration;

    char exchangeIdBuffer[32];
    snprintf(exchangeIdBuffer, sizeof(exchangeIdBuffer), "%012llX",
             static_cast<unsigned long long>(
                 std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count()));
    std::string exchangeId(exchangeIdBuffer);

    auto firstStartConfig =
        exchange::core::common::config::InitialStateConfiguration::CleanStart(
            exchangeId);

    long originalPrefillStateHash;
    float originalPerfMt;

    {
      auto container = ExchangeTestContainer::Create(
          performanceCfg, firstStartConfig,
          exchange::core::common::config::SerializationConfiguration::DiskSnapshotOnly());

      // Load symbols, users and prefill orders
      container->LoadSymbolsUsersAndPrefillOrders(testDataFutures);

      // Create snapshot
      auto persistState = std::make_unique<exchange::core::common::api::ApiPersistState>(
          stateId, false);
      auto future = container->GetApi()->SubmitCommandAsync(persistState.release());
      auto result = future.get();
      if (result != exchange::core::common::cmd::CommandResultCode::SUCCESS) {
        throw std::runtime_error("Failed to create snapshot");
      }

      // Get original prefill state hash
      originalPrefillStateHash = container->RequestStateHash();

      // Benchmark original state and measure throughput
      auto genResult = testDataFutures.genResult.get();
      auto benchmarkCommandsFuture = genResult.GetApiCommandsBenchmark();
      auto benchmarkCommands = benchmarkCommandsFuture.get();
      
      auto tStart = std::chrono::steady_clock::now();
      if (!benchmarkCommands.empty()) {
        container->GetApi()->SubmitCommandsSync(benchmarkCommands);
      }
      auto tDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - tStart).count();
      originalPerfMt = benchmarkCommands.size() / static_cast<float>(tDuration) / 1000.0f;

      // Verify total balance report is zero
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
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Recreate from snapshot
    auto fromSnapshotConfig =
        exchange::core::common::config::InitialStateConfiguration::FromSnapshotOnly(
            exchangeId, stateId, 0);

    {
      auto recreatedContainer = ExchangeTestContainer::Create(
          performanceCfg, fromSnapshotConfig,
          exchange::core::common::config::SerializationConfiguration::DiskSnapshotOnly());

      // Wait for core to be ready
      recreatedContainer->TotalBalanceReport();

      // Verify restored state hash
      long restoredPrefillStateHash = recreatedContainer->RequestStateHash();
      if (restoredPrefillStateHash != originalPrefillStateHash) {
        throw std::runtime_error("State hash mismatch after restore");
      }

      // Verify total balance report is zero
      auto balanceReport = recreatedContainer->TotalBalanceReport();
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
          throw std::runtime_error("Restored total balance report is not zero");
        }
      }

      // Benchmark restored state and compare with original
      auto genResult2 = testDataFutures.genResult.get();
      auto benchmarkCommandsFuture2 = genResult2.GetApiCommandsBenchmark();
      auto benchmarkCommands2 = benchmarkCommandsFuture2.get();
      
      auto tStart2 = std::chrono::steady_clock::now();
      if (!benchmarkCommands2.empty()) {
        recreatedContainer->GetApi()->SubmitCommandsSync(benchmarkCommands2);
      }
      auto tDuration2 = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - tStart2).count();
      float perfMt = benchmarkCommands2.size() / static_cast<float>(tDuration2) / 1000.0f;
      float perfRatioPerc = perfMt / originalPerfMt * 100.0f;
      
      // Log performance comparison (can be used for reporting)
      (void)perfRatioPerc; // Suppress unused variable warning for now
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange

