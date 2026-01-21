# 技术笔记

## 一、高性能设计思路 (C++)

### 1.1 核心性能指标

| 指标 | 达成 | 行业标准 | 倍数 |
|------|------|----------|------|
| **P50 延迟** | 0.49-0.82µs | < 1µs | ✅ 超越 |
| **P99 延迟** | 0.73-1.6µs | < 5µs | ✅ 3x+ |
| **P99.9 延迟** | 5.8-18µs | < 20µs | ✅ 超越 |
| **吞吐量** | 9M TPS | 1M TPS | ✅ 9x |

### 1.2 关键设计决策

#### Lock-Free 架构
- **Disruptor 模式**: 单 Ring Buffer + Sequence Barrier 实现无锁流水线
- **避免互斥锁**: MPSC 使用 `fetch_add` + `acq_rel` (x86: `lock xadd`) 原子获取 sequence，消除锁竞争
- **多生产者支持**: `MultiProducerRingBuffer` 处理多 Gateway 线程并发写入

#### 零分配热路径
- **对象预分配**: Ring Buffer 启动时分配所有 `OrderCommand`
- **对象池**: 自定义 `ObjectsPool` 复用 ART Node、Order 对象
- **mimalloc**: 全局替换分配器，减少 50%+ 分配开销

#### 数据结构优化
| 场景 | 选型 | 原因 |
|------|------|------|
| 价格索引 | ART Tree | O(k) 常数时间，3-5x 优于 `std::map` |
| HashMap | `ankerl::unordered_dense` | 1.5-2.5x 优于 `std::unordered_map` (实测) |
| 订单链表 | Intrusive Linked List | 零额外内存分配 |
| 并发队列 | `moodycamel::ConcurrentQueue` | 14ns 延迟，70M ops/s |

#### Memory Order 优化
- **避免 seq_cst**: `xchg` 指令隐含 lock 前缀，5-10ns 开销
- **x86-64 TSO 模型**: acquire load 编译为普通 `mov`，无额外屏障
- **Acquire-Release 足够**: 生产者-消费者模式无需全序

### 1.3 流水线并行设计

```
Gateway → Grouping → Risk(R1) → Matching → Risk(R2) → Results
                  ↘ Journaling ──────────────────────↗
```

- **分组机制**: 每 N 条消息创建 Group，允许 R1 提前 publish Sequence
- **Two-Step Processor**: Master 检测边界 → Slave 处理 → 下游提前启动
- **并行路径**: Journaling 与 Trading Chain 并行执行

---

## 二、代码优化细节

### 2.1 热路径识别与优化

| 问题 | 位置 | 代价 | 状态 |
|------|------|------|------|
| Virtual Function | `IOrderBook` | 10-20ns/call | 考虑 CRTP |
| Exception Handling | `TwoStepMasterProcessor` | 1.7-3.3µs | 已对齐 Java |
| Hash Lookup | `orderIdIndex_` | - | ART → unordered_dense |

### 2.2 关键优化案例

**orderIdIndex_ 优化 (2026-01-16)**
- Before: ART Tree (树查找)
- After: `ankerl::unordered_dense::map` (O(1) 哈希)
- Result: P50 @ 6M TPS 超越 Java 版本

### 2.3 CPU 亲和性
- 每个 Processor 绑定独立 CPU 核心
- `AffinityThreadFactory` 设置线程亲和性
- 避免上下文切换和缓存失效

---

## 三、工程能力与方法论

### 3.1 代码质量保证

| 维度 | 实践 |
|------|------|
| **编译警告** | `-Wall -Wextra -Werror`，零警告策略 |
| **静态分析** | clang-tidy + cppcheck |
| **单元测试** | GoogleTest，覆盖核心数据结构 |
| **集成测试** | 完整订单生命周期测试 |
| **性能测试** | `TestLatencyMargin` 基准测试 |

### 3.2 Code Review 关注点

1. **内存安全**: 所有权清晰、避免 use-after-free
2. **并发正确性**: Memory Order 使用是否正确
3. **热路径影响**: 是否引入分配、异常、虚函数
4. **API 兼容性**: 是否保持与 Java 版本行为一致

### 3.3 架构文档化

- `ARCHITECTURE.md`: Sequence/Barrier 完整流程
- `LOW_LATENCY_CORE.md`: 性能指标与对比
- `HOT_PATH_PERFORMANCE_TODO.md`: 待优化清单
- `guides/`: 内存序、ART 树等深度技术文档

---

## 四、个人成长与技术敏感度

### 4.1 C++23 最有价值的特性（业界反馈）

