# Fence 理解指南

## 1. 核心定义

### Release Fence（释放栅栏）
```cpp
std::atomic_thread_fence(std::memory_order_release);
```
- **作用**：本线程之前的所有内存写操作（store）不能重排到 fence 之后
- **范围**：影响本线程的所有内存写操作（不限定变量）

### Acquire Fence（获取栅栏）
```cpp
std::atomic_thread_fence(std::memory_order_acquire);
```
- **作用**：本线程后续的所有内存操作（load/store）不能重排到 fence 之前
- **范围**：影响本线程的所有内存操作（不限定变量）

### 同步关系（Synchronizes-With）
- **条件**：必须通过**同一个原子变量**的读写来建立
- **建立方式**：
  1. 线程 A：release fence → relaxed store 到变量 X
  2. 线程 B：relaxed load 从变量 X → acquire fence
  3. **关键**：只有当线程 B 读取到线程 A 写入的值时，才建立同步关系

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

**语义**：
1. **Release fence**：本线程之前的所有内存写操作不能重排到 fence 之后
2. **Relaxed store**：写入 `value_`，无同步语义
3. **Relaxed load**：读取 `value_`，可能从缓存读取（可能读到旧值）
4. **Acquire fence**：本线程后续的所有内存操作不能重排到 fence 之前
5. **同步关系**：只有当 relaxed load 读取到新值时，才建立 synchronizes-with

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

**语义**：
1. **Release store**：本线程之前的所有内存写操作不能重排到 store 之后，且 store 本身带同步语义
2. **Acquire load**：本线程后续的所有内存操作不能重排到 load 之前，且 load 本身带同步语义
3. **同步关系**：acquire load 通常能读取到最新值，更容易建立 synchronizes-with

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
- **Release fence**：防止本线程之前的写操作重排到 fence 之后
- **Acquire fence**：防止本线程后续的操作重排到 fence 之前
- **范围**：影响本线程的所有内存操作（不限定变量）

### 同步关系的建立（线程间）
- **必须条件**：通过同一个原子变量的读写
- **方式 1**：release fence → relaxed store，relaxed load → acquire fence
- **方式 2**：release store，acquire load
- **关键区别**：方式 1 的 relaxed load 可能读到旧值，不建立同步关系

### 原子性
- **必须用 `std::atomic`**：保证读写的原子性（不会读到部分写入的值）
- **Fence 不提供原子性**：只提供同步语义

---

## 5. CAS（Compare-And-Swap）

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

## 6. 记忆要点

| 概念 | 作用 | 范围 |
|------|------|------|
| **Release fence** | 本线程之前的所有内存写操作不能重排到 fence 之后 | 本线程的所有内存写操作 |
| **Acquire fence** | 本线程后续的所有内存操作不能重排到 fence 之前 | 本线程的所有内存操作 |
| **同步关系** | 必须通过同一个原子变量的读写建立 | 跨线程 |
| **方式 1** | 条件性同步（只有读取到新值时建立） | 适合 busy-spin |
| **方式 2** | 更强同步（通常能读取到新值） | 一般场景 |
| **CAS** | 原子化"读-判断-写"，失败时更新 expected 并重试 | 无锁并发 |
