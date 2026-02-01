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
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace exchange::core::collections::objpool {

/**
 * ConcurrentObjectPool<T> - Thread-safe typed object pool using mimalloc
 *
 * Design directly adapted from moodycamel::ConcurrentQueue's internal block pool:
 * - Lock-free FreeList with reference counting (FreeListNode pattern)
 * - Atomic initial pool index for thread-safe acquisition from pre-allocated region
 * - Single block fallback allocation (like requisition_block)
 *
 * Key properties:
 * - Thread-safe (lock-free CAS operations)
 * - Uses mimalloc for all allocations (mi_malloc_aligned / mi_free)
 * - Type-safe: Acquire returns T*, Release takes T*
 * - Automatic construction/destruction: Acquire calls constructor, Release calls destructor
 * - Alignment automatically inherited from T (like moodycamel MOODYCAMEL_ALIGNED_TYPE_LIKE)
 * - False sharing prevention: Block aligned to 128 bytes (L2 prefetch 2 cache lines)
 *
 * @code
 *   ConcurrentObjectPool<MyStruct> pool(1024);  // 1K pre-allocated
 *   MyStruct* obj = pool.Acquire(arg1, arg2);   // constructor called (thread-safe)
 *   // ... use obj ...
 *   pool.Release(obj);  // destructor called, returned to pool (thread-safe)
 * @endcode
 */
template <typename T>
class ConcurrentObjectPool {
private:
  // Cache line size for false sharing prevention
  // Use 128 bytes to account for L2 prefetch (2 cache lines)
  static constexpr size_t kCacheLineSize = 128;

  // Reference counting constants (from moodycamel)
  static constexpr std::uint32_t REFS_MASK = 0x7FFFFFFF;
  static constexpr std::uint32_t SHOULD_BE_ON_FREELIST = 0x80000000;

  // Block struct - mirrors moodycamel Block with FreeListNode fields
  // freeListRefs: lower 31 bits = refcount, bit 31 = should-be-on-freelist flag
  //
  // False sharing prevention: freeListRefs/freeListNext are hot atomics accessed
  // concurrently by multiple threads during free list operations. Align to cache
  // line to prevent false sharing with atomics in adjacent Blocks.
  struct Block {
    // Hot atomics - aligned to prevent false sharing between Blocks
    alignas(kCacheLineSize) std::atomic<std::uint32_t> freeListRefs{0};
    std::atomic<Block*> freeListNext{nullptr};
    bool dynamicallyAllocated{true};
    // Storage alignment automatically inherits from T
    alignas(T) std::array<char, sizeof(T)> storage{};
  };

  // Block alignment: max of cache line size (for false sharing prevention between Blocks)
  // and T's alignment requirement
  static constexpr size_t kBlockAlignment = kCacheLineSize > alignof(T) ? kCacheLineSize
                                                                        : alignof(T);

public:
  /**
   * Construct concurrent object pool
   *
   * @param initial_capacity Number of objects to pre-allocate
   * @param recycle_dynamic If true, dynamically allocated blocks go to free list on release
   */
  explicit ConcurrentObjectPool(size_t initial_capacity, bool recycle_dynamic = true)
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
  ~ConcurrentObjectPool() {
    // Destroy free list: traverse and mi_free dynamically allocated blocks
    Block* block = free_list_head_.load(std::memory_order_relaxed);
    while (block != nullptr) {
      Block* next = block->freeListNext.load(std::memory_order_relaxed);
      if (block->dynamicallyAllocated) {
        mi_free(block);
      }
      block = next;
    }
    // Destroy initial pool
    if (initial_block_pool_ != nullptr) {
      mi_free(initial_block_pool_);
    }
  }

  ConcurrentObjectPool(const ConcurrentObjectPool&) = delete;
  ConcurrentObjectPool& operator=(const ConcurrentObjectPool&) = delete;
  ConcurrentObjectPool(ConcurrentObjectPool&&) = delete;
  ConcurrentObjectPool& operator=(ConcurrentObjectPool&&) = delete;

  /**
   * Acquire an object from the pool, calling constructor with forwarded args
   * Thread-safe: uses atomic operations
   * Order: initial pool (atomic index) -> free list (lock-free) -> create<Block>
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
   * Thread-safe: uses lock-free free list
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
   * Block alignment (automatically derived, includes cache line padding)
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
    // Alignment automatically derived from Block (includes cache line alignment)
    void* region = mi_malloc_aligned(sizeof(Block) * block_count, kBlockAlignment);
    if (region != nullptr) {
      initial_block_pool_ = static_cast<Block*>(region);
      for (size_t i = 0; i < initial_block_pool_size_; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        Block* block = initial_block_pool_ + i;
        // Placement new to initialize atomics
        new (block) Block();
        block->dynamicallyAllocated = false;  // from initial pool
      }
    } else {
      initial_block_pool_size_ = 0;
    }
  }

  /**
   * Lock-free add to free list (adapted from moodycamel FreeList::add)
   * Uses reference counting to handle concurrent access
   */
  void AddBlockToFreeList(Block* block) noexcept {
    // We know that the should-be-on-freelist bit is 0 at this point, so it's safe to
    // set it using a fetch_add
    if (block->freeListRefs.fetch_add(SHOULD_BE_ON_FREELIST, std::memory_order_acq_rel) == 0) {
      // Oh look! We were the last ones referencing this node, and we know
      // we want to add it to the free list, so let's do it!
      AddBlockKnowingRefcountIsZero(block);
    }
  }

  /**
   * Actually add block to free list when refcount is zero
   * (adapted from moodycamel FreeList::add_knowing_refcount_is_zero)
   */
  void AddBlockKnowingRefcountIsZero(Block* block) noexcept {
    // Since the refcount is zero, and nobody can increase it once it's zero (except us, and we run
    // only one copy of this method per node at a time, i.e. the single thread case), then we know
    // we can safely change the next pointer of the node; however, once the refcount is back above
    // zero, then other threads could increase it (happens under heavy contention, when the refcount
    // goes to zero in between a load and a refcount increment of a node in try_get, then back up to
    // something non-zero, then the refcount increment is done by the other thread) -- so, if the
    // CAS to add the node to the actual list fails, decrease the refcount and leave the add
    // operation to the next thread who puts the refcount back at zero (which could be us, hence the
    // loop).
    auto head = free_list_head_.load(std::memory_order_relaxed);
    while (true) {
      block->freeListNext.store(head, std::memory_order_relaxed);
      block->freeListRefs.store(1, std::memory_order_release);
      if (!free_list_head_.compare_exchange_strong(head, block, std::memory_order_release,
                                                   std::memory_order_relaxed)) {
        // Hmm, the add failed, but we can only try again when the refcount goes back to zero
        if (block->freeListRefs.fetch_add(SHOULD_BE_ON_FREELIST - 1, std::memory_order_release)
            == 1) {
          continue;
        }
      }
      return;
    }
  }

  /**
   * Lock-free get from free list (adapted from moodycamel FreeList::try_get)
   * Uses reference counting to safely remove from head
   */
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

      // Good, reference count has been incremented (it wasn't at zero), which means we can read the
      // next and not worry about it changing between now and the time we do the CAS
      auto next = head->freeListNext.load(std::memory_order_relaxed);
      if (free_list_head_.compare_exchange_strong(head, next, std::memory_order_acquire,
                                                  std::memory_order_relaxed)) {
        // Yay, got the node. This means it was on the list, which means shouldBeOnFreeList must be
        // false no matter the refcount (because nobody else knows it's been taken off yet, it can't
        // have been put back on).
        assert((head->freeListRefs.load(std::memory_order_relaxed) & SHOULD_BE_ON_FREELIST) == 0);

        // Decrease refcount twice, once for our ref, and once for the list's ref
        head->freeListRefs.fetch_sub(2, std::memory_order_release);
        return head;
      }

      // OK, the head must have changed on us, but we still need to decrease the refcount we
      // increased. Note that we don't need to release any memory effects, but we do need to ensure
      // that the reference count decrement happens-after the CAS on the head.
      refs = prevHead->freeListRefs.fetch_sub(1, std::memory_order_acq_rel);
      if (refs == SHOULD_BE_ON_FREELIST + 1) {
        AddBlockKnowingRefcountIsZero(prevHead);
      }
    }

    return nullptr;
  }

  /**
   * Thread-safe get from initial pool (atomic index increment)
   * Fast path check before fetch_add (like moodycamel)
   */
  Block* TryGetBlockFromInitialPool() noexcept {
    if (initial_block_pool_ == nullptr) {
      return nullptr;
    }
    // Fast path: check if pool is exhausted (like moodycamel)
    // This avoids unnecessary fetch_add when pool is empty
    if (initial_block_pool_index_.load(std::memory_order_relaxed) >= initial_block_pool_size_) {
      return nullptr;
    }
    // Atomic fetch_add ensures each thread gets unique index
    size_t index = initial_block_pool_index_.fetch_add(1, std::memory_order_relaxed);
    if (index >= initial_block_pool_size_) {
      return nullptr;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return initial_block_pool_ + index;
  }

  Block* CreateBlock() {
    void* p = mi_malloc_aligned(sizeof(Block), kBlockAlignment);
    if (p == nullptr) {
      return nullptr;
    }
    // Placement new to initialize atomics
    Block* block = new (p) Block();
    block->dynamicallyAllocated = true;
    return block;
  }

  size_t initial_capacity_;
  bool recycle_dynamic_;

  // Initial pool: array of Blocks
  Block* initial_block_pool_{nullptr};
  size_t initial_block_pool_size_{0};

  // Hot atomics - aligned to prevent false sharing between each other
  // These are accessed concurrently by multiple threads
  alignas(kCacheLineSize) std::atomic<size_t> initial_block_pool_index_{0};
  alignas(kCacheLineSize) std::atomic<Block*> free_list_head_{nullptr};
};

}  // namespace exchange::core::collections::objpool