**业界共识**: C++23 是"打磨版"发布，稳定 C++20 主要特性，同时为库作者和系统开发者提供显著的生活质量改进。

#### 核心语言特性（最高价值）

| 特性 | 业界反馈 | 实际工程价值 | 应用场景 |
|------|----------|--------------|----------|
| **Deducing `this`** | ⭐⭐⭐⭐⭐ | 简化 CRTP，减少 50%+ 模板样板代码 | 简化 CRTP 实现，减少模板样板代码 |
| **`if consteval`** | ⭐⭐⭐⭐ | 更直观的编译期分支，替代 `std::is_constant_evaluated()` | 编译期优化分支，配置表预生成 |
| **多维下标运算符** | ⭐⭐⭐⭐ | 科学计算、矩阵库标准支持 | 市场数据处理、多维数组视图 |
| **扩展 `constexpr`** | ⭐⭐⭐⭐⭐ | 更多标准库容器/算法编译期可用 | 配置表预生成，消除运行时错误 |

#### 标准库特性（最高价值）

| 特性 | 业界反馈 | 实际工程价值 | 应用场景 |
|------|----------|--------------|----------|
| **`std::expected`** | ⭐⭐⭐⭐⭐ | 异常替代，热路径零开销错误处理 | 替代异常处理，热路径错误码返回 |
| **`std::mdspan`** | ⭐⭐⭐⭐ | 多维数组视图标准，性能优于原始指针 | 多维数组视图，市场数据处理 |
| **`std::flat_map`/`std::flat_set`** | ⭐⭐⭐⭐ | 内存高效、缓存友好，1.5-2x 性能提升 | 替代 `std::map`/`std::set` 在特定场景 |
| **Range 改进** | ⭐⭐⭐ | `views::enumerate`, `views::zip` 简化代码 | 数据转换、调试输出 |

#### 业界实际应用反馈

1. **构建性能**: Modules 从"实验性玩具"变为生产就绪，显著减少构建复杂度和"头文件地狱"，编译时间减少 30-50%
2. **模板调试**: Concepts 成为编写模板代码的默认方式，错误消息更清晰，节省数天调试时间
3. **系统安全**: 通过 `constexpr` 和 `std::expected`，消除整类运行时错误，构建更安全的系统
4. **编译器支持**: 主要特性（`deducing this`, `if consteval`, 多维下标）在 GCC 13+, Clang 17+, MSVC 2022+ 已有稳定支持

### 4.2 C++26 最有价值的特性（业界反馈）

**业界共识**: C++26 被视为与 C++11 相当的重大进化，开启"第二波现代化浪潮"。高频交易公司（如 Citadel Securities）已在生产环境集成草案特性。

#### 容器特性（最高价值）

| 特性 | 业界反馈 | 实际工程价值 | 应用场景 | 优先级 |
|------|----------|--------------|----------|--------|
| **`std::inplace_vector`** | ⭐⭐⭐⭐⭐ | 固定容量栈分配，零堆分配延迟 | `VectorBytesOut` 小缓冲区，避免堆分配 | 🔥 立即考虑 |
| **`std::hive`** | ⭐⭐⭐⭐ | 稳定指针、对象池、无重新分配 | 对象池、事件缓冲区，无重新分配 | 📋 中期考虑 |
| **`std::hazard_pointer`** | ⭐⭐⭐⭐ | 基于 Facebook `folly`（2017 年生产使用） | 自定义无锁数据结构，解决 ABA 问题 | 🔍 按需使用 |

**`std::inplace_vector` 应用示例**:
```cpp
// 当前：可能触发堆分配
std::vector<uint8_t> data;
data.resize(100);  // 可能分配堆内存

// 使用 inplace_vector：小数据栈分配
std::inplace_vector<uint8_t, 256> data;  // 256字节内栈分配
data.resize(100);  // 无堆分配，零延迟
```

**适用场景**:
- `VectorBytesOut`: 序列化缓冲区（通常 < 256 字节）
- 临时缓冲区: 避免频繁堆分配
- 性能收益: 减少内存分配延迟，提升热路径性能

**`std::hive` 特性**:
- 稳定指针：元素插入/删除后指针仍有效
- 内存重用：自动重用已删除元素的内存
- 迭代效率：自动移除空块，保持迭代性能
- 适用场景：游戏编程、对象池、事件缓冲区

#### 并发与执行特性（最高价值）

