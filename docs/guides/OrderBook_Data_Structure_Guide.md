# OrderBook Data Structure Guide

本文档详细介绍撮合引擎订单簿的核心数据结构设计，包括 ART 树索引和侵入式链表的实现原理。

## 整体架构

订单簿采用 **ART (自适应基数树) + 侵入式链表** 的混合数据结构，支持**价格-时间优先**撮合。

### 架构图（价格不连续的实际场景）

```
                        bestAskOrder_
                             │
                             ▼
   ┌─────────────────────────────────────────────────────────────────────────────┐
   │                                                                             │
   │   价格 100.5 (卖一)      价格 102.0           价格 105.5 (最差)              │
   │   ┌───────────────┐    ┌───────────────┐    ┌───────────────┐              │
   │   │ Order1 (t=1)  │◄──►│ Order3 (t=5)  │◄──►│ Order5 (t=8)  │              │
   │   │ 100 shares    │    │ 200 shares    │    │ 50 shares     │              │
   │   └───────────────┘    └───────────────┘    └───────────────┘              │
   │          ▲                    ▲                    ▲                       │
   │          │ prev               │ prev               │ prev                  │
   │   ┌───────────────┐    ┌───────────────┐    ┌───────────────┐              │
   │   │ Order2 (t=3)  │    │ Order4 (t=6)  │    │ Order6 (t=9)  │ ◄─ lastOrder │
   │   │ 150 shares    │    │ 80 shares     │    │ 120 shares    │              │
   │   └───────────────┘    └───────────────┘    └───────────────┘              │
   │                                                                             │
   │   注意：价格 100.5 → 102.0 → 105.5 是不连续的！                              │
   │   中间可能有很多空档位（如 101.0, 101.5, 103.0 等都没有订单）                │
   │                                                                             │
   └─────────────────────────────────────────────────────────────────────────────┘

   askPriceBuckets_ (ART 树):
   ┌────────────────────────────────────────────────────────────────┐
   │  100.5 → Bucket{lastOrder=Order2, volume=250, count=2}         │
   │  102.0 → Bucket{lastOrder=Order4, volume=280, count=2}         │
   │  105.5 → Bucket{lastOrder=Order6, volume=170, count=2}         │
   │                                                                │
   │  支持: Get(price), Put(price), Remove(price)                   │
   │        GetLowerValue(price), GetHigherValue(price) ← 关键！    │
   └────────────────────────────────────────────────────────────────┘
```

### 为什么不能用 Hash Map？

**关键问题：插入新价格档位时需要有序操作**

当一个订单的价格是**新价格档位**（之前没有这个价格的订单）时，需要将其插入到链表的正确位置：

```cpp
// insertOrder() 中的关键代码
Bucket *lowerBucket = isAsk ? buckets.GetLowerValue(order->price)   // 找更低价格
                            : buckets.GetHigherValue(order->price); // 找更高价格
```

| 数据结构 | Get/Put/Remove | GetLowerValue/GetHigherValue | 适用性 |
|----------|----------------|------------------------------|--------|
| **Hash Map** | ✅ O(1) | ❌ 不支持 | ❌ 无法用于 priceBuckets |
| **std::map** | ✅ O(log n) | ✅ O(log n) | ✅ 可用但慢 |
| **ART** | ✅ O(k) | ✅ O(k) | ✅ **最佳选择** |

**只要有一个操作需要有序查询，就必须用有序数据结构。**

### 操作与有序需求对照表

| 操作 | 需要有序操作 | 频率 | 说明 |
|------|-------------|------|------|
| 插入订单（已有价格） | ❌ 否 | 高 | `Get(price)` 获取 Bucket，链表操作 |
| **插入订单（新价格）** | **✅ 是** | 中 | `GetLowerValue`/`GetHigherValue` 找链表位置 |
| 删除订单 | ❌ 否 | 高 | 纯链表指针操作 |
| 撮合遍历 | ❌ 否 | 很高 | 从 bestOrder 沿 prev 指针遍历 |
| L2 行情快照 | ✅ 是 | 低 | `ForEach`/`ForEachDesc` 有序遍历 |

## 数据结构选择

### 当前实现

```cpp
class OrderBookDirectImpl {
    // 价格 → Bucket（必须有序，用 ART）
    LongAdaptiveRadixTreeMap<Bucket> askPriceBuckets_;
    LongAdaptiveRadixTreeMap<Bucket> bidPriceBuckets_;
    
    // 订单ID → DirectOrder（纯 KV，用高性能 Hash Map）
    ankerl::unordered_dense::map<int64_t, DirectOrder*> orderIdIndex_;
    
    // 最优订单指针（O(1) 访问）
    DirectOrder *bestAskOrder_;  // 卖一（最低卖价的第一个订单）
    DirectOrder *bestBidOrder_;  // 买一（最高买价的第一个订单）
};
```

