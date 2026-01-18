# MPSC Disruptor 工作原理

## 整体架构

```
多生产者（Gateway IO 线程）                        单消费者（撮合线程）
    ┌─────┐  ┌─────┐  ┌─────┐                      ┌─────────────┐
    │ P1  │  │ P2  │  │ P3  │                      │  Consumer   │
    └──┬──┘  └──┬──┘  └──┬──┘                      └──────┬──────┘
       │       │       │                                  │
       ▼       ▼       ▼                                  │
    ┌──────────────────────────────────────────────────────────────┐
    │                      MultiProducerSequencer                   │
    │  ┌────────────────────────────────────────────────────────┐  │
    │  │ cursor (atomic<int64>)       当前已分配的最大序号       │  │
    │  │ gatingSequence               消费者已处理的最小序号     │  │
    │  │ availableBuffer[bufferSize]  每槽位发布状态标记         │  │
    │  └────────────────────────────────────────────────────────┘  │
    └──────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
    ┌──────────────────────────────────────────────────────────────┐
    │                         RingBuffer                            │
    │  ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐           │
    │  │  0  │  1  │  2  │  3  │  4  │  5  │  6  │  7  │           │
    │  └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘           │
    │    ▲                   ▲                       ▲              │
    │    │                   │                       │              │
    │  已消费              正在写入                待消费            │
    └──────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
    ┌──────────────────────────────────────────────────────────────┐
    │                    ProcessingSequenceBarrier                  │
    │  ┌────────────────────────────────────────────────────────┐  │
    │  │ 消费者通过 barrier.waitFor(seq) 等待数据可用           │  │
    │  │ → WaitStrategy 等待 cursor >= seq                      │  │
    │  │ → getHighestPublishedSequence() 检查连续可用范围        │  │
    │  └────────────────────────────────────────────────────────┘  │
    └──────────────────────────────────────────────────────────────┘
```

---

## 核心数据结构

### 1. cursor（生产者序号）

```cpp
class Sequence {
    alignas(256) std::atomic<int64_t> value_;  // 带 padding 防伪共享
    
    int64_t getAndAdd(int64_t n) {
        return value_.fetch_add(n, std::memory_order_acq_rel);  // FAA 原子加
    }
};
```

- **作用**：记录已分配的最大序号
- **操作**：`fetch_add` (FAA)，**非 CAS**，无条件成功

### 2. availableBuffer（发布状态）

```cpp
std::vector<std::atomic<int>> availableBuffer_;  // 大小 = bufferSize

// 标记某序号已发布
void setAvailable(int64_t sequence) {
    int index = sequence & indexMask_;           // 槽位
    int flag = sequence >> indexShift_;          // 圈数（区分新旧数据）
    availableBuffer_[index].store(flag, std::memory_order_release);
}

// 检查某序号是否已发布
bool isAvailable(int64_t sequence) {
    int index = sequence & indexMask_;
    int flag = sequence >> indexShift_;
    return availableBuffer_[index].load(std::memory_order_acquire) == flag;
}
```

- **作用**：解决多生产者乱序写入问题
- **原理**：用圈数 (flag) 区分新旧数据，避免消费者读到未完成的写入

### 3. gatingSequence（消费者序号）

```cpp
Sequence consumerSequence;  // 消费者持有，记录已处理位置
```

- **作用**：生产者检查此值，防止覆盖未消费的数据

---

## 生产者流程

### next() - 申请序号

```
P1, P2, P3 同时调用 next(1)
         │
         ▼
┌─────────────────────────────────────┐
│ cursor.getAndAdd(1)                 │  ← FAA 原子操作
│                                     │
│  初始 cursor = 10                   │
│  P1 获得 11 (cursor 变为 11)         │
│  P2 获得 12 (cursor 变为 12)         │
│  P3 获得 13 (cursor 变为 13)         │
│                                     │
│  三个操作原子化，无竞争，无重试      │
└─────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────┐
│ 检查是否会覆盖未消费数据             │
│                                     │
│  wrapPoint = nextSeq - bufferSize   │
│  if wrapPoint > gatingSequence:     │
│      yield() 等待消费者追上          │
└─────────────────────────────────────┘
         │
         ▼
    返回分配的序号
```

**关键代码**：
```cpp
int64_t next(int n) {
    int64_t current = this->cursor_.getAndAdd(n);  // FAA，无条件成功
    int64_t nextSequence = current + n;
    int64_t wrapPoint = nextSequence - bufferSize_;
    
    // 防止覆盖：等待消费者
    while (wrapPoint > minimumSequence(current)) {
        std::this_thread::yield();
    }
    return nextSequence;
}
```

### publish() - 发布数据

```
P1 获得 seq=11，写入数据后调用 publish(11)
         │
         ▼
┌─────────────────────────────────────┐
│ setAvailable(11)                    │
│                                     │
│  index = 11 & 7 = 3                 │  (假设 bufferSize=8)
│  flag = 11 >> 3 = 1                 │
│                                     │
│  availableBuffer_[3] = 1            │  ← release store
└─────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────┐
│ signalAllWhenBlocking()             │  ← 唤醒等待的消费者
└─────────────────────────────────────┘
```

---

## 消费者流程