| 特性 | 业界反馈 | 实际工程价值 | 应用场景 |
|------|----------|--------------|----------|
| **`std::execution`** | ⭐⭐⭐⭐⭐ | Citadel Securities 已用于消息基础设施 | 异步执行、任务调度、消息处理 |
| **`std::hazard_pointer`** | ⭐⭐⭐⭐ | Facebook `folly` 生产验证（2017+） | 无锁数据结构、ABA 问题解决 |

**业界实际应用**:
- **Citadel Securities**: 使用 `std::execution` 作为消息基础设施和资产类别交易的基础
- **Facebook `folly`**: `hazard_pointer` 自 2017 年起在生产环境使用，性能稳定

#### 中等价值特性

| 特性 | 价值 | 应用场景 | 备注 |
|------|------|----------|------|
| **`std::simd`** | ⭐⭐⭐ | 批量数据处理、数据转换 | 需要明确热点场景 |
| **`std::rcu`** | ⭐⭐ | 配置读取（读多写少） | 项目主要是写密集型 |

#### 低价值特性（不太相关）

| 特性 | 价值 | 原因 |
|------|------|------|
| **`std::contracts`** | ⭐⭐ | 调试有用，但可能影响性能 |
| **`std::linalg`** | ⭐ | 线性代数库，项目不涉及 |
| **`std::text_encoding`** | ⭐ | 文本编码，项目主要是二进制协议 |
| **`std::debugging`** | ⭐⭐ | 调试工具，生产环境通常不需要 |

#### 业界最关注的 C++26 特性（综合评估）

1. **Reflection（反射）**: 元编程、序列化、代码生成（✅ 已确定进入 C++26，2025年6月 Sofia 会议投票，P2996；⚠️ 编译器支持：GCC 实验性 `-freflection`，Clang 仅在 Bloomberg fork，MSVC 不支持）
2. **Contracts（契约）**: 系统安全性、调试能力（✅ 已确定进入 C++26，2025年2月 Hagenberg 会议投票，P2900R14；⚠️ 编译器支持：GCC/Clang 仅在 fork 中，MSVC 不支持）
3. **`std::execution`**: 异步执行、性能优化（✅ 已确定进入 C++26）
4. **`std::inplace_vector`**: 零分配热路径优化（✅ 已确定进入 C++26）
5. **`std::hive`**: 对象池、稳定指针场景（✅ 已确定进入 C++26）

**注意**: `std::hazard_pointer` 主要用于解决 ABA 问题，但项目使用的 Disruptor 已内部处理，当前可能不需要。不过，如果未来需要自定义无锁数据结构，这是标准库提供的可靠选择。

### 4.3 关注的开源项目

| 项目 | 原因 |
|------|------|
| **LMAX Disruptor** | 参考架构，C++ 实现对标 |
| **mimalloc** | 已集成，性能最优分配器 |
| **folly** | Meta 高性能库，学习并发原语设计 |
| **abseil** | Google 通用库，高质量 API 设计参考 |
| **seastar** | 无共享架构，未来网关层参考 |

### 4.4 实质性行动

1. **完整翻译** exchange-core Java → C++，含 ART Tree、ObjectsPool
2. **性能调优**: 通过 perf + flamegraph 定位瓶颈，优化 `orderIdIndex_`
3. **文档输出**: 撰写 Memory Order 性能分析、ART 树深度教程
4. **基准测试**: 建立 CI 性能回归测试，防止性能退化

---

## 五、业务理解与想法

### 5.1 撮合引擎核心挑战

- **公平性**: 严格价格-时间优先级 (Price-Time Priority)
- **确定性**: 相同输入 → 相同内存状态 (Deterministic)
- **容错**: Journaling + Snapshot 支持状态恢复

### 5.2 可能的优化方向

1. **FPGA 卸载**: 网络解析、风控前置检查硬件加速
2. **DPDK 网络栈**: 绕过内核，减少网络延迟
3. **持久化内存 (PMem)**: Journaling 直接写 PMem，降低 I/O 延迟

### 5.3 扩展思考

- **多品种撮合**: Symbol 分片，水平扩展 Matching Engine
- **跨市场套利**: 多交易所行情聚合、低延迟路由
- **风控升级**: 实时盯市、异常订单检测

---

## 六、技术讨论要点

1. **量化表达**: P50=0.5µs、9M TPS、1.5-5x 性能提升
2. **权衡思维**: 为什么选 ART 不选 B+ Tree？为什么用 Disruptor？
3. **延伸思考**: 进一步优化方向（CRTP、FPGA 等）
4. **工程闭环**: 设计 → 实现 → 测试 → 监控的完整思路

---

**Last Updated**: 2026-01-20