### 选择理由

| 组件 | 数据结构 | 理由 |
|------|----------|------|
| **priceBuckets_** | ART | 需要 `GetLowerValue`/`GetHigherValue` |
| **orderIdIndex_** | unordered_dense | 纯 KV 操作，无需有序，Hash 更快 |

## 核心数据结构

### DirectOrder 结构

```cpp
struct DirectOrder {
    // 订单基本信息
    int64_t orderId = 0;
    int64_t price = 0;
    int64_t size = 0;           // 原始数量
    int64_t filled = 0;         // 已成交数量
    int64_t reserveBidPrice = 0;
    int64_t uid = 0;
    OrderAction action = OrderAction::ASK;
    int64_t timestamp = 0;

    // 侵入式链表指针 (Intrusive List)
    DirectOrder *next = nullptr;  // 指向更优价格方向的订单
    DirectOrder *prev = nullptr;  // 指向更差价格方向的订单
    
    // 所属价格桶
    Bucket *bucket = nullptr;
};
```

### Bucket 结构

```cpp
struct Bucket {
    int64_t price = 0;               // 价格级别
    DirectOrder *lastOrder = nullptr; // 该价格的尾订单（最晚到达）
    int64_t totalVolume = 0;         // 该价格的剩余挂单量
    int32_t numOrders = 0;           // 该价格的订单数量
};
```

## 侵入式链表 (Intrusive List)

### 什么是侵入式链表？

**侵入式链表**的核心特点是：**链表的链接指针（next/prev）直接嵌入在数据对象内部**，而不是使用外部容器（如 `std::list`）。

### 对比非侵入式链表

| 特性 | 非侵入式链表 (std::list) | 侵入式链表 |
|------|-------------------------|-----------|
| 内存分配 | 每个元素需要额外分配链表节点 | 零额外内存分配 |
| 缓存局部性 | 节点和数据分离，缓存不友好 | 数据和指针在一起，缓存友好 |
| 删除操作 | 需要先查找节点位置 O(n) | 直接操作指针 O(1) |
| 内存碎片 | 更多碎片 | 更少碎片 |

### 为什么选择侵入式链表？

在高频交易撮合引擎中，订单的**取消**和**修改**是高频操作：

1. **O(1) 删除**：通过 `orderIdIndex_` 查到订单后，直接操作 `next/prev` 指针删除
2. **零拷贝**：订单对象本身就是链表节点，无需额外的节点分配
3. **缓存友好**：订单数据和链接指针在同一缓存行，减少 cache miss

## 链表操作详解

### 1. 插入订单 (insertOrder)

#### 情况 A：价格级别已存在（无需有序操作）

新订单插入到该价格级别的尾部（时间优先）：

```cpp
Bucket *toBucket = buckets.Get(order->price);  // 简单 KV 查询
if (toBucket != nullptr) {
    DirectOrder *oldTail = toBucket->lastOrder;
    DirectOrder *prevOrder = oldTail->prev;
    
    toBucket->lastOrder = order;      // 更新 tail 指针
    oldTail->prev = order;            // 链接新订单
    order->next = oldTail;
    order->prev = prevOrder;
    if (prevOrder) prevOrder->next = order;
    order->bucket = toBucket;
}
```

#### 情况 B：新价格级别（需要有序操作！）

必须通过 ART 的 `GetLowerValue`/`GetHigherValue` 找到相邻价格级别：

```cpp
// 创建新 Bucket
buckets.Put(order->price, newBucket);

// ⚠️ 这里需要有序操作！Hash Map 无法实现！
Bucket *lowerBucket = isAsk ? buckets.GetLowerValue(order->price)
                            : buckets.GetHigherValue(order->price);

if (lowerBucket != nullptr) {
    // 插入到相邻价格级别之间
    DirectOrder *lowerTail = lowerBucket->lastOrder;
    DirectOrder *prevOrder = lowerTail->prev;
    lowerTail->prev = order;
    if (prevOrder) prevOrder->next = order;
    order->next = lowerTail;
    order->prev = prevOrder;
} else {
    // 成为新的最优价格（卖一/买一）
    DirectOrder *oldBestOrder = isAsk ? bestAskOrder_ : bestBidOrder_;
    if (oldBestOrder) oldBestOrder->next = order;
    if (isAsk) bestAskOrder_ = order;
    else bestBidOrder_ = order;
    order->next = nullptr;
    order->prev = oldBestOrder;
}
```

