# Spin-Yield 等待模式

## 概述

Spin-Yield 是多线程编程中的**经典等待策略**，特别适用于高性能、低延迟场景。它结合了自旋（spin）和让出（yield）两种机制，在延迟和 CPU 使用之间取得平衡。

## 基本模式

```cpp
int64_t spin = spinLimit;
while (condition_not_met && spin > 0) {
    if (spin < yieldLimit) {
        std::this_thread::yield();  // 让出 CPU
    }
    // 否则继续自旋（占用 CPU）
    spin--;
}
```

## 三个阶段

### 1. Spin（自旋）
- **行为**：占用 CPU，循环检查条件
- **适用**：条件很快会满足（微秒级）
- **优点**：延迟最低（纳秒级响应）
- **缺点**：占用 CPU 资源

### 2. Yield（让出）
- **行为**：让出当前时间片，但线程仍在就绪队列
- **适用**：条件会在较短时间内满足（毫秒级）
- **优点**：给其他线程运行机会，延迟仍然较低
- **缺点**：有上下文切换开销（微秒级）

### 3. Block（阻塞）
- **行为**：完全让出 CPU，进入等待状态
- **适用**：条件可能需要较长时间才满足
- **优点**：不占用 CPU
- **缺点**：延迟最高（毫秒到秒级）

## 为什么是经典模型？

### 1. 性能与延迟的平衡

| 策略 | 延迟 | CPU 使用 | 适用场景 |
|------|------|----------|----------|
| **Spin** | 最低（ns级） | 最高（100%） | 条件很快满足 |
| **Spin-Yield** | 低（µs级） | 中等 | 条件较快满足 |
| **Block** | 高（ms级） | 最低（0%） | 条件较慢满足 |

### 2. 自适应等待

Spin-Yield 根据等待时间自动调整策略：
- **短期等待**：自旋（低延迟）
- **中期等待**：让出（平衡延迟和 CPU）
- **长期等待**：阻塞（节省 CPU）

### 3. 广泛应用

- **Disruptor**：高性能消息传递框架
- **Linux 内核**：自旋锁实现
- **数据库系统**：事务等待
- **游戏引擎**：帧同步等待

## 实现要点

### 关键参数

```cpp
constexpr int32_t SPIN_LIMIT = 5000;      // 总自旋次数
constexpr int32_t YIELD_LIMIT = 2500;     // 开始 yield 的阈值
```

### 参数选择

- **SPIN_LIMIT**：
  - 太小：可能过早让出，增加延迟
  - 太大：浪费 CPU，影响其他线程
  - **经验值**：1000-10000（取决于 CPU 频率和预期等待时间）

- **YIELD_LIMIT**：
  - 通常设为 `SPIN_LIMIT / 2`
  - 前半段自旋，后半段让出

### 时间计算（3GHz CPU）

- 每次循环：~1-2ns
- 1000 次循环：~1-2µs
- 5000 次循环：~5-10µs

## 与其他策略对比

### Spin-Yield vs Busy Spin

```cpp
// Busy Spin：一直自旋，不让出
while (condition_not_met) {
    // 占用 CPU 100%
}
```

**适用场景**：
- 条件**极快**满足（< 1µs）
- CPU 核心数 > 线程数
- 延迟要求极高

### Spin-Yield vs Blocking

```cpp
// Blocking：直接阻塞
std::unique_lock<std::mutex> lock(mutex);
condition_variable.wait(lock, [&] { return condition_met; });
```

**适用场景**：
- 条件**较慢**满足（> 1ms）
- CPU 资源紧张
- 延迟要求不高

## 在 Disruptor 中的应用

### WaitSpinningHelper 实现

```cpp
int64_t spin = spinLimit_;
while (availableSequence < seq && spin > 0) {
    if (spin < yieldLimit_ && spin > 1) {
        std::this_thread::yield();  // 让出阶段
    } else if (spin <= 1 && block_) {
        condition_variable.wait(lock);  // 阻塞阶段
    }
    // 否则继续自旋
    spin--;
}
```

### 为什么 Disruptor 使用 Spin-Yield？

1. **低延迟要求**：金融交易系统需要微秒级延迟
2. **事件驱动**：事件通常很快到达（微秒级）
3. **CPU 充足**：专用服务器，CPU 核心数 > 线程数

## 最佳实践

### 1. 根据场景调整参数

```cpp
// 低延迟场景（HFT）
constexpr int32_t SPIN_LIMIT = 10000;
constexpr int32_t YIELD_LIMIT = 5000;

// 平衡场景
constexpr int32_t SPIN_LIMIT = 5000;
constexpr int32_t YIELD_LIMIT = 2500;

// CPU 敏感场景
constexpr int32_t SPIN_LIMIT = 1000;
constexpr int32_t YIELD_LIMIT = 500;
```

### 2. 使用 CPU Pause 指令

```cpp
while (condition_not_met && spin > 0) {
    #if defined(__x86_64__)
        _mm_pause();  // 减少 CPU 功耗和缓存竞争
    #endif
    spin--;
}
```

### 3. 监控和调优

- 监控等待时间分布
- 根据实际场景调整参数
- 避免过度自旋（浪费 CPU）

## 总结

Spin-Yield 是**多线程编程中的经典等待模式**，特别适用于：
- ✅ 高性能系统（HFT、游戏引擎）
- ✅ 低延迟要求（微秒级）
- ✅ CPU 资源充足
- ✅ 事件驱动架构

**核心思想**：在延迟和 CPU 使用之间取得平衡，根据等待时间自动调整策略。

