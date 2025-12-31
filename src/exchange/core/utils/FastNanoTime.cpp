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

#include <exchange/core/utils/FastNanoTime.h>

#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
#include <time.h>
#elif defined(_WIN32)
#include <intrin.h>
#include <windows.h>

#pragma intrinsic(__rdtsc)
#else
#include <chrono>
#endif

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64) ||            \
    defined(_M_AMD64)
// x86_64: Can use RDTSC for fastest performance
#define USE_RDTSC 1
#endif

namespace exchange {
namespace core {
namespace utils {

#if defined(USE_RDTSC) &&                                                      \
    (defined(__linux__) || defined(__APPLE__) || defined(__unix__))
// x86_64 Linux/Unix: Use RDTSC + calibration (fastest, ~1-5ns)
static double g_tsc_to_ns = 0.0;
static int64_t g_tsc_base = 0;
static int64_t g_ns_base = 0;

static inline uint64_t ReadTSC() {
#if defined(__x86_64__) || defined(__amd64__)
  uint32_t low, high;
  __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
  return (static_cast<uint64_t>(high) << 32) | low;
#elif defined(_M_X64) || defined(_M_AMD64)
  return __rdtsc();
#else
  return 0;
#endif
}

int64_t FastNanoTime::Now() {
  if (g_tsc_to_ns == 0.0) {
    // Fallback to clock_gettime if not calibrated
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1'000'000'000LL +
           static_cast<int64_t>(ts.tv_nsec);
  }

  uint64_t tsc = ReadTSC();
  int64_t tsc_delta = static_cast<int64_t>(tsc) - g_tsc_base;
  return g_ns_base + static_cast<int64_t>(tsc_delta * g_tsc_to_ns);
}

void FastNanoTime::Initialize() {
  if (g_tsc_to_ns != 0.0) {
    return; // Already calibrated
  }

  // Calibrate TSC to nanoseconds
  // Sample TSC and clock_gettime multiple times for accuracy
  const int samples = 10;
  int64_t tsc_samples[samples];
  int64_t ns_samples[samples];

  for (int i = 0; i < samples; i++) {
    uint64_t tsc1 = ReadTSC();
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t tsc2 = ReadTSC();

    // Use average TSC to reduce measurement error
    uint64_t tsc_avg = (tsc1 + tsc2) / 2;
    int64_t ns = static_cast<int64_t>(ts.tv_sec) * 1'000'000'000LL +
                 static_cast<int64_t>(ts.tv_nsec);

    tsc_samples[i] = static_cast<int64_t>(tsc_avg);
    ns_samples[i] = ns;

    // Small delay to ensure different timestamps
    struct timespec delay = {0, 100000}; // 100 microseconds
    nanosleep(&delay, nullptr);
  }

  // Calculate average TSC-to-ns ratio
  double total_ratio = 0.0;
  int valid_samples = 0;
  for (int i = 1; i < samples; i++) {
    int64_t tsc_delta = tsc_samples[i] - tsc_samples[0];
    int64_t ns_delta = ns_samples[i] - ns_samples[0];
    if (tsc_delta > 0 && ns_delta > 0) {
      total_ratio +=
          static_cast<double>(ns_delta) / static_cast<double>(tsc_delta);
      valid_samples++;
    }
  }

  if (valid_samples > 0) {
    g_tsc_to_ns = total_ratio / valid_samples;
    g_tsc_base = tsc_samples[0];
    g_ns_base = ns_samples[0];
  }
}

#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
// Linux/Unix: Use clock_gettime directly (~10-20ns)
int64_t FastNanoTime::Now() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<int64_t>(ts.tv_sec) * 1'000'000'000LL +
         static_cast<int64_t>(ts.tv_nsec);
}

void FastNanoTime::Initialize() {
  // No initialization needed for clock_gettime
}
#elif defined(USE_RDTSC) && defined(_WIN32)
// x86_64 Windows: Use RDTSC + calibration (fastest, ~1-5ns)
static double g_tsc_to_ns = 0.0;
static int64_t g_tsc_base = 0;
static int64_t g_ns_base = 0;

static inline uint64_t ReadTSC() { return __rdtsc(); }

int64_t FastNanoTime::Now() {
  if (g_tsc_to_ns == 0.0) {
    // Fallback to QPC if not calibrated
    static int64_t g_frequency = 0;
    if (g_frequency == 0) {
      LARGE_INTEGER freq;
      QueryPerformanceFrequency(&freq);
      g_frequency = freq.QuadPart;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1'000'000'000LL) / g_frequency;
  }

  uint64_t tsc = ReadTSC();
  int64_t tsc_delta = static_cast<int64_t>(tsc) - g_tsc_base;
  return g_ns_base + static_cast<int64_t>(tsc_delta * g_tsc_to_ns);
}

void FastNanoTime::Initialize() {
  if (g_tsc_to_ns != 0.0) {
    return; // Already calibrated
  }

  // Calibrate TSC to nanoseconds using QueryPerformanceCounter
  const int samples = 10;
  int64_t tsc_samples[samples];
  int64_t ns_samples[samples];

  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  int64_t frequency = freq.QuadPart;

  for (int i = 0; i < samples; i++) {
    uint64_t tsc1 = ReadTSC();
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    uint64_t tsc2 = ReadTSC();

    uint64_t tsc_avg = (tsc1 + tsc2) / 2;
    int64_t ns = (counter.QuadPart * 1'000'000'000LL) / frequency;

    tsc_samples[i] = static_cast<int64_t>(tsc_avg);
    ns_samples[i] = ns;

    // Small delay
    Sleep(1); // 1ms delay
  }

  // Calculate average TSC-to-ns ratio
  double total_ratio = 0.0;
  int valid_samples = 0;
  for (int i = 1; i < samples; i++) {
    int64_t tsc_delta = tsc_samples[i] - tsc_samples[0];
    int64_t ns_delta = ns_samples[i] - ns_samples[0];
    if (tsc_delta > 0 && ns_delta > 0) {
      total_ratio +=
          static_cast<double>(ns_delta) / static_cast<double>(tsc_delta);
      valid_samples++;
    }
  }

  if (valid_samples > 0) {
    g_tsc_to_ns = total_ratio / valid_samples;
    g_tsc_base = tsc_samples[0];
    g_ns_base = ns_samples[0];
  }
}

#elif defined(_WIN32)
// Windows: Use QueryPerformanceCounter (fast, ~20-30ns)
// Note: Windows doesn't have CLOCK_MONOTONIC, but QPC is monotonic
static int64_t g_frequency = 0;

int64_t FastNanoTime::Now() {
  if (g_frequency == 0) {
    Initialize();
  }
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  // Convert to nanoseconds: counter * 1e9 / frequency
  return (counter.QuadPart * 1'000'000'000LL) / g_frequency;
}

void FastNanoTime::Initialize() {
  if (g_frequency == 0) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    g_frequency = freq.QuadPart;
  }
}
#else
// Fallback: Use chrono (slower but portable, ~50-100ns)
int64_t FastNanoTime::Now() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void FastNanoTime::Initialize() {
  // No initialization needed for chrono
}
#endif

} // namespace utils
} // namespace core
} // namespace exchange
