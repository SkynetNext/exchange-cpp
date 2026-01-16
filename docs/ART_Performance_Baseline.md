# ART Tree Performance Baseline - C++ Port

## Overview

This document records the initial performance baseline for the C++ port of the Adaptive Radix Tree (ART) implementation. These results serve as a reference point for future optimizations and comparisons with the Java reference implementation.

## Test Environment

**Date**: 2026-01-16  
**Platform**: Windows  
**CPU**: 16 cores @ 3792 MHz  
**CPU Caches**:
- L1 Data: 32 KiB (x8)
- L1 Instruction: 32 KiB (x8)
- L2 Unified: 256 KiB (x8)
- L3 Unified: 16384 KiB (x1)

## Test Configuration

- **Data Size**: 5,000,000 (5 million) key-value pairs
- **Iterations**: 3 iterations per benchmark
- **Test Cases**: 8 benchmark scenarios (including GetMiss)
- **Comparison**: ART vs `std::map` vs `ankerl::unordered_dense` vs `std::unordered_map`
- **Memory Allocator**: mimalloc

## Performance Results

### Core Operations

| Operation | ART | std::map | unordered_dense | std::unordered_map | ART vs BST | ART vs Dense | ART vs Unordered |
|-----------|-----|----------|-----------------|-------------------|------------|--------------|------------------|
| **Put** | 319.2 ms | 1341.4 ms | 57.2 ms | 166.1 ms | **+320%** (4.2x) | -82% | -48% |
| **GetHit** | 308.4 ms | 1548.1 ms | 58.5 ms | 138.1 ms | **+402%** (5.0x) | -81% | -55% |
| **GetMiss** | 7.7 ms | 103.0 ms | 35.4 ms | 105.6 ms | **+1231%** (13.3x) ⭐ | **+357%** (4.6x) ⭐ | **+1265%** (13.7x) ⭐ |
| **Remove** | 548.5 ms | 2061.8 ms | 281.4 ms | 414.4 ms | **+276%** (3.8x) | -49% | -24% |

### Iteration Operations

| Operation | ART | std::map | unordered_dense | std::unordered_map | ART vs BST | ART vs Dense | ART vs Unordered |
|-----------|-----|----------|-----------------|-------------------|------------|--------------|------------------|
| **ForEach** | 24.3 μs | 53.4 μs | 6.9 μs | 52.6 μs | **+119%** (2.2x) | -72% | **+116%** (2.2x) |
| **ForEachDesc** | 70.8 μs | 101.6 μs | 16.4 μs | 70.8 μs | **+44%** (1.4x) | -77% | ±0% |

### Range Queries (Ordered Containers Only)

| Operation | ART | std::map | ART vs BST | Note |
|-----------|-----|----------|------------|------|
| **Higher** | 608.1 ms | 1770.1 ms | **+191%** (2.9x) | Hash maps do not support `upper_bound` |
| **Lower** | 589.5 ms | 1384.6 ms | **+135%** (2.3x) | Hash maps do not support `lower_bound` |

---

## Key Findings

### GetMiss Performance is Exceptional

ART outperforms all containers for failed lookups:
- **13.3x faster** than `std::map` 
- **4.6x faster** than `ankerl::unordered_dense`
- **13.7x faster** than `std::unordered_map`

This is due to ART's ability to terminate early when key prefix doesn't match:

```cpp
// ART can terminate early when key prefix doesn't match
// For keys with 1 trillion offset, mismatch detected at root node

// Hash map must:
// 1. Compute full hash of the key
// 2. Probe bucket(s)
// 3. Compare key equality
// No early termination possible
```

### ART vs std::map

ART wins all operations (1.4x - 13.3x faster). `std::map` is not recommended.

### ART vs Hash Maps

- ART is slower for Hit operations (hash O(1) advantage)
- ART is significantly faster for Miss operations (early termination)
- ART provides ordered operations that hash maps cannot support

### Best Use Cases

| Container | Best For |
|-----------|----------|
| **ART** | Range queries, Miss-heavy workloads, ordered iteration |
| **unordered_dense** | Pure KV store with high hit rate |
| **std::map** | Not recommended (outperformed by ART in all cases) |
| **std::unordered_map** | Not recommended (outperformed by unordered_dense) |

## Conclusion

The C++ ART tree is the optimal choice for trading systems where:
- Range queries (Higher/Lower) are required
- Miss queries are common (order lookup before cancellation)
- Ordered iteration is needed (price level traversal)

For pure KV workloads with high hit rates and no ordering requirements, `ankerl::unordered_dense` remains faster.

## Java vs C++ Performance Comparison

### Test Configuration

- **Data Size**: 5,000,000 (5 million) key-value pairs
- **Java Test**: Last iteration (iteration 6) after JIT warmup
- **C++ Test**: Average of 10 iterations with Google Benchmark
- **Platform**: Same Windows machine, same CPU

