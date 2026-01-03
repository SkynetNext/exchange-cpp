# Fence 理解指南

## 参考来源

本文档基于 C++ 标准库官方文档，主要参考：
- [std::atomic_thread_fence](https://en.cppreference.com/w/cpp/atomic/atomic_thread_fence)
- [std::memory_order](https://en.cppreference.com/w/cpp/atomic/memory_order)

---

## 1. 核心定义

### Release Fence（释放栅栏）
```cpp
std::atomic_thread_fence(std::memory_order_release);
```

根据 [cppreference](https://en.cppreference.com/w/cpp/atomic/atomic_thread_fence)：
- **作用**：防止所有在 fence **之前**的读写操作重排到 fence **之后**的所有后续**存储操作**之后
- **范围**：影响本线程的所有内存操作（包括非原子和 relaxed 原子操作）
- **关键**：Release fence 比 release store 更强，它防止所有先前的读写操作重排到**所有后续存储操作**之后

### Acquire Fence（获取栅栏）
```cpp
std::atomic_thread_fence(std::memory_order_acquire);
```

根据 [cppreference](https://en.cppreference.com/w/cpp/atomic/atomic_thread_fence)：
- **作用**：防止所有在 fence **之后**的读写操作重排到 fence **之前**的所有先前**加载操作**之前
- **范围**：影响本线程的所有内存操作（包括非原子和 relaxed 原子操作）
- **关键**：Acquire fence 比 acquire load 更强，它防止所有后续的读写操作重排到**所有先前加载操作**之前

### 同步关系（Synchronizes-With）

根据 [cppreference](https://en.cppreference.com/w/cpp/atomic/atomic_thread_fence)，同步关系的建立需要满足以下条件：

#### Fence-Fence 同步
一个 release fence `FA` 在线程 A 中 synchronizes-with 一个 acquire fence `FB` 在线程 B 中，如果：
1. 存在一个原子对象 `M`
2. 存在一个原子写操作 `X`（任意内存序）在线程 A 中修改 `M`
3. `FA` 在线程 A 中 sequenced-before `X`
4. 存在一个原子读操作 `Y`（任意内存序）在线程 B 中
5. `Y` 读取到 `X` 写入的值（或 release sequence 中的值）
6. `Y` 在线程 B 中 sequenced-before `FB`

在这种情况下，所有在线程 A 中 `FA` 之前 sequenced-before 的非原子和 relaxed 原子存储操作，都会 happen-before 所有在线程 B 中 `FB` 之后从相同位置进行的非原子和 relaxed 原子加载操作。

#### 关键要点
- **必须通过同一个原子变量**的读写来建立同步关系
- **sequenced-before** 关系：在同一线程内的执行顺序
- **happens-before** 关系：跨线程的可见性保证
- 只有当读取到写入的值时，才建立同步关系

---

## 2. 两种实现方式

### 方式 1：`thread_fence` + relaxed（当前使用）

```cpp
// 写入端
void set(int64_t v) {
    std::atomic_thread_fence(std::memory_order_release);  // Release fence
    value_.store(v, std::memory_order_relaxed);            // Relaxed store
}

// 读取端
int64_t get() const {
    int64_t value = value_.load(std::memory_order_relaxed);  // Relaxed load
    std::atomic_thread_fence(std::memory_order_acquire);      // Acquire fence
    return value;
}
```

**语义**（基于 [cppreference](https://en.cppreference.com/w/cpp/atomic/atomic_thread_fence)）：
1. **Release fence**：防止所有在 fence 之前的读写操作重排到 fence 之后的所有后续存储操作之后
2. **Relaxed store**：写入 `value_`，无同步语义（不建立 synchronizes-with 关系）
3. **Relaxed load**：读取 `value_`，可能从缓存读取（可能读到旧值）
4. **Acquire fence**：防止所有在 fence 之后的读写操作重排到 fence 之前的所有先前加载操作之前
5. **同步关系**：只有当 relaxed load 读取到新值时，才建立 synchronizes-with 关系，此时所有在 release fence 之前 sequenced-before 的写操作都会 happen-before 所有在 acquire fence 之后进行的读操作

**特点**：
- ⚠️ relaxed load 可能读取到旧值（如 99），此时**不建立同步关系**
- ✅ 一旦读取到新值（100），建立同步关系，能看到生产者的所有写操作
- ✅ 性能可能更好（允许读取过时值）

### 方式 2：直接 release/acquire（更简单）

```cpp
// 写入端
void set(int64_t v) {
    value_.store(v, std::memory_order_release);  // Release store
}

// 读取端
int64_t get() const {
    return value_.load(std::memory_order_acquire);  // Acquire load
}
```

**语义**（基于 [cppreference](https://en.cppreference.com/w/cpp/atomic/memory_order)）：
1. **Release store**：防止所有在 store 之前的读写操作重排到 store 之后，且 store 本身建立同步语义（如果被 acquire load 读取到）
2. **Acquire load**：防止所有在 load 之后的读写操作重排到 load 之前，且 load 本身建立同步语义（如果读取到 release store 的值）
3. **同步关系**：当 acquire load 读取到 release store 写入的值时，建立 synchronizes-with 关系，此时所有在 release store 之前 sequenced-before 的写操作都会 happen-before 所有在 acquire load 之后进行的读操作

**特点**：
- ✅ acquire load 强制缓存一致性检查，通常能读取到最新值
- ✅ 更容易建立同步关系
- ✅ 代码更简单

### 对比

| 方面 | `thread_fence` + relaxed | 直接 release/acquire |
|------|-------------------------|---------------------|
| **同步建立** | ⚠️ 条件性（只有读取到新值时建立） | ✅ 更强（通常能读取到新值） |
| **影响范围** | ✅ 线程级别（所有内存操作） | ✅ 线程级别（所有内存操作） |
| **性能** | 可能略好（允许读取过时值） | 可能略差（强制最新值） |
| **复杂度** | 更复杂（两步操作） | 更简单（一步操作） |
| **使用场景** | Busy-spin、匹配 Java 语义 | 一般场景、更可靠 |

---

## 3. 实际应用

```cpp
// 典型的生产者-消费者模式
struct Event {
    int orderId;
    int price;
    int size;
};

Event event;

// 生产者
event.orderId = 12345;        // 内存写操作 1
event.price = 10000;          // 内存写操作 2
event.size = 100;             // 内存写操作 3
cursor_.set(seq);             // Release fence + relaxed store

// 消费者（busy-spin）
while (true) {
    int64_t seq = cursor_.get();  // Relaxed load + acquire fence
    if (seq >= targetSeq) {
        // ✅ 如果读取到新值，建立同步关系
        int orderId = event.orderId;  // 能看到 orderId = 12345
        int price = event.price;      // 能看到 price = 10000
        int size = event.size;        // 能看到 size = 100
        break;
    }
    // ⚠️ 如果读取到旧值，不建立同步关系，继续循环
}
```

**为什么方式 1 在 busy-spin 场景中有效？**
- 消费者会**不断循环**调用 `get()`
- 即使某次读到旧值（99），下一次循环很快就能读到新值（100）
- 一旦读到新值，就建立同步关系，能看到生产者的所有写操作
- 这种"延迟几个循环"的代价很小，但性能可能更好

---

## 4. 关键理解

### Fence 的作用（线程内）

根据 [cppreference](https://en.cppreference.com/w/cpp/atomic/atomic_thread_fence)：

- **Release fence**：防止所有在 fence 之前的读写操作重排到 fence 之后的所有后续存储操作之后
- **Acquire fence**：防止所有在 fence 之后的读写操作重排到 fence 之前的所有先前加载操作之前
- **范围**：影响本线程的所有内存操作（包括非原子和 relaxed 原子操作）
- **关键**：Fence 比原子操作更强，它影响所有后续/先前的操作，而不仅仅是特定的原子变量

### 同步关系的建立（线程间）

根据 [cppreference](https://en.cppreference.com/w/cpp/atomic/atomic_thread_fence)：

- **必须条件**：通过同一个原子变量的读写，且满足 sequenced-before 和 happens-before 关系
- **方式 1**：release fence → relaxed store，relaxed load → acquire fence（Fence-Fence 同步）
- **方式 2**：release store，acquire load（Atomic-Atomic 同步）
- **关键区别**：
  - 方式 1 的 relaxed load 可能读到旧值，此时不建立同步关系
  - 方式 2 的 acquire load 更容易读取到最新值，更容易建立同步关系
- **可见性保证**：一旦建立 synchronizes-with 关系，所有在 release 操作之前 sequenced-before 的写操作都会 happen-before 所有在 acquire 操作之后进行的读操作

### 原子性
- **必须用 `std::atomic`**：保证读写的原子性（不会读到部分写入的值）
- **Fence 不提供原子性**：只提供同步语义

---

## 5. 进阶：为什么需要 `seq_cst` (StoreLoad 屏障)？

在 Disruptor 的 `Sequence::setVolatile` 中，我们看到了这种写法：
```cpp
void setVolatile(int64_t v) {
    std::atomic_thread_fence(std::memory_order_release);
    value_.store(v, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst); // ← 全局全屏障
}
```

### 为什么普通的 Release/Acquire 不够？

先看典型代码：
```cpp
// 线程 A
data = 42;                                      // Store 1（普通变量写入）
std::atomic_thread_fence(std::memory_order_release);  // Release fence
flag.store(1, std::memory_order_relaxed);       // Store 2（原子变量写入）
int x = other_flag.load(std::memory_order_relaxed);   // Load 1（原子变量读取）
std::atomic_thread_fence(std::memory_order_acquire);  // Acquire fence
int y = other_data;                             // Load 2（普通变量读取）
```

**Release fence 的作用**（基于 [cppreference](https://en.cppreference.com/w/cpp/atomic/atomic_thread_fence)）：
- 防止所有在 fence **之前**的读写操作（包括非原子和 relaxed 原子）重排到 fence **之后**的所有后续**存储操作**之后
- ✅ 保证：`data = 42` 不会重排到 `flag.store(1)` 之后
- ⚠️ 但 Release fence **不能防止** fence 之后的 Store 和 Load 之间的重排

**Acquire fence 的作用**（基于 [cppreference](https://en.cppreference.com/w/cpp/atomic/atomic_thread_fence)）：
- 防止所有在 fence **之后**的读写操作（包括非原子和 relaxed 原子）重排到 fence **之前**的所有先前**加载操作**之前
- ✅ 保证：`int y = other_data` 不会重排到 `other_flag.load()` 之前
- ⚠️ 但 Acquire fence **不能防止** fence 之前的 Store 和 Load 之间的重排

**漏洞**：
- ❌ Release fence **管不了** fence 之后的 `flag.store(1)`（Store）和 `other_flag.load()`（Load）之间的重排
- ❌ Acquire fence **管不了** fence 之前的 `flag.store(1)`（Store）和 `other_flag.load()`（Load）之间的重排
- **结果**：Store 2 和 Load 1 之间**没有任何屏障**，可以发生 **StoreLoad 重排**

**注意**：Store/Load 不仅指原子变量操作，也包括普通变量的读写。根据 [cppreference](https://en.cppreference.com/w/cpp/atomic/memory_order)，只有 `memory_order_seq_cst` 能防止 StoreLoad 重排。

### 场景：Dekker 算法（经典的 StoreLoad 重排问题）
```cpp
// 线程 A
std::atomic_thread_fence(std::memory_order_release);
my_flag.store(1, std::memory_order_relaxed);         // Store（内存写入）
int x = your_flag.load(std::memory_order_relaxed);   // Load（内存读取）
std::atomic_thread_fence(std::memory_order_acquire);

// 线程 B（对称操作）
std::atomic_thread_fence(std::memory_order_release);
your_flag.store(1, std::memory_order_relaxed);       // Store（内存写入）
int y = my_flag.load(std::memory_order_relaxed);     // Load（内存读取）
std::atomic_thread_fence(std::memory_order_acquire);
```

在硬件层面（尤其是 x86 的 Store Buffer），CPU 可以让 Load（内存读取）先于 Store（内存写入）执行：
- 线程 A：`Load(your_flag)` 先于 `Store(my_flag)` 生效 → 读到 0
- 线程 B：`Load(my_flag)` 先于 `Store(your_flag)` 生效 → 读到 0
- **后果**：两个线程都认为对方没有设置标志，同时进入临界区！

**关键**：这里的 Store/Load 是指**内存层面的写入和读取操作**，不仅限于原子变量。

### 解决方案：`seq_cst` 屏障

根据 [cppreference](https://en.cppreference.com/w/cpp/atomic/memory_order)：
- **`memory_order_seq_cst`** 是顺序一致性（Sequentially Consistent）内存序
- 它提供 acquire-release 语义，并且所有 seq_cst 操作形成一个单一的全局总顺序
- **`std::atomic_thread_fence(std::memory_order_seq_cst)`** 是一个顺序一致性的 acquire-release fence
- **StoreLoad 屏障**：确保所有在 fence 之前的内存写操作对全局可见，且所有在 fence 之后的内存读操作必须在此之后执行
- 这是开销最大的屏障，因为它强制同步了**本线程**与内存系统，并参与全局顺序一致性

**使用 `seq_cst` 修复 Dekker 算法**：
```cpp
// 线程 A
std::atomic_thread_fence(std::memory_order_release);
my_flag.store(1, std::memory_order_relaxed);         // Store（内存写入）
std::atomic_thread_fence(std::memory_order_seq_cst);  // ← StoreLoad 屏障
int x = your_flag.load(std::memory_order_relaxed);   // Load（内存读取）
std::atomic_thread_fence(std::memory_order_acquire);

// 线程 B（对称操作）
std::atomic_thread_fence(std::memory_order_release);
your_flag.store(1, std::memory_order_relaxed);       // Store（内存写入）
std::atomic_thread_fence(std::memory_order_seq_cst);  // ← StoreLoad 屏障
int y = my_flag.load(std::memory_order_relaxed);     // Load（内存读取）
std::atomic_thread_fence(std::memory_order_acquire);
```

**关键改进**：
- `seq_cst` 屏障插入在 Store 和 Load 之间，防止 StoreLoad 重排
- 确保 `my_flag.store(1)` 对全局可见后，才执行 `your_flag.load()`
- 两个线程不能同时读到 0，至少有一个线程会看到对方已设置标志

**替代方案：使用 `seq_cst` 原子操作**
```cpp
// 线程 A
my_flag.store(1, std::memory_order_seq_cst);  // ← seq_cst store（自带 StoreLoad 屏障）
int x = your_flag.load(std::memory_order_seq_cst);  // ← seq_cst load

// 线程 B
your_flag.store(1, std::memory_order_seq_cst);  // ← seq_cst store（自带 StoreLoad 屏障）
int y = my_flag.load(std::memory_order_seq_cst);  // ← seq_cst load
```

这种方式更简洁，`seq_cst` 原子操作自带 StoreLoad 屏障语义。

### 实际应用：SingleProducerSequencer 中的 `setVolatile`

在 `SingleProducerSequencer::next()` 中，`setVolatile` 用于 wrap point 检查：

```cpp
int64_t nextValue = this->nextValue_;        // 当前已分配的最后一个 sequence
int64_t nextSequence = nextValue + n;        // 要分配的下一个 sequence
int64_t wrapPoint = nextSequence - this->bufferSize_;  // 计算 wrap point

if (wrapPoint > cachedGatingSequence || cachedGatingSequence > nextValue) {
    cursor_.setVolatile(nextValue);  // ← StoreLoad 屏障
    int64_t minSequence = minimumSequence(nextValue);  // Load 消费者的 sequence
    while (wrapPoint > minSequence) {
        // 等待消费者消费，避免覆盖未消费的数据
        std::this_thread::yield();
        minSequence = minimumSequence(nextValue);  // 重新读取
    }
}
```

**关键变量说明**：

| 变量 | 含义 | 说明 |
|------|------|------|
| `cursor` | 已发布给消费者的最新 sequence | 消费者通过检查 `cursor` 判断是否有新事件 |
| `wrapPoint` | 回环检查点 | `wrapPoint = nextSequence - bufferSize`，表示要写入 `nextSequence` 时会覆盖的 sequence 位置 |
| `minSequence` | 所有消费者的最小 sequence | 表示消费者已处理到的最小位置 |

**为什么需要 StoreLoad 屏障？**

存在一个**循环依赖**：
- 生产者需要读取消费者的 sequence 来判断容量（`wrapPoint > minSequence`）
- 但消费者需要看到生产者的 cursor 才能更新 sequence
- 因此，**必须先发布 cursor，消费者才能更新 sequence，然后生产者才能读取到最新的 sequence**

**性能优化，而非正确性必需**

关键点：代码中有 `while` 循环会不断重新读取 `minSequence`，因此即使第一次读取到过时的值，循环中也会不断重新读取，直到读取到正确的值。**理论上不会产生覆盖**，因为循环会一直等待直到条件满足。

**没有 StoreLoad 屏障（性能较差）**：

```
生产者线程执行顺序：
┌─────────────────────────────────────────┐
│ T1: Load 读取消费者sequence → 45 (旧值)   │ ← 此时cursor还是99，Load先执行
│ T2: Store 写入 cursor=100                │ ← Store后执行，但Load已读取到旧值
│ T3: 判断：wrapPoint(51) > minSequence(45) │
│     51 > 45 为 true → 进入while循环等待   │
│ T4: 循环中不断重新读取 minSequence         │
│     第一次、第二次...可能还是45（过时值）  │ ← ⚠️ 需要多次循环
│ T5: 最终读取到105，退出循环               │
└─────────────────────────────────────────┘
```

**问题**：由于第一次读取到过时的值，可能需要多次循环才能读取到正确的值，导致不必要的等待，影响性能。

**有 StoreLoad 屏障（性能更好）**：

```
生产者线程执行顺序：
┌─────────────────────────────────────────┐
│ T1: Store 写入 cursor=100               │ ← 先发布cursor
│ T2: StoreLoad 屏障 → 确保Store可见        │
│ T3: Load 读取消费者sequence → 105 (新值) │ ← 更可能读取到最新值
│ T4: 判断：wrapPoint(51) > minSequence(105)│
│     51 > 105 为 false → 退出while循环    │ ← ✅ 可能一次就成功
└─────────────────────────────────────────┘
```

**优势**：通过强制 Store → Load 顺序，确保第一次读取就更可能读取到最新的 sequence，减少循环次数，提高性能。

**总结**：

- `setVolatile` 主要是**性能优化**，不是正确性必需
- 即使没有它，`while` 循环也会保证正确性（不会覆盖）
- 但有了它，可以减少不必要的等待循环，提高吞吐量

**注意**：`cursor_.setVolatile(nextValue)` 是临时设置，用于让消费者看到当前进度。正式的 `cursor` 更新是通过 `publish(sequence)` 完成的。

---

## 6. CAS（Compare-And-Swap）

### 解决的问题
CAS 将"读-判断-写"变成**不可分割的原子操作**，避免多线程竞争时的更新丢失。

### 典型用法

```cpp
// compareAndSet 返回值
// - true：成功（value 等于 expected，已更新为 desired）
// - false：失败（value 不等于 expected，expected 被更新为实际值）

virtual bool compareAndSet(int64_t expected, int64_t desired) {
    return value_.compare_exchange_weak(expected, desired,
                                        std::memory_order_acq_rel,
                                        std::memory_order_acquire);
}
```

### CAS 循环模式

```cpp
// MultiProducerSequencer::tryNext()
int64_t current;
int64_t next;

do {
    current = cursor_.get();      // 1. 读取当前值
    next = current + n;           // 2. 计算新值
    
    // 3. 检查容量...
    
} while (!cursor_.compareAndSet(current, next));
//       ↑ 成功（返回 true）：!true = false，退出循环
//       ↑ 失败（返回 false）：!false = true，继续循环
//         失败时 current 被更新为实际值，用新值重试

return next;  // 4. 成功返回
```

### 执行示例

```cpp
// 初始：cursor = 100，线程 A 和 B 同时调用 tryNext(1)

// 线程 A
current = 100;
next = 101;
compareAndSet(100, 101);  // ✅ 成功，cursor = 101，退出循环

// 线程 B（几乎同时）
current = 100;            // 读到旧值
next = 101;
compareAndSet(100, 101);  // ❌ 失败，current 被更新为 101
// 继续循环
current = 101;            // 使用更新后的值
next = 102;
compareAndSet(101, 102);  // ✅ 成功，cursor = 102，退出循环

// 最终：cursor = 102 ✅ 正确
```

### 为什么用 `acq_rel`？

```cpp
compareAndSet(expected, desired) {
    return value_.compare_exchange_weak(
        expected, desired,
        std::memory_order_acq_rel,   // 成功时：既读又写，需要双向同步
        std::memory_order_acquire    // 失败时：只读，只需 acquire
    );
}
```

- **成功时**：需要 `acq_rel`
  - **Release**：本线程之前的写操作对其他线程可见
  - **Acquire**：能看到其他线程之前的写操作
- **失败时**：只需要 `acquire`（只读取，没有写入）

---

## 7. `acq_rel` 内存序

### 核心定义

根据 [cppreference](https://en.cppreference.com/w/cpp/atomic/memory_order)：
- **`memory_order_acq_rel`** 用于读-改-写（RMW）操作，如 `fetch_add`、`compare_exchange` 等
- 它**既是 acquire 操作也是 release 操作**，因为 RMW 操作既读又写，需要双向同步
- **不仅仅是简单的组合**：它在单个原子操作中同时提供获取和释放语义

### 为什么用于 RMW 操作？

RMW 操作（Read-Modify-Write）的特点：
- **既读又写**：原子地读取当前值，修改后写回
- **需要双向同步**：
  - **Release 语义**：本线程之前的写操作对其他线程可见（写入端）
  - **Acquire 语义**：能看到其他线程之前的写操作（读取端）

### 与单独的 release/acquire 的区别

| 特性 | 单独的 release/acquire | `acq_rel` RMW 操作 |
|------|----------------------|-------------------|
| **读取保证** | ⚠️ 条件性（需要建立同步关系） | ✅ 原子性保证（总是读取到当前值） |
| **可见性** | ⚠️ 需要配对使用 | ✅ RMW 操作本身保证原子性和可见性 |
| **适用场景** | 纯读或纯写操作 | RMW 操作（fetch_add、CAS 等） |

**关键区别**：
- **`acq_rel` RMW 操作**：通过原子性保证总是读取到当前值，即使多个线程同时执行，每个线程都能获得唯一且正确的值
- **配对的 release/acquire**：不保证一定读到新值，只有当 acquire load 读取到 release store 写入的值时，才建立同步关系；如果读取到旧值，不建立同步关系，可能看不到其他线程的写操作

### 实际使用场景：MultiProducerSequencer

```cpp
// MultiProducerSequencer::next()
int64_t current = this->cursor_.getAndAdd(n);  // ← acq_rel RMW
int64_t nextSequence = current + n;
// ...
```

**实现**：
```cpp
virtual int64_t getAndAdd(int64_t increment) {
    return value_.fetch_add(increment, std::memory_order_acq_rel);
}
```

**为什么使用 `acq_rel`？**

1. **原子性保证**：
   - `fetch_add` 是原子操作，保证每个线程都能获得唯一的 sequence 值
   - 多个生产者同时调用时，不会丢失更新

2. **可见性保证**：
   - **Release 语义**：确保本线程之前的写操作对其他线程可见
   - **Acquire 语义**：确保能看到其他线程之前的写操作
   - 通过缓存一致性协议（如 MESI）保证可见性

3. **多核环境**：
   - 在多核处理器中，缓存一致性协议确保所有核心看到一致的数据
   - 数据可能通过缓存之间直接同步，也可能需要刷到主内存（取决于架构）

**执行示例**：
```cpp
// 初始：cursor = 100，线程 A 和 B 同时调用 next(1)

// 线程 A（CPU Core A）
int64_t seq1 = cursor_.getAndAdd(1);  // 原子读取 100，写入 101，返回 100
// ✅ 保证：seq1 = 100，cursor = 101

// 线程 B（CPU Core B，几乎同时）
int64_t seq2 = cursor_.getAndAdd(1);  // 原子读取 101，写入 102，返回 101
// ✅ 保证：seq2 = 101，cursor = 102

// 最终：两个线程都获得唯一值，cursor = 102 ✅ 正确
```

### 关键要点

- **`acq_rel` 用于 RMW 操作**：因为 RMW 操作既读又写，需要双向同步
- **原子性 + 可见性**：`acq_rel` RMW 操作同时保证原子性和可见性
- **缓存一致性协议**：通过缓存一致性协议（如 MESI）保证多核环境下的可见性
- **不需要配对**：单个 `acq_rel` RMW 操作就能保证双向同步，比单独的 release/acquire 更强

**参考**：[cppreference - memory_order](https://en.cppreference.com/w/cpp/atomic/memory_order)

---

## 8. 记忆要点

### Memory Fence（内存栅栏）

| 概念 | 作用 | 范围 |
|------|------|------|
| **Release fence** | 防止所有在 fence 之前的读写操作重排到 fence 之后的所有后续存储操作之后 | 本线程的所有内存操作（包括非原子和 relaxed 原子） |
| **Acquire fence** | 防止所有在 fence 之后的读写操作重排到 fence 之前的所有先前加载操作之前 | 本线程的所有内存操作（包括非原子和 relaxed 原子） |
| **Acq_Rel fence** | 既是 release fence 也是 acquire fence，同时提供双向同步 | 本线程的所有内存操作（包括非原子和 relaxed 原子） |
| **Seq_Cst fence** | 顺序一致性屏障：防止 StoreLoad 重排，参与全局总顺序 | 全局顺序一致性 |

### Atomic 操作（原子操作）

| 概念 | 作用 | 范围 |
|------|------|------|
| **Release store** | 防止所有在 store 之前的读写操作重排到 store 之后，且 store 本身建立同步语义 | 本线程的所有内存操作 + 特定原子变量 |
| **Acquire load** | 防止所有在 load 之后的读写操作重排到 load 之前，且 load 本身建立同步语义 | 本线程的所有内存操作 + 特定原子变量 |
| **Acq_Rel RMW** | 用于读-改-写操作（fetch_add、CAS 等）：既是 acquire 也是 release，需要双向同步，保证原子性和可见性 | 本线程的所有内存操作 + 特定原子变量 |
| **Seq_Cst store/load** | 顺序一致性操作：提供 acquire-release 语义，防止 StoreLoad 重排，参与全局总顺序 | 本线程的所有内存操作 + 特定原子变量 + 全局顺序一致性 |
| **CAS** | 原子化"读-判断-写"，失败时更新 expected 并重试 | 特定原子变量 |

### 同步关系

| 概念 | 作用 | 范围 |
|------|------|------|
| **同步关系** | 必须通过同一个原子变量的读写建立，满足 sequenced-before 和 happens-before 关系 | 跨线程 |
| **Fence-Fence** | 条件性同步（只有读取到新值时建立） | 适合 busy-spin |
| **Atomic-Atomic** | 更强同步（通常能读取到新值） | 一般场景 |

**参考**：[cppreference - atomic_thread_fence](https://en.cppreference.com/w/cpp/atomic/atomic_thread_fence) | [cppreference - memory_order](https://en.cppreference.com/w/cpp/atomic/memory_order)
