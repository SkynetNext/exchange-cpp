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
#include <cstddef>
#include <utility>

namespace exchange::core::collections::objpool {

/**
 * ObjectPool<T> - Single-threaded typed object pool
 *
 * Intrusive design:
 * - Free list uses the object storage itself to store the next pointer (zero overhead)
 * - Pre-allocation similar to ObjectsPool (Production: 1M/64K/32K etc.; default 1024)
 * - Uses placement new and ~T() for construction/destruction
 *
 * Key properties:
 * - Single-threaded only (no atomics, no locks)
 * - Uses PoolAllocator (mimalloc in production, std allocator under sanitizers)
 * - Type-safe: Acquire returns T*, Release takes T*
 * - Zero memory overhead: free list pointer stored in object storage when free
 * - Requires sizeof(T) >= sizeof(void*) and alignof(T) >= alignof(void*)
 */
template <typename T>
class ObjectPool {
  static_assert(sizeof(T) >= sizeof(void*), "ObjectPool<T> requires sizeof(T) >= sizeof(void*)");
  static_assert(alignof(T) >= alignof(void*),
                "ObjectPool<T> requires alignof(T) >= alignof(void*)");

public:
  // Default pre-allocation (reference ObjectsPool: production uses 4K-1M; 1024 for generic)
  static constexpr size_t kDefaultInitialCapacity = 1024;

private:
  // Intrusive slot: union overlays free list pointer with object storage
  // When free: stores next pointer; when in use: stores T object
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
    for (size_t i = 0; i < count; ++i) {
      Slot* slot = AllocSlot();
      if (slot == nullptr) {
        return;
      }
      slot->next = free_list_head_;
      free_list_head_ = slot;
    }
  }

public:
  explicit ObjectPool(size_t initial_capacity = kDefaultInitialCapacity) {
    PopulateInitialPool(initial_capacity);
  }

  ~ObjectPool() {
    while (free_list_head_ != nullptr) {
      Slot* slot = free_list_head_;
      free_list_head_ = slot->next;
      PoolAllocator::FreeAligned(slot);
    }
  }

  ObjectPool(const ObjectPool&) = delete;
  ObjectPool& operator=(const ObjectPool&) = delete;
  ObjectPool(ObjectPool&&) = delete;
  ObjectPool& operator=(ObjectPool&&) = delete;

  template <typename... Args>
  T* Acquire(Args&&... args) {
    Slot* slot = free_list_head_;
    if (slot == nullptr) {
      slot = AllocSlot();
      if (slot == nullptr) {
        return nullptr;
      }
    } else {
      free_list_head_ = slot->next;
    }
    return new (static_cast<void*>(slot->storage)) T(std::forward<Args>(args)...);
  }

  void Release(T* ptr) {
    if (ptr == nullptr) {
      return;
    }
    ptr->~T();
    // With intrusive union design, ptr points directly to storage which is at offset 0
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    Slot* slot = reinterpret_cast<Slot*>(ptr);
    slot->next = free_list_head_;
    free_list_head_ = slot;
  }

private:
  Slot* free_list_head_{nullptr};
};

}  // namespace exchange::core::collections::objpool