### waitFor() - 等待可消费序号

```
Consumer 调用 barrier.waitFor(12)
         │
         ▼
┌─────────────────────────────────────────────────────────┐
│ Step 1: WaitStrategy 等待 cursor >= 12                  │
│                                                          │
│  while (cursor.get() < 12) {                            │
│      // BusySpin / Yield / Block 等策略                  │
│  }                                                       │
│  返回 availableSequence = cursor.get()  (假设 = 15)     │
└─────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────┐
│ Step 2: getHighestPublishedSequence(12, 15)             │
│                                                          │
│  // 检查 12~15 中连续已发布的最大序号                     │
│  for seq = 12 to 15:                                     │
│      if (!isAvailable(seq)):                            │
│          return seq - 1   ← 发现空洞，返回前一个         │
│  return 15                ← 全部连续可用                 │
│                                                          │
│  假设 P2(seq=12) 还没 publish，返回 11                   │
└─────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────┐
│ Step 3: 消费者处理 [lastSeq+1, returnedSeq] 范围的数据   │
│                                                          │
│  for seq = lastProcessed+1 to highestAvailable:          │
│      process(ringBuffer[seq])                           │
│  consumerSequence.set(highestAvailable)                  │
└─────────────────────────────────────────────────────────┘
```

---

## MPSC 的空洞问题与解决

### 问题场景

```
时间 ──────────────────────────────────────────────────────►

cursor:  10 → 11 → 12 → 13
              │     │     │
              ▼     ▼     ▼
           P1    P2    P3
           ↓     ↓     ↓
        publish  ✗   publish     ← P2 还没写完
           ↓         ↓
availableBuffer:
  [11] ✓   [12] ✗   [13] ✓       ← 出现空洞！
```

### 解决：getHighestPublishedSequence

```cpp
int64_t getHighestPublishedSequence(int64_t lowerBound, int64_t availableSequence) {
    for (int64_t seq = lowerBound; seq <= availableSequence; ++seq) {
        if (!isAvailable(seq)) {
            return seq - 1;  // 遇到空洞，返回前一个连续可用的
        }
    }
    return availableSequence;
}
```

消费者只消费 **连续可用** 的序号，跳过空洞后面的数据。

---

## 完整时序图

```
时间 ──────────────────────────────────────────────────────────────────────────►

Producer P1          Producer P2          Consumer               状态
    │                    │                    │
    │                    │                    │               cursor=10
    │                    │                    │               consumerSeq=8
    │                    │                    │
    ├─ next(1) ──────────┼────────────────────┤
    │  FAA: 10→11        │                    │               cursor=11
    │  返回 11            │                    │
    │                    │                    │
    │                    ├─ next(1) ──────────┤
    │                    │  FAA: 11→12        │               cursor=12
    │                    │  返回 12            │
    │                    │                    │
    ├─ 写入 RingBuffer[11 & 7] ───────────────┤
    ├─ publish(11) ──────┼────────────────────┤
    │  available[3]=1    │                    │               available[3]=1
    │  signal            │                    │
    │                    │                    │
    │                    │                    ├─ waitFor(9)
    │                    │                    │  cursor=12 ≥ 9 ✓
    │                    │                    │  getHighestPublished(9,12)
    │                    │                    │    seq=9 ✓, 10 ✓, 11 ✓, 12 ✗
    │                    │                    │  返回 11
    │                    │                    │
    │                    │                    ├─ 消费 9,10,11
    │                    │                    ├─ consumerSeq=11
    │                    │                    │               consumerSeq=11
    │                    │                    │
    │                    ├─ 写入 RingBuffer[12 & 7]
    │                    ├─ publish(12) ──────┤
    │                    │  available[4]=1    │               available[4]=1
    │                    │                    │
    │                    │                    ├─ waitFor(12)
    │                    │                    │  getHighestPublished(12,12)=12
    │                    │                    ├─ 消费 12
    │                    │                    ├─ consumerSeq=12
```

---

## SPSC vs MPSC 对比

| 特性 | SPSC | MPSC |
|------|------|------|
| **序号分配** | 直接递增 | FAA 原子加 |
| **availableBuffer** | 不需要 | 需要（解决空洞） |
| **publish** | 直接 set cursor | 标记 availableBuffer |
| **消费者检查** | 直接读 cursor | getHighestPublishedSequence |
| **性能** | 更快（无原子操作） | 略慢（FAA + 额外检查） |
| **适用场景** | 单线程生产 | 多线程生产 |

---

## exchange-cpp 中的使用

```cpp
// ExchangeCore.cpp
using DisruptorT = disruptor::dsl::Disruptor<
    common::cmd::OrderCommand,
    disruptor::dsl::ProducerType::MULTI,  // ← MPSC
    WaitStrategyT,
    common::utils::AffinityThreadFactory
>;

// 多个 Gateway IO 线程作为生产者
// 单个撮合线程作为消费者
```

---

## 参考

- Java 原版: `com.lmax.disruptor.MultiProducerSequencer`
- C++ 移植: `disruptor-cpp/include/disruptor/MultiProducerSequencer.h`
- LMAX 论文: https://lmax-exchange.github.io/disruptor/disruptor.html
