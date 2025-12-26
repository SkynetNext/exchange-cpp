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

#include "JournalingTestsModule.h"
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

void JournalingTestsModule::JournalingTestImpl(
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

    const std::string exchangeId = ExchangeTestContainer::TimeBasedExchangeId();
    long originalFinalStateHash = 0;

    // First start: clean start with journaling
    auto firstStartConfig =
        exchange::core::common::config::InitialStateConfiguration::CleanStartJournaling(
            exchangeId);

    {
      auto container = ExchangeTestContainer::Create(
          performanceCfg, firstStartConfig,
          exchange::core::common::config::SerializationConfiguration::DiskJournaling());

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

      // Run benchmark commands
      auto genResult = testDataFutures.genResult.get();
      auto benchmarkCommandsFuture = genResult.GetApiCommandsBenchmark();
      auto benchmarkCommands = benchmarkCommandsFuture.get();
      if (!benchmarkCommands.empty()) {
        container->GetApi()->SubmitCommandsSync(benchmarkCommands);
      }

      // Get original final state hash
      originalFinalStateHash = container->RequestStateHash();

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

    // Recreate from snapshot + journal
    const long snapshotBaseSeq = 0L;
    auto fromSnapshotConfig =
        exchange::core::common::config::InitialStateConfiguration::LastKnownStateFromJournal(
            exchangeId, stateId, snapshotBaseSeq);

    {
      auto recreatedContainer = ExchangeTestContainer::Create(
          performanceCfg, fromSnapshotConfig,
          exchange::core::common::config::SerializationConfiguration::DiskJournaling());

      // Wait for core to be ready
      recreatedContainer->TotalBalanceReport();

      // Verify restored state hash matches original
      long restoredStateHash = recreatedContainer->RequestStateHash();
      if (restoredStateHash != originalFinalStateHash) {
        throw std::runtime_error("Restored state hash mismatch");
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
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange

