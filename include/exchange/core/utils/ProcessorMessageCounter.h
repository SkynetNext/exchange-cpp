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

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace exchange {
namespace core {
namespace utils {

/**
 * Processor type enum for fast lookup (no string operations)
 */
enum class ProcessorType : uint8_t {
  GROUPING = 0,
  R1 = 1,
  R2 = 2,
  ME = 3,
  MAX_TYPES = 4
};

/**
 * ProcessorMessageCounter - High-performance batch size statistics
 *
 * Optimization strategies:
 * 1. Use enum instead of string for processor type (O(1) lookup)
 * 2. Use thread-local storage to reduce lock contention
 * 3. Batch writes to global storage only when needed
 * 4. Fixed-size arrays to avoid frequent allocations
 */
class ProcessorMessageCounter {
public:
  /**
   * Record a batch size (number of messages processed in one loop iteration)
   * High-performance: uses enum + processorId, thread-local storage
   */
  static void RecordBatchSize(ProcessorType type, int32_t processorId,
                              int64_t batchSize);

  /**
   * Get batch size statistics for a processor
   * Returns: {total_batches, min, max, p50, p90, p95, p99, p99.9}
   */
  static std::vector<int64_t> GetStatistics(ProcessorType type,
                                            int32_t processorId);

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
  static void Reset(ProcessorType type, int32_t processorId);

  /**
   * Print statistics for a processor to log
   */
  static void PrintStatistics(ProcessorType type, int32_t processorId);

  /**
   * Print all processor statistics to log
   */
  static void PrintAllStatistics();

  /**
   * Flush thread-local data to global storage (call periodically or at
   * shutdown)
   */
  static void FlushThreadLocalData();

  // Thread-local storage to reduce lock contention
  struct ThreadLocalData {
    static constexpr size_t BATCH_SIZE = 64;
    int64_t batchSizes_[BATCH_SIZE];
    size_t count_ = 0;
    ProcessorType type_;
    int32_t processorId_;

    ThreadLocalData()
        : batchSizes_{}, count_(0), type_(ProcessorType::GROUPING),
          processorId_(0) {}

    void Add(ProcessorType type, int32_t processorId, int64_t batchSize) {
      if (count_ < BATCH_SIZE) {
        batchSizes_[count_] = batchSize;
        count_++;
        type_ = type;
        processorId_ = processorId;
      } else {
        // Flush when full
        FlushToGlobal();
        batchSizes_[0] = batchSize;
        count_ = 1;
        type_ = type;
        processorId_ = processorId;
      }
    }

    void FlushToGlobal();
  };

  // Global storage structure
  struct BatchSizeData {
    std::vector<int64_t>
        batchSizes; // Store batch sizes for percentile calculation
    std::mutex mutex;
    int64_t min = 0;
    int64_t max = 0;
    int64_t totalBatches = 0;
  };

private:
  // Fast lookup: [type][processorId] -> BatchSizeData*
  static constexpr size_t MAX_PROCESSORS_PER_TYPE = 64;
  static BatchSizeData *processors_[static_cast<size_t>(
      ProcessorType::MAX_TYPES)][MAX_PROCESSORS_PER_TYPE];

  // Thread-local storage
  static thread_local ThreadLocalData threadLocalData_;

  // Helper functions
  static BatchSizeData *GetOrCreateProcessor(ProcessorType type,
                                             int32_t processorId);
  static int64_t CalculatePercentile(const std::vector<int64_t> &sorted,
                                     double percentile);
  static std::string GetProcessorName(ProcessorType type, int32_t processorId);
};

} // namespace utils
} // namespace core
} // namespace exchange
