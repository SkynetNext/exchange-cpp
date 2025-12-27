# Exchange Core Performance Benchmark: C++ vs Java Comparison

## Overview

Performance comparison between C++ and Java implementations of the exchange core system, focusing on throughput for `CURRENCY_EXCHANGE_PAIR` (Exchange) operations.

## Test Environment

**Date**: 2025-12-28  
**Platform**: Linux  
**CPU**: AMD EPYC 9Y24 96-Core Processor (16 cores, 8 cores/socket, 2 threads/core)  
**CPU Caches**: L1d: 256 KiB (8×), L1i: 256 KiB (8×), L2: 8 MiB (8×), L3: 32 MiB (1×)  
**Memory**: 61 GiB total, 48 GiB available  
**Virtualization**: KVM  
**Build**: C++ (optimized native), Java (JVM with JIT)  
**CPU Affinity**: Enabled

## Test Configuration

| Parameter | Value |
|-----------|-------|
| Symbol Type | `CURRENCY_EXCHANGE_PAIR` |
| Symbols | 1 |
| Benchmark Commands | 1,000,000 |
| PreFill Commands | 1,000 |
| Users | 2,000 accounts (1,325 unique) |
| Currencies | 2 |
| Iterations | 5 |
| Ring Buffer | 32,768 |
| Matching Engines | 1 |
| Risk Engines | 1 |

**Command Distribution**: GTC ~17%, IOC ~1%, Cancel ~7-12%, Move ~60-71%, Reduce ~7-8%

## Results

### C++ Implementation

| Iteration | Throughput (MT/s) | Time (ms) |
|-----------|-------------------|-----------|
| 0-4 | 8.772, 8.929, 9.009, 8.929, 8.929 | 114, 112, 111, 112, 112 |
| **Average** | **8.913 MT/s** | **112.2 ms** |

**Total Time**: 1.5 seconds  
**Variance**: ±0.1 MT/s (minimal)  
**Warm-up**: None

### Java Implementation

| Iteration | Throughput (MT/s) | Time (ms) |
|-----------|-------------------|-----------|
| 0 | 1.350 | ~741 (warm-up) |
| 1-4 | 5.505, 5.556, 5.837, 5.747 | ~182, 180, 171, 174 |
| **Average** | **4.799 MT/s** | **209.6 ms** |
| **Post-warm-up** | **5.661 MT/s** | **~176 ms** |

**Total Time**: 14.7 seconds  
**Variance**: ±0.3 MT/s (excluding warm-up)  
**Warm-up**: First iteration 4.4x slower

## Performance Comparison

| Metric | C++ | Java | Ratio |
|--------|-----|------|-------|
| Average Throughput | 8.913 MT/s | 4.799 MT/s | **1.86x** |
| Peak Throughput | 9.009 MT/s | 5.837 MT/s | **1.54x** |
| Stable Throughput | 8.913 MT/s | 5.661 MT/s | **1.57x** |
| Avg Execution Time | 112.2 ms | 209.6 ms | **1.87x faster** |
| Total Test Time | 1.5s | 14.7s | **9.8x faster** |
| Performance Variance | ±0.1 MT/s | ±0.3 MT/s | C++ more stable |

## Observations

- C++ throughput: 8.913 MT/s average, variance ±0.1 MT/s
- Java throughput: 4.799 MT/s average (5.661 MT/s post-warm-up), variance ±0.3 MT/s
- C++ execution time: 112.2 ms per iteration, 1.5s total
- Java execution time: 209.6 ms per iteration (176 ms post-warm-up), 14.7s total
- Java first iteration: 1.350 MT/s (4.4x slower than average)

---

**Version**: 1.0 | **Date**: 2025-12-28 | **Test**: `TestThroughputExchange`
