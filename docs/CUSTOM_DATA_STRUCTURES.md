# 自定义数据结构需求分析

## 概述

本文档列出 `exchange-core` 移植到 C++ 时的数据结构选型。**exchange-core 自己实现的组件直接翻译，第三方库使用成熟开源替代。**

---

## 数据结构选型

| 组件 | Java 原实现 | C++ 方案 | 说明 |
| :--- | :--- | :--- | :--- |
| **哈希映射** | Eclipse Collections | `ankerl::unordered_dense` | 第三方库替代 |
| **价格索引** | ART 树（自实现） | **自定义 ART 树** | **直接翻译** |
| **对象池** | ObjectsPool（自实现） | **自定义 ObjectsPool** | **直接翻译** |
| **订单链** | 侵入式链表（自实现） | **自定义侵入式链表** | **直接翻译** |
| **无锁队列** | LMAX Disruptor | `disruptor-cpp` | 已集成 |
| **全局分配器** | - | `mimalloc` | 第三方库 |

---

## 需要自己实现的组件（直接翻译 exchange-core）

### 1. ART 树 (`LongAdaptiveRadixTreeMap`)

**参考实现**：`exchange.core2.collections.art.LongAdaptiveRadixTreeMap`

**用途**：价格索引（`price → Bucket*`）、订单 ID 索引（`orderId → Order*`）

**实现要点**：
- 节点类型：Node4, Node16, Node48, Node256（自适应）
- 支持范围查询（`lower_bound`, `upper_bound`）
- 支持对象池集成

---

### 2. ObjectsPool

**参考实现**：`exchange.core2.collections.objpool.ObjectsPool`

**用途**：线程局部对象池，零分配设计

**实现要点**：
- 支持多种对象类型（DIRECT_ORDER, DIRECT_BUCKET, ART_NODE_4/16/48/256 等）
- 线程局部存储（thread_local）
- 链式回收机制

---

### 3. 侵入式链表

**参考实现**：`OrderBookDirectImpl.DirectOrder` 的 `next`/`prev` 指针

**用途**：同价格订单队列（FIFO）

**实现要点**：
- `DirectOrder` 包含 `next`、`prev`、`parent` 指针
- 手动维护链表操作
- 零额外内存分配

---

## 第三方库依赖

### ankerl::unordered_dense

**用途**：哈希映射（替代 `IntObjectHashMap`, `IntLongHashMap`）

**集成**：`git submodule add https://github.com/martinus/unordered_dense.git third_party/unordered_dense`

```cpp
#include <ankerl/unordered_dense.h>
ankerl::unordered_dense::map<int32_t, OrderBook*> symbol_to_orderbook;
```

---

### mimalloc

**用途**：高性能全局分配器

**集成**：`find_package(mimalloc REQUIRED)`

---

### disruptor-cpp

**用途**：无锁环形缓冲区（线程间通信）

**状态**：已集成

---

## CMake 配置

```cmake
# 第三方库
find_package(mimalloc REQUIRED)
target_link_libraries(exchange-cpp PRIVATE mimalloc)

# Header-only 库
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/unordered_dense/include
)

# 自定义实现（无需外部依赖）
# - ART 树
# - ObjectsPool  
# - 侵入式链表
```

---

## 实现顺序

1. **集成第三方库**（ankerl, mimalloc）
2. **实现核心数据结构**（直接翻译 exchange-core）：
   - ART 树
   - ObjectsPool
   - 侵入式链表
3. **实现业务逻辑**（订单簿、撮合引擎等）
4. **性能测试与优化**
