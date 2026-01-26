# Adaptive Radix Tree (ART) 完全指南

> 面向高频交易系统的内存索引数据结构

## 核心概念

**ART 树**是一种自适应基数树，通过动态调整节点大小来平衡内存效率和查找性能。

```
┌─────────────────────────────────────────────────────────────────┐
│  传统 Trie vs ART                                               │
│                                                                 │
│  Trie (固定256子节点):          ART (自适应节点):               │
│  ┌───────────────────┐          ┌─────────┐                     │
│  │ children[256]     │          │ Node4   │ ← 只存4个子节点     │
│  │ (2KB per node!)   │          │ ~40B    │                     │
│  └───────────────────┘          └─────────┘                     │
│                                                                 │
│  内存浪费严重                    按需扩展，内存高效              │
└─────────────────────────────────────────────────────────────────┘
```

---

## 1. 四种节点类型

```
┌──────────┬──────────┬────────────┬─────────────┬───────────────┐
│ 节点类型 │ 子节点数 │ 内存占用   │ 查找方式    │ 缓存级别      │
├──────────┼──────────┼────────────┼─────────────┼───────────────┤
│ Node4    │ 1-4      │ ~40 bytes  │ 线性O(4)    │ L1 (热点)     │
│ Node16   │ 5-16     │ ~80 bytes  │ 线性O(16)   │ L1            │
│ Node48   │ 17-48    │ ~640 bytes │ 间接索引O(1)│ L2            │
│ Node256  │ 49-256   │ ~2KB       │ 直接索引O(1)│ L2/L3         │
└──────────┴──────────┴────────────┴─────────────┴───────────────┘
```

### 节点结构图示

```
Node4 (≤4 children)              Node16 (5-16 children)
┌────────────────────┐           ┌─────────────────────────────────┐
│ keys:  [A][B][C][D]│           │ keys[16]: 16个uint8_t           │
│ nodes: [→][→][→][→]│           │ nodes[16]: 16个指针             │
└────────────────────┘           │ • 线性查找 O(16)                │
  紧凑存储，顺序查找              └─────────────────────────────────┘

Node256 (49-256 children)
┌─────────────────────────────┐
│ children[256]               │
│ ┌─┬─┬─┬─┬─┬─┬─┬─┬───┬─┐     │
│ │ │ │→│ │ │→│ │...│ │     │
│ └─┴─┴─┴─┴─┴─┴─┴─┴───┴─┘     │
└─────────────────────────────┘
  直接索引: children[key_byte]

Node48 (17-48 children)
┌─────────────────────────────────────────────┐
│ indexes_[256]: 稀疏索引数组                 │
│ ┌───┬───┬───┬───┬───┬───┬───┬───┐           │
│ │ - │ 0 │ - │ 1 │ - │ - │ 2 │...│           │
│ └───┴───┴───┴───┴───┴───┴───┴───┘           │
│ nodes_[48]: 实际指针                        │
│ ┌───┬───┬───┬───────────────────┐           │
│ │ → │ → │ → │ ... (最多48个)    │           │
│ └───┴───┴───┴───────────────────┘           │
└─────────────────────────────────────────────┘
  查找: nodes_[indexes_[key_byte]]
```

### 关键代码：节点数据结构

```cpp
// ArtNode4.h - 最小节点类型
template <typename V>
class ArtNode4 : public IArtNode<V> {
private:
  std::array<uint8_t, 4> keys_{};   // 子节点的键字节 (0-255)
  std::array<void*, 4> nodes_{};    // 子节点指针
  int64_t nodeKey_ = 0;             // 路径压缩的前缀
  int nodeLevel_ = 0;               // 当前处理的字节位置
  uint8_t numChildren_ = 0;         // 当前子节点数
};
```

---

## 2. 路径压缩

路径压缩是 ART 的核心优化：**共享公共前缀，减少节点数量**。

### nodeLevel 和 nodeKey 的含义

```
64位键: 0x12345678_90ABCDEF
        ├──────────────────┤
        byte7...byte1 byte0

nodeLevel: 当前处理的字节位置 (0, 8, 16, 24, 32, 40, 48, 56)
           表示从最低字节开始，已经处理了多少位

nodeKey:   高位对齐的前缀值，低 nodeLevel 位清零
           计算: nodeKey = key & (-1LL << nodeLevel)
```

### 图示：路径压缩效果

```
存储键: 0x1234_5678, 0x1234_5679, 0x1234_ABCD

未压缩 (每字节一个节点):        路径压缩后:
                               
     [Root]                         [Root]
        │                              │
     [0x12]                    [prefix=0x1234, level=16]
        │                         ╱           ╲
     [0x34]                    [0x56]       [0xAB]
      ╱   ╲                      │             │
  [0x56] [0xAB]               [Node4]       [Node4]
     │      │                 ╱    ╲           │
  [Node4] [0xCD]          [0x78][0x79]     [0xCD]
   ╱   ╲     │               │     │          │
[0x78][0x79] V3             V1    V2         V3
  │     │          
 V1    V2          

节点数: 7个                  节点数: 4个 (节省43%)
```

### 关键代码：前缀匹配

