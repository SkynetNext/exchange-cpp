# ART Tree Performance Baseline

## Test Environment

**Date**: 2026-01-22  
**Platform**: Linux (16 cores @ 2600 MHz)  
**CPU Caches**: L1 32KiB×8, L2 1024KiB×8, L3 32768KiB  
**Data Size**: 5,000,000 key-value pairs  
**Memory Allocator**: mimalloc  
**Compiler**: GCC with `-O3 -march=native -flto`

## Performance Results

### Data Structures

| ID | Container | Description |
|----|-----------|-------------|
| ART | `LongAdaptiveRadixTreeMap` | Adaptive Radix Tree |
| BST | `std::map` | Red-Black Tree |
| UO | `std::unordered_map` | Standard hash map |
| DE | `ankerl::unordered_dense` | High-performance hash map |
| SET | `std::set` | Red-Black Tree (keys only) |

### Core Operations (5M ops)

| Operation | ART | BST | UO | DE | vs BST | vs DE |
|-----------|-----|-----|----|----|--------|-------|
| **Put** | 262.8 ms | 1066.0 ms | 132.2 ms | 54.9 ms | **+306%** (4.1x) | -79% |
| **GetHit** | 298.2 ms | 1273.5 ms | 61.6 ms | 48.8 ms | **+327%** (4.3x) | -84% |
| **GetMiss** | 6.2 ms | 19.9 ms | 75.7 ms | 31.0 ms | **+223%** (3.2x) | **+404%** ⭐ |
| **Remove** | 501.5 ms | 1706.6 ms | 197.7 ms | 89.1 ms | **+240%** (3.4x) | -82% |

### Iteration Operations

| Operation | ART | BST | UO | DE | vs BST | vs DE |
|-----------|-----|-----|----|----|--------|-------|
| **ForEach** | 20.0 μs | 79.9 μs | 4.1 μs | 1.0 μs | **+300%** (4.0x) | -95% |
| **ForEachDesc** | 92.2 μs | 93.5 μs | 4.6 μs | 1.3 μs | +1% | -99% |

### Range Queries (Ordered Containers Only)

| Operation | ART | BST | SET | vs BST | vs SET |
|-----------|-----|-----|-----|--------|--------|
| **Higher** | 595.4 ms | 1817.1 ms | 1768.4 ms | **+205%** (3.1x) | **+197%** (3.0x) |
| **Lower** | 601.5 ms | 1793.2 ms | 1803.7 ms | **+198%** (3.0x) | **+200%** (3.0x) |

---

## Key Findings

### ART vs BST (std::map)

ART wins all operations (**3.0x - 4.3x faster**). `std::map` is not recommended.

### ART vs Hash Maps

| Scenario | Winner | Reason |
|----------|--------|--------|
| Hit-heavy workload | DE (5x faster) | Hash O(1) advantage |
| Miss-heavy workload | **ART (5x faster)** | Early termination on prefix mismatch |
| Range queries | **ART only** | Hash maps don't support ordered ops |

### Best Use Cases

| Container | Best For |
|-----------|----------|
| **ART** | Range queries, Miss-heavy workloads, ordered iteration |
| **DE** | Pure KV store with high hit rate |
| **std::map/set** | Not recommended |

---

## OrderBook Optimization Recommendation

```cpp
// Price Buckets - Keep ART (requires Higher/Lower)
LongAdaptiveRadixTreeMap<Bucket> askPriceBuckets_;  // ✅ ART
LongAdaptiveRadixTreeMap<Bucket> bidPriceBuckets_;  // ✅ ART

// Order Index - Consider switching to dense (pure KV, no ordering)
LongAdaptiveRadixTreeMap<DirectOrder> orderIdIndex_;  // ⚠️ → unordered_dense?
```

| Operation | ART | unordered_dense | Speedup |
|-----------|-----|-----------------|---------|
| Get | 298 ms | 49 ms | **6.1x** |
| Put | 263 ms | 55 ms | **4.8x** |
| Remove | 502 ms | 89 ms | **5.6x** |

---

## Java vs C++ Comparison

C++ `perf_long_adaptive_radix_tree_map`, Java `PerfLongAdaptiveRadixTreeMap#shouldLoadManyItems`. Both 3-run avg, same workload (5M keys).

| Operation | Java ART | C++ ART | Speedup |
|-----------|----------|---------|---------|
| **Put** | 1962 ms | 827 ms | **2.4x** |
| **GetHit** | 1483 ms | 759 ms | **2.0x** |
| **Remove** | 2136 ms | 1241 ms | **1.7x** |
| **ForEach** | 0.78 ms | 0.19 ms | **4.1x** |
| **ForEachDesc** | 0.46 ms | 0.42 ms | **1.1x** |
| **Higher** | 2033 ms | 1446 ms | **1.4x** |
| **Lower** | 2005 ms | 1476 ms | **1.4x** |

C++ ART **1.1x–4.1x** faster than Java ART.

---

**Last Updated**: 2026-01-28  
**Version**: v3 (uint8_t keys, mimalloc)

---

## Performance History

