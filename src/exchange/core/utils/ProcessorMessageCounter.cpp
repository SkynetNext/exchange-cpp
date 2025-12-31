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

#include <exchange/core/utils/ProcessorMessageCounter.h>
#include <algorithm>
#include <mutex>

namespace exchange {
namespace core {
namespace utils {

std::mutex ProcessorMessageCounter::mutex_;
std::unordered_map<std::string, ProcessorMessageCounter::BatchSizeData *> ProcessorMessageCounter::processors_;

ProcessorMessageCounter::BatchSizeData *ProcessorMessageCounter::GetOrCreateProcessor(const std::string &processorName) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = processors_.find(processorName);
  if (it != processors_.end()) {
    return it->second;
  }
  // Create new processor data
  auto *data = new BatchSizeData();
  processors_[processorName] = data;
  return data;
}

void ProcessorMessageCounter::RecordBatchSize(const std::string &processorName, int64_t batchSize) {
  if (batchSize <= 0) {
    return; // Skip zero or negative batch sizes
  }
  
  BatchSizeData *data = GetOrCreateProcessor(processorName);
  std::lock_guard<std::mutex> lock(data->mutex);
  
  data->batchSizes.push_back(batchSize);
  data->totalBatches++;
  
  if (data->totalBatches == 1) {
    data->min = batchSize;
    data->max = batchSize;
  } else {
    if (batchSize < data->min) {
      data->min = batchSize;
    }
    if (batchSize > data->max) {
      data->max = batchSize;
    }
  }
}

int64_t ProcessorMessageCounter::CalculatePercentile(const std::vector<int64_t> &sorted, double percentile) {
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
  return static_cast<int64_t>(sorted[lower] * (1.0 - weight) + sorted[upper] * weight);
}

std::vector<int64_t> ProcessorMessageCounter::GetStatistics(const std::string &processorName) {
  BatchSizeData *data = GetOrCreateProcessor(processorName);
  std::lock_guard<std::mutex> lock(data->mutex);
  
  std::vector<int64_t> result(8, 0); // {total_batches, min, max, p50, p90, p95, p99, p99.9}
  
  if (data->batchSizes.empty()) {
    return result;
  }
  
  result[0] = data->totalBatches;
  result[1] = data->min;
  result[2] = data->max;
  
  // Calculate percentiles
  std::vector<int64_t> sorted = data->batchSizes;
  std::sort(sorted.begin(), sorted.end());
  
  result[3] = CalculatePercentile(sorted, 50.0);   // P50
  result[4] = CalculatePercentile(sorted, 90.0);   // P90
  result[5] = CalculatePercentile(sorted, 95.0);   // P95
  result[6] = CalculatePercentile(sorted, 99.0);   // P99
  result[7] = CalculatePercentile(sorted, 99.9);  // P99.9
  
  return result;
}

std::unordered_map<std::string, std::vector<int64_t>> ProcessorMessageCounter::GetAllStatistics() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::unordered_map<std::string, std::vector<int64_t>> result;
  
  for (const auto &pair : processors_) {
    std::lock_guard<std::mutex> dataLock(pair.second->mutex);
    
    std::vector<int64_t> stats(8, 0);
    if (!pair.second->batchSizes.empty()) {
      stats[0] = pair.second->totalBatches;
      stats[1] = pair.second->min;
      stats[2] = pair.second->max;
      
      std::vector<int64_t> sorted = pair.second->batchSizes;
      std::sort(sorted.begin(), sorted.end());
      
      stats[3] = CalculatePercentile(sorted, 50.0);
      stats[4] = CalculatePercentile(sorted, 90.0);
      stats[5] = CalculatePercentile(sorted, 95.0);
      stats[6] = CalculatePercentile(sorted, 99.0);
      stats[7] = CalculatePercentile(sorted, 99.9);
    }
    
    result[pair.first] = stats;
  }
  
  return result;
}

void ProcessorMessageCounter::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto &pair : processors_) {
    std::lock_guard<std::mutex> dataLock(pair.second->mutex);
    pair.second->batchSizes.clear();
    pair.second->min = 0;
    pair.second->max = 0;
    pair.second->totalBatches = 0;
  }
}

void ProcessorMessageCounter::Reset(const std::string &processorName) {
  BatchSizeData *data = GetOrCreateProcessor(processorName);
  std::lock_guard<std::mutex> lock(data->mutex);
  data->batchSizes.clear();
  data->min = 0;
  data->max = 0;
  data->totalBatches = 0;
}

} // namespace utils
} // namespace core
} // namespace exchange

