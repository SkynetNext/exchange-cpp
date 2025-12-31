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

namespace exchange {
namespace core {
namespace utils {

/**
 * FastNanoTime - High-performance nanosecond timestamp
 *
 * Optimized for low-latency measurement scenarios:
 * - x86_64: Uses RDTSC + calibration (~1-5ns, fastest)
 * - Linux/Unix: Uses clock_gettime(CLOCK_MONOTONIC) (~10-20ns)
 * - Windows: Uses QueryPerformanceCounter (~20-30ns)
 * - Fallback: Uses chrono (~50-100ns)
 *
 * Performance comparison:
 * - RDTSC: ~1-5ns (fastest, requires calibration)
 * - clock_gettime: ~10-20ns (good balance)
 * - chrono: ~50-100ns (portable but slower)
 */
class FastNanoTime {
public:
  /**
   * Get current time in nanoseconds (monotonic clock)
   * Equivalent to Java System.nanoTime()
   *
   * @return nanoseconds since some arbitrary point (monotonic, not wall-clock)
   */
  static int64_t Now();

  /**
   * Initialize (calibrate TSC if available)
   * Should be called once at startup
   */
  static void Initialize();
};

} // namespace utils
} // namespace core
} // namespace exchange
