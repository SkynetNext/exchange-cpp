# 数据结构选型表

| 组件 | Java 原实现 | C++ 方案 | 实现方式 | 说明 |
| :--- | :--- | :--- | :--- | :--- |
| **哈希映射** | Eclipse Collections (`IntObjectHashMap`, `IntLongHashMap`) | `ankerl::unordered_dense` | 第三方库（Header-only） | 比 `std::unordered_map` 快 2-3 倍 |
| **并发哈希映射** | `ConcurrentHashMap` | `tbb::concurrent_hash_map` | 第三方库（子模块，静态链接） | 无锁并发哈希表，性能优于 Java `ConcurrentHashMap`，用于 `promises_` 映射。使用静态库避免 DLL 依赖 |
| **价格索引** | ART 树（`LongAdaptiveRadixTreeMap`，自实现） | **自定义 ART 树** | **直接翻译 Java 实现** | 比 `std::map` 快 3-5 倍，固定 8 字节键，O(k) 常数时间 |
| **对象池** | ObjectsPool（自实现） | **自定义 ObjectsPool** | **直接翻译 Java 实现** | 线程局部对象池，零分配设计，支持多种对象类型 |
| **订单链** | 侵入式链表（`DirectOrder.next/prev`，自实现） | **自定义侵入式链表** | **直接翻译 Java 实现** | 同价格订单队列（FIFO），零额外内存分配 |
| **无锁队列** | LMAX Disruptor | `disruptor-cpp` | 第三方库（子模块） | 无锁环形缓冲区，线程间通信 |
| **全局分配器** | - | `mimalloc` | 第三方库（子模块） | 高性能内存分配器，减少 50%+ 内存分配开销 |
| **压缩算法** | LZ4 Java (`lz4-java`) | `lz4` (C库) | 第三方库（子模块） | 快速压缩/解压缩，必需依赖（1:1 与 Java 版本） |

## 实现说明

- **第三方库**：使用成熟开源库，无需从零实现
- **直接翻译**：exchange-core 自己实现的组件，需要 1:1 翻译 Java 代码
- **参考实现路径**：
  - ART 树：`reference/exchange-core/src/main/java/exchange/core2/collections/art/LongAdaptiveRadixTreeMap.java`
  - ObjectsPool：`reference/exchange-core/src/main/java/exchange/core2/collections/objpool/ObjectsPool.java`
  - 侵入式链表：`reference/exchange-core/src/main/java/exchange/core2/core/orderbook/OrderBookDirectImpl.java` (DirectOrder 类)
