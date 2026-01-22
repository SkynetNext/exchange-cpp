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
#include <string>

namespace exchange::core::tests::util {

/**
 * LatencyTools - utility class for latency measurement and formatting
 */
class LatencyTools {
public:
  /**
   * Format nanoseconds to human-readable string (µs, ms, s)
   * @param ns - nanoseconds
   * @return formatted string
   */
  static std::string FormatNanos(int64_t ns);

  // Note: createLatencyReportFast requires HdrHistogram library
  // Will be implemented when needed for performance tests
};

}  // namespace exchange::core::tests::util
