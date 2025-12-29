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

#include <algorithm>
#include <cmath>
#include <exchange/core/utils/LatencyBreakdown.h>

namespace exchange {
namespace core {
namespace utils {

// Thread-local storage (lock-free in hot path)
thread_local std::unordered_map<int64_t, LatencyBreakdown::CommandLatency>
    *LatencyBreakdown::threadRecords_ = nullptr;

// Global state (only accessed during statistics collection)
bool LatencyBreakdown::enabled_ = false;
std::mutex LatencyBreakdown::mergeMutex_;
std::vector<std::unordered_map<int64_t, LatencyBreakdown::CommandLatency> *>
    LatencyBreakdown::allThreadRecords_;

void LatencyBreakdown::EnsureThreadLocal() {
  if (threadRecords_ == nullptr) {
    threadRecords_ = new std::unordered_map<int64_t, CommandLatency>();
    std::lock_guard<std::mutex> lock(mergeMutex_);
    allThreadRecords_.push_back(threadRecords_);
  }
}

void LatencyBreakdown::Record(common::cmd::OrderCommand *cmd, int64_t seq,
                              Stage stage) {
  if (!enabled_) {
    return;
  }

  // Initialize thread-local storage if needed (first call only)
  EnsureThreadLocal();

  int64_t nowNs = GetNanoTime();
  int stageIdx = static_cast<int>(stage);

  // Lock-free: write to thread-local map
  auto &record = (*threadRecords_)[seq];

  if (stage == Stage::SUBMIT) {
    record.submitTimeNs = cmd->timestamp;
    record.stageTimes[stageIdx] = cmd->timestamp;
    record.hasStage[stageIdx] = true;
  } else {
    record.stageTimes[stageIdx] = nowNs;
    record.hasStage[stageIdx] = true;
  }
}

std::vector<LatencyBreakdown::StageLatency>
LatencyBreakdown::GetBreakdown(int64_t seq) {
  // Check thread-local storage first (lock-free)
  if (threadRecords_ != nullptr) {
    auto it = threadRecords_->find(seq);
    if (it != threadRecords_->end()) {
      const auto &record = it->second;
      std::vector<StageLatency> result;

      int64_t lastTimeNs = record.submitTimeNs;
      for (int i = 0; i < static_cast<int>(Stage::MAX_STAGES); i++) {
        if (record.hasStage[i]) {
          StageLatency latency;
          latency.cumulativeNs = record.stageTimes[i] - record.submitTimeNs;
          latency.stageTimeNs = record.stageTimes[i] - lastTimeNs;
          result.push_back(latency);
          lastTimeNs = record.stageTimes[i];
        }
      }
      return result;
    }
  }

  // If not found in thread-local, search all threads (requires lock)
  std::lock_guard<std::mutex> lock(mergeMutex_);
  for (auto *records : allThreadRecords_) {
    if (records != nullptr) {
      auto it = records->find(seq);
      if (it != records->end()) {
        const auto &record = it->second;
        std::vector<StageLatency> result;

        int64_t lastTimeNs = record.submitTimeNs;
        for (int i = 0; i < static_cast<int>(Stage::MAX_STAGES); i++) {
          if (record.hasStage[i]) {
            StageLatency latency;
            latency.cumulativeNs = record.stageTimes[i] - record.submitTimeNs;
            latency.stageTimeNs = record.stageTimes[i] - lastTimeNs;
            result.push_back(latency);
            lastTimeNs = record.stageTimes[i];
          }
        }
        return result;
      }
    }
  }

  return {};
}

std::map<std::string, std::vector<int64_t>> LatencyBreakdown::GetStatistics() {
  // Merge all thread-local records (requires lock, but only during statistics)
  std::lock_guard<std::mutex> lock(mergeMutex_);

  // Collect stage latencies from all threads
  std::map<int, std::vector<int64_t>> stageLatencies;
  for (auto *records : allThreadRecords_) {
    if (records != nullptr) {
      for (const auto &[seq, record] : *records) {
        int64_t lastTimeNs = record.submitTimeNs;
        for (int i = 0; i < static_cast<int>(Stage::MAX_STAGES); i++) {
          if (record.hasStage[i]) {
            int64_t stageTimeNs = record.stageTimes[i] - lastTimeNs;
            stageLatencies[i].push_back(stageTimeNs);
            lastTimeNs = record.stageTimes[i];
          }
        }
      }
    }
  }

  // Calculate percentiles for each stage
  std::map<std::string, std::vector<int64_t>> result;
  auto getPercentile = [](std::vector<int64_t> &values,
                          double percentile) -> int64_t {
    if (values.empty()) {
      return 0;
    }
    std::sort(values.begin(), values.end());
    size_t index =
        static_cast<size_t>(std::ceil(values.size() * percentile / 100.0) - 1);
    if (index >= values.size()) {
      index = values.size() - 1;
    }
    return values[index];
  };

  for (const auto &[stageIdx, latencies] : stageLatencies) {
    if (stageIdx < static_cast<int>(Stage::MAX_STAGES)) {
      std::vector<int64_t> sorted = latencies;
      std::vector<int64_t> percentiles;
      percentiles.push_back(getPercentile(sorted, 50.0)); // P50
      percentiles.push_back(getPercentile(sorted, 90.0)); // P90
      percentiles.push_back(getPercentile(sorted, 95.0)); // P95
      percentiles.push_back(getPercentile(sorted, 99.0)); // P99
      percentiles.push_back(getPercentile(sorted, 99.9)); // P99.9
      result[StageNames[stageIdx]] = percentiles;
    }
  }

  return result;
}

void LatencyBreakdown::Clear() {
  // Clear thread-local storage
  if (threadRecords_ != nullptr) {
    threadRecords_->clear();
  }

  // Clear all thread records (requires lock)
  std::lock_guard<std::mutex> lock(mergeMutex_);
  for (auto *records : allThreadRecords_) {
    if (records != nullptr) {
      records->clear();
    }
  }
  allThreadRecords_.clear();
}

} // namespace utils
} // namespace core
} // namespace exchange
