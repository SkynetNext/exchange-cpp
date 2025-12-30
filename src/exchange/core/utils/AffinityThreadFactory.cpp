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

  // Check for duplicate reservations (match Java behavior)
  // Note: We use function object address as identifier (not perfect, but
  // works for detecting most duplicates)
  void *taskId = static_cast<void *>(const_cast<std::function<void()> *>(&r));
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (affinityReservations_.find(taskId) != affinityReservations_.end()) {
      // Match Java: log.warn("Task {} was already pinned", runnable);
      LOG_WARN("Task {} was already pinned", taskId);
    }
    affinityReservations_.insert(taskId);
  }

  // Create thread that will execute with affinity
  // Capture shared_from_this() to ensure AffinityThreadFactory lifetime
  // extends beyond thread completion (thread accesses affinityReservations_)
  auto self = shared_from_this();
  return std::thread([self, r, taskId]() {
    self->ExecutePinned(r);
    // Remove reservation after thread completes
    std::lock_guard<std::mutex> lock(self->mutex_);
    self->affinityReservations_.erase(taskId);
  });
}

bool AffinityThreadFactory::IsTaskPinned(void *task) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return affinityReservations_.find(task) != affinityReservations_.end();
}

void AffinityThreadFactory::ExecutePinned(std::function<void()> runnable) {
  // Increment thread counter before acquiring lock to get thread ID
  int32_t threadId = threadsCounter_.fetch_add(1) + 1;

  // Acquire affinity lock (if enabled) - this sets CPU affinity
  // Returns the CPU ID that was actually set (or -1 if failed)
  int cpuId = AcquireAffinityLock(threadId);

  try {
    // Format thread name similar to Java: "Thread-AF-{threadId}-cpu{cpuId}"
    std::string threadName = "Thread-AF-" + std::to_string(threadId);
    if (cpuId >= 0) {
      threadName += "-cpu" + std::to_string(cpuId);
    }

    // Note: Thread naming is platform-specific and not implemented here
    // On Windows: SetThreadDescription API (Windows 10+)
    // On Linux: pthread_setname_np

    // Match Java: log.debug() is visible in tests, so use LOG_INFO for
    // visibility
    // Always log, even if cpuId is -1 (affinity failed)
    LOG_INFO("{} will be running on thread={} pinned to cpu {}", "Task",
             threadName, cpuId >= 0 ? cpuId : -1);

    // Execute the runnable
    runnable();

  } catch (...) {
    // Ensure cleanup happens even if runnable throws
    // Note: CPU affinity is automatically restored when thread exits
    throw;
  }

  // Cleanup: CPU affinity is automatically restored when thread exits
  // Match Java: log.debug() is visible in tests, so use LOG_INFO for visibility
  LOG_INFO("Removing cpu lock/reservation from {}", "Task");
}

int AffinityThreadFactory::AcquireAffinityLock(int32_t threadId) {
  if (threadAffinityMode_ == ThreadAffinityMode::THREAD_AFFINITY_DISABLE) {
    return -1;
  }

#ifdef _WIN32
  // Windows implementation - match OpenHFT Affinity behavior
  HANDLE threadHandle = GetCurrentThread();
  DWORD_PTR processAffinityMask = 0;
  DWORD_PTR systemAffinityMask = 0;

  if (GetProcessAffinityMask(GetCurrentProcess(), &processAffinityMask,
                             &systemAffinityMask)) {
    // Build list of available CPUs from affinity mask
    std::vector<int> availableCpus;
    DWORD_PTR mask = processAffinityMask;
    int cpuIndex = 0;
    while (mask != 0) {
      if (mask & 1) {
        availableCpus.push_back(cpuIndex);
      }
      mask >>= 1;
      cpuIndex++;
    }

    if (availableCpus.empty()) {
      LOG_WARN("[AffinityThreadFactory] No available CPU cores in affinity "
               "mask (processAffinityMask=0x{:x}), thread will run without "
               "CPU pinning",
               processAffinityMask);
      return -1;
    }

    // Match OpenHFT Affinity: assign from last CPU backwards
    // availableCpus is already sorted from low to high, so use reverse order
    int cpuIdx = availableCpus.size() - 1 - (threadId % availableCpus.size());
    int coreId = availableCpus[cpuIdx];

    if (threadAffinityMode_ ==
        ThreadAffinityMode::THREAD_AFFINITY_ENABLE_PER_PHYSICAL_CORE) {
      // For physical cores, we would need to detect hyperthreading
      // For simplicity, use every other core (assuming hyperthreading)
      // Find the closest even CPU in availableCpus
      int evenCpuId = (coreId / 2) * 2;
      bool found = false;
      for (int cpu : availableCpus) {
        if (cpu == evenCpuId) {
          coreId = evenCpuId;
          found = true;
          break;
        }
      }
      if (!found && coreId % 2 == 1) {
        // If odd CPU, try to find next even CPU in availableCpus
        for (int cpu : availableCpus) {
          if (cpu > coreId && cpu % 2 == 0) {
            coreId = cpu;
            found = true;
            break;
          }
        }
      }
    }

    // Set affinity mask to the selected core
    DWORD_PTR affinityMask = static_cast<DWORD_PTR>(1) << coreId;
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
        return -1;
      }
      return coreId;
    } else {
      // No available cores in affinity mask - thread will run without pinning
      LOG_WARN("[AffinityThreadFactory] No available CPU cores in affinity "
               "mask (processAffinityMask=0x{:x}), thread will run without "
               "CPU pinning",
               processAffinityMask);
      return -1;
    }
  }
  return -1;

#elif defined(__linux__)
  // Linux implementation
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  // Get number of available CPUs
  int numCpus = sysconf(_SC_NPROCESSORS_ONLN);
  if (numCpus <= 0) {
    LOG_WARN("[AffinityThreadFactory] Cannot determine CPU count, thread will "
             "run without CPU pinning");
    return -1;
  }

  // Match OpenHFT Affinity behavior: assign from last CPU backwards
  // This avoids CPU 0 (system reserved) and prefers higher-numbered CPUs
  // which are typically less used by system processes
  int cpuId = numCpus - 1 - (threadId % numCpus);

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
    return -1;
  }
  return cpuId;

#else
  // Unsupported platform - no-op
  // macOS and other platforms would need different implementations
  return -1;
#endif
}

} // namespace utils
} // namespace core
} // namespace exchange