```cpp
// ArtNode4::GetValue - 查找时的前缀检查
V* GetValue(int64_t key, int level) override {
  // 检查前缀是否匹配
  if (level != nodeLevel_ && ((key ^ nodeKey_) & (-1LL << (nodeLevel_ + 8))) != 0)
    return nullptr;  // 前缀不匹配，键不存在
  
  // 提取当前字节作为索引
  const uint8_t nodeIndex = static_cast<uint8_t>((key >> nodeLevel_) & 0xFF);
  
  // 在子节点中查找
  for (int i = 0; i < numChildren_; i++) {
    if (keys_[i] == nodeIndex) {
      return nodeLevel_ == 0 
        ? static_cast<V*>(nodes_[i])                              // 叶子节点
        : static_cast<IArtNode<V>*>(nodes_[i])->GetValue(key, nodeLevel_ - 8);  // 递归
    }
    if (nodeIndex < keys_[i]) break;  // 有序数组，提前终止
  }
  return nullptr;
}
```

---

## 3. 节点升级与降级

### 升级流程 (Node4 → Node16)

```
触发条件: Node4 已有4个子节点，插入第5个

Before:                          After:
┌─────────────────┐              ┌─────────────────────┐
│ Node4           │              │ Node16              │
│ keys: [A,B,C,D] │  ──升级──►   │ keys: [A,B,C,D,E]   │
│ numChildren: 4  │              │ numChildren: 5      │
└─────────────────┘              └─────────────────────┘
     40 bytes                         96 bytes
```

### 关键代码：升级逻辑

```cpp
// ArtNode4::Put - 插入时检查是否需要升级
if (numChildren_ < 4) {
  // 还有空间，直接插入
  keys_[pos] = nodeIndex;
  nodes_[pos] = newElement;
  numChildren_++;
  return nullptr;
} else {
  // 需要升级到 Node16
  auto* node16 = objectsPool_->Get<ArtNode16<V>>(
    ObjectsPool::ART_NODE_16,
    [this]() { return new ArtNode16<V>(objectsPool_); });
  node16->InitFromNode4(this, nodeIndex, newElement);
  RecycleNodeToPool<V>(this);  // 回收旧节点
  return node16;               // 返回新节点，替换父节点中的指针
}
```

### 升级/降级阈值

```
插入方向 (升级):
Node4(4) ──5th──► Node16(16) ──17th──► Node48(48) ──49th──► Node256

删除方向 (降级):
Node256 ──≤48──► Node48 ──≤16──► Node16 ──≤4──► Node4 ──0──► 删除
```

---

## 4. 前缀分裂

当插入的键与现有前缀不匹配时，需要分裂节点。

### 图示：分裂过程

```
初始状态:                        插入 0x1234_ABCD 后:
                                
[prefix=0x1234_56, level=24]    [prefix=0x1234, level=16] ← 新分支节点
       ╱    ╲                          ╱         ╲
   [0x78]  [0x79]                  [0x56]       [0xAB] ← 新分支
      │       │                      │             │
     V1      V2                 [原节点]      [新节点]
                                  ╱   ╲            │
                              [0x78][0x79]      [0xCD]
                                 │     │          │
                                V1    V2         V3
```

### 关键代码：分裂逻辑

```cpp
// BranchIfRequired - 检查是否需要分裂
template <typename V>
IArtNode<V>* BranchIfRequired(int64_t key, V* value, int64_t nodeKey, 
                               int nodeLevel, IArtNode<V>* caller) {
  // 计算键的差异
  const int64_t keyDiff = key ^ nodeKey;
  if ((keyDiff & (-1LL << nodeLevel)) == 0) {
    return nullptr;  // 前缀匹配，无需分裂
  }
  
  // 找到第一个不同的字节位置
  const int newLevel = (63 - __builtin_clzll(keyDiff)) & 0xF8;
  if (newLevel == nodeLevel) {
    return nullptr;
  }
  
  // 创建新的分支节点
  auto* newSubNode = pool->Get<ArtNode4<V>>(...);
  newSubNode->InitFirstKey(key, value);
  
  auto* newNode = pool->Get<ArtNode4<V>>(...);
  newNode->InitTwoKeys(nodeKey, caller, key, newSubNode, newLevel);
  
  return newNode;  // 返回新的分支节点
}
```

---

## 5. 有序遍历

ART 天然支持有序遍历，这对订单簿非常重要。

### 关键代码：ForEach 遍历

```cpp
// ArtNode4::ForEach - 有序遍历（升序）
int ForEach(LongObjConsumer<V>* consumer, int limit) const override {
  if (nodeLevel_ == 0) {
    // 叶子层：直接遍历
    const int64_t keyBase = nodeKey_ & (-1LL << 8);
    const int n = std::min(static_cast<int>(numChildren_), limit);
    for (int i = 0; i < n; i++)
      consumer->Accept(keyBase + keys_[i], static_cast<V*>(nodes_[i]));
    return n;
  }
  // 内部节点：递归遍历子节点
  int numLeft = limit;
  for (int i = 0; i < numChildren_ && numLeft > 0; i++)
    numLeft -= static_cast<IArtNode<V>*>(nodes_[i])->ForEach(consumer, numLeft);
  return limit - numLeft;
}
```

