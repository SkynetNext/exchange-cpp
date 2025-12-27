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

#include "LatencyTestsModule.h"
#include "ExchangeTestContainer.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace exchange {
namespace core {
namespace tests {
namespace util {

void LatencyTestsModule::LatencyTestImpl(
    const exchange::core::common::config::PerformanceConfiguration
        &performanceCfg,
    const TestDataParameters &testDataParameters,
    const exchange::core::common::config::InitialStateConfiguration
        &initialStateCfg,
    const exchange::core::common::config::SerializationConfiguration
        &serializationCfg,
    int warmupCycles) {

  const int targetTps = 200'000; // transactions per second
  const int targetTpsStep = 100'000;
  const int warmupTps = 1'000'000;

  auto testDataFutures =
      ExchangeTestContainer::PrepareTestDataAsync(testDataParameters, 1);

  auto container = ExchangeTestContainer::Create(
      performanceCfg, initialStateCfg, serializationCfg);

  // Simple latency measurement (without HDR histogram for now)
  // Can be enhanced with HDR histogram library later

  // Warmup cycles
  for (int i = 0; i < warmupCycles; i++) {
    container->LoadSymbolsUsersAndPrefillOrdersNoLog(testDataFutures);

    auto genResult = testDataFutures.genResult.get();
    auto benchmarkCommandsFuture = genResult->GetApiCommandsBenchmark();
    auto benchmarkCommands = benchmarkCommandsFuture.get();

    // Run warmup at high TPS
    const int nanosPerCmd = 1'000'000'000 / warmupTps;
    auto plannedTime = std::chrono::steady_clock::now();

    for (auto *cmd : benchmarkCommands) {
      while (std::chrono::steady_clock::now() < plannedTime) {
        std::this_thread::yield();
      }
      container->GetApi()->SubmitCommand(cmd);
      plannedTime += std::chrono::nanoseconds(nanosPerCmd);
    }

    // Wait for completion
    container->GetApi()->SubmitCommand(
        new exchange::core::common::api::ApiNop());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    container->ResetExchangeCore();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  // Main test iterations - match Java: max 10000 iterations
  for (int i = 0; i < 10000; i++) {
    int tps = targetTps + targetTpsStep * i;
    container->LoadSymbolsUsersAndPrefillOrdersNoLog(testDataFutures);

    auto genResult = testDataFutures.genResult.get();
    auto benchmarkCommandsFuture = genResult->GetApiCommandsBenchmark();
    auto benchmarkCommands = benchmarkCommandsFuture.get();

    // Simple latency tracking
    std::vector<int64_t> latencies;
    latencies.reserve(benchmarkCommands.size());

    std::mutex latenciesMutex;
    container->SetConsumer([&latencies, &latenciesMutex](
                               exchange::core::common::cmd::OrderCommand *cmd,
                               int64_t seq) {
      if (cmd && cmd->timestamp > 0) {
        auto now = std::chrono::system_clock::now();
        auto cmdTime = std::chrono::time_point<std::chrono::system_clock>(
            std::chrono::milliseconds(cmd->timestamp));
        auto latency =
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - cmdTime)
                .count();
        std::lock_guard<std::mutex> lock(latenciesMutex);
        latencies.push_back(latency);
      }
    });

    const int nanosPerCmd = 1'000'000'000 / tps;
    auto startTime = std::chrono::steady_clock::now();
    auto plannedTime = startTime;

    for (auto *cmd : benchmarkCommands) {
      while (std::chrono::steady_clock::now() < plannedTime) {
        std::this_thread::yield();
      }
      auto now = std::chrono::system_clock::now();
      cmd->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch())
                           .count();
      container->GetApi()->SubmitCommand(cmd);
      plannedTime += std::chrono::nanoseconds(nanosPerCmd);
    }

    // Wait for completion
    container->GetApi()->SubmitCommand(
        new exchange::core::common::api::ApiNop());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    container->SetConsumer(nullptr);

    // Calculate median latency
    std::lock_guard<std::mutex> lock(latenciesMutex);
    bool shouldContinue = true;
    if (!latencies.empty()) {
      std::sort(latencies.begin(), latencies.end());
      int64_t medianLatency = latencies[latencies.size() / 2];

      // Stop if median latency > 1ms (10,000,000 nanoseconds)
      // Match Java: return warmup || histogram.getValueAtPercentile(50.0) <
      // 10_000_000;
      if (medianLatency >= 10'000'000) {
        shouldContinue = false;
      }
    }

    container->ResetExchangeCore();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Match Java: .allMatch(x -> x) - stop if any iteration returns false
    if (!shouldContinue) {
      break;
    }
  }
}

void LatencyTestsModule::HiccupTestImpl(
    const exchange::core::common::config::PerformanceConfiguration
        &performanceCfg,
    const TestDataParameters &testDataParameters,
    const exchange::core::common::config::InitialStateConfiguration
        &initialStateCfg,
    int warmupCycles) {

  const int targetTps = 500'000;
  const long hiccupThresholdNs = 200'000; // 0.2ms

  auto testDataFutures =
      ExchangeTestContainer::PrepareTestDataAsync(testDataParameters, 1);

  auto container = ExchangeTestContainer::Create(
      performanceCfg, initialStateCfg,
      exchange::core::common::config::SerializationConfiguration::Default());

  // Hiccup detection - track latency spikes above threshold
  std::map<int64_t, int64_t> hiccupTimestamps; // timestamp -> delay

  // Warmup cycles
  for (int i = 0; i < warmupCycles; i++) {
    container->LoadSymbolsUsersAndPrefillOrdersNoLog(testDataFutures);

    auto genResult = testDataFutures.genResult.get();
    auto benchmarkCommandsFuture = genResult->GetApiCommandsBenchmark();
    auto benchmarkCommands = benchmarkCommandsFuture.get();

    const int nanosPerCmd = 1'000'000'000 / targetTps;
    auto plannedTime = std::chrono::steady_clock::now();

    for (auto *cmd : benchmarkCommands) {
      while (std::chrono::steady_clock::now() < plannedTime) {
        std::this_thread::yield();
      }
      auto now = std::chrono::system_clock::now();
      cmd->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch())
                           .count();
      container->GetApi()->SubmitCommand(cmd);
      plannedTime += std::chrono::nanoseconds(nanosPerCmd);
    }

    container->GetApi()->SubmitCommand(
        new exchange::core::common::api::ApiNop());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    container->ResetExchangeCore();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  // Main test iterations
  for (int i = 0; i < 10000; i++) {
    container->LoadSymbolsUsersAndPrefillOrdersNoLog(testDataFutures);

    auto genResult = testDataFutures.genResult.get();
    auto benchmarkCommandsFuture = genResult->GetApiCommandsBenchmark();
    auto benchmarkCommands = benchmarkCommandsFuture.get();

    std::mutex hiccupMutex;
    int64_t nextHiccupAcceptTimestamp = 0;

    container->SetConsumer([&hiccupTimestamps, &hiccupMutex,
                            &nextHiccupAcceptTimestamp, hiccupThresholdNs](
                               exchange::core::common::cmd::OrderCommand *cmd,
                               int64_t seq) {
      if (!cmd || cmd->timestamp <= 0) {
        return;
      }

      auto now = std::chrono::system_clock::now();
      auto cmdTime = std::chrono::time_point<std::chrono::system_clock>(
          std::chrono::milliseconds(cmd->timestamp));
      auto diffNs =
          std::chrono::duration_cast<std::chrono::nanoseconds>(now - cmdTime)
              .count();

      std::lock_guard<std::mutex> lock(hiccupMutex);

      // Skip other messages in delayed group
      if (cmd->timestamp < nextHiccupAcceptTimestamp) {
        return;
      }

      // Register hiccup timestamps
      if (diffNs > hiccupThresholdNs) {
        hiccupTimestamps[cmd->timestamp] = diffNs;
        nextHiccupAcceptTimestamp = cmd->timestamp + diffNs;
      }
    });

    const int nanosPerCmd = 1'000'000'000 / targetTps;
    auto startTimeNs = std::chrono::steady_clock::now();
    auto plannedTime = startTimeNs;

    for (auto *cmd : benchmarkCommands) {
      while (std::chrono::steady_clock::now() < plannedTime) {
        std::this_thread::yield();
      }
      auto now = std::chrono::system_clock::now();
      cmd->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch())
                           .count();
      container->GetApi()->SubmitCommand(cmd);
      plannedTime += std::chrono::nanoseconds(nanosPerCmd);
    }

    container->GetApi()->SubmitCommand(
        new exchange::core::common::api::ApiNop());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    container->SetConsumer(nullptr);

    // Report hiccups if any
    std::lock_guard<std::mutex> lock(hiccupMutex);
    if (hiccupTimestamps.empty()) {
      // No hiccups detected
    } else {
      // Hiccups detected - can log them here
      hiccupTimestamps.clear();
    }

    container->ResetExchangeCore();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
