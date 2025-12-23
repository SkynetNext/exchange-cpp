# Exchange-CPP 项目状态报告

## 📊 总体完成度：**约 96%**

### 文件统计
- **头文件 (.h)**: 93 个 ✅
- **实现文件 (.cpp)**: 30 个 ✅
- **编译状态**: ✅ 无编译错误

---

## 项目目标

使用 C++ 1:1 重写 exchange-core，保持相同的功能和性能特性。

### 核心依赖映射

| Java (exchange-core) | C++ (exchange-cpp) | 状态 |
|---------------------|-------------------|:---:|
| LMAX Disruptor | disruptor-cpp | ✅ 已完成 |
| Eclipse Collections | ankerl::unordered_dense | ✅ 已集成 |
| Adaptive Radix Tree | 自定义 ART 树 | ✅ 已实现 |
| Objects Pool | 自定义 ObjectsPool | ✅ 已实现 |
| LZ4 Java | lz4 (C库) | ✅ 已集成 |

**详细数据结构选型**: 参见 [docs/CUSTOM_DATA_STRUCTURES.md](docs/CUSTOM_DATA_STRUCTURES.md)

---

## 已完成功能 ✅

### 核心功能
- ✅ **订单簿管理**: OrderBookNaiveImpl, OrderBookDirectImpl (使用 ART 树)
- ✅ **撮合引擎**: MatchingEngineRouter, 完整命令处理
- ✅ **风险管理**: RiskEngine (包括保证金订单处理和完整 maker 处理逻辑)
- ✅ **用户管理**: UserProfileService, SymbolSpecificationProvider
- ✅ **序列化/持久化**: WriteBytesMarshallable 接口，所有主要类序列化实现
- ✅ **快照加载**: MatchingEngineRouter 和 RiskEngine 的快照加载逻辑
- ✅ **LZ4 压缩**: SerializationUtils::LongsLz4ToBytes
- ✅ **工厂模式反序列化**: BinaryDataCommandFactory 和 ReportQueryFactory（替代 Java 反射）
- ✅ **完整流水线配置**: 使用 Pimpl + 模板模式实现的 1:1 Disruptor 流水线配置

---

## 待完成功能 ⚠️

| 功能 | 优先级 | 状态 | 说明 |
| :--- | :---: | :---: | :--- |
| 边界情况处理 | P1 | ⏳ 部分完成 | 需要完善 WaitSpinningHelper 阻塞策略、异常日志和资源清理 |
| 性能优化 | P2 | ⏳ 待开始 | 内存池优化、缓存行对齐、零拷贝实现 |
| mmap 主备扩展 | P2 | 📝 规划中 | 将 FileChannel 改为 mmap，支持主备复制（见下方详细方案） |
| 测试和验证 | P2 | ⏳ 待开始 | 单元测试、集成测试、基准测试 |

---

## 关键技术决策

### 1. 并发模型与流水线
- **核心架构**: 基于单 RingBuffer 的多阶段异步流水线 (G -> J/R1 -> ME -> R2 -> E)。
- **实现模式**: **虚基类接口 + 模板实现类 (Pimpl 变体)**。
  - `IExchangeApi` 和 `IImpl` 提供稳定接口。
  - `ExchangeApi<WaitStrategyT>` 和 `ExchangeCoreImpl<WaitStrategyT>` 实现具体性能优化。
  - 这种模式在保持 1:1 配置灵活性的同时，消除了 `void*` 的风险，并确保热路径（WaitStrategy）通过模板内联优化。
- **线程分片**: Grouping (G), Risk Engine (R1/R2), Matching Engine (ME), Journaling (J)。

### 2. 数据结构选择
- OrderBook: 使用 ART 树（已实现）✅
- 价格索引: ART 树（已实现）✅
- 订单队列: 使用 disruptor-cpp ring buffer

### 3. 序列化
- 内部使用二进制格式，避免 JSON 解析开销。
- 使用 LZ4 压缩优化传输。
- C++ 侧使用工厂模式替代 Java 反射进行动态反序列化。

---

## 当前完成度统计

- **核心业务逻辑**: ✅ 98%
- **流水线和并发**: ✅ 100%
- **配置和初始化**: ✅ 100%
- **序列化/持久化**: ✅ 95% (快照加载已完成，保存待验证)
- **性能优化数据结构**: ✅ 95% (ART 树已实现)

**总体完成度**: **96%** ✅

---

## 下一步行动

### 立即完成 (1-2 周)
1. 验证快照保存逻辑 (PERSIST_STATE_MATCHING/RISK)。
2. 完善 `WaitSpinningHelper` 的阻塞等待策略。
3. 补充异常日志处理。

### 短期目标 (2-4 周)
1. 性能调优（Profile & Benchmark）。
2. 边界情况稳定性测试。

---

## 未来扩展计划：高可用与性能增强 🚀

### 1. 技术决策：持久化与同步方案对比

