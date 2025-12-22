# Exchange-CPP Development Plan

## 项目目标

使用 C++ 1:1 重写 exchange-core，保持相同的功能和性能特性。

### 目录结构映射策略

为了便于移植时对照 Java 源码，C++ 版的目录结构**与 Java 版保持逻辑对应**（去除了版本号 `core2`）：

| Java 包路径 | C++ 头文件路径 | 说明 |
| :--- | :--- | :--- |
| `exchange.core2.core.common.*` | `include/exchange/core/common/*` | 通用数据模型 |
| `exchange.core2.core.orderbook.*` | `include/exchange/core/orderbook/*` | 订单簿实现 |
| `exchange.core2.core.processors.*` | `include/exchange/core/processors/*` | 流水线处理器 |
| `exchange.core2.core.utils.*` | `include/exchange/core/utils/*` | 工具函数 |

**设计决策：**
- **简化命名空间**：去除了 Java 包名中的版本号 `core2`，使用更简洁的 `exchange::core::*`
- **保持逻辑对应**：子目录结构（common, orderbook, processors, utils）与 Java 版完全一致
- **便于对照**：移植时可以通过目录名快速定位对应的 Java 文件

**优势：**
- 移植时可以直接对照 Java 文件位置（只需忽略 `core2` 这一层）
- 减少查找对应代码的时间
- C++ 命名空间更简洁：`exchange::core::common::cmd::OrderCommand`

## 核心依赖映射

### Java → C++ 技术栈映射

| Java (exchange-core) | C++ (exchange-cpp) | 说明 | 优先级 |
|---------------------|-------------------|------|:---:|
| LMAX Disruptor | disruptor-cpp | 无锁环形缓冲区，线程间通信 | ✅ 已完成 |
| Eclipse Collections (`IntObjectHashMap`, `IntLongHashMap`) | **自定义实现** | 原始类型哈希映射，避免装箱开销 | **P0** |
| Adaptive Radix Tree (`LongAdaptiveRadixTreeMap`) | **自定义实现** | 价格索引，比 `std::map` 快 3-5 倍 | **P0** |
| Objects Pool | **自定义实现** | 线程局部对象池，零分配 | **P0** |
| Real Logic Agrona | 自定义实现 | 内存布局、缓存行对齐 | P1 |
| OpenHFT Chronicle | 待定 | 持久化、序列化 | P2 |
| LZ4 Java | lz4 (C库) | 压缩算法 | P2 |

**注意**: 标记为 **P0** 的数据结构必须在实现核心业务逻辑之前完成，否则性能无法达到要求。

## 开发阶段

### Phase 1: 项目初始化与依赖集成 ✅

- [x] 创建项目结构
- [x] 添加 exchange-core Java 实现到 reference/ 目录（作为子模块，方便对比）
- [x] 集成 disruptor-cpp 作为子模块 (`third_party/disruptor-cpp`)
- [x] 配置 CMake 构建系统
- [x] 创建基础目录结构 (include, src, tests, benchmarks, examples)
- [ ] 设置测试框架 (Google Test) - 需要安装 GTest
- [ ] 设置基准测试框架 (Google Benchmark) - 需要安装 Google Benchmark

### Phase 2: 第三方依赖集成与数据模型

#### 2.1 第三方高性能库集成（无需自定义实现）

经重新评估，**所有数据结构均可使用成熟开源库**，无需从零实现：

- [ ] **ankerl::unordered_dense**: 高性能哈希表（Header-only）
  - 替代：`IntObjectHashMap`, `IntLongHashMap`
  - 性能：比 `std::unordered_map` 快 2-3 倍
  - 集成：`git submodule add https://github.com/martinus/unordered_dense.git third_party/unordered_dense`

- [ ] **自定义 ART 树** (`LongAdaptiveRadixTreeMap`): 价格索引
  - 替代：exchange-core 的 `LongAdaptiveRadixTreeMap`（直接翻译 Java 实现）
  - 性能：比 `absl::btree_map` 快 20-30%（固定 8 字节键，O(k) 常数时间）
  - 实现：参考 `exchange.core2.collections.art.LongAdaptiveRadixTreeMap` 的 Java 实现
  - 预计工作量：5-7 天

- [ ] **Boost.Intrusive**: 侵入式链表
  - 替代：订单链（同价格订单队列）
  - 性能：零额外内存分配
  - 集成：Header-only

- [ ] **C++17 PMR** + **mimalloc**: 内存池与高性能分配器
  - 替代：`ObjectsPool`
  - 性能：减少 50%+ 内存分配开销
  - 集成：`find_package(mimalloc REQUIRED)`

**详细分析**: 参见 [docs/CUSTOM_DATA_STRUCTURES.md](docs/CUSTOM_DATA_STRUCTURES.md)

#### 2.2 核心数据模型

- [ ] **OrderCommand**: Disruptor 事件核心结构（缓存行对齐）
- [ ] **Order 模型**: 订单数据结构 (限价单、市价单、IOC、FOK 等)
- [ ] **Trade 模型**: 成交记录（MatcherTradeEvent）
- [ ] **OrderBook 模型**: 订单簿（使用 `flat_map` + `Intrusive List`）
- [ ] **Symbol 模型**: 交易对定义
- [ ] **UserProfile 模型**: 用户账户信息

### Phase 3: 订单簿管理

- [ ] **OrderBook 实现**: 
  - 买卖盘维护 (Price-Time Priority)
  - 订单插入/删除/修改
  - 深度查询 (L2 Market Data)
- [ ] **价格索引**: 高效的价格查找结构
- [ ] **订单匹配逻辑**: 价格优先、时间优先

### Phase 4: 撮合引擎核心 (ME - M Threads)

