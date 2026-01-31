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
#include <array>
#include <cassert>
#include <cstddef>
#include <utility>

namespace exchange::core::collections::objpool {

/**
 * ObjectPool<T> - Single-threaded typed object pool using mimalloc
 *
 * Design inspired by moodycamel::ConcurrentQueue's internal block pool:
 * - Block struct with freeListNext, dynamicallyAllocated (like moodycamel Block)
 * - Initial pool: create_array<Block>(blockCount) style - array of Block structs
 * - Free list: intrusive linked list of Block* (Block.freeListNext)
 * - Fallback: create<Block>() - single block allocation (like requisition_block)
 *
 * Key properties:
 * - Single-threaded only (no atomics, no locks)
 * - Uses mimalloc for all allocations (mi_malloc_aligned / mi_free)
 * - Type-safe: Acquire returns T*, Release takes T*
 * - Automatic construction/destruction: Acquire calls constructor, Release calls destructor
 * - Alignment automatically inherited from T (like moodycamel MOODYCAMEL_ALIGNED_TYPE_LIKE)
 * - Optional recycling of dynamically allocated blocks (RECYCLE_ALLOCATED_BLOCKS)
 *
 * @code
 *   ObjectPool<MyStruct> pool(1024);  // 1K pre-allocated
 *   MyStruct* obj = pool.Acquire(arg1, arg2);  // constructor called
 *   // ... use obj ...
 *   pool.Release(obj);  // destructor called, returned to pool
 * @endcode
 */
template <typename T>
class ObjectPool {
private:
  // Block struct - mirrors moodycamel Block (freeListNext, dynamicallyAllocated)
  // Storage alignment automatically inherited from T (like MOODYCAMEL_ALIGNED_TYPE_LIKE)
  struct Block {
    Block* freeListNext;
    bool dynamicallyAllocated;
    // Key: storage alignment automatically inherits from T
    // Works for both aligned types (e.g., alignas(64) struct) and normal types
    alignas(T) std::array<char, sizeof(T)> storage;
  };

  // Block alignment: max of Block's natural alignment and T's alignment
  static constexpr size_t kBlockAlignment = alignof(Block) > alignof(T) ? alignof(Block)
                                                                        : alignof(T);

public:
  /**
   * Construct object pool
   *
   * @param initial_capacity Number of objects to pre-allocate
   * @param recycle_dynamic If true, dynamically allocated blocks go to free list on release;
   *                        if false, they are mi_free'd (like RECYCLE_ALLOCATED_BLOCKS=false)
   */
  explicit ObjectPool(size_t initial_capacity, bool recycle_dynamic = true)
    : initial_capacity_(initial_capacity), recycle_dynamic_(recycle_dynamic) {
    PopulateInitialBlockList(initial_capacity_);
  }

  /**
   * Destructor
   *
   * IMPORTANT: All acquired objects MUST be released before destroying the pool.
   * Destroying the pool while objects are still acquired results in undefined behavior.
   * This is the same contract as moodycamel::ConcurrentQueue.
   */
  ~ObjectPool() {
    // Destroy free list: mi_free dynamically allocated blocks
    // Note: Destructors already called by Release()
    while (free_list_head_ != nullptr) {
      Block* block = free_list_head_;
      free_list_head_ = block->freeListNext;
      if (block->dynamicallyAllocated) {
        mi_free(block);
      }
    }
    // Destroy initial pool
    if (initial_block_pool_ != nullptr) {
      mi_free(initial_block_pool_);
      initial_block_pool_ = nullptr;
      initial_block_pool_size_ = 0;
    }
  }

  ObjectPool(const ObjectPool&) = delete;
  ObjectPool& operator=(const ObjectPool&) = delete;
  ObjectPool(ObjectPool&& other) noexcept = delete;
  ObjectPool& operator=(ObjectPool&& other) noexcept = delete;

