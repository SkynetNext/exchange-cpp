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

#include <disruptor/dsl/ThreadFactory.h>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

namespace exchange::core::utils {

/**
 * ThreadAffinityMode - thread affinity configuration mode
 */
enum class ThreadAffinityMode {
    THREAD_AFFINITY_DISABLE,
    THREAD_AFFINITY_ENABLE_PER_PHYSICAL_CORE,
    THREAD_AFFINITY_ENABLE_PER_LOGICAL_CORE
};

/**
 * AffinityThreadFactory - factory for creating threads with CPU affinity
 * Pins threads to specific CPU cores for better cache locality
 * Implements disruptor::dsl::ThreadFactory (matches Java version)
 *
 * Note: Must be managed via shared_ptr to ensure lifetime extends beyond
 * threads it creates (which access affinityReservations_ in their cleanup).
 */
class AffinityThreadFactory : public disruptor::dsl::ThreadFactory,
                              public std::enable_shared_from_this<AffinityThreadFactory> {
 public:
    explicit AffinityThreadFactory(ThreadAffinityMode threadAffinityMode);

    // disruptor::dsl::ThreadFactory interface
    std::thread newThread(std::function<void()> r) override;

    /**
     * Check if task was already pinned
     */
    bool IsTaskPinned(void* task) const;

 private:
    ThreadAffinityMode threadAffinityMode_;
    mutable std::mutex mutex_;
    std::set<void*> affinityReservations_;
    static std::atomic<int32_t> threadsCounter_;

    void ExecutePinned(std::function<void()> runnable);
    int AcquireAffinityLock(int32_t threadId);
};

}  // namespace exchange::core::utils
