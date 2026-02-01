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
#include <cassert>
#include <cstddef>
#include <utility>

namespace exchange::core::collections::objpool {

/**
 * ObjectPool<T> - Single-threaded typed object pool using mimalloc
 *
 * Design inspired by moodycamel::ConcurrentQueue's internal block pool:
 * - BLOCK_SIZE objects per block (default 32, like concurrentqueue)
 * - Block contains: elements storage, freeListNext, dynamicallyAllocated, slotsInUse
 * - Block is recycled to freeList only when completely empty (slotsInUse == 0)
 * - Slot free list within block for O(1) acquire/release
 *
 * Key properties:
 * - Single-threaded only (no atomics, no locks)
 * - Uses mimalloc for all allocations
 * - Type-safe: Acquire returns T*, Release takes T*
 */
template <typename T>
class ObjectPool {
public:
  static constexpr size_t BLOCK_SIZE = 32;
  static constexpr size_t kDefaultInitialCapacity = 32 * BLOCK_SIZE;  // 1024

private:
  struct Block;

  // Slot: backPtr + storage for T
  // When free: storage reused as next free slot index
  struct Slot {
    Block* backPtr;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
    alignas(T) char value[sizeof(T)];
  };

  // Block: like moodycamel Block - holds BLOCK_SIZE elements
  struct Block {
    // Free list linkage (like moodycamel freeListNext)
    Block* freeListNext;
    // From initial pool or dynamically allocated (like moodycamel dynamicallyAllocated)
    bool dynamicallyAllocated;
    // Counter: block is empty when == 0 (like moodycamel elementsCompletelyDequeued)
    size_t slotsInUse;
    // Head of internal free slot list (BLOCK_SIZE means no free slot)
    size_t freeSlotHead;
    // Storage for BLOCK_SIZE slots
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
    alignas(Slot) char storage[sizeof(Slot) * BLOCK_SIZE];
  };

  static Slot* SlotAt(Block* block, size_t index) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return reinterpret_cast<Slot*>(block->storage) + index;
  }

  // Initialize block's internal free list: 0 -> 1 -> 2 -> ... -> BLOCK_SIZE-1 -> BLOCK_SIZE(end)
  static void InitBlockFreeList(Block* block) noexcept {
    block->freeSlotHead = 0;
    block->slotsInUse = 0;
    for (size_t i = 0; i < BLOCK_SIZE - 1; ++i) {
      *reinterpret_cast<size_t*>(SlotAt(block, i)) = i + 1;
    }
    *reinterpret_cast<size_t*>(SlotAt(block, BLOCK_SIZE - 1)) = BLOCK_SIZE;
  }

  Block* TryGetBlockFromInitialPool() noexcept {
    if (initial_pool_index_ >= initial_pool_size_) {
      return nullptr;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return initial_pool_ + initial_pool_index_++;
  }

  Block* TryGetBlockFromFreeList() noexcept {
    if (free_list_head_ == nullptr) {
      return nullptr;
    }
    Block* block = free_list_head_;
    free_list_head_ = block->freeListNext;
    return block;
  }

  Block* CreateBlock() {
    void* p = mi_malloc_aligned(sizeof(Block), alignof(Block));
    if (p == nullptr) {
      return nullptr;
    }
    Block* block = static_cast<Block*>(p);
    block->freeListNext = nullptr;
    block->dynamicallyAllocated = true;
    InitBlockFreeList(block);
    return block;
  }

  void AddBlockToFreeList(Block* block) noexcept {
    block->freeListNext = free_list_head_;
    free_list_head_ = block;
  }

  void PopulateInitialPool(size_t block_count) {
    if (block_count == 0) {
      return;
    }
    void* region = mi_malloc_aligned(sizeof(Block) * block_count, alignof(Block));
    if (region == nullptr) {
      return;
    }
    initial_pool_ = static_cast<Block*>(region);
    initial_pool_size_ = block_count;
    for (size_t i = 0; i < block_count; ++i) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      Block* block = initial_pool_ + i;
      block->freeListNext = nullptr;
      block->dynamicallyAllocated = false;
      InitBlockFreeList(block);
    }
  }

public:
  explicit ObjectPool(size_t initial_capacity = kDefaultInitialCapacity,
                      bool recycle_dynamic = true)
    : recycle_dynamic_(recycle_dynamic) {
    size_t block_count = (initial_capacity + BLOCK_SIZE - 1) / BLOCK_SIZE;
    PopulateInitialPool(block_count);
  }

  ~ObjectPool() {
    // Free dynamically allocated blocks from free list
    while (free_list_head_ != nullptr) {
      Block* block = free_list_head_;
      free_list_head_ = block->freeListNext;
      if (block->dynamicallyAllocated) {
        mi_free(block);
      }
    }
    // Free current block if dynamically allocated
    if (current_block_ != nullptr && current_block_->dynamicallyAllocated) {
      mi_free(current_block_);
    }
    // Free initial pool
    if (initial_pool_ != nullptr) {
      mi_free(initial_pool_);
    }
  }

  ObjectPool(const ObjectPool&) = delete;
  ObjectPool& operator=(const ObjectPool&) = delete;
  ObjectPool(ObjectPool&&) = delete;
  ObjectPool& operator=(ObjectPool&&) = delete;

  template <typename... Args>
  T* Acquire(Args&&... args) {
    // Fast path: current block has free slot
    if (current_block_ != nullptr && current_block_->freeSlotHead != BLOCK_SIZE) {
      return AcquireFromBlock(current_block_, std::forward<Args>(args)...);
    }
    // Need new block: initial pool -> free list -> create
    Block* block = TryGetBlockFromInitialPool();
    if (block == nullptr) {
      block = TryGetBlockFromFreeList();
    }
    if (block == nullptr) {
      block = CreateBlock();
    }
    if (block == nullptr) {
      return nullptr;
    }
    current_block_ = block;
    return AcquireFromBlock(block, std::forward<Args>(args)...);
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

    // Return slot to block's free list
    size_t slotIndex = static_cast<size_t>(slot - SlotAt(block, 0));
    *reinterpret_cast<size_t*>(slot) = block->freeSlotHead;
    block->freeSlotHead = slotIndex;
    block->slotsInUse--;

    // If block is now empty, recycle it (like moodycamel: block empty -> add to freeList)
    if (block->slotsInUse == 0) {
      InitBlockFreeList(block);
      if (block == current_block_) {
        current_block_ = nullptr;
      }
      if (!block->dynamicallyAllocated || recycle_dynamic_) {
        AddBlockToFreeList(block);
      } else {
        mi_free(block);
      }
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
  template <typename... Args>
  T* AcquireFromBlock(Block* block, Args&&... args) {
    size_t slotIndex = block->freeSlotHead;
    Slot* slot = SlotAt(block, slotIndex);
    block->freeSlotHead = *reinterpret_cast<size_t*>(slot);
    block->slotsInUse++;
    slot->backPtr = block;
    return new (static_cast<void*>(slot->value)) T(std::forward<Args>(args)...);
  }

  bool recycle_dynamic_;

  // Initial pool (like moodycamel initialBlockPool)
  Block* initial_pool_{nullptr};
  size_t initial_pool_size_{0};
  size_t initial_pool_index_{0};

  // Free list of empty blocks (like moodycamel freeList)
  Block* free_list_head_{nullptr};

  // Current working block (optimization: avoid searching for block with free slots)
  Block* current_block_{nullptr};
};

}  // namespace exchange::core::collections::objpool
