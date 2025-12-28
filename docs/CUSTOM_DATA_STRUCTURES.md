# Custom Data Structures Selection

| Component | Java Implementation | C++ Solution | Implementation | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Hash Map** | Eclipse Collections (`IntObjectHashMap`, `IntLongHashMap`) | `ankerl::unordered_dense` | Third-party library (Header-only) | 2-3x faster than `std::unordered_map` |
| **Concurrent Hash Map** | `ConcurrentHashMap` | `tbb::concurrent_hash_map` | Third-party library (submodule, static linking) | Lock-free concurrent hash table, outperforms Java `ConcurrentHashMap`, used for `promises_` mapping. Static linking avoids DLL dependencies |
| **Price Index** | ART Tree (`LongAdaptiveRadixTreeMap`, custom) | **Custom ART Tree** | **Direct translation from Java** | 3-5x faster than `std::map`, fixed 8-byte keys, O(k) constant time |
| **Object Pool** | ObjectsPool (custom) | **Custom ObjectsPool** | **Direct translation from Java** | Thread-local object pool, zero-allocation design, supports multiple object types |
| **Order Chain** | Intrusive Linked List (`DirectOrder.next/prev`, custom) | **Custom Intrusive Linked List** | **Direct translation from Java** | Same-price order queue (FIFO), zero additional memory allocation |
| **Lock-Free Ring Buffer** | LMAX Disruptor | `disruptor-cpp` | Third-party library (submodule) | Lock-free ring buffer for inter-thread communication |
| **Concurrent Queue** | `LinkedBlockingQueue` | `tbb::concurrent_bounded_queue` | Third-party library (submodule, static linking) | Bounded lock-free concurrent queue, replaces `std::queue + std::mutex` in `SharedPool`. Matches Java `LinkedBlockingQueue` behavior: `try_push()` returns `false` when queue is full (chain is discarded), `set_capacity()` enforces `poolMaxSize` limit. Used for `MatcherTradeEvent` chain pooling |
| **Global Allocator** | - | `mimalloc` | Third-party library (submodule) | High-performance memory allocator, reduces memory allocation overhead by 50%+ |
| **Compression** | LZ4 Java (`lz4-java`) | `lz4` (C library) | Third-party library (submodule) | Fast compression/decompression, required dependency (1:1 with Java version) |

## Implementation Notes

- **Third-party Libraries**: Use mature open-source libraries, no need to implement from scratch
- **Direct Translation**: Components implemented by exchange-core itself, require 1:1 translation from Java code
- **Reference Implementation Paths**:
  - ART Tree: `reference/exchange-core/src/main/java/exchange/core2/collections/art/LongAdaptiveRadixTreeMap.java`
  - ObjectsPool: `reference/exchange-core/src/main/java/exchange/core2/collections/objpool/ObjectsPool.java`
  - Intrusive Linked List: `reference/exchange-core/src/main/java/exchange/core2/core/orderbook/OrderBookDirectImpl.java` (DirectOrder class)
  - SharedPool: `reference/exchange-core/src/main/java/exchange/core2/core/processors/SharedPool.java` (uses `LinkedBlockingQueue` in Java, `moodycamel::ConcurrentQueue` in C++)
