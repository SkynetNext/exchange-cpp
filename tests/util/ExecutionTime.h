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

#include <functional>
#include <string>

namespace exchange {
namespace core {
namespace tests {
namespace util {

/**
 * ExecutionTime - RAII class for measuring execution time
 * Automatically measures time from construction to destruction
 */
class ExecutionTime {
public:
  /**
   * Constructor with custom consumer callback
   * @param executionTimeConsumer - callback to receive formatted time string
   */
  explicit ExecutionTime(
      std::function<void(const std::string &)> executionTimeConsumer);

  /**
   * Default constructor (no-op consumer)
   */
  ExecutionTime();

  /**
   * Destructor - automatically calls consumer with formatted time
   */
  ~ExecutionTime();

  /**
   * Get formatted execution time string
   * @return formatted time string (e.g., "1.23ms")
   */
  std::string GetTimeFormatted();

  /**
   * Get execution time in nanoseconds
   * @return nanoseconds elapsed
   */
  int64_t GetTimeNs() const;

private:
  std::function<void(const std::string &)> executionTimeConsumer_;
  int64_t startTimeNs_;
  mutable int64_t elapsedNs_;
  mutable bool timeCalculated_;
};

} // namespace util
} // namespace tests
} // namespace core
} // namespace exchange
