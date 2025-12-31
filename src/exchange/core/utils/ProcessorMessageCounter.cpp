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
#include <exchange/core/utils/Logger.h>
#include <exchange/core/utils/ProcessorMessageCounter.h>
#include <mutex>

namespace exchange {
namespace core {
namespace utils {

// Fast lookup array: [type][processorId] -> BatchSizeData*
ProcessorMessageCounter::BatchSizeData
    *ProcessorMessageCounter::processors_[static_cast<size_t>(
        ProcessorType::MAX_TYPES)][MAX_PROCESSORS_PER_TYPE] = {};

// Thread-local storage
thread_local ProcessorMessageCounter::ThreadLocalData
    ProcessorMessageCounter::threadLocalData_;

void ProcessorMessageCounter::ThreadLocalData::FlushToGlobal() {
  if (count_ == 0) {
    return;
  }

  // Group entries by processor to minimize lock contention
  for (size_t i = 0; i < count_; i++) {
    const auto &entry = entries_[i];
    if (entry.batchSize <= 0) {
      continue;
    }

    BatchSizeData *data = ProcessorMessageCounter::GetOrCreateProcessor(
        entry.type, entry.processorId);
    if (data == nullptr) {
      continue; // Skip invalid processor
    }

    std::lock_guard<std::mutex> lock(data->mutex);
    data->batchSizes.push_back(entry.batchSize);
    data->totalBatches++;

    if (data->totalBatches == 1) {
      data->min = entry.batchSize;
      data->max = entry.batchSize;
    } else {
      if (entry.batchSize < data->min) {
        data->min = entry.batchSize;
      }
      if (entry.batchSize > data->max) {
        data->max = entry.batchSize;
      }
    }
  }

  count_ = 0;
}

ProcessorMessageCounter::BatchSizeData *
ProcessorMessageCounter::GetOrCreateProcessor(ProcessorType type,
                                              int32_t processorId) {
  if (processorId < 0 ||
      processorId >= static_cast<int32_t>(MAX_PROCESSORS_PER_TYPE)) {
    return nullptr; // Invalid processor ID
  }

  size_t typeIdx = static_cast<size_t>(type);
  if (typeIdx >= static_cast<size_t>(ProcessorType::MAX_TYPES)) {
    return nullptr; // Invalid processor type
  }

  BatchSizeData *&data = processors_[typeIdx][processorId];
  if (data == nullptr) {
    data = new BatchSizeData();
  }
  return data;
}

void ProcessorMessageCounter::RecordBatchSize(ProcessorType type,
                                              int32_t processorId,
                                              int64_t batchSize) {
  if (batchSize <= 0) {
    return;
  }

  // Use thread-local storage to reduce lock contention
  threadLocalData_.Add(type, processorId, batchSize);
}

int64_t
ProcessorMessageCounter::CalculatePercentile(const std::vector<int64_t> &sorted,
                                             double percentile) {
  if (sorted.empty()) {
    return 0;
  }
  if (percentile <= 0.0) {
    return sorted.front();
  }
  if (percentile >= 100.0) {
    return sorted.back();
  }

  double index = (percentile / 100.0) * (sorted.size() - 1);
  size_t lower = static_cast<size_t>(index);
  size_t upper = lower + 1;

  if (upper >= sorted.size()) {
    return sorted.back();
  }

  double weight = index - lower;
  return static_cast<int64_t>(sorted[lower] * (1.0 - weight) +
                              sorted[upper] * weight);
}

std::vector<int64_t>
ProcessorMessageCounter::GetStatistics(ProcessorType type,
                                       int32_t processorId) {
  // Flush thread-local data first
  FlushThreadLocalData();

  BatchSizeData *data = GetOrCreateProcessor(type, processorId);
  std::lock_guard<std::mutex> lock(data->mutex);

  std::vector<int64_t> result(
      8, 0); // {total_batches, min, max, p50, p90, p95, p99, p99.9}

  if (data->batchSizes.empty()) {
    return result;
  }

  result[0] = data->totalBatches;
  result[1] = data->min;
  result[2] = data->max;

  // Calculate percentiles
  std::vector<int64_t> sorted = data->batchSizes;
  std::sort(sorted.begin(), sorted.end());

  result[3] = CalculatePercentile(sorted, 50.0); // P50
  result[4] = CalculatePercentile(sorted, 90.0); // P90
  result[5] = CalculatePercentile(sorted, 95.0); // P95
  result[6] = CalculatePercentile(sorted, 99.0); // P99
  result[7] = CalculatePercentile(sorted, 99.9); // P99.9

  return result;
}

std::unordered_map<std::string, std::vector<int64_t>>
ProcessorMessageCounter::GetAllStatistics() {
  // Flush all thread-local data first
  FlushThreadLocalData();

  std::unordered_map<std::string, std::vector<int64_t>> result;

  // Iterate through enum-based processors
  for (size_t typeIdx = 0;
       typeIdx < static_cast<size_t>(ProcessorType::MAX_TYPES); typeIdx++) {
    ProcessorType type = static_cast<ProcessorType>(typeIdx);
    for (int32_t processorId = 0; processorId < MAX_PROCESSORS_PER_TYPE;
         processorId++) {
      BatchSizeData *data = processors_[typeIdx][processorId];
      if (data == nullptr) {
        continue;
      }

      std::lock_guard<std::mutex> dataLock(data->mutex);
      std::vector<int64_t> stats(8, 0);
      if (!data->batchSizes.empty()) {
        stats[0] = data->totalBatches;
        stats[1] = data->min;
        stats[2] = data->max;

        std::vector<int64_t> sorted = data->batchSizes;
        std::sort(sorted.begin(), sorted.end());

        stats[3] = CalculatePercentile(sorted, 50.0);
        stats[4] = CalculatePercentile(sorted, 90.0);
        stats[5] = CalculatePercentile(sorted, 95.0);
        stats[6] = CalculatePercentile(sorted, 99.0);
        stats[7] = CalculatePercentile(sorted, 99.9);
      }

      result[GetProcessorName(type, processorId)] = stats;
    }
  }

  return result;
}

void ProcessorMessageCounter::Reset() {
  FlushThreadLocalData();

  // Reset enum-based processors
  for (size_t typeIdx = 0;
       typeIdx < static_cast<size_t>(ProcessorType::MAX_TYPES); typeIdx++) {
    for (int32_t processorId = 0; processorId < MAX_PROCESSORS_PER_TYPE;
         processorId++) {
      BatchSizeData *data = processors_[typeIdx][processorId];
      if (data != nullptr) {
        std::lock_guard<std::mutex> dataLock(data->mutex);
        data->batchSizes.clear();
        data->min = 0;
        data->max = 0;
        data->totalBatches = 0;
      }
    }
  }
}

void ProcessorMessageCounter::Reset(ProcessorType type, int32_t processorId) {
  FlushThreadLocalData();

  BatchSizeData *data = GetOrCreateProcessor(type, processorId);
  std::lock_guard<std::mutex> lock(data->mutex);
  data->batchSizes.clear();
  data->min = 0;
  data->max = 0;
  data->totalBatches = 0;
}

void ProcessorMessageCounter::FlushThreadLocalData() {
  threadLocalData_.FlushToGlobal();
}

void ProcessorMessageCounter::PrintStatistics(ProcessorType type,
                                              int32_t processorId) {
  auto stats = GetStatistics(type, processorId);
  std::string name = GetProcessorName(type, processorId);

  if (stats[0] == 0) {
    LOG_INFO("[{}] No batch statistics available", name);
    return;
  }

  LOG_INFO("[{}] Batch Size Statistics:", name);
  LOG_INFO("  Total Batches: {}", stats[0]);
  LOG_INFO("  Min: {}, Max: {}", stats[1], stats[2]);
  LOG_INFO("  P50: {}, P90: {}, P95: {}, P99: {}, P99.9: {}", stats[3],
           stats[4], stats[5], stats[6], stats[7]);
}

void ProcessorMessageCounter::PrintAllStatistics() {
  auto allStats = GetAllStatistics();

  if (allStats.empty()) {
    LOG_INFO("No processor batch statistics available");
    return;
  }

  LOG_INFO("=== Processor Batch Size Statistics ===");
  for (const auto &[name, stats] : allStats) {
    if (stats[0] == 0) {
      LOG_INFO("[{}] No data", name);
      continue;
    }

    LOG_INFO("[{}] Batches: {}, Min: {}, Max: {}, P50: {}, P90: {}, P95: {}, "
             "P99: {}, P99.9: {}",
             name, stats[0], stats[1], stats[2], stats[3], stats[4], stats[5],
             stats[6], stats[7]);
  }
  LOG_INFO("========================================");
}

std::string ProcessorMessageCounter::GetProcessorName(ProcessorType type,
                                                      int32_t processorId) {
  switch (type) {
  case ProcessorType::GROUPING:
    return "GroupingProcessor";
  case ProcessorType::R1:
    return "R1_" + std::to_string(processorId);
  case ProcessorType::R2:
    return "R2_" + std::to_string(processorId);
  case ProcessorType::ME:
    return "ME_" + std::to_string(processorId);
  default:
    return "Unknown_" + std::to_string(static_cast<int>(type)) + "_" +
           std::to_string(processorId);
  }
}

} // namespace utils
} // namespace core
} // namespace exchange