### 使用示例

```cpp
// 获取前20档卖价
askPriceBuckets_->ForEach([](int64_t price, PriceBucket* bucket) {
  std::cout << "Price: " << price << ", Qty: " << bucket->GetTotalQty() << "\n";
}, 20);
```

---

## 6. 对象池集成

ART 使用对象池减少内存分配开销。

```
┌─────────────────────────────────────────────────────────────┐
│                      ObjectsPool                            │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │
│  │ Node4    │ │ Node16   │ │ Node48   │ │ Node256  │       │
│  │ Pool     │ │ Pool     │ │ Pool     │ │ Pool     │       │
│  │ [→→→→→]  │ │ [→→→]    │ │ [→→]     │ │ [→]      │       │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘       │
└─────────────────────────────────────────────────────────────┘
         ↑ Get()                    ↓ Put()
         │                          │
    ┌────┴────┐                ┌────┴────┐
    │ ART Tree│                │ ART Tree│
    │ (使用)   │                │ (回收)   │
    └─────────┘                └─────────┘

优势:
• 避免频繁 new/delete
• 减少内存碎片
• 提高缓存命中率
• 多个 ART 树可共享同一个池
```

---

## 7. 性能对比

### 基准测试 (500万键值对)

> 测试环境: Linux 16核 @ 2600MHz, GCC -O3 -march=native -flto, mimalloc

```
┌────────────┬──────────┬─────────────────┬─────────┬────────────┐
│ 操作       │ std::map │ unordered_dense │ ART     │ vs std::map│
├────────────┼──────────┼─────────────────┼─────────┼────────────┤
│ Put        │ 1066 ms  │ 55 ms           │ 263 ms  │ 4.1x 快    │
│ GetHit     │ 1274 ms  │ 49 ms           │ 298 ms  │ 4.3x 快    │
│ GetMiss    │ 20 ms    │ 31 ms           │ 6 ms    │ 3.2x 快 ⭐ │
│ Remove     │ 1707 ms  │ 89 ms           │ 502 ms  │ 3.4x 快    │
│ Higher     │ 1817 ms  │ N/A             │ 595 ms  │ 3.1x 快    │
│ Lower      │ 1793 ms  │ N/A             │ 602 ms  │ 3.0x 快    │
└────────────┴──────────┴─────────────────┴─────────┴────────────┘

```

### 为什么 ART 更快？

```
1. 缓存友好
   ┌─────────────────────────────────────────┐
   │ CPU Cache Hierarchy                     │
   │ L1: 32KB  (~1 cycle)   ← Node4/16 适配  │
   │ L2: 256KB (~10 cycles) ← Node48/256     │
   │ L3: 8MB   (~40 cycles)                  │
   │ RAM:      (~100+ cycles)                │
   └─────────────────────────────────────────┘

2. 时间复杂度
   • ART: O(k) 其中 k=8 (64位键的字节数)
   • 红黑树: O(log n) 其中 n=1,000,000 → ~20次比较
   
3. 分支预测友好
   • Node4: 最多4次比较，CPU分支预测准确
   • 顺序内存访问，预取有效
```

---

## 8. 交易系统应用

ART 树在订单簿中用于**价格桶索引**，核心用途：

```cpp
// 价格桶索引 (价格 → 订单链表)
LongAdaptiveRadixTreeMap<DirectBucket> askPriceBuckets_;  // 卖价
LongAdaptiveRadixTreeMap<DirectBucket> bidPriceBuckets_;  // 买价
```

### 插入时定位

```cpp
// 插入新价格桶后，找到相邻的更优价格桶（用于维护有序链表）
Bucket* lowerBucket = isAsk 
  ? buckets.GetLowerValue(order->price)   // 卖单：找更低价格
  : buckets.GetHigherValue(order->price); // 买单：找更高价格
```

### 有序遍历

```cpp
// 遍历前20档卖价（生成 L2 市场数据）
askPriceBuckets_.ForEach([](int64_t price, DirectBucket* bucket) {
  // 处理每个价格档位
}, 20);
```

---

## 9. 适用场景

| 场景 | 适合 | 原因 |
|------|------|------|
| ✅ 订单簿索引 | 是 | 价格连续，前缀压缩效果好 |
| ❌ 订单ID索引 | 否 | 订单ID随机分布，无公共前缀，更适合 hash map |
| ✅ 内存数据库 | 是 | 低延迟，支持范围查询 |
| ✅ 实时系统 | 是 | 亚微秒级操作 |
| ❌ 磁盘存储 | 否 | 专为内存设计 |
| ❌ 随机键 | 否 | 无公共前缀时效率降低 |

---

## 参考资料

1. Leis, V., Kemper, A., & Neumann, T. (2013). **The Adaptive Radix Tree: ARTful Indexing for Main-Memory Databases**. VLDB.
2. [ART 论文 PDF](https://db.in.tum.de/~leis/papers/ART.pdf)
3. [本项目实现](../../include/exchange/core/collections/art/)

---

*最后更新: 2026-01*