  /**
   * Acquire an object from the pool, calling constructor with forwarded args
   * Order: initial pool -> free list -> create<Block> (moodycamel requisition_block)
   *
   * @param args Arguments forwarded to T's constructor
   * @return Constructed object, or nullptr on allocation failure
   */
  template <typename... Args>
  T* Acquire(Args&&... args) {
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
    // Placement new: construct T in block's storage
    return new (static_cast<void*>(block->storage.data())) T(std::forward<Args>(args)...);
  }

  /**
   * Release an object back to the pool, calling destructor
   * - If from initial pool or recycle_dynamic: adds to free list
   * - Otherwise: mi_free
   *
   * @param ptr Object previously returned by Acquire()
   */
  void Release(T* ptr) {
    if (ptr == nullptr) {
      return;
    }
    // Call destructor
    ptr->~T();
    // Get Block from object pointer
    Block* block = ToBlock(ptr);
    if (!block->dynamicallyAllocated || recycle_dynamic_) {
      AddBlockToFreeList(block);
    } else {
      mi_free(block);
    }
  }

  /**
   * Check if ptr was allocated from the initial pool (for debug/stats)
   */
  bool IsFromInitialPool(const T* ptr) const noexcept {
    if (initial_block_pool_ == nullptr || initial_block_pool_size_ == 0) {
      return false;
    }
    const char* p = reinterpret_cast<const char*>(ptr);
    const char* start = reinterpret_cast<const char*>(initial_block_pool_);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const char* end = start + sizeof(Block) * initial_block_pool_size_;
    return p >= start && p < end;
  }

  size_t initial_capacity() const noexcept {
    return initial_capacity_;
  }

  /**
   * Block alignment (automatically derived from T)
   */
  static constexpr size_t block_alignment() noexcept {
    return kBlockAlignment;
  }

private:
  static Block* ToBlock(T* obj) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return reinterpret_cast<Block*>(reinterpret_cast<char*>(obj) - offsetof(Block, storage));
  }

  void PopulateInitialBlockList(size_t block_count) {
    if (block_count == 0) {
      return;
    }
    initial_block_pool_size_ = block_count;
    // create_array<Block> style: allocate Block array (like moodycamel)
    // Alignment automatically derived from Block (which inherits T's alignment)
    void* region = mi_malloc_aligned(sizeof(Block) * block_count, kBlockAlignment);
    if (region != nullptr) {
      initial_block_pool_ = static_cast<Block*>(region);
      for (size_t i = 0; i < initial_block_pool_size_; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        Block* block = initial_block_pool_ + i;
        block->freeListNext = nullptr;
        block->dynamicallyAllocated = false;  // from initial pool (moodycamel)
      }
    } else {
      initial_block_pool_size_ = 0;
    }
  }

  void AddBlockToFreeList(Block* block) noexcept {
    block->freeListNext = free_list_head_;
    free_list_head_ = block;
  }

  Block* TryGetBlockFromFreeList() noexcept {
    if (free_list_head_ == nullptr) {
      return nullptr;
    }
    Block* block = free_list_head_;
    free_list_head_ = block->freeListNext;
    return block;
  }

  Block* TryGetBlockFromInitialPool() noexcept {
    if (initial_block_pool_ == nullptr || initial_block_pool_index_ >= initial_block_pool_size_) {
      return nullptr;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    Block* block = initial_block_pool_ + initial_block_pool_index_;
    ++initial_block_pool_index_;
    return block;
  }

  Block* CreateBlock() {
    void* p = mi_malloc_aligned(sizeof(Block), kBlockAlignment);
    if (p == nullptr) {
      return nullptr;
    }
    Block* block = static_cast<Block*>(p);
    block->freeListNext = nullptr;
    block->dynamicallyAllocated = true;  // from create<Block> (moodycamel)
    return block;
  }

  size_t initial_capacity_;
  bool recycle_dynamic_;

  // Initial pool: array of Blocks (create_array style, like moodycamel)
  Block* initial_block_pool_{nullptr};
  size_t initial_block_pool_size_{0};
  size_t initial_block_pool_index_{0};

  // Free list: intrusive linked list of Block* (Block.freeListNext)
  Block* free_list_head_{nullptr};
};

}  // namespace exchange::core::collections::objpool