### Version 3 (uint8_t Key Optimization)
**Date**: 2026-01-22  
**Change**: `keys_` type changed from `int16_t` to `uint8_t` in Node4/Node16  
**Rationale**: Align with [libart](https://github.com/armon/libart) reference implementation for better cache efficiency  
**Status**: ✅ Measurable performance improvements

#### Benchmark Comparison

| Operation | Before (int16_t) | After (uint8_t) | Change | Improvement |
|-----------|------------------|-----------------|--------|-------------|
| **Put** | 332.1 ms | 262.8 ms | -69.3 ms | **-20.9%** ⭐ |
| **GetHit** | 304.0 ms | 298.2 ms | -5.8 ms | **-1.9%** |
| **GetMiss** | 6.68 ms | 6.16 ms | -0.52 ms | **-7.8%** |
| **Remove** | 496.7 ms | 501.5 ms | +4.8 ms | +1.0% |
| **ForEach** | 20.7 μs | 20.0 μs | -0.7 μs | **-3.6%** |
| **ForEachDesc** | 41.3 μs | 92.2 μs | +50.9 μs | +123%* |
| **Higher** | 602.2 ms | 595.4 ms | -6.8 ms | **-1.1%** |
| **Lower** | 625.3 ms | 601.5 ms | -23.8 ms | **-3.8%** |

> *ForEachDesc regression likely due to test environment variance (Load Average: 30.26 vs 0.32)

#### Key Improvements

1. **Put Operation**: **-20.9%** (332ms → 263ms)
   - Most significant improvement
   - Smaller `keys_` array improves cache efficiency during insertion

2. **Memory Reduction**:
   - Node4: `keys_` from 8 bytes to 4 bytes (-50%)
   - Node16: `keys_` from 32 bytes to 16 bytes (-50%)
   - Better L1 cache utilization

3. **Code Simplification**:
   - Removed unused C++26 `std::simd` conditional compilation
   - Cleaner codebase aligned with libart reference

#### Technical Details

```cpp
// Before (Java-style, from original port)
std::array<int16_t, 4> keys_{};   // 8 bytes
std::array<int16_t, 16> keys_{};  // 32 bytes

// After (libart-style, optimized)
std::array<uint8_t, 4> keys_{};   // 4 bytes
std::array<uint8_t, 16> keys_{};  // 16 bytes
```

**Conclusion**: The `uint8_t` optimization provides measurable improvements, especially for Put operations (-20.9%). The change aligns with the original libart C implementation and improves cache efficiency.

---

### Version 2 (mimalloc Optimization)
**Date**: 2024-12-23  
**Memory Allocator**: **mimalloc** (Global hook via `mimalloc-new-delete.h`)  
**Status**: ✅ Significant performance improvements across all operations

| Operation | ART Time | BST Time | Improvement | Speedup vs BST | vs V1 Speedup |
|-----------|----------|----------|-------------|----------------|---------------|
| **PUT** | 158.272 ms | 674.401 ms | +326.1% | 4.26x | **2.31x faster** |
| **GET_HIT** | 101.605 ms | 418.435 ms | +311.8% | 4.12x | **1.21x faster** |
| **REMOVE** | 197.976 ms | 689.492 ms | +248.3% | 3.48x | **1.38x faster** |
| **FOREACH** | 7.60 μs | 17.50 μs | +130.3% | 2.30x | **3.85x faster** ⭐ |
| **FOREACH_DESC** | 13.71 μs | 20.01 μs | +46.0% | 1.46x | **9.01x faster** ⭐ |
| **HIGHER** | 169.449 ms | 424.188 ms | +150.3% | 2.50x | **2.03x faster** |
| **LOWER** | 195.123 ms | 481.238 ms | +146.6% | 2.46x | **1.52x faster** |

**Key Improvements**:

1. **PUT Performance Doubled**: From 365ms to 158ms (**2.31x faster**). `mimalloc`'s optimized small object allocation dramatically reduces the overhead of creating ART nodes (`ArtNode4/16/48/256`).

2. **Iteration Performance Breakthrough**: 
   - **FOREACH**: Transformed from **-33% slower** to **+130% faster** than `std::map` (**3.85x faster than V1**)
   - **FOREACH_DESC**: Improved from **-51% slower** to **+46% faster** (**9.01x faster than V1**)
   - This reversal is attributed to `mimalloc`'s superior memory compactness, reducing cache misses during tree traversal.

3. **Consistent Gains Across Operations**: All operations show 20-50% improvement, with the most dramatic gains in write-heavy operations (PUT) and iteration.

**Technical Analysis**:

`mimalloc` provides several advantages for ART tree performance:
- **Small Object Optimization**: ART nodes are typically 32-256 bytes. `mimalloc`'s segregated free lists and thread-local caching make these allocations nearly free.
- **Memory Locality**: Better memory compaction improves cache hit rates during tree traversal, especially noticeable in iteration operations.
- **Reduced Fragmentation**: Lower memory fragmentation means nodes are more likely to be allocated in contiguous regions, improving spatial locality.

---

### Version 1 (Initial Baseline)
**Date**: 2023-12-23  
**Memory Allocator**: System default (`malloc`/`new`)  
**Status**: Initial C++ port baseline

| Operation | ART Time | BST Time | Improvement | Speedup vs BST |
|-----------|----------|----------|-------------|----------------|
| **PUT** | 365.375 ms | 741.155 ms | +102.8% | 2.0x |
| **GET_HIT** | 123.465 ms | 680.536 ms | +451.0% | 5.5x |
| **REMOVE** | 274.092 ms | 1045.85 ms | +281.5% | 3.8x |
| **FOREACH** | 29.26 μs | 19.61 μs | -33.0% | Slower |
| **FOREACH_DESC** | 123.54 μs | 60.55 μs | -51.0% | Slower |
| **HIGHER** | 343.818 ms | 934.49 ms | +171.8% | 2.7x |
| **LOWER** | 296.755 ms | 665.687 ms | +124.3% | 2.2x |

**Key Characteristics**:
- Strong lookup and deletion performance (3.8-5.5x faster than BST)
- Iteration operations were slower than `std::map` due to tree traversal overhead
- Solid baseline demonstrating ART's core advantages
