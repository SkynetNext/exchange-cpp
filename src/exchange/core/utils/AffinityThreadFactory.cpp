/*
 * Copyright 2019 Maksim Zheravin
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

#include <atomic>
#include <exchange/core/utils/AffinityThreadFactory.h>
#include <exchange/core/utils/Logger.h>
#include <mutex>
#include <thread>

// Platform-specific includes for CPU affinity
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif

namespace exchange {
namespace core {
namespace utils {

std::atomic<int32_t> AffinityThreadFactory::threadsCounter_{0};

AffinityThreadFactory::AffinityThreadFactory(
    ThreadAffinityMode threadAffinityMode)
    : threadAffinityMode_(threadAffinityMode) {}

std::thread AffinityThreadFactory::newThread(std::function<void()> r) {
  if (threadAffinityMode_ == ThreadAffinityMode::THREAD_AFFINITY_DISABLE) {
    // No affinity - just create a regular thread
    return std::thread(r);
  }

  // Note: In Java version, it checks if runnable is TwoStepSlaveProcessor
  // and skips pinning. In C++, we can't easily do type checking on
  // std::function, so we proceed with affinity for all threads.

  // Note: Java version also tracks affinityReservations to avoid duplicate
  // reservations. In C++, std::function doesn't provide a reliable way to
  // get a unique identifier, so we skip this check for now.

  // Create thread that will execute with affinity
  return std::thread([this, r]() { ExecutePinned(r); });
}

bool AffinityThreadFactory::IsTaskPinned(void *task) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return affinityReservations_.find(task) != affinityReservations_.end();
}

void AffinityThreadFactory::ExecutePinned(std::function<void()> runnable) {
  // Increment thread counter before acquiring lock to get thread ID
  int32_t threadId = threadsCounter_.fetch_add(1) + 1;

  // Acquire affinity lock (if enabled) - this sets CPU affinity
  int cpuId = -1;
#ifdef _WIN32
  // Get CPU ID from Windows affinity mask (simplified)
  DWORD_PTR processAffinityMask = 0;
  DWORD_PTR systemAffinityMask = 0;
  if (GetProcessAffinityMask(GetCurrentProcess(), &processAffinityMask,
                             &systemAffinityMask)) {
    cpuId = threadId % (sizeof(DWORD_PTR) * 8);
    if (threadAffinityMode_ ==
        ThreadAffinityMode::THREAD_AFFINITY_ENABLE_PER_PHYSICAL_CORE) {
      cpuId = (cpuId / 2) * 2;
    }
  }
#elif defined(__linux__)
  int numCpus = sysconf(_SC_NPROCESSORS_ONLN);
  if (numCpus > 0) {
    cpuId = threadId % numCpus;
    if (threadAffinityMode_ ==
        ThreadAffinityMode::THREAD_AFFINITY_ENABLE_PER_PHYSICAL_CORE) {
      cpuId = (cpuId / 2) * 2;
    }
  }
#endif

  AcquireAffinityLock();

  try {
    // Format thread name similar to Java: "Thread-AF-{threadId}-cpu{cpuId}"
    std::string threadName = "Thread-AF-" + std::to_string(threadId);
    if (cpuId >= 0) {
      threadName += "-cpu" + std::to_string(cpuId);
    }

    // Note: Thread naming is platform-specific and not implemented here
    // On Windows: SetThreadDescription API (Windows 10+)
    // On Linux: pthread_setname_np

    if (cpuId >= 0) {
      LOG_DEBUG("{} will be running on thread={} pinned to cpu {}", "Task",
                threadName, cpuId);
    }

    // Execute the runnable
    runnable();

  } catch (...) {
    // Ensure cleanup happens even if runnable throws
    // Note: CPU affinity is automatically restored when thread exits
    throw;
  }

  // Cleanup: CPU affinity is automatically restored when thread exits
  LOG_DEBUG("Removing cpu lock/reservation from {}", "Task");
}

void AffinityThreadFactory::AcquireAffinityLock() {
  if (threadAffinityMode_ == ThreadAffinityMode::THREAD_AFFINITY_DISABLE) {
    return;
  }

#ifdef _WIN32
  // Windows implementation
  HANDLE threadHandle = GetCurrentThread();
  DWORD_PTR processAffinityMask = 0;
  DWORD_PTR systemAffinityMask = 0;

  if (GetProcessAffinityMask(GetCurrentProcess(), &processAffinityMask,
                             &systemAffinityMask)) {
    // Find an available CPU core
    DWORD_PTR affinityMask = 0;
    int coreId = threadsCounter_.load() %
                 (sizeof(DWORD_PTR) * 8); // Use modulo to cycle through cores

    if (threadAffinityMode_ ==
        ThreadAffinityMode::THREAD_AFFINITY_ENABLE_PER_PHYSICAL_CORE) {
      // For physical cores, we would need to detect hyperthreading
      // For simplicity, use every other core (assuming hyperthreading)
      coreId = (coreId / 2) * 2;
    }

    // Set affinity mask to the selected core
    affinityMask = static_cast<DWORD_PTR>(1) << coreId;
    // Ensure the mask is within available cores
    affinityMask &= processAffinityMask;

    if (affinityMask != 0) {
      // SetThreadAffinityMask returns previous affinity mask on success, 0 on
      // failure If it fails, thread will still run but may not be on expected
      // CPU
      DWORD_PTR prevMask = SetThreadAffinityMask(threadHandle, affinityMask);
      if (prevMask == 0) {
        // Failed to set affinity - thread will still run but may not be pinned
        LOG_WARN("[AffinityThreadFactory] Failed to set thread affinity mask "
                 "0x{:x}, thread will run without CPU pinning",
                 affinityMask);
      }
    } else {
      // No available cores in affinity mask - thread will run without pinning
      LOG_WARN("[AffinityThreadFactory] No available CPU cores in affinity "
               "mask (processAffinityMask=0x{:x}), thread will run without "
               "CPU pinning",
               processAffinityMask);
    }
  }

#elif defined(__linux__)
  // Linux implementation
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  // Get number of available CPUs
  int numCpus = sysconf(_SC_NPROCESSORS_ONLN);
  if (numCpus <= 0) {
    LOG_WARN("[AffinityThreadFactory] Cannot determine CPU count, thread will "
             "run without CPU pinning");
    return;
  }

  // Calculate which CPU to use
  int threadId = threadsCounter_.load();
  int cpuId = threadId % numCpus;

  if (threadAffinityMode_ ==
      ThreadAffinityMode::THREAD_AFFINITY_ENABLE_PER_PHYSICAL_CORE) {
    // For physical cores, use every other CPU (assuming hyperthreading)
    // This is a simplification - ideally we'd detect actual physical cores
    cpuId = (cpuId / 2) * 2;
  }

  // Set the CPU in the affinity mask
  CPU_SET(cpuId, &cpuset);

  // Apply the affinity mask to current thread
  pthread_t currentThread = pthread_self();
  int ret = pthread_setaffinity_np(currentThread, sizeof(cpu_set_t), &cpuset);
  if (ret != 0) {
    LOG_WARN("[AffinityThreadFactory] Failed to set thread affinity to CPU "
             "{}, error={}, thread will run without CPU pinning",
             cpuId, ret);
  }

#else
  // Unsupported platform - no-op
  // macOS and other platforms would need different implementations
#endif
}

} // namespace utils
} // namespace core
} // namespace exchange
