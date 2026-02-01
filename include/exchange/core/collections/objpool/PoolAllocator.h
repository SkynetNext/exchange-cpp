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

#include <cstddef>
#include <cstdlib>

// Detect sanitizers - when any sanitizer is enabled, use standard allocator
// to allow proper memory tracking and error detection.
//
// Supported sanitizers:
// - AddressSanitizer (ASAN): memory errors, buffer overflows, use-after-free
// - MemorySanitizer (MSAN): uninitialized memory reads
// - ThreadSanitizer (TSAN): data races
// - UndefinedBehaviorSanitizer (UBSAN): undefined behavior
// - LeakSanitizer (LSAN): memory leaks

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
// GCC style sanitizer detection
#  define EXCHANGE_SANITIZER_ENABLED 1
#elif defined(__has_feature)
// Clang style sanitizer detection
#  if __has_feature(address_sanitizer) || __has_feature(memory_sanitizer) \
    || __has_feature(thread_sanitizer) || __has_feature(undefined_behavior_sanitizer)
#    define EXCHANGE_SANITIZER_ENABLED 1
#  endif
#endif

// Allow explicit override via build system
// -DEXCHANGE_FORCE_STD_ALLOCATOR=1 to force standard allocator
// -DEXCHANGE_FORCE_MIMALLOC=1 to force mimalloc (use with caution under sanitizers)
#if defined(EXCHANGE_FORCE_STD_ALLOCATOR)
#  undef EXCHANGE_SANITIZER_ENABLED
#  define EXCHANGE_SANITIZER_ENABLED 1
#elif defined(EXCHANGE_FORCE_MIMALLOC)
#  undef EXCHANGE_SANITIZER_ENABLED
#endif

// Select allocator based on sanitizer detection
#if defined(EXCHANGE_SANITIZER_ENABLED)

// Standard allocator for sanitizer compatibility
#  ifdef _WIN32
#    include <malloc.h>
#    define EXCHANGE_MALLOC_ALIGNED(size, alignment) _aligned_malloc((size), (alignment))
#    define EXCHANGE_FREE_ALIGNED(ptr)               _aligned_free(ptr)
#  else
#    define EXCHANGE_MALLOC_ALIGNED(size, alignment) std::aligned_alloc((alignment), (size))
#    define EXCHANGE_FREE_ALIGNED(ptr)               std::free(ptr)
#  endif

#else

// Mimalloc for production performance
#  include <mimalloc.h>
#  define EXCHANGE_MALLOC_ALIGNED(size, alignment) mi_malloc_aligned((size), (alignment))
#  define EXCHANGE_FREE_ALIGNED(ptr)               mi_free(ptr)

#endif

namespace exchange::core::collections::objpool {

/**
 * PoolAllocator - Unified memory allocation for object pools
 *
 * Automatically selects between mimalloc (production) and standard allocator
 * (sanitizer builds) based on compile-time detection.
 *
 * Usage:
 *   void* p = PoolAllocator::AllocAligned(size, alignment);
 *   PoolAllocator::FreeAligned(p);
 */
struct PoolAllocator {
  static void* AllocAligned(size_t size, size_t alignment) noexcept {
    return EXCHANGE_MALLOC_ALIGNED(size, alignment);
  }

  static void FreeAligned(void* ptr) noexcept {
    EXCHANGE_FREE_ALIGNED(ptr);
  }
};

}  // namespace exchange::core::collections::objpool
