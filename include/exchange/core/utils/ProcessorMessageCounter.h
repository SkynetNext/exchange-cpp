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

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace exchange {
namespace core {
namespace utils {

/**
 * ProcessorMessageCounter - Batch size statistics for each processor
 * Tracks the distribution of batch sizes (messages processed per loop
 * iteration)
 */
class ProcessorMessageCounter {
public:
  /**
   * Record a batch size (number of messages processed in one loop iteration)
   */
  static void RecordBatchSize(const std::string &processorName,
                              int64_t batchSize);

  /**
   * Get batch size statistics for a processor
   * Returns: {total_batches, min, max, p50, p90, p95, p99, p99.9}
   */
  static std::vector<int64_t> GetStatistics(const std::string &processorName);

  /**
   * Get all processor statistics
   * Returns a map of processor name -> statistics vector
   */
  static std::unordered_map<std::string, std::vector<int64_t>>
  GetAllStatistics();

  /**
   * Reset all statistics
   */
  static void Reset();

  /**
   * Reset statistics for a specific processor
   */
  static void Reset(const std::string &processorName);

private:
  struct BatchSizeData {
    std::vector<int64_t>
        batchSizes; // Store batch sizes for percentile calculation
    std::mutex mutex;
    int64_t min = 0;
    int64_t max = 0;
    int64_t totalBatches = 0;
  };

  static std::mutex mutex_;
  static std::unordered_map<std::string, BatchSizeData *> processors_;

  static BatchSizeData *GetOrCreateProcessor(const std::string &processorName);

  static int64_t CalculatePercentile(const std::vector<int64_t> &sorted,
                                     double percentile);
};

} // namespace utils
} // namespace core
} // namespace exchange