**为什么不能用链表遍历找插入位置？**

| 方法 | 复杂度 | 1000 个价格档的耗时 |
|------|--------|---------------------|
| 从 bestOrder 遍历链表 | O(n) | 慢 |
| ART GetLowerValue | O(k) ≈ O(1) | 快 |

### 2. 删除订单 (RemoveOrder)

**完全不需要有序操作**，纯链表指针操作，O(1) 复杂度：

```cpp
// 更新相邻节点的指针
if (order->next != nullptr)
    order->next->prev = order->prev;
if (order->prev != nullptr)
    order->prev->next = order->next;

// 如果是最优订单，更新 best 指针
if (order == bestAskOrder_)
    bestAskOrder_ = order->prev;
else if (order == bestBidOrder_)
    bestBidOrder_ = order->prev;

// 如果该价格档位空了，从 ART 中删除
if (bucket->numOrders == 0) {
    buckets.Remove(order->price);  // 简单 KV 删除
}
```

### 3. 撮合遍历 (tryMatchInstantly)

**完全不需要有序操作**，从最优价格开始，沿着 `prev` 指针遍历：

```cpp
// 直接获取最优订单，O(1)
DirectOrder *makerOrder = isBidAction ? bestAskOrder_ : bestBidOrder_;

do {
    // 撮合逻辑
    int64_t tradeSize = std::min(remainingSize, makerOrder->size - makerOrder->filled);
    makerOrder->filled += tradeSize;
    remainingSize -= tradeSize;
    
    if (makerOrder->size == makerOrder->filled) {
        // 订单完全成交
        orderIdIndex_.erase(makerOrder->orderId);  // Hash O(1)
        if (makerOrder == priceBucketTail) {
            buckets.Remove(makerOrder->price);      // ART 删除
        }
    }
    
    // 沿 prev 方向遍历下一个订单，O(1)
    makerOrder = makerOrder->prev;
    
} while (makerOrder != nullptr && remainingSize > 0 && 价格满足条件);
```

**撮合热路径是纯指针遍历，零 ART 树操作！**

## 复杂度分析

| 操作 | 复杂度 | ART 操作 | 链表操作 |
|------|--------|----------|----------|
| 新增订单（已有价格） | O(k) | Get | O(1) 链表插入 |
| 新增订单（新价格） | O(k) | Get, Put, GetLower/Higher | O(1) 链表插入 |
| 取消订单 | O(1) + O(k) | Remove（仅空档位） | O(1) 链表删除 |
| 撮合单笔 | O(1) | 无 | O(1) 指针遍历 |
| 获取最优价格 | O(1) | 无 | 直接访问 bestOrder |

> 其中 k = 8（int64_t 键长度），实际接近 O(1)

## 内存布局优化

### 对象池 (ObjectsPool)

订单和 Bucket 对象通过对象池管理，避免频繁的 `new`/`delete`：

```cpp
// 获取订单对象
DirectOrder *order = objectsPool_->Get<DirectOrder>(
    ObjectsPool::DIRECT_ORDER, []() { return new DirectOrder(); });

// 归还订单对象
objectsPool_->Put(ObjectsPool::DIRECT_ORDER, order);
```

### 内存分配器

使用 **mimalloc** 高性能内存分配器，对小对象分配（DirectOrder, Bucket）有显著优化。

## 性能基准

基于 5M 订单的基准测试（详见 [ART_Performance_Baseline.md](../ART_Performance_Baseline.md)）：

| 操作 | ART | std::map | ART 优势 |
|------|-----|----------|----------|
| Higher/Lower | 578-590 ms | 1819-1959 ms | **3.1-3.4x 更快** |
| Get (Hit) | 301 ms | 1349 ms | **4.5x 更快** |
| Put | 276 ms | 1099 ms | **4.0x 更快** |

## 相关文件

- 头文件: `include/exchange/core/orderbook/OrderBookDirectImpl.h`
- 实现: `src/exchange/core/orderbook/OrderBookDirectImpl.cpp`
- ART 树: `include/exchange/core/collections/art/LongAdaptiveRadixTreeMap.h`
- 对象池: `include/exchange/core/collections/objpool/ObjectsPool.h`

## 参考资料

- [ART Performance Baseline](../ART_Performance_Baseline.md) - ART 树性能基准测试
- [ART Tree Deep Dive Tutorial](./ART_Tree_Deep_Dive_Tutorial.md) - ART 树原理详解
