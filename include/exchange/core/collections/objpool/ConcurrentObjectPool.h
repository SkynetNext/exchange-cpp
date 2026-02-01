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

#include <mimalloc.h>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace exchange::core::collections::objpool {

/**
 * ConcurrentObjectPool<T> - Thread-safe typed object pool using mimalloc
 *
 * Design aligned with moodycamel::ConcurrentQueue's FreeList<Block>:
 * - BLOCK_SIZE objects per block (default 32)
 * - Lock-free FreeList using REFS_MASK/SHOULD_BE_ON_FREELIST technique
 * - Block is recycled to freeList only when completely empty (slotsInUse == 0)
 *
 * Key properties:
 * - Thread-safe (lock-free CAS operations)
 * - Uses mimalloc for all allocations
 * - Type-safe: Acquire returns T*, Release takes T*
 */
template <typename T>
class ConcurrentObjectPool {
public:
  static constexpr size_t BLOCK_SIZE = 32;
  static constexpr size_t kDefaultInitialCapacity = 32 * BLOCK_SIZE;  // 1024

private:
  static constexpr size_t kCacheLineSize = 128;

  // Lock-free free list constants (from moodycamel::FreeList)
  static constexpr std::uint32_t REFS_MASK = 0x7FFFFFFF;
  static constexpr std::uint32_t SHOULD_BE_ON_FREELIST = 0x80000000;

  struct Block;

  // Slot: backPtr + storage for T
  struct Slot {
    Block* backPtr;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
    alignas(T) char value[sizeof(T)];
  };

  // Block: like moodycamel Block with FreeListNode fields
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
  struct Block {
    // FreeList fields (from moodycamel::FreeListNode)
    alignas(kCacheLineSize) std::atomic<std::uint32_t> freeListRefs{0};
    std::atomic<Block*> freeListNext{nullptr};
    // Block metadata
    bool dynamicallyAllocated{true};
    std::atomic<size_t> slotsInUse{0};
    std::atomic<size_t> freeSlotHead{BLOCK_SIZE};  // BLOCK_SIZE = no free slot
    // Storage for BLOCK_SIZE slots (initialized by InitBlockFreeList)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
    alignas(Slot) char storage[sizeof(Slot) * BLOCK_SIZE];
  };

  static constexpr size_t kBlockAlignment = kCacheLineSize > alignof(Block) ? kCacheLineSize
                                                                            : alignof(Block);

  static Slot* SlotAt(Block* block, size_t index) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return reinterpret_cast<Slot*>(block->storage) + index;
  }

  // Initialize block's internal free list
  static void InitBlockFreeList(Block* block) noexcept {
    block->freeSlotHead.store(0, std::memory_order_relaxed);
    block->slotsInUse.store(0, std::memory_order_relaxed);
    for (size_t i = 0; i < BLOCK_SIZE - 1; ++i) {
      *reinterpret_cast<size_t*>(SlotAt(block, i)) = i + 1;
    }
    *reinterpret_cast<size_t*>(SlotAt(block, BLOCK_SIZE - 1)) = BLOCK_SIZE;
  }

  // === Lock-free FreeList operations (from moodycamel::FreeList) ===

  void AddBlockToFreeList(Block* block) noexcept {
    if (!block->dynamicallyAllocated || recycle_dynamic_) {
      // Set SHOULD_BE_ON_FREELIST flag; if refcount was 0, we can add immediately
      if (block->freeListRefs.fetch_add(SHOULD_BE_ON_FREELIST, std::memory_order_acq_rel) == 0) {
        AddBlockKnowingRefcountIsZero(block);
      }
    } else {
      mi_free(block);
    }
  }

  void AddBlockKnowingRefcountIsZero(Block* block) noexcept {
    auto head = free_list_head_.load(std::memory_order_relaxed);
    while (true) {
      block->freeListNext.store(head, std::memory_order_relaxed);
      block->freeListRefs.store(1, std::memory_order_release);
      if (free_list_head_.compare_exchange_strong(head, block, std::memory_order_release,
                                                  std::memory_order_relaxed)) {
        return;
      }
      // CAS failed, try again if refcount goes back to zero
      if (block->freeListRefs.fetch_add(SHOULD_BE_ON_FREELIST - 1, std::memory_order_release)
          == 1) {
        continue;
      }
      return;
    }
  }

  Block* TryGetBlockFromFreeList() noexcept {
    auto head = free_list_head_.load(std::memory_order_acquire);
    while (head != nullptr) {
      auto prevHead = head;
      auto refs = head->freeListRefs.load(std::memory_order_relaxed);
      if ((refs & REFS_MASK) == 0
          || !head->freeListRefs.compare_exchange_strong(refs, refs + 1, std::memory_order_acquire,
                                                         std::memory_order_relaxed)) {
        head = free_list_head_.load(std::memory_order_acquire);
        continue;
      }
      // Got reference, try to remove from list
      auto next = head->freeListNext.load(std::memory_order_relaxed);
      if (free_list_head_.compare_exchange_strong(head, next, std::memory_order_acquire,
                                                  std::memory_order_relaxed)) {
        // Success: decrease refcount by 2 (our ref + list's ref)
        head->freeListRefs.fetch_sub(2, std::memory_order_release);
        return head;
      }
      // CAS failed, release our reference
      refs = prevHead->freeListRefs.fetch_sub(1, std::memory_order_acq_rel);
      if (refs == SHOULD_BE_ON_FREELIST + 1) {
        AddBlockKnowingRefcountIsZero(prevHead);
      }
    }
    return nullptr;
  }

  // === Block acquisition ===

  Block* TryGetBlockFromInitialPool() noexcept {
    size_t index = initial_pool_index_.fetch_add(1, std::memory_order_relaxed);
    if (index >= initial_pool_size_) {
      return nullptr;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return initial_pool_ + index;
  }

  Block* CreateBlock() {
    void* p = mi_malloc_aligned(sizeof(Block), kBlockAlignment);
    if (p == nullptr) {
      return nullptr;
    }
    Block* block = new (p) Block();
    block->dynamicallyAllocated = true;
    InitBlockFreeList(block);
    return block;
  }

  void PopulateInitialPool(size_t block_count) {
    if (block_count == 0) {
      return;
    }
    void* region = mi_malloc_aligned(sizeof(Block) * block_count, kBlockAlignment);
    if (region == nullptr) {
      return;
    }
    initial_pool_ = static_cast<Block*>(region);
    initial_pool_size_ = block_count;
    for (size_t i = 0; i < block_count; ++i) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      Block* block = new (initial_pool_ + i) Block();
      block->dynamicallyAllocated = false;
      InitBlockFreeList(block);
    }
  }

