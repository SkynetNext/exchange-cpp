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
#include "LatencyTools.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exchange/core/utils/Logger.h>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
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

  // Match Java: log.debug("Warming up {} cycles...", warmupCycles);
  LOG_DEBUG("Warming up {} cycles...", warmupCycles);

  // Warmup cycles - match Java: warmup also outputs latency results
  for (int i = 0; i < warmupCycles; i++) {
    // Use same test iteration logic as main loop, but with warmupTps and
    // warmup=true
    int tps = warmupTps;
    container->LoadSymbolsUsersAndPrefillOrdersNoLog(testDataFutures);

    auto genResult = testDataFutures.genResult.get();
    auto benchmarkCommandsFuture = genResult->GetApiCommandsBenchmark();
    auto benchmarkCommands = benchmarkCommandsFuture.get();

    // Simple latency tracking
    std::vector<int64_t> latencies;
    latencies.reserve(benchmarkCommands.size());

    std::mutex latenciesMutex;

    // CountDownLatch implementation (match Java: CountDownLatch)
    class CountDownLatch {
    public:
      explicit CountDownLatch(int64_t count) : count_(count) {}

      void countDown() {
        std::lock_guard<std::mutex> lock(mu_);
        if (count_ > 0) {
          --count_;
        }
        if (count_ == 0) {
          cv_.notify_all();
        }
      }

      void await() {
        std::unique_lock<std::mutex> lock(mu_);
        cv_.wait(lock, [this] { return count_ == 0; });
      }

    private:
      mutable std::mutex mu_;
      std::condition_variable cv_;
      int64_t count_;
    };

    // Match Java: CountDownLatch latchBenchmark = new
    // CountDownLatch(genResult.getBenchmarkCommandsSize());
    CountDownLatch latchBenchmark(
        static_cast<int64_t>(benchmarkCommands.size()));

    // Helper function to get absolute nanoseconds (matching Java
    // System.nanoTime())
    auto getNanoTime = []() -> int64_t {
      return std::chrono::duration_cast<std::chrono::nanoseconds>(
                 std::chrono::steady_clock::now().time_since_epoch())
          .count();
    };

    // Match Java: final AtomicLong orderProgressCounter = new
    // AtomicLong(0);
    std::atomic<int64_t> orderProgressCounter{0};

    // Match Java: container.setConsumer((cmd, seq) -> {
    //     orderProgressCounter.lazySet(cmd.timestamp);
    //     final long latency = System.nanoTime() - cmd.timestamp;
    //     ...
    // });
    container->SetConsumer([&latencies, &latenciesMutex, &latchBenchmark,
                            &orderProgressCounter, getNanoTime](
                               exchange::core::common::cmd::OrderCommand *cmd,
                               int64_t seq) {
      if (cmd && cmd->timestamp > 0) {
        // Match Java: orderProgressCounter.lazySet(cmd.timestamp);
        // Use relaxed memory order (equivalent to Java lazySet)
        orderProgressCounter.store(cmd->timestamp, std::memory_order_relaxed);

        // Match Java: final long latency = System.nanoTime() -
        // cmd.timestamp;
        auto nowNs = getNanoTime();
        // Latency = current absolute time (ns) - command absolute timestamp
        // (ns)
        auto latency = nowNs - cmd->timestamp;
        std::lock_guard<std::mutex> lock(latenciesMutex);
        latencies.push_back(latency);
      }
      // Match Java: latchBenchmark.countDown();
      latchBenchmark.countDown();
    });

    const int nanosPerCmd = 1'000'000'000 / tps;
    auto startTime = std::chrono::steady_clock::now();

    // Match Java: for (ApiCommand cmd :
    // genResult.getApiCommandsBenchmark().join())
    // Match Java: long t = System.nanoTime();
    //           cmd.timestamp = t;
    //           api.submitCommand(cmd);
    //           while (orderProgressCounter.get() != t) {
    //               // spin until command is processed
    //           }
    for (auto *cmd : benchmarkCommands) {
      // Match Java: long t = System.nanoTime();
      int64_t t = getNanoTime();
      // Match Java: cmd.timestamp = t;
      cmd->timestamp = t;
      // Match Java: api.submitCommand(cmd);
      container->GetApi()->SubmitCommand(cmd);
      // Match Java: while (orderProgressCounter.get() != t) {
      //               // spin until command is processed
      //           }
      while (orderProgressCounter.load(std::memory_order_relaxed) != t) {
        std::this_thread::yield();
      }
    }

    // Match Java: latchBenchmark.await();
    latchBenchmark.await();

    // Match Java: container.setConsumer((cmd, seq) -> { });
    container->SetConsumer(nullptr);

    // Calculate latency statistics and output results (warmup also outputs)
    std::lock_guard<std::mutex> lock(latenciesMutex);
    bool shouldContinue = true;

    if (!latencies.empty()) {
      std::sort(latencies.begin(), latencies.end());

      // Calculate percentiles (match Java HDR histogram percentiles)
      auto getPercentile = [&latencies](double percentile) -> int64_t {
        if (latencies.empty())
          return 0;
        size_t index = static_cast<size_t>(
            std::round((percentile / 100.0) * (latencies.size() - 1)));
        return latencies[std::min(index, latencies.size() - 1)];
      };

      int64_t p50 = getPercentile(50.0);
      int64_t p90 = getPercentile(90.0);
      int64_t p95 = getPercentile(95.0);
      int64_t p99 = getPercentile(99.0);
      int64_t p99_9 = getPercentile(99.9);
      int64_t p99_99 = getPercentile(99.99);
      int64_t maxLatency = latencies.back();

      // Calculate throughput (MT/s)
      auto endTime = std::chrono::steady_clock::now();
      auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           endTime - startTime)
                           .count();
      float perfMt = (elapsedMs > 0)
                         ? (static_cast<float>(benchmarkCommands.size()) /
                            static_cast<float>(elapsedMs) / 1000.0f)
                         : 0.0f;

      // Output latency report (match Java format) - warmup also outputs
      std::ostringstream report;
      report << std::fixed << std::setprecision(3) << perfMt << " MT/s ";
      report << "50%:" << LatencyTools::FormatNanos(p50) << " ";
      report << "90%:" << LatencyTools::FormatNanos(p90) << " ";
      report << "95%:" << LatencyTools::FormatNanos(p95) << " ";
      report << "99%:" << LatencyTools::FormatNanos(p99) << " ";
      report << "99.9%:" << LatencyTools::FormatNanos(p99_9) << " ";
      report << "99.99%:" << LatencyTools::FormatNanos(p99_99) << " ";
      report << "W:" << LatencyTools::FormatNanos(maxLatency);

      LOG_INFO("{}", report.str());

      // Match Java: warmup doesn't stop on high latency
      // return warmup || histogram.getValueAtPercentile(50.0) < 10_000_000;
      // Warmup always continues
    }

    container->ResetExchangeCore();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  // Match Java: log.debug("Warmup done, starting tests");
  LOG_DEBUG("Warmup done, starting tests");

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

    // CountDownLatch implementation (match Java: CountDownLatch)
    class CountDownLatch {
    public:
      explicit CountDownLatch(int64_t count) : count_(count) {}

      void countDown() {
        std::lock_guard<std::mutex> lock(mu_);
        if (count_ > 0) {
          --count_;
        }
        if (count_ == 0) {
          cv_.notify_all();
        }
      }

      void await() {
        std::unique_lock<std::mutex> lock(mu_);
        cv_.wait(lock, [this] { return count_ == 0; });
      }

    private:
      mutable std::mutex mu_;
      std::condition_variable cv_;
      int64_t count_;
    };

    // Match Java: CountDownLatch latchBenchmark = new
    // CountDownLatch(genResult.getBenchmarkCommandsSize());
    CountDownLatch latchBenchmark(
        static_cast<int64_t>(benchmarkCommands.size()));

    // Helper function to get absolute nanoseconds (matching Java
    // System.nanoTime())
    auto getNanoTime = []() -> int64_t {
      return std::chrono::duration_cast<std::chrono::nanoseconds>(
                 std::chrono::steady_clock::now().time_since_epoch())
          .count();
    };

    // Match Java: final AtomicLong orderProgressCounter = new
    // AtomicLong(0);
    std::atomic<int64_t> orderProgressCounter{0};

    // Match Java: container.setConsumer((cmd, seq) -> {
    //     orderProgressCounter.lazySet(cmd.timestamp);
    //     final long latency = System.nanoTime() - cmd.timestamp;
    //     ...
    // });
    container->SetConsumer([&latencies, &latenciesMutex, &latchBenchmark,
                            &orderProgressCounter, getNanoTime](
                               exchange::core::common::cmd::OrderCommand *cmd,
                               int64_t seq) {
      if (cmd && cmd->timestamp > 0) {
        // Match Java: orderProgressCounter.lazySet(cmd.timestamp);
        // Use relaxed memory order (equivalent to Java lazySet)
        orderProgressCounter.store(cmd->timestamp, std::memory_order_relaxed);

        // Match Java: final long latency = System.nanoTime() -
        // cmd.timestamp;
        auto nowNs = getNanoTime();
        // Latency = current absolute time (ns) - command absolute timestamp
        // (ns)
        auto latency = nowNs - cmd->timestamp;
        std::lock_guard<std::mutex> lock(latenciesMutex);
        latencies.push_back(latency);
      }
      // Match Java: latchBenchmark.countDown();
      latchBenchmark.countDown();
    });

    const int nanosPerCmd = 1'000'000'000 / tps;
    auto startTime = std::chrono::steady_clock::now();

    // Match Java: for (ApiCommand cmd :
    // genResult.getApiCommandsBenchmark().join())
    // Match Java: long t = System.nanoTime();
    //           cmd.timestamp = t;
    //           api.submitCommand(cmd);
    //           while (orderProgressCounter.get() != t) {
    //               // spin until command is processed
    //           }
    for (auto *cmd : benchmarkCommands) {
      // Match Java: long t = System.nanoTime();
      int64_t t = getNanoTime();
      // Match Java: cmd.timestamp = t;
      cmd->timestamp = t;
      // Match Java: api.submitCommand(cmd);
      container->GetApi()->SubmitCommand(cmd);
      // Match Java: while (orderProgressCounter.get() != t) {
      //               // spin until command is processed
      //           }
      while (orderProgressCounter.load(std::memory_order_relaxed) != t) {
        std::this_thread::yield();
      }
    }

    // Match Java: latchBenchmark.await();
    latchBenchmark.await();

    // Match Java: container.setConsumer((cmd, seq) -> { });
    container->SetConsumer(nullptr);

    // Calculate latency statistics and output results
    std::lock_guard<std::mutex> lock(latenciesMutex);
    bool shouldContinue = true;

    // Debug: log if latencies are empty
    if (latencies.empty()) {
      LOG_INFO("Warning: No latency data collected (expected: {} commands)",
               benchmarkCommands.size());
    }

    if (!latencies.empty()) {
      std::sort(latencies.begin(), latencies.end());

      // Calculate percentiles (match Java HDR histogram percentiles)
      auto getPercentile = [&latencies](double percentile) -> int64_t {
        if (latencies.empty())
          return 0;
        size_t index = static_cast<size_t>(
            std::round((percentile / 100.0) * (latencies.size() - 1)));
        return latencies[std::min(index, latencies.size() - 1)];
      };

      int64_t p50 = getPercentile(50.0);
      int64_t p90 = getPercentile(90.0);
      int64_t p95 = getPercentile(95.0);
      int64_t p99 = getPercentile(99.0);
      int64_t p99_9 = getPercentile(99.9);
      int64_t p99_99 = getPercentile(99.99);
      int64_t maxLatency = latencies.back();

      // Calculate throughput (MT/s)
      auto endTime = std::chrono::steady_clock::now();
      auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           endTime - startTime)
                           .count();
      float perfMt = (elapsedMs > 0)
                         ? (static_cast<float>(benchmarkCommands.size()) /
                            static_cast<float>(elapsedMs) / 1000.0f)
                         : 0.0f;

      // Output latency report (match Java format)
      std::ostringstream report;
      report << std::fixed << std::setprecision(3) << perfMt << " MT/s ";
      report << "50%:" << LatencyTools::FormatNanos(p50) << " ";
      report << "90%:" << LatencyTools::FormatNanos(p90) << " ";
      report << "95%:" << LatencyTools::FormatNanos(p95) << " ";
      report << "99%:" << LatencyTools::FormatNanos(p99) << " ";
      report << "99.9%:" << LatencyTools::FormatNanos(p99_9) << " ";
      report << "99.99%:" << LatencyTools::FormatNanos(p99_99) << " ";
      report << "W:" << LatencyTools::FormatNanos(maxLatency);

      LOG_INFO("{}", report.str());

      // Stop if median latency > 1ms (10,000,000 nanoseconds)
      // Match Java: return warmup || histogram.getValueAtPercentile(50.0) <
      // 10_000_000;
      if (p50 >= 10'000'000) {
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