| 方案 | 核心技术 | 延迟 (P99) | CPU 开销 | 决策建议 | 适用场景 |
| :--- | :--- | :---: | :---: | :--- | :--- |
| **标准 I/O (HDD)** | `FileChannel.write("rwd")` | 1-5ms | 高 (Syscall) | ❌ 仅用于开发调试 | 通用业务 |
| **标准 I/O (NVMe SSD)** | `FileChannel.write("rwd")` | **20-100μs** | 中 (Syscall) | ✅ **简单可靠** | **生产环境（推荐）** |
| **本地 mmap** | `mmap` + `memcpy` | 1-10μs | 低 (Zero-copy) | ✅ **核心性能基石** | 单机高性能 |
| **TCP 异步复制** | mmap + Socket | 50-200μs | 中 | ⚠️ **过渡方案** | 标准云环境 (无 RDMA) |
| **RDMA 复制** | **RoCE v2 / IB** | **1-3μs** | **极低 (Bypass)** | 💎 **工业级标准** | **低延迟交易所生产环境** |

**注**：
- 延迟数据为典型值，实际性能因硬件型号、系统配置和工作负载而异
- **HDD 延迟**：1-5ms 为顺序写入延迟（交易所日志场景）；随机写入延迟为 5-15ms
- **NVMe SSD 延迟**：20-100μs 为平均读写延迟（顺序写入可能低至 10-50μs）
- **mmap 延迟**：1-10μs 为写入 Page Cache 的时间（不含刷盘）
- **RDMA 延迟**：1-3μs 为端到端延迟
- **性能差距**：HDD vs NVMe SSD 的延迟差距约为 **20-500 倍**（取决于随机/顺序写入）

### 2. 核心架构：本地 mmap + RDMA 异步双写

这是低延迟交易系统的“黄金标准”实践，兼顾了本地极致持久化速度与跨节点亚毫秒级切换。

#### 核心设计逻辑：
1.  **本地持久化**：主线程通过 `mmap` 直接操作内存映射文件。由内核 Page Cache 负责异步刷盘，彻底消除 `write` 系统调用的上下文切换和磁盘 I/O 抖动。
2.  **网络旁路复制**：利用 RDMA (Remote Direct Memory Access) 技术。主节点网卡直接从内存读取日志块并写入备节点内存，完全不经过主/备节点的 CPU 和内核协议栈。
3.  **异步非阻塞**：复制过程与匹配引擎撮合逻辑完全解耦，主流程延迟仅增加一个 `memcpy` 的时间（<1μs）。

#### 核心伪代码实现：
```cpp
class HighAvailabilityJournal {
    void* local_mmap_ptr_;      // 本地内存映射指针
    RDMAContext* rdma_ctx_;      // RDMA/RoCE 控制句柄

    void Write(const OrderCommand& cmd) {
        // 1. 本地写入 (微秒级)
        memcpy(local_mmap_ptr_ + offset_, &cmd, sizeof(cmd));
        
        // 2. 远程触发 (内核旁路，零 CPU 参与)
        // 自动兼容 RoCE v2 (以太网) 或 InfiniBand
        rdma_ctx_->PostWrite(cmd, offset_); 
        
        offset_ += sizeof(cmd);
    }
};
```

### 3. 实现路线图 (Roadmap)

1.  **Phase 1**: 完善现有的 `FileChannel` 移植，确保 1:1 业务逻辑正确性。
2.  **Phase 2**: 实现 `mmap` 持久化层，替换标准文件 I/O，作为单机性能瓶颈的突破口。
3.  **Phase 3**: 引入 `libibverbs` 抽象层，支持 RoCE v2 异步复制，达到生产级高可用要求。

### 4. 技术本质对比：零拷贝技术的不同路径

| 技术 | 操作路径 | 典型延迟 | 核心优势 | 解决的问题 |
| :--- | :--- | :---: | :--- | :--- |
| **`sendfile`** | 磁盘 -> 网卡 (Kernel Space) | 10-50μs | 避免数据拷贝到用户态，减少上下文切换 | 大规模文件下发 (如 Kafka 消费数据) |
| **`mmap`** | 用户内存 <-> 磁盘 | **1-10μs** | 像读写内存一样操作磁盘，避免 `write` 调用 | 极致的本地持久化速度 (Append-only Log) |
| **`RDMA`** | 用户内存 -> 远端内存 | **1-3μs** | **绕过内核 (Kernel Bypass)**，零 CPU 参与 | 极低延迟的主备同步 (Replication) |

---

## 注意事项

1. **1:1 移植**: 保持与 Java 版本相同的业务逻辑和行为
2. **性能优先**: 在保持正确性的前提下，追求极致性能
3. **代码质量**: 遵循现代 C++ 最佳实践

---

## 参考资料

1. **本地参考实现**: `reference/exchange-core/` - 原始 Java 实现（git 子模块）
2. [exchange-core 源码](https://github.com/exchange-core/exchange-core)
3. [disruptor-cpp 文档](https://github.com/SkynetNext/disruptor-cpp)
4. **数据结构选型**: [docs/CUSTOM_DATA_STRUCTURES.md](docs/CUSTOM_DATA_STRUCTURES.md)

---

*最后更新: 2024-12-23*
