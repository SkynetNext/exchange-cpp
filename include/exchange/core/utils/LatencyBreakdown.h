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

#pragma once

#include "../common/cmd/OrderCommand.h"
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace exchange {
namespace core {
namespace utils {

/**
 * LatencyBreakdown - Records timestamps at each processing stage
 *
 * Usage:
 *   #ifdef ENABLE_LATENCY_BREAKDOWN
 *   LatencyBreakdown::Record(cmd, seq, Stage::GROUPING_START);
 *   // ... process ...
 *   LatencyBreakdown::Record(cmd, seq, Stage::GROUPING_END);
 *   #endif
 */
class LatencyBreakdown {
public:
  enum class Stage : int8_t {
    SUBMIT = 0, // Command submitted (timestamp already in cmd->timestamp)
    RING_BUFFER_PUBLISH, // Published to ring buffer
    GROUPING_START,      // GroupingProcessor starts processing
    GROUPING_END,        // GroupingProcessor finishes processing
    R1_START,            // RiskEngine (R1) starts processing
    R1_END,              // RiskEngine (R1) finishes processing
    ME_START,            // MatchingEngine starts processing
    ME_END,              // MatchingEngine finishes processing
    R2_START,            // RiskRelease (R2) starts processing
    R2_END,              // RiskRelease (R2) finishes processing
    RESULTS,             // ResultsHandler callback (end-to-end latency)
    MAX_STAGES
  };

  static constexpr const char *StageNames[] = {
      "SUBMIT",         "RING_BUFFER_PUBLISH",
      "GROUPING_START", "GROUPING_END",
      "R1_START",       "R1_END",
      "ME_START",       "ME_END",
      "R2_START",       "R2_END",
      "RESULTS"};

  struct StageLatency {
    int64_t stageTimeNs = 0;  // Time spent in this stage (ns)
    int64_t cumulativeNs = 0; // Cumulative time from submit (ns)
  };

  struct CommandLatency {
    int64_t submitTimeNs = 0;
    int64_t stageTimes[static_cast<int>(Stage::MAX_STAGES)] = {0};
    bool hasStage[static_cast<int>(Stage::MAX_STAGES)] = {false};
  };

  /**
   * Record timestamp for a stage
   * Lock-free: uses thread-local storage, no mutex in hot path
   */
  static void Record(common::cmd::OrderCommand *cmd, int64_t seq, Stage stage);

  /**
   * Get latency breakdown for a command
   * Returns stage latencies relative to submit time
   */
  static std::vector<StageLatency> GetBreakdown(int64_t seq);

  /**
   * Collect statistics for all recorded commands
   * Returns vector of pairs: (stage_name, {P50, P90, P95, P99, P99.9}) in
   * nanoseconds Ordered by stage index (so START comes before END)
   */
  static std::vector<std::pair<std::string, std::vector<int64_t>>>
  GetStatistics();

  /**
   * Clear all recorded data
   */
  static void Clear();

  /**
   * Enable/disable recording (for performance)
   */
  static void SetEnabled(bool enabled) { enabled_ = enabled; }
  static bool IsEnabled() { return enabled_; }

private:
  static int64_t GetNanoTime() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  // Thread-local storage: each thread maintains its own records (lock-free)
  static thread_local std::unordered_map<int64_t, CommandLatency>
      *threadRecords_;

  // Global state (only accessed during statistics collection)
  static bool enabled_;
  static std::mutex mergeMutex_; // Only used when merging thread-local data
  static std::vector<std::unordered_map<int64_t, CommandLatency> *>
      allThreadRecords_;

  // Initialize thread-local storage
  static void EnsureThreadLocal();
};

} // namespace utils
} // namespace core
} // namespace exchange