public:
  explicit ConcurrentObjectPool(size_t initial_capacity = kDefaultInitialCapacity,
                                bool recycle_dynamic = true)
    : recycle_dynamic_(recycle_dynamic) {
    size_t block_count = (initial_capacity + BLOCK_SIZE - 1) / BLOCK_SIZE;
    PopulateInitialPool(block_count);
  }

  ~ConcurrentObjectPool() {
    // Free dynamically allocated blocks from free list
    Block* block = free_list_head_.load(std::memory_order_relaxed);
    while (block != nullptr) {
      Block* next = block->freeListNext.load(std::memory_order_relaxed);
      if (block->dynamicallyAllocated) {
        mi_free(block);
      }
      block = next;
    }
    // Free initial pool
    if (initial_pool_ != nullptr) {
      mi_free(initial_pool_);
    }
  }

  ConcurrentObjectPool(const ConcurrentObjectPool&) = delete;
  ConcurrentObjectPool& operator=(const ConcurrentObjectPool&) = delete;
  ConcurrentObjectPool(ConcurrentObjectPool&&) = delete;
  ConcurrentObjectPool& operator=(ConcurrentObjectPool&&) = delete;

  template <typename... Args>
  T* Acquire(Args&&... args) {
    // Try to get a block with free slots
    Block* block = nullptr;
    size_t slotIndex = BLOCK_SIZE;

    // First try initial pool, then free list, then create new
    block = TryGetBlockFromInitialPool();
    if (block == nullptr) {
      block = TryGetBlockFromFreeList();
    }
    if (block == nullptr) {
      block = CreateBlock();
    }
    if (block == nullptr) {
      return nullptr;
    }

    // Try to acquire a slot from this block
    for (;;) {
      slotIndex = block->freeSlotHead.load(std::memory_order_acquire);
      if (slotIndex == BLOCK_SIZE) {
        // Block is full, try another
        block = TryGetBlockFromInitialPool();
        if (block == nullptr) {
          block = TryGetBlockFromFreeList();
        }
        if (block == nullptr) {
          block = CreateBlock();
        }
        if (block == nullptr) {
          return nullptr;
        }
        continue;
      }
      size_t nextFree = *reinterpret_cast<size_t*>(SlotAt(block, slotIndex));
      if (block->freeSlotHead.compare_exchange_strong(
            slotIndex, nextFree, std::memory_order_acq_rel, std::memory_order_relaxed)) {
        break;
      }
    }

    block->slotsInUse.fetch_add(1, std::memory_order_relaxed);
    Slot* slot = SlotAt(block, slotIndex);
    slot->backPtr = block;
    return new (static_cast<void*>(slot->value)) T(std::forward<Args>(args)...);
  }

  void Release(T* ptr) {
    if (ptr == nullptr) {
      return;
    }
    // Get slot and block from ptr
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    Slot* slot = reinterpret_cast<Slot*>(reinterpret_cast<char*>(ptr) - offsetof(Slot, value));
    Block* block = slot->backPtr;

    // Call destructor
    ptr->~T();

    // Return slot to block's free list (lock-free)
    size_t slotIndex = static_cast<size_t>(slot - SlotAt(block, 0));
    size_t oldHead = block->freeSlotHead.load(std::memory_order_relaxed);
    for (;;) {
      *reinterpret_cast<size_t*>(slot) = oldHead;
      if (block->freeSlotHead.compare_exchange_weak(oldHead, slotIndex, std::memory_order_acq_rel,
                                                    std::memory_order_relaxed)) {
        break;
      }
    }

    // Decrement slotsInUse; if block becomes empty, recycle it
    size_t prev = block->slotsInUse.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1) {
      // Block is now empty, reinitialize and add to free list
      InitBlockFreeList(block);
      AddBlockToFreeList(block);
    }
  }

  bool IsFromInitialPool(const T* ptr) const noexcept {
    if (initial_pool_ == nullptr) {
      return false;
    }
    const char* p = reinterpret_cast<const char*>(ptr);
    const char* start = reinterpret_cast<const char*>(initial_pool_);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const char* end = start + sizeof(Block) * initial_pool_size_;
    return p >= start && p < end;
  }

private:
  bool recycle_dynamic_;

  // Initial pool
  Block* initial_pool_{nullptr};
  size_t initial_pool_size_{0};
  alignas(kCacheLineSize) std::atomic<size_t> initial_pool_index_{0};

  // Lock-free free list of empty blocks (like moodycamel::FreeList<Block>)
  alignas(kCacheLineSize) std::atomic<Block*> free_list_head_{nullptr};
};

}  // namespace exchange::core::collections::objpool