- [ ] **Matching Engine Router**:
  - 根据 `SymbolID` 进行命令路由
  - 撮合逻辑线程绑定 (Thread Affinity)
- [ ] **OrderBook 实现**:
  - 价格优先、时间优先匹配算法
  - 成交事件 (MatcherTradeEvent) 生成与池化管理
- [ ] **多线程并行**:
  - 不同 Symbol 运行在独立的 ME 线程中

### Phase 5: 风险管理 (R1/R2 - N Threads)

- [ ] **Risk Engine (R1 - Pre-hold)**:
  - 根据 `UID` 进行账户分片
  - 余额检查与预扣逻辑 (Place Order Check)
- [ ] **Risk Engine (R2 - Release)**:
  - 处理 ME 生成的成交事件
  - 资金结算、盈亏计算与释放
- [ ] **UserProfile 管理**:
  - 线程安全的账户状态维护 (利用单线程分片保证)

### Phase 6: API 层与 Gateway (Ingress - K Threads)

- [ ] **Exchange API**:
  - `OrderCommand` 的发布接口封装
  - 使用 `disruptor-cpp` 的 `MultiProducerSequencer` 实现多线程并发写入
- [ ] **Gateway 模块**:
  - 基础协议接入 (TCP/WebSocket)
  - 集成 **Aeron** 或 **ENet** 用于低延迟外部接入
  - 身份验证与初步权限校验
- [ ] **零拷贝发布**:
  - 实现从网卡缓冲区到 RingBuffer 的最小拷贝路径

### Phase 7: 持久化与恢复

- [ ] **事件持久化**: 关键事件写入日志
- [ ] **状态快照**: 定期保存系统状态
- [ ] **恢复机制**: 从快照和日志恢复

### Phase 8: 性能优化

- [ ] **内存池**: 减少内存分配开销
- [ ] **缓存优化**: CPU 缓存友好设计
- [ ] **NUMA 优化**: 多 NUMA 节点支持
- [ ] **SIMD 优化**: 向量化关键路径
- [ ] **性能基准测试**: 与 Java 版本对比

### Phase 9: 测试与文档

- [ ] **单元测试**: 覆盖所有核心模块
- [ ] **集成测试**: 端到端测试
- [ ] **压力测试**: 高并发场景测试
- [ ] **API 文档**: 使用 Doxygen 生成
- [ ] **架构文档**: 系统设计说明

## 关键技术决策

### 1. 内存管理
- 使用内存池减少分配开销
- 考虑使用 `std::pmr` (C++17) 或自定义分配器
- 避免频繁的堆分配

### 2. 并发模型 (Disruptor Pipeline)
- **核心架构**: 基于单 RingBuffer 的多阶段异步流水线 (G -> J/R1 -> ME -> R2 -> E)。
- **线程分片**:
    - **Grouping (G)**: 单线程预处理。
    - **Risk Engine (R1/R2)**: 按 `UserID` 分片，保证用户维度操作的原子性。
    - **Matching Engine (ME)**: 按 `SymbolID` 分片，保证订单簿操作的原子性。
    - **Journaling (J)**: 旁路顺序持久化。
- **通信机制**: 使用 `disruptor-cpp` 实现内存屏障和线程间的高效通知，消除锁竞争。

### 3. 数据结构选择
- OrderBook: 考虑使用 `std::map` 或自定义红黑树
- 价格索引: 考虑 Adaptive Radix Tree 或跳表
- 订单队列: 使用 disruptor-cpp ring buffer

### 4. 序列化
- 考虑使用 FlatBuffers 或 Cap'n Proto 用于跨进程通信
- 内部使用二进制格式，避免 JSON 解析开销

## 性能目标

- **吞吐量**: 与 Java 版本相当或更好
- **延迟**: P99 延迟 < 10μs (本地测试)
- **内存**: 内存占用 < Java 版本的 50%

## 开发优先级

1. **P0 (核心功能)**:
   - 订单簿管理
   - 基本撮合逻辑
   - 单线程版本先实现

2. **P1 (关键功能)**:
   - 多线程并发处理
   - 风险管理基础功能
   - 基本 API

3. **P2 (增强功能)**:
   - 完整风险管理
   - 持久化
   - 性能优化

4. **P3 (扩展功能)**:
   - 高级报告功能
   - 集群支持
   - 更多交易类型

## 里程碑

- **M1**: 完成单线程撮合引擎，通过基础测试
- **M2**: 完成多线程版本，性能达到 Java 版本 80%
- **M3**: 完成所有核心功能，性能达到或超过 Java 版本
- **M4**: 生产就绪，完整测试和文档

## 注意事项

1. **1:1 移植**: 保持与 Java 版本相同的业务逻辑和行为
2. **性能优先**: 在保持正确性的前提下，追求极致性能
3. **可测试性**: 每个模块都要有对应的单元测试
4. **代码质量**: 遵循现代 C++ 最佳实践，使用静态分析工具

## 参考资料

- **本地参考实现**: `reference/exchange-core/` - 原始 Java 实现（git 子模块）
- [exchange-core 源码](https://github.com/exchange-core/exchange-core) - GitHub 仓库
- [disruptor-cpp 文档](https://github.com/SkynetNext/disruptor-cpp)
- [LMAX Disruptor 论文](https://lmax-exchange.github.io/disruptor/files/Disruptor-1.0.pdf)

## 参考实现使用说明

原始 Java 实现在 `reference/exchange-core/` 目录中，作为 git 子模块：

```bash
# 初始化子模块（如果还没有）
git submodule update --init --recursive

# 更新到最新版本
cd reference/exchange-core
git pull origin master
cd ../..

# 查看 Java 实现的具体代码，用于对比和参考
```

在开发过程中，可以随时参考 `reference/exchange-core/src/` 中的 Java 代码来确保 C++ 实现的正确性和完整性。

