# OrderBook Data Structure Guide

本文档详细介绍撮合引擎订单簿的核心数据结构设计，包括 ART 树索引和侵入式链表的实现原理。

## 整体架构

订单簿采用 **ART (自适应基数树) + 侵入式链表** 的混合数据结构，支持**价格-时间优先**撮合：

```
           bestAskOrder_ ──────────────────────────────────┐
                                                           ↓
   ┌──────────────────────────────────────────────────────────────────────┐
   │  价格 100 (best)         价格 101              价格 102 (worst)       │
   │  ┌───────┐              ┌───────┐              ┌───────┐             │
   │  │Order1 │◄────────────►│Order3 │◄────────────►│Order5 │             │
   │  │ t=1   │   next/prev  │ t=3   │   next/prev  │ t=5   │             │
   │  └───────┘              └───────┘              └───────┘             │
   │      ↑                      ↑                      ↑                 │
   │      │                      │                      │                 │
   │  ┌───────┐              ┌───────┐              ┌───────┐             │
   │  │Order2 │              │Order4 │              │Order6 │             │
   │  │ t=2   │              │ t=4   │              │ t=6   │  ◄── lastOrder
   │  └───────┘              └───────┘              └───────┘             │
   │                                                                      │
   │                           ART Tree                                   │
   │  askPriceBuckets_:  100 → Bucket1,  101 → Bucket2,  102 → Bucket3    │
   └──────────────────────────────────────────────────────────────────────┘

图例：
- 横向 (next/prev): 不同价格级别之间的链接，以及同价格级别内按时间排序的订单链
- 纵向: 同一价格级别内的订单（时间优先，先到先得）
- ART Tree: 价格 → Bucket 的映射索引，O(k) 查找复杂度
- bestAskOrder_: 指向最优卖价的第一个订单（最早到达的）
- lastOrder: 指向该价格级别的最后一个订单（最晚到达的）
```

## 核心数据结构

### DirectOrder 结构

```cpp
struct DirectOrder {
    // 订单基本信息
    int64_t orderId = 0;
    int64_t price = 0;
    int64_t size = 0;
    int64_t filled = 0;
    int64_t reserveBidPrice = 0;
    int64_t uid = 0;
    OrderAction action = OrderAction::ASK;
    int64_t timestamp = 0;

    // 侵入式链表指针 (Intrusive List)
    DirectOrder *next = nullptr;  // 指向更优价格方向，或同价格内更早的订单
    DirectOrder *prev = nullptr;  // 指向更差价格方向，或同价格内更晚的订单
    
    // 所属价格桶
    Bucket *bucket = nullptr;
};
```

### Bucket 结构

```cpp
struct Bucket {
    int64_t price = 0;           // 价格级别
    DirectOrder *lastOrder = nullptr;  // 该价格的尾订单（最晚到达）
    int64_t totalVolume = 0;     // 该价格的总挂单量
    int32_t numOrders = 0;       // 该价格的订单数量
};
```

### OrderBookDirectImpl 主要成员

```cpp
class OrderBookDirectImpl {
    // ART 树索引：价格 → Bucket
    LongAdaptiveRadixTreeMap<Bucket> askPriceBuckets_;
    LongAdaptiveRadixTreeMap<Bucket> bidPriceBuckets_;
    
    // ART 树索引：订单ID → DirectOrder
    LongAdaptiveRadixTreeMap<DirectOrder> orderIdIndex_;
    
    // 最优订单指针
    DirectOrder *bestAskOrder_;  // 最低卖价的第一个订单
    DirectOrder *bestBidOrder_;  // 最高买价的第一个订单
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
| 一个对象多个链表 | 容易（多次插入不同容器） | 需要多组指针 |

### 为什么选择侵入式链表？

在高频交易撮合引擎中，订单的**取消**和**修改**是高频操作：

1. **O(1) 删除**：通过 `orderIdIndex_` 查到订单后，直接操作 `next/prev` 指针删除，无需遍历链表
2. **零拷贝**：订单对象本身就是链表节点，无需额外的节点分配
3. **缓存友好**：订单数据和链接指针在同一缓存行，减少 cache miss

## 链表操作详解

### 1. 插入订单 (insertOrder)

#### 情况 A：价格级别已存在

新订单插入到该价格级别的尾部（时间优先）：

```
插入前 (价格 100):
  [Order1] <--> [Order2] <--> [Order3(tail)]
                               ↑
                            lastOrder

