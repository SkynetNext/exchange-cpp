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
#include <chrono>
#include <climits>
#include <condition_variable>
#include <ctime>
#include <exchange/core/utils/Logger.h>
#include <exchange/core/utils/ProcessorMessageCounter.h>
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

  // Helper function to get absolute nanoseconds (matching Java
  // System.nanoTime())
  auto getNanoTime = []() -> int64_t {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  };

  // Helper function to get current time in milliseconds (matching Java
  // System.currentTimeMillis())
  auto getCurrentTimeMillis = []() -> int64_t {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  };

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

  // Match Java: testIteration function
  // BiFunction<Integer, Boolean, Boolean> testIteration = (tps, warmup) -> {
  auto testIteration = [&](int tps, bool warmup) -> bool {
    container->LoadSymbolsUsersAndPrefillOrdersNoLog(testDataFutures);

    auto genResult = testDataFutures.genResult.get();
    auto benchmarkCommandsFuture = genResult->GetApiCommandsBenchmark();
    auto benchmarkCommands = benchmarkCommandsFuture.get();

    // Simple latency tracking (match Java: using HDR histogram, but we use
    // vector for now)
    std::vector<int64_t> latencies;
    latencies.reserve(benchmarkCommands.size());

    std::mutex latenciesMutex;

    // Match Java: CountDownLatch latchBenchmark = new
    // CountDownLatch(genResult.getBenchmarkCommandsSize());
    CountDownLatch latchBenchmark(
        static_cast<int64_t>(benchmarkCommands.size()));

    // Match Java: container.setConsumer((cmd, seq) -> {
    //     final long latency = System.nanoTime() - cmd.timestamp;
    //     hdrRecorder.recordValue(Math.min(latency, Integer.MAX_VALUE));
    //     latchBenchmark.countDown();
    // });
    container->SetConsumer(
        [&latencies, &latenciesMutex, &latchBenchmark, getNanoTime](
            exchange::core::common::cmd::OrderCommand *cmd, int64_t seq) {
          // Match Java: final long latency = System.nanoTime() - cmd.timestamp;
          auto nowNs = getNanoTime();
          auto latency = nowNs - cmd->timestamp;
          // Match Java: hdrRecorder.recordValue(Math.min(latency,
          // Integer.MAX_VALUE));
          auto latencyClamped =
              std::min(latency, static_cast<int64_t>(INT_MAX));
          std::lock_guard<std::mutex> lock(latenciesMutex);
          latencies.push_back(latencyClamped);
          // Match Java: latchBenchmark.countDown();
          latchBenchmark.countDown();
        });

    // Match Java: final int nanosPerCmd = 1_000_000_000 / tps;
    const int nanosPerCmd = 1'000'000'000 / tps;
    // Match Java: final long startTimeMs = System.currentTimeMillis();
    auto startTimeMs = getCurrentTimeMillis();

    // Match Java: long plannedTimestamp = System.nanoTime();
    int64_t plannedTimestamp = getNanoTime();

    // Match Java: for (ApiCommand cmd :
    // genResult.getApiCommandsBenchmark().join())
    // Match Java: while (System.nanoTime() < plannedTimestamp) {
    //     // spin until its time to send next command
    // }
    // Match Java: cmd.timestamp = plannedTimestamp;
    // Match Java: api.submitCommand(cmd);
    // Match Java: plannedTimestamp += nanosPerCmd;
    for (auto *cmd : benchmarkCommands) {
      // Match Java: while (System.nanoTime() < plannedTimestamp) {
      //     // spin until its time to send next command
      // }
      while (getNanoTime() < plannedTimestamp) {
        // spin until its time to send next command
      }
      // Match Java: cmd.timestamp = plannedTimestamp;
      cmd->timestamp = plannedTimestamp;
      // Match Java: api.submitCommand(cmd);
      container->GetApi()->SubmitCommand(cmd);
      // Match Java: plannedTimestamp += nanosPerCmd;
      plannedTimestamp += nanosPerCmd;
    }

    // Match Java: latchBenchmark.await();
    latchBenchmark.await();
    // Match Java: container.setConsumer((cmd, seq) -> { });
    container->SetConsumer(nullptr);

    // Match Java: final long processingTimeMs = System.currentTimeMillis() -
    // startTimeMs;
    auto processingTimeMs = getCurrentTimeMillis() - startTimeMs;
    // Match Java: final float perfMt = (float)
    // genResult.getBenchmarkCommandsSize() / (float) processingTimeMs /
    // 1000.0f;
    float perfMt = (processingTimeMs > 0)
                       ? (static_cast<float>(benchmarkCommands.size()) /
                          static_cast<float>(processingTimeMs) / 1000.0f)
                       : 0.0f;
    // Match Java: String tag = String.format("%.3f MT/s", perfMt);
    std::ostringstream tagStream;
    tagStream << std::fixed << std::setprecision(3) << perfMt << " MT/s";

    // Calculate latency statistics (match Java HDR histogram)
    std::lock_guard<std::mutex> lock(latenciesMutex);

    // Match Java: HDR histogram always has values (returns 0 if no records)
    // Calculate percentiles (match Java HDR histogram percentiles)
    auto getPercentile = [&latencies](double percentile) -> int64_t {
      if (latencies.empty())
        return 0; // Match Java: histogram.getValueAtPercentile returns 0 if no
                  // records
      size_t index = static_cast<size_t>(
          std::round((percentile / 100.0) * (latencies.size() - 1)));
      return latencies[std::min(index, latencies.size() - 1)];
    };

    // Always calculate percentiles (match Java: histogram always has values)
    if (!latencies.empty()) {
      std::sort(latencies.begin(), latencies.end());
    }

    int64_t p50 = getPercentile(50.0);
    int64_t p90 = getPercentile(90.0);
    int64_t p95 = getPercentile(95.0);
    int64_t p99 = getPercentile(99.0);
    int64_t p99_9 = getPercentile(99.9);
    int64_t p99_99 = getPercentile(99.99);
    int64_t maxLatency = latencies.empty() ? 0 : latencies.back();

    // Match Java: log.info("{} {}", tag,
    // LatencyTools.createLatencyReportFast(histogram));
    std::ostringstream report;
    report << tagStream.str() << " ";
    report << "50%:" << LatencyTools::FormatNanos(p50) << " ";
    report << "90%:" << LatencyTools::FormatNanos(p90) << " ";
    report << "95%:" << LatencyTools::FormatNanos(p95) << " ";
    report << "99%:" << LatencyTools::FormatNanos(p99) << " ";
    report << "99.9%:" << LatencyTools::FormatNanos(p99_9) << " ";
    report << "99.99%:" << LatencyTools::FormatNanos(p99_99) << " ";
    report << "W:" << LatencyTools::FormatNanos(maxLatency);

    LOG_INFO("{}", report.str());

    // Print processor batch size statistics for this iteration
    if (!warmup) {
      utils::ProcessorMessageCounter::PrintAllStatistics();
    }

    // Match Java: container.resetExchangeCore();
    container->ResetExchangeCore();

    // Clean up ApiCommand objects to prevent memory leak
    for (auto *cmd : benchmarkCommands) {
      delete cmd;
    }

    // Match Java: System.gc();
    // Note: C++ doesn't have direct equivalent, but we can hint the compiler
    // Match Java: Thread.sleep(500);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Reset statistics for next iteration
    utils::ProcessorMessageCounter::Reset();

    // Match Java: return warmup || histogram.getValueAtPercentile(50.0) <
    // 10_000_000;
    // stop testing if median latency above 10 milliseconds (10,000,000
    // nanoseconds)
    return warmup || p50 < 10'000'000;
  };

  // Match Java: container.executeTestingThread(() -> {
  //     log.debug("Warming up {} cycles...", warmupCycles);
  //     IntStream.range(0, warmupCycles)
  //             .forEach(i -> testIteration.apply(warmupTps, true));
  //     log.debug("Warmup done, starting tests");
  //
  //     return IntStream.range(0, 10000)
  //             .map(i -> targetTps + targetTpsStep * i)
  //             .mapToObj(tps -> testIteration.apply(tps, false))
  //             .allMatch(x -> x);
  // });
  // Reset statistics before starting
  utils::ProcessorMessageCounter::Reset();

  LOG_DEBUG("Warming up {} cycles...", warmupCycles);
  for (int i = 0; i < warmupCycles; i++) {
    testIteration(warmupTps, true);
  }
  LOG_DEBUG("Warmup done, starting tests");

  // Match Java: IntStream.range(0, 10000)
  //         .map(i -> targetTps + targetTpsStep * i)
  //         .mapToObj(tps -> testIteration.apply(tps, false))
  //         .allMatch(x -> x);
  for (int i = 0; i < 10000; i++) {
    int tps = targetTps + targetTpsStep * i;
    if (!testIteration(tps, false)) {
      break; // Match Java: .allMatch(x -> x) - stop if any iteration returns
             // false
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

  const int targetTps = 500'000; // transactions per second

  // will print each occurrence if latency>0.2ms
  const long hiccupThresholdNs = 200'000;

  auto testDataFutures =
      ExchangeTestContainer::PrepareTestDataAsync(testDataParameters, 1);

  auto container = ExchangeTestContainer::Create(
      performanceCfg, initialStateCfg,
      exchange::core::common::config::SerializationConfiguration::Default());

  // Helper function to get absolute nanoseconds (matching Java
  // System.nanoTime())
  auto getNanoTime = []() -> int64_t {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  };

  // Helper function to get current time in milliseconds (matching Java
  // System.currentTimeMillis())
  auto getCurrentTimeMillis = []() -> int64_t {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  };

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

  // Match Java: IntFunction<TreeMap<ZonedDateTime, Long>> testIteration = tps
  // -> {
  auto testIteration = [&](int tps) -> std::map<int64_t, int64_t> {
    container->LoadSymbolsUsersAndPrefillOrdersNoLog(testDataFutures);

    auto genResult = testDataFutures.genResult.get();
    auto benchmarkCommandsFuture = genResult->GetApiCommandsBenchmark();
    auto benchmarkCommands = benchmarkCommandsFuture.get();

    // Match Java: final LongLongHashMap hiccupTimestampsNs = new
    // LongLongHashMap(10000);
    std::map<int64_t, int64_t> hiccupTimestampsNs;

    // Match Java: final CountDownLatch latchBenchmark = new
    // CountDownLatch(genResult.getBenchmarkCommandsSize());
    CountDownLatch latchBenchmark(
        static_cast<int64_t>(benchmarkCommands.size()));

    // Match Java: final MutableLong nextHiccupAcceptTimestampNs = new
    // MutableLong(0);
    std::mutex hiccupMutex;
    int64_t nextHiccupAcceptTimestampNs = 0;

    // Match Java: container.setConsumer((cmd, seq) -> {
    //     long now = System.nanoTime();
    //     // skip other messages in delayed group
    //     if (now < nextHiccupAcceptTimestampNs.value) {
    //         return;
    //     }
    //     long diffNs = now - cmd.timestamp;
    //     // register hiccup timestamps
    //     if (diffNs > hiccupThresholdNs) {
    //         hiccupTimestampsNs.put(cmd.timestamp, diffNs);
    //         nextHiccupAcceptTimestampNs.value = cmd.timestamp + diffNs;
    //     }
    //     latchBenchmark.countDown();
    // });
    container->SetConsumer(
        [&hiccupTimestampsNs, &hiccupMutex, &nextHiccupAcceptTimestampNs,
         hiccupThresholdNs, &latchBenchmark, getNanoTime](
            exchange::core::common::cmd::OrderCommand *cmd, int64_t seq) {
          // Match Java: long now = System.nanoTime();
          int64_t now = getNanoTime();
          // Match Java: // skip other messages in delayed group
          // Match Java: if (now < nextHiccupAcceptTimestampNs.value) {
          //     return;
          // }
          std::lock_guard<std::mutex> lock(hiccupMutex);
          // Match Java: if (now < nextHiccupAcceptTimestampNs.value) {
          //     return;
          // }
          // Note: Java code returns without countDown, which may cause issues
          // but we match Java behavior exactly
          if (now < nextHiccupAcceptTimestampNs) {
            return;
          }
          // Match Java: long diffNs = now - cmd.timestamp;
          int64_t diffNs = now - cmd->timestamp;
          // Match Java: // register hiccup timestamps
          // Match Java: if (diffNs > hiccupThresholdNs) {
          //     hiccupTimestampsNs.put(cmd.timestamp, diffNs);
          //     nextHiccupAcceptTimestampNs.value = cmd.timestamp + diffNs;
          // }
          if (diffNs > hiccupThresholdNs) {
            // Match Java: hiccupTimestampsNs.put(cmd.timestamp, diffNs);
            // Use max for merging (match Java: Math::max)
            auto it = hiccupTimestampsNs.find(cmd->timestamp);
            if (it != hiccupTimestampsNs.end()) {
              it->second = std::max(it->second, diffNs);
            } else {
              hiccupTimestampsNs[cmd->timestamp] = diffNs;
            }
            // Match Java: nextHiccupAcceptTimestampNs.value = cmd.timestamp +
            // diffNs;
            nextHiccupAcceptTimestampNs = cmd->timestamp + diffNs;
          }
          // Match Java: latchBenchmark.countDown();
          latchBenchmark.countDown();
        });

    // Match Java: final long startTimeNs = System.nanoTime();
    int64_t startTimeNs = getNanoTime();
    // Match Java: final long startTimeMs = System.currentTimeMillis();
    int64_t startTimeMs = getCurrentTimeMillis();
    // Match Java: final int nanosPerCmd = 1_000_000_000 / tps;
    const int nanosPerCmd = 1'000'000'000 / tps;

    // Match Java: long plannedTimestamp = System.nanoTime();
    int64_t plannedTimestamp = getNanoTime();

    // Match Java: for (final ApiCommand cmd :
    // genResult.getApiCommandsBenchmark().join())
    // Match Java: // spin until its time to send next command
    // Match Java: while (System.nanoTime() < plannedTimestamp) ;
    // Match Java: cmd.timestamp = plannedTimestamp;
    // Match Java: api.submitCommand(cmd);
    // Match Java: plannedTimestamp += nanosPerCmd;
    for (auto *cmd : benchmarkCommands) {
      // Match Java: // spin until its time to send next command
      // Match Java: while (System.nanoTime() < plannedTimestamp) ;
      while (getNanoTime() < plannedTimestamp) {
        // spin until its time to send next command
      }
      // Match Java: cmd.timestamp = plannedTimestamp;
      cmd->timestamp = plannedTimestamp;
      // Match Java: api.submitCommand(cmd);
      container->GetApi()->SubmitCommand(cmd);
      // Match Java: plannedTimestamp += nanosPerCmd;
      plannedTimestamp += nanosPerCmd;
    }

    // Match Java: latchBenchmark.await();
    latchBenchmark.await();

    // Match Java: container.setConsumer((cmd, seq) -> { });
    container->SetConsumer(nullptr);

    // Clean up ApiCommand objects to prevent memory leak
    for (auto *cmd : benchmarkCommands) {
      delete cmd;
    }

    // Match Java: final TreeMap<ZonedDateTime, Long> sorted = new TreeMap<>();
    // Match Java: // convert nanosecond timestamp into Instant
    // Match Java: // not very precise, but for 1ms resolution is ok (0,05%
    // accuracy is required)...
    // Match Java: // delay (nanoseconds) merging as max value
    // Match Java: hiccupTimestampsNs.forEachKeyValue(
    //         (eventTimestampNs, delay) -> sorted.merge(
    //                 ZonedDateTime.ofInstant(Instant.ofEpochMilli(startTimeMs
    //                 + (eventTimestampNs - startTimeNs) / 1_000_000),
    //                 ZoneId.systemDefault()),
    //                 delay,
    //                 Math::max));
    // Note: We'll use a simple map with timestamp in milliseconds as key
    // (matching the Java TreeMap<ZonedDateTime, Long> behavior)
    std::map<int64_t, int64_t> sorted;
    {
      std::lock_guard<std::mutex> lock(hiccupMutex);
      for (const auto &pair : hiccupTimestampsNs) {
        int64_t eventTimestampNs = pair.first;
        int64_t delay = pair.second;
        // Match Java: Instant.ofEpochMilli(startTimeMs + (eventTimestampNs -
        // startTimeNs) / 1_000_000)
        int64_t eventTimestampMs =
            startTimeMs + (eventTimestampNs - startTimeNs) / 1'000'000;
        // Match Java: sorted.merge(..., delay, Math::max)
        auto it = sorted.find(eventTimestampMs);
        if (it != sorted.end()) {
          it->second = std::max(it->second, delay);
        } else {
          sorted[eventTimestampMs] = delay;
        }
      }
    }

    // Match Java: container.resetExchangeCore();
    container->ResetExchangeCore();

    // Match Java: System.gc();
    // Note: C++ doesn't have direct equivalent, but we can hint the compiler
    // Match Java: Thread.sleep(500);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Match Java: return sorted;
    return sorted;
  };

  // Match Java: container.executeTestingThread(() -> {
  //     log.debug("Warming up {} cycles...", 3_000_000);
  //     IntStream.range(0, warmupCycles)
  //             .mapToObj(i -> testIteration.apply(targetTps))
  //             .forEach(res -> log.debug("warming up ({} hiccups)",
  //             res.size()));
  //
  //     log.debug("Warmup done, starting tests");
  //     IntStream.range(0, 10000)
  //             .mapToObj(i -> testIteration.apply(targetTps))
  //             .forEach(res -> {
  //                 if (res.isEmpty()) {
  //                     log.debug("no hiccups");
  //                 } else {
  //                     log.debug("------------------ {} hiccups
  //                     -------------------", res.size());
  //                     res.forEach((timestamp, delay) -> log.debug("{}: {}µs",
  //                     timestamp.toLocalTime(), delay / 1000));
  //                 }
  //             });
  //
  //     return null;
  // });
  LOG_DEBUG("Warming up {} cycles...", warmupCycles);
  for (int i = 0; i < warmupCycles; i++) {
    auto res = testIteration(targetTps);
    LOG_DEBUG("warming up ({} hiccups)", res.size());
  }

  LOG_DEBUG("Warmup done, starting tests");
  for (int i = 0; i < 10000; i++) {
    auto res = testIteration(targetTps);
    if (res.empty()) {
      LOG_DEBUG("no hiccups");
    } else {
      LOG_DEBUG("------------------ {} hiccups -------------------",
                res.size());
      for (const auto &pair : res) {
        int64_t timestampMs = pair.first;
        int64_t delay = pair.second;
        // Match Java: log.debug("{}: {}µs", timestamp.toLocalTime(), delay /
        // 1000);
        // Convert timestampMs to local time for display
        auto timePoint = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(timestampMs));
        auto timeT = std::chrono::system_clock::to_time_t(timePoint);
        std::tm localTime;
#ifdef _WIN32
        localtime_s(&localTime, &timeT);
#else
        localtime_r(&timeT, &localTime);
#endif
        std::ostringstream timeStr;
        timeStr << std::setfill('0') << std::setw(2) << localTime.tm_hour << ":"
                << std::setw(2) << localTime.tm_min << ":" << std::setw(2)
                << localTime.tm_sec;
        LOG_DEBUG("{}: {}µs", timeStr.str(), delay / 1000);
      }
    }
  }
}

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
