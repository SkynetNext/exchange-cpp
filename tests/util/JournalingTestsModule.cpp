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

#include "JournalingTestsModule.h"
#include <exchange/core/common/api/ApiPersistState.h>
#include <exchange/core/common/config/InitialStateConfiguration.h>
#include <exchange/core/common/config/SerializationConfiguration.h>
#include <exchange/core/utils/FastNanoTime.h>
#include <exchange/core/utils/Logger.h>
#include <iomanip>
#include <sstream>
#include "ExchangeTestContainer.h"
#include "LatencyTools.h"

namespace exchange {
namespace core {
namespace tests {
namespace util {

void JournalingTestsModule::JournalingTestImpl(
  const exchange::core::common::config::PerformanceConfiguration& performanceCfg,
  const TestDataParameters& testDataParameters,
  int iterations) {
  for (int iteration = 0; iteration < iterations; iteration++) {
    // Match Java: log.debug(" ----------- journaling test --- iteration {} of
    // {}
    // ----", iteration, iterations);
    LOG_DEBUG(" ----------- journaling test --- iteration {} of {} ----", iteration, iterations);

    auto testDataFutures =
      ExchangeTestContainer::PrepareTestDataAsync(testDataParameters, iteration);

    const std::string exchangeId = ExchangeTestContainer::TimeBasedExchangeId();
    long stateId;
    long originalFinalStateHash = 0;

    // First start: clean start with journaling
    auto firstStartConfig =
      exchange::core::common::config::InitialStateConfiguration::CleanStartJournaling(exchangeId);

    {
      auto container = ExchangeTestContainer::Create(
        performanceCfg, firstStartConfig,
        exchange::core::common::config::SerializationConfiguration::DiskJournaling());

      // Load symbols, users and prefill orders
      container->LoadSymbolsUsersAndPrefillOrders(testDataFutures);

      // Match Java: log.info("Creating snapshot...");
      LOG_INFO("Creating snapshot...");
      // Match Java: stateId = System.currentTimeMillis() * 1000 + iteration;
      stateId = exchange::core::utils::FastNanoTime::NowMillis() * 1000 + iteration;
      // Match Java: try (ExecutionTime ignore = new ExecutionTime(t ->
      // log.debug("Snapshot {} created in {}", stateId, t))) {
      int64_t snapshotStartNs = exchange::core::utils::FastNanoTime::Now();
      auto persistState =
        std::make_unique<exchange::core::common::api::ApiPersistState>(stateId, false);
      auto future = container->GetApi()->SubmitCommandAsync(persistState.release());
      auto result = future.get();
      int64_t snapshotEndNs = exchange::core::utils::FastNanoTime::Now();
      int64_t snapshotDurationNs = snapshotEndNs - snapshotStartNs;
      // Match Java: log.debug("Snapshot {} created in {}", stateId, t);
      LOG_DEBUG("Snapshot {} created in {}", stateId,
                LatencyTools::FormatNanos(snapshotDurationNs));
      // Match Java: assertThat(resultCode, Is.is(CommandResultCode.SUCCESS));
      LOG_DEBUG("Snapshot result code: {}", static_cast<int32_t>(result));
      if (result != exchange::core::common::cmd::CommandResultCode::SUCCESS) {
        throw std::runtime_error("Failed to create snapshot");
      }

      // Match Java: log.info("Running commands on original state...");
      LOG_INFO("Running commands on original state...");
      // Run benchmark commands
      auto genResult = testDataFutures.genResult.get();
      auto benchmarkCommandsFuture = genResult->GetApiCommandsBenchmark();
      auto benchmarkCommands = benchmarkCommandsFuture.get();
      if (!benchmarkCommands.empty()) {
        container->GetApi()->SubmitCommandsSync(benchmarkCommands);
      }
      // Clean up ApiCommand objects to prevent memory leak
      for (auto* cmd : benchmarkCommands) {
        delete cmd;
      }
      // Match Java:
      // assertTrue(container.totalBalanceReport().isGlobalBalancesAllZero());
      // Verify total balance report is zero
      auto balanceReport = container->TotalBalanceReport();
      if (balanceReport) {
        if (!balanceReport->IsGlobalBalancesAllZero()) {
          // Log non-zero balances for debugging
          auto globalBalances = balanceReport->GetGlobalBalancesSum();
          std::string errorMsg = "Total balance report is not zero. Non-zero balances: ";
          bool first = true;
          for (const auto& pair : globalBalances) {
            if (pair.second != 0) {
              if (!first)
                errorMsg += ", ";
              errorMsg +=
                "currency " + std::to_string(pair.first) + " = " + std::to_string(pair.second);
              first = false;
            }
          }

          // DIAGNOSIS: Log detailed breakdown for non-zero currency
          for (const auto& pair : globalBalances) {
            if (pair.second != 0) {
              int32_t currency = pair.first;
              LOG_ERROR("Balance breakdown for currency {}:", currency);
              LOG_ERROR("  GetGlobalBalancesSum() result: {}", pair.second);

              // Manual calculation to verify
              int64_t manualSum = 0;
              if (balanceReport->accountBalances) {
                auto it = balanceReport->accountBalances->find(currency);
                if (it != balanceReport->accountBalances->end()) {
                  LOG_ERROR("  accountBalances: {}", it->second);
                  manualSum += it->second;
                }
              }
              if (balanceReport->ordersBalances) {
                auto it = balanceReport->ordersBalances->find(currency);
                if (it != balanceReport->ordersBalances->end()) {
                  LOG_ERROR("  ordersBalances: {}", it->second);
                  manualSum += it->second;
                }
              }
              if (balanceReport->fees) {
                auto it = balanceReport->fees->find(currency);
                if (it != balanceReport->fees->end()) {
                  LOG_ERROR("  fees: {}", it->second);
                  manualSum += it->second;
                }
              }
              LOG_ERROR("  Manual sum: {}", manualSum);
              LOG_ERROR("  Difference (GetGlobalBalancesSum - manual): {}",
                        pair.second - manualSum);
            }
          }

          throw std::runtime_error(errorMsg);
        }
      }

      // Match Java: originalFinalStateHash = container.requestStateHash();
      originalFinalStateHash = container->RequestStateHash();
      // Match Java: log.info("Original state checks completed");
      LOG_INFO("Original state checks completed");
    }

    // Match Java: // TODO Discover snapshots and journals with
    // DiskSerializationProcessor
    const long snapshotBaseSeq = 0L;
    auto fromSnapshotConfig =
      exchange::core::common::config::InitialStateConfiguration::LastKnownStateFromJournal(
        exchangeId, stateId, snapshotBaseSeq);

    // Match Java: log.debug("Creating new exchange from persisted state...");
    LOG_DEBUG("Creating new exchange from persisted state...");
    // Match Java: final long tLoad = System.currentTimeMillis();
    int64_t tLoadNs = exchange::core::utils::FastNanoTime::Now();
    {
      auto recreatedContainer = ExchangeTestContainer::Create(
        performanceCfg, fromSnapshotConfig,
        exchange::core::common::config::SerializationConfiguration::DiskJournaling());

      // Match Java: // simple sync query in order to wait until core is started
      // to respond
      // Match Java: recreatedContainer.totalBalanceReport();
      recreatedContainer->TotalBalanceReport();

      // Match Java: float loadTimeSec = (float) (System.currentTimeMillis() -
      // tLoad) / 1000.0f;
      int64_t loadEndNs = exchange::core::utils::FastNanoTime::Now();
      int64_t loadTimeMs = (loadEndNs - tLoadNs) / 1'000'000LL;
      float loadTimeSec = static_cast<float>(loadTimeMs) / 1000.0f;
      // Match Java: log.debug("Load+start+replay time: {}s",
      // String.format("%.3f", loadTimeSec));
      std::ostringstream loadTimeStr;
      loadTimeStr << std::fixed << std::setprecision(3) << loadTimeSec;
      LOG_DEBUG("Load+start+replay time: {}s", loadTimeStr.str());

      // Match Java: final long restoredStateHash =
      // recreatedContainer.requestStateHash();
      // Match Java: assertThat(restoredStateHash, is(originalFinalStateHash));
      long restoredStateHash = recreatedContainer->RequestStateHash();
      if (restoredStateHash != originalFinalStateHash) {
        throw std::runtime_error("Restored state hash mismatch");
      }

      // Match Java:
      // assertTrue(recreatedContainer.totalBalanceReport().isGlobalBalancesAllZero());
      // Verify total balance report is zero
      auto balanceReport = recreatedContainer->TotalBalanceReport();
      if (balanceReport) {
        if (!balanceReport->IsGlobalBalancesAllZero()) {
          // Log non-zero balances for debugging
          auto globalBalances = balanceReport->GetGlobalBalancesSum();
          std::string errorMsg = "Restored total balance report is not zero. Non-zero balances: ";
          bool first = true;
          for (const auto& pair : globalBalances) {
            if (pair.second != 0) {
              if (!first)
                errorMsg += ", ";
              errorMsg +=
                "currency " + std::to_string(pair.first) + " = " + std::to_string(pair.second);
              first = false;
            }
          }

          // DIAGNOSIS: Log detailed breakdown for non-zero currency
          for (const auto& pair : globalBalances) {
            if (pair.second != 0) {
              int32_t currency = pair.first;
              LOG_ERROR("Restored balance breakdown for currency {}:", currency);
              LOG_ERROR("  GetGlobalBalancesSum() result: {}", pair.second);

              // Manual calculation to verify
              int64_t manualSum = 0;
              if (balanceReport->accountBalances) {
                auto it = balanceReport->accountBalances->find(currency);
                if (it != balanceReport->accountBalances->end()) {
                  LOG_ERROR("  accountBalances: {}", it->second);
                  manualSum += it->second;
                }
              }
              if (balanceReport->ordersBalances) {
                auto it = balanceReport->ordersBalances->find(currency);
                if (it != balanceReport->ordersBalances->end()) {
                  LOG_ERROR("  ordersBalances: {}", it->second);
                  manualSum += it->second;
                }
              }
              if (balanceReport->fees) {
                auto it = balanceReport->fees->find(currency);
                if (it != balanceReport->fees->end()) {
                  LOG_ERROR("  fees: {}", it->second);
                  manualSum += it->second;
                }
              }
              LOG_ERROR("  Manual sum: {}", manualSum);
              LOG_ERROR("  Difference (GetGlobalBalancesSum - manual): {}",
                        pair.second - manualSum);
            }
          }

          throw std::runtime_error(errorMsg);
        }
      }
      // Match Java: log.info("Restored snapshot+journal is valid");
      LOG_INFO("Restored snapshot+journal is valid");
    }
  }
}

}  // namespace util
}  // namespace tests
}  // namespace core
}  // namespace exchange