插入 Order4 后:
  [Order1] <--> [Order2] <--> [Order4(新tail)] <--> [Order3]
                               ↑
                            lastOrder
```

核心代码：
```cpp
DirectOrder *oldTail = toBucket->lastOrder;
DirectOrder *prevOrder = oldTail->prev;

toBucket->lastOrder = order;      // 更新 tail 指针
oldTail->prev = order;            // Order3.prev = Order4
order->next = oldTail;            // Order4.next = Order3
order->prev = prevOrder;          // Order4.prev = Order2
if (prevOrder)
    prevOrder->next = order;      // Order2.next = Order4
order->bucket = toBucket;
```

#### 情况 B：新价格级别

创建新 Bucket，通过 ART 树的 `GetLowerValue`/`GetHigherValue` 找到相邻价格级别，插入到全局链表中：

```cpp
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
    // 成为新的最优价格
    DirectOrder *oldBestOrder = isAsk ? bestAskOrder_ : bestBidOrder_;
    if (oldBestOrder) oldBestOrder->next = order;
    if (isAsk) bestAskOrder_ = order;
    else bestBidOrder_ = order;
    order->next = nullptr;
    order->prev = oldBestOrder;
}
```

### 2. 删除订单 (RemoveOrder)

经典双向链表删除，**O(1) 复杂度**：

```
删除前:
  [prev] <--> [order] <--> [next]

删除后:
  [prev] <-----------> [next]
```

核心代码：
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

// 如果是该价格的尾订单，更新 lastOrder 或删除 Bucket
if (bucket->lastOrder == order) {
    if (order->next == nullptr || order->next->bucket != bucket) {
        buckets.Remove(order->price);  // 删除空 Bucket
    } else {
        bucket->lastOrder = order->next;
    }
}
```

### 3. 撮合遍历 (tryMatchInstantly)

从最优价格开始，沿着 `prev` 指针遍历，按**价格-时间优先**顺序撮合：

```cpp
DirectOrder *makerOrder = isBidAction ? bestAskOrder_ : bestBidOrder_;

do {
    // 撮合逻辑
    int64_t tradeSize = std::min(remainingSize, makerOrder->size - makerOrder->filled);
    makerOrder->filled += tradeSize;
    remainingSize -= tradeSize;
    
    if (makerOrder->size == makerOrder->filled) {
        // 订单完全成交，从链表中移除
        orderIdIndex_.Remove(makerOrder->orderId);
        // ... 释放订单 ...
    }
    
    // 沿 prev 方向遍历下一个订单
    makerOrder = makerOrder->prev;
    
} while (makerOrder != nullptr && remainingSize > 0 && 价格满足条件);
```

遍历顺序示例（Ask 侧）：
```
bestAskOrder_ → Order1(价格100,t=1) → Order2(价格100,t=2) → Order3(价格101,t=3) → ...
                     ↑                      ↑                      ↑
                 最优价格+最早          同价格+次早            次优价格+最早
```

## 复杂度分析

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| 新增订单 | O(k) | k 为 ART 树键长（固定 8 字节），实际 O(1) |
| 取消订单 | O(k) | 通过 orderIdIndex_ 查找 + O(1) 链表删除 |
| 修改订单 | O(k) | 删除 + 插入 |
| 按价格查找 | O(k) | ART 树查找 |
| 撮合遍历 | O(n) | n 为成交订单数，每个订单 O(1) 处理 |
| 获取最优价格 | O(1) | 直接访问 bestAskOrder_/bestBidOrder_ |

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

### 缓存行对齐

`DirectOrder` 结构体的关键字段（orderId, price, size, filled）在内存中连续存放，提高缓存命中率。

## 相关文件

- 头文件: `include/exchange/core/orderbook/OrderBookDirectImpl.h`
- 实现: `src/exchange/core/orderbook/OrderBookDirectImpl.cpp`
- ART 树: `include/exchange/core/collections/art/LongAdaptiveRadixTreeMap.h`
- 对象池: `include/exchange/core/collections/objpool/ObjectsPool.h`

## 参考资料

- [ART Tree Deep Dive Tutorial](./ART_Tree_Deep_Dive_Tutorial.md)
- [Custom Data Structures Selection](../CUSTOM_DATA_STRUCTURES.md)
