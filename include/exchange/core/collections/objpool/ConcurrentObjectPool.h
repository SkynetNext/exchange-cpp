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

#include <exchange/core/collections/objpool/PoolAllocator.h>
#include <atomic>
#include <cstddef>
#include <utility>

namespace exchange::core::collections::objpool {

/**
 * ConcurrentObjectPool<T> - Thread-safe typed object pool
 *
 * Aligned with ObjectPool design:
 * - Intrusive slot: union overlays free list pointer with object storage (zero overhead)
 * - Single-arg constructor: initial_capacity only
 * - Uses PoolAllocator (mimalloc in production, std allocator under sanitizers)
 * - Type-safe: Acquire returns T*, Release takes T*
 *
 * Thread safety: lock-free stack (CAS) for the free list. Same Slot layout as ObjectPool.
 * Requires sizeof(T) >= sizeof(void*) and alignof(T) >= alignof(void*).
 */
template <typename T>
class ConcurrentObjectPool {
  static_assert(sizeof(T) >= sizeof(void*),
                "ConcurrentObjectPool<T> requires sizeof(T) >= sizeof(void*)");
  static_assert(alignof(T) >= alignof(void*),
                "ConcurrentObjectPool<T> requires alignof(T) >= alignof(void*)");

public:
  static constexpr size_t kDefaultInitialCapacity = 1024;

private:
  // Same intrusive slot as ObjectPool: free list next vs object storage
  union Slot {
    Slot* next;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
    alignas(T) char storage[sizeof(T)];
  };

  static Slot* AllocSlot() {
    void* p = PoolAllocator::AllocAligned(sizeof(Slot), alignof(Slot));
    return static_cast<Slot*>(p);
  }

  void PopulateInitialPool(size_t count) {
    Slot* head = nullptr;
    for (size_t i = 0; i < count; ++i) {
      Slot* slot = AllocSlot();
      if (slot == nullptr) {
        break;
      }
      slot->next = head;
      head = slot;
    }
    free_list_head_.store(head, std::memory_order_relaxed);
  }

  Slot* TryPop() {
    Slot* head = free_list_head_.load(std::memory_order_acquire);
    while (head != nullptr) {
      Slot* next = head->next;
      if (free_list_head_.compare_exchange_weak(head, next, std::memory_order_acquire,
                                                std::memory_order_relaxed)) {
        return head;
      }
    }
    return nullptr;
  }

  void Push(Slot* slot) {
    slot->next = free_list_head_.load(std::memory_order_relaxed);
    while (!free_list_head_.compare_exchange_weak(slot->next, slot, std::memory_order_release,
                                                  std::memory_order_relaxed)) {
    }
  }

public:
  explicit ConcurrentObjectPool(size_t initial_capacity = kDefaultInitialCapacity) {
    PopulateInitialPool(initial_capacity);
  }

  ~ConcurrentObjectPool() {
    Slot* slot;
    while ((slot = TryPop()) != nullptr) {
      PoolAllocator::FreeAligned(slot);
    }
  }

  ConcurrentObjectPool(const ConcurrentObjectPool&) = delete;
  ConcurrentObjectPool& operator=(const ConcurrentObjectPool&) = delete;
  ConcurrentObjectPool(ConcurrentObjectPool&&) = delete;
  ConcurrentObjectPool& operator=(ConcurrentObjectPool&&) = delete;

  template <typename... Args>
  T* Acquire(Args&&... args) {
    Slot* slot = TryPop();
    if (slot == nullptr) {
      slot = AllocSlot();
      if (slot == nullptr) {
        return nullptr;
      }
    }
    return new (static_cast<void*>(slot->storage)) T(std::forward<Args>(args)...);
  }

  void Release(T* ptr) {
    if (ptr == nullptr) {
      return;
    }
    ptr->~T();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    Slot* slot = reinterpret_cast<Slot*>(ptr);
    Push(slot);
  }

private:
  static constexpr size_t kCacheLineSize = 128;
  alignas(kCacheLineSize) std::atomic<Slot*> free_list_head_{nullptr};
};

}  // namespace exchange::core::collections::objpool
