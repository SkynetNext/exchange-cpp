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

#include "ExecutionTime.h"
#include "LatencyTools.h"
#include <chrono>

namespace exchange {
namespace core {
namespace tests {
namespace util {

ExecutionTime::ExecutionTime(
    std::function<void(const std::string &)> executionTimeConsumer)
    : executionTimeConsumer_(std::move(executionTimeConsumer)),
      startTime_(std::chrono::high_resolution_clock::now()), elapsedNs_(0),
      timeCalculated_(false) {}

ExecutionTime::ExecutionTime()
    : executionTimeConsumer_([](const std::string &) {}),
      startTime_(std::chrono::high_resolution_clock::now()), elapsedNs_(0),
      timeCalculated_(false) {}

ExecutionTime::~ExecutionTime() {
  if (executionTimeConsumer_) {
    executionTimeConsumer_(GetTimeFormatted());
  }
}

std::string ExecutionTime::GetTimeFormatted() {
  if (!timeCalculated_) {
    GetTimeNs(); // Calculate elapsed time
  }
  return LatencyTools::FormatNanos(elapsedNs_);
}

int64_t ExecutionTime::GetTimeNs() const {
  if (!timeCalculated_) {
    auto endTime = std::chrono::high_resolution_clock::now();
    elapsedNs_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
                     endTime - startTime_)
                     .count();
    timeCalculated_ = true;
  }
  return elapsedNs_;
}

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