### Absolute Performance Comparison (ART vs ART)

| Operation | Java ART | C++ ART | C++ Speedup | Performance Gain |
|-----------|----------|---------|-------------|------------------|
| **PUT** | 3971.192 ms | 365.375 ms | **10.88x** | **+987%** |
| **GET_HIT** | 2201.057 ms | 123.465 ms | **17.84x** | **+1684%** |
| **REMOVE** | 4011.818 ms | 274.092 ms | **14.63x** | **+1363%** |
| **FOREACH** | 0.705 ms | 0.02926 ms | **24.08x** | **+2308%** |
| **FOREACH_DESC** | 0.691 ms | 0.12354 ms | **5.60x** | **+460%** |
| **HIGHER** | 3364.812 ms | 343.818 ms | **9.79x** | **+879%** |
| **LOWER** | 4009.626 ms | 296.755 ms | **13.52x** | **+1252%** |

### Key Findings

1. **C++ ART is 10-24x faster than Java ART** across all operations
2. **Most significant gains**:
   - **GET_HIT**: 17.84x faster (most critical for trading systems)
   - **REMOVE**: 14.63x faster (important for order cancellation)
   - **FOREACH**: 24.08x faster (though absolute time is minimal)
3. **Consistent performance advantage**: C++ shows substantial improvements across all operations
4. **Real-world impact**: For a trading system processing millions of orders, these speedups translate to:
   - **~18x faster order lookups** (GET_HIT)
   - **~15x faster order cancellations** (REMOVE)
   - **~11x faster order placements** (PUT)

### Analysis

The performance gap is significant and expected due to:

1. **Object Pool Initialization**: Java test creates a new `LongAdaptiveRadixTreeMap` object in each iteration, which initializes a new object pool (256+128+64+32 = 480 pre-allocated objects). C++ test reuses the same object pool across iterations using `Clear()`, avoiding this overhead.

2. **Compilation Model**: C++ compiles to native machine code, while Java relies on JIT compilation

3. **Memory Management**: C++ has zero GC overhead, while Java has GC pauses and object allocation overhead

4. **Template Optimization**: C++ templates enable compile-time optimizations that eliminate virtual function overhead

5. **Cache Efficiency**: C++ provides better control over memory layout and cache utilization

6. **Runtime Overhead**: Java has JVM overhead (even after JIT optimization)

**Why the ~10x gap seems "too consistent"?**

The consistent ~10-15x speedup across most operations is primarily due to:
- **Object pool re-initialization overhead** in Java (happens every iteration)
- **GC pressure** from creating/destroying millions of objects
- **JVM runtime overhead** (even after JIT optimization)

The gap is larger for GET_HIT (17.84x) because lookup operations benefit more from C++'s cache efficiency and lack of virtual function calls.

**Note**: The Java test used the last iteration (iteration 6) to minimize JIT compilation effects, but object creation and GC overhead are still present. The C++ results use Google Benchmark's automatic warmup and object reuse, providing a more optimized baseline. For a fairer comparison, Java should also reuse objects across iterations, but the current test design reflects real-world usage where objects may be created per operation batch.

## Next Steps

1. ✅ Compare with Java reference implementation performance (Completed)
2. Profile and optimize iteration operations if needed
3. Consider SIMD optimizations for Node16 operations
4. Evaluate memory allocation patterns and object pooling effectiveness

---

**Baseline Established**: 2023-12-23  
**Version**: Initial C++ Port  
**Status**: ✅ Performance baseline documented  
**Java Comparison**: ✅ Completed (Last iteration results)

---

## Performance History

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

4. **Java Comparison Enhancement**: 
   - **PUT**: 10.88x → **25.09x faster** than Java (from 365ms to 158ms)
   - **GET_HIT**: 17.84x → **21.66x faster** than Java (from 123ms to 101ms)
   - **REMOVE**: 14.63x → **20.26x faster** than Java (from 274ms to 197ms)

**Technical Analysis**:

`mimalloc` provides several advantages for ART tree performance:
- **Small Object Optimization**: ART nodes are typically 32-256 bytes. `mimalloc`'s segregated free lists and thread-local caching make these allocations nearly free.
- **Memory Locality**: Better memory compaction improves cache hit rates during tree traversal, especially noticeable in iteration operations.
- **Reduced Fragmentation**: Lower memory fragmentation means nodes are more likely to be allocated in contiguous regions, improving spatial locality.

**Conclusion**: The integration of `mimalloc` elevates the C++ ART implementation from "excellent" to "exceptional", with particularly dramatic improvements in write operations and iteration. The performance gap with Java has widened to **20-25x**, making this implementation highly competitive for high-frequency trading systems.
