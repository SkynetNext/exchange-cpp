# Gateway 技术选型文档

## 1. 背景

将 [exchange-gateway-rest](https://github.com/exchange-core/exchange-gateway-rest) 从 Java 移植到 C++，实现：
- **HTTP REST API**：下单、撤单、查询订单簿
- **WebSocket**：实时推送成交、订单状态更新
- **低延迟**：目标延迟 <50μs（P99）
- **高吞吐**：支持 100K+ TPS

## 2. 技术选型

### 2.1 HTTP/WebSocket 服务器

| 库 | 特点 | 延迟 | 推荐度 | 说明 |
|---|------|------|--------|------|
| **uWebSockets** | 最快 HTTP/WS 库 | 5-10μs | ⭐⭐⭐⭐⭐ | **推荐**：单头文件，性能最佳 |
| **Drogon** | C++17 全栈框架 | 50-100μs | ⭐⭐⭐⭐ | 功能完整，类似 Spring Boot |
| **Boost.Beast** | Boost 官方 | 30-50μs | ⭐⭐⭐⭐ | 稳定，企业级 |
| **Crow** | 简单易用 | 50-100μs | ⭐⭐⭐ | 快速开发 |
| **httplib** | 单头文件 | 50-100μs | ⭐⭐⭐ | 简单场景 |

**选择：uWebSockets**
- 性能最佳（业界最快）
- 单头文件，易集成
- 同时支持 HTTP 和 WebSocket
- 广泛用于高频交易系统

### 2.2 JSON 序列化

| 库 | 性能 | 易用性 | 推荐度 | 说明 |
|---|------|-------|--------|------|
| **rapidjson** | 极快 | 中等 | ⭐⭐⭐⭐⭐ | **推荐**：零拷贝，性能好 |
| **simdjson** | 最快（解析） | 只读 | ⭐⭐⭐⭐ | SIMD 加速，仅解析 |
| **glaze** | 极快（编译期） | 好 | ⭐⭐⭐⭐ | 新选择，编译期优化 |
| **nlohmann/json** | 中等 | 最好 | ⭐⭐⭐ | 开发效率高，性能一般 |

**选择：rapidjson**
- 性能优秀（解析和生成都很快）
- 零拷贝支持
- 成熟稳定，广泛使用
- 与 uWebSockets 配合良好

### 2.3 异步 I/O

| 方案 | 特点 | 推荐度 |
|---|------|--------|
| **uWebSockets 内置** | 基于 libuv | ⭐⭐⭐⭐⭐ | **推荐**：已集成 |
| **io_uring** | Linux 5.1+，最低延迟 | ⭐⭐⭐⭐ | 极致性能时考虑 |
| **Boost.Asio** | 跨平台，成熟 | ⭐⭐⭐ | 需要额外功能时 |

**选择：uWebSockets 内置（libuv）**
- uWebSockets 已集成异步 I/O
- 无需额外依赖
- 性能足够好

**注意**：
- libuv 事件循环是单线程的
- 需要多线程架构：每个线程运行独立的事件循环
- 使用 `SO_REUSEPORT` 实现内核级负载均衡
- 推荐线程数 = CPU 核心数（或 2x 核心数）

### 2.4 内存分配器

| 库 | 特点 | 性能 | 推荐度 |
|---|------|------|--------|
| **mimalloc** | 微软开发，低开销 | 最快 | ⭐⭐⭐⭐⭐ | **选择** |
| **jemalloc** | 低碎片，成熟稳定 | 快 | ⭐⭐⭐ | 对比参考 |
| **tcmalloc** | Google 出品 | 快 | ⭐⭐⭐ | 对比参考 |
| **标准 malloc** | 默认 | 中等 | ⭐⭐ | 对比参考 |

**选择：mimalloc**

**性能优势**：
- **分配速度最快**：比 jemalloc 快约 10-15%（Redis 单线程场景快 14%）
- **低内存碎片**：内存效率更高
- **多线程优化**：线程间对象迁移性能优秀
- **低延迟**：适合高频交易场景
- **轻量级**：开销更小

**技术对比**（仅供参考）：
- mimalloc vs jemalloc：mimalloc 在大多数场景下更快
- mimalloc vs tcmalloc：mimalloc 通常更快，内存碎片更少
- 结论：mimalloc 是目前性能最佳的内存分配器

## 3. 推荐技术栈

### 3.1 核心组件

```
HTTP/WebSocket: uWebSockets
JSON:           rapidjson
异步 I/O:        uWebSockets 内置（libuv）
内存分配:       mimalloc
```

### 3.2 依赖管理

```cmake
# CMakeLists.txt
find_package(PkgConfig REQUIRED)

# uWebSockets (单头文件，直接包含)
# rapidjson (单头文件，直接包含)

# mimalloc
find_package(mimalloc REQUIRED)
target_link_libraries(gateway mimalloc)
```

## 4. 架构设计

### 4.0 多线程架构说明

**libuv 单线程限制**：
- libuv 事件循环是单线程的，一个 `uv_loop_t` 只能在一个线程中运行
- 单线程无法充分利用多核 CPU
- 需要多线程架构：每个线程运行独立的事件循环

**多线程方案**：
1. **每个线程一个事件循环**：创建 N 个线程，每个线程运行独立的 `uWS::App().run()`
2. **SO_REUSEPORT 负载均衡**：所有线程绑定同一端口，内核自动分发连接
3. **无锁设计**：线程间通过 ExchangeCore 的 RingBuffer 通信（已线程安全）

**线程数选择**：
- **推荐**：线程数 = CPU 核心数
- **高负载**：线程数 = 2x CPU 核心数（I/O 密集型）
- **避免过多**：超过 2x 核心数会引入上下文切换开销

### 4.1 整体架构

```
┌─────────────────────────────────────────────────┐
│  客户端                                          │
└─────────────────────────────────────────────────┘
         │                           │
         │ HTTP REST                 │ WebSocket
         │ (请求/响应)                │ (订阅/推送)
         ↓                           ↓
┌─────────────────────────────────────────────────┐
│  Gateway (C++) - 多线程架构                      │
│  ┌──────────────┐  ┌──────────────┐            │
│  │ Thread 0     │  │ Thread 1     │  ...       │
│  │ uWebSockets  │  │ uWebSockets  │            │
│  │ (libuv loop) │  │ (libuv loop) │            │
│  │ rapidjson    │  │ rapidjson    │            │
│  └──────────────┘  └──────────────┘            │
│         │                  │                     │
│         └──────────────────┘                     │
│              │                                    │
│              ↓                                    │
└─────────────────────────────────────────────────┘
         │                           │
         ↓                           ↑
┌─────────────────────────────────────────────────┐
│  ExchangeCore (C++)                              │
│  ┌───────────────────────────────────────────┐  │
│  │  RingBuffer (Disruptor)                   │  │
│  └───────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
         │                           │
         └───────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────┐
│  Results Handler                                 │
│  → WebSocket 推送成交、订单状态                   │
└─────────────────────────────────────────────────┘
```

### 4.2 数据流

```
1. HTTP 请求
   Client → uWebSockets → rapidjson 解析
   → ExchangeApi.placeOrder() → RingBuffer

2. 处理结果
   ExchangeCore → Results Handler
   → rapidjson 生成 → uWebSockets WebSocket 推送
   → Client
```

## 5. 代码示例

### 5.1 HTTP REST 接口

```cpp
#include <uWebSockets/App.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include "exchange/core/ExchangeApi.h"

uWS::App()
    .post("/api/v1/orders", [](auto *res, auto *req) {
        res->onData([res](std::string_view data, bool last) {
            // 解析 JSON
            rapidjson::Document doc;
            doc.Parse(data.data(), data.size());
            
            int64_t uid = doc["uid"].GetInt64();
            int64_t price = doc["price"].GetInt64();
            int64_t size = doc["size"].GetInt64();
            
            // 调用 ExchangeCore
            auto orderId = exchangeApi->placeOrder(uid, price, size);
            
            // 返回 JSON
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            writer.StartObject();
            writer.Key("orderId");
            writer.Int64(orderId);
            writer.Key("status");
            writer.String("success");
            writer.EndObject();
            
            res->writeHeader("Content-Type", "application/json");
            res->end(buffer.GetString());
        });
    })
    .listen(8080, [](auto *token) {
        if (token) {
            std::cout << "Listening on port 8080" << std::endl;
        }
    })
    .run();
```

### 5.2 WebSocket 推送

```cpp
uWS::App()
    .ws<UserData>("/ws/orders", {
        .open = [](auto *ws) {
            // 连接建立，订阅用户订单
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            // 处理客户端消息
        }
    })
    .run();

// Results Handler 中推送
void onOrderResult(OrderCommand* cmd) {
    // 生成 JSON
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    // ... 写入数据
    
    // 通过 WebSocket 推送
    wsHub->publish("/topic/orders/uid/" + std::to_string(cmd->uid),
                   buffer.GetString(), uWS::OpCode::TEXT);
}
```

## 6. 性能目标

| 指标 | 目标 | 说明 |
|------|------|------|
| **HTTP 延迟** | <50μs (P99) | 请求到响应 |
| **WebSocket 延迟** | <10μs (P99) | 推送延迟 |
| **吞吐量** | 100K+ TPS | 单机 |
| **内存** | <1GB | 空闲状态 |

## 6.0 网络收包→Ring Buffer 时延对比

### 核心结论

**收包→转发到Ring Buffer（不含写数据）的业界典型时延（单向），按方案从高到低：**

**epoll(10~50μs) > io_uring(3~15μs) > DPDK(0.5~5μs) > FPGA(20~500ns)**

### 各方案典型时延对比表

| 方案 | 典型值（单向） | 极致优化值（单向） | 纯转发路径（单向） | 瓶颈因素 | 备注 |
|------|--------|-----------|-----------|---------|------|
| **epoll** | 10~50μs | 5~20μs | - | 内核协议栈、系统调用、上下文切换、调度抖动 | 很难低于 5μs，破 1μs 几乎不可能 |
| **io_uring** | 3~15μs | 2~8μs | - | 内核态操作、内存拷贝（零拷贝可优化） | 减少上下文切换，但破 1μs 依然极难 |
| **DPDK** | 0.5~5μs | 0.3~2μs | 0.5~2μs | PCIe DMA、用户态指令开销 | 绕开内核，UIO/PMD+大页+绑核，1μs 是多数场景下限；数据为单向收包延迟 |
| **FPGA** | 20~500ns | 20~100ns (片上) | 100~300ns (PCIe DMA) | 硬件时钟周期、PCIe 链路延迟 | 无软件开销，延迟确定性极高；PCIe DMA 往返延迟约 500~800ns（Gen3×8） |

### 详细说明

#### 1. epoll（Linux 标准异步 I/O）

- **典型场景**：10~50μs
- **极致优化**（绑核/关中断/大页）：5~20μs
- **瓶颈**：
  - 内核协议栈处理开销
  - 系统调用上下文切换（~0.67μs 直接开销）
  - 调度抖动和缓存失效（总开销可达 3.3~333μs）
  - 内存拷贝（零拷贝可部分缓解）
- **限制**：很难低于 5μs，破 1μs 几乎不可能

#### 2. io_uring（Linux 5.1+ 异步 I/O）

- **典型场景**：3~15μs
- **极致优化**（SQPOLL/IOPOLL/零拷贝）：2~8μs
- **优势**：
  - 减少系统调用和上下文切换
  - 支持批处理提交
  - 零拷贝支持（`IORING_OP_RECV_ZC`）
- **瓶颈**：
  - 仍有内核态操作
  - 内存拷贝（零拷贝可优化）
- **限制**：破 1μs 依然极难

#### 3. DPDK（用户态网络栈）

- **典型场景**：0.5~5μs
- **极致优化**（专用核/大页/零拷贝/轮询）：0.3~2μs
- **纯转发路径**：0.5~2μs
- **优势**：
  - 完全绕过内核协议栈
  - UIO/VFIO 直接访问网卡
  - Poll-Mode Driver (PMD) 轮询模式
  - 大页内存减少 TLB miss
  - CPU 亲和性绑定
- **瓶颈**：
  - PCIe DMA 延迟（~500~800ns）
  - 用户态指令开销
- **限制**：1μs 是多数场景的下限，亚微秒级需要极简处理逻辑

#### 4. FPGA（硬件加速）

- **典型场景**：20~500ns
- **片上 BRAM/URAM 实现**：20~100ns
  - 纯硬件无锁 Ring Buffer
  - 无 CPU 介入
  - 延迟仅几十纳秒
- **PCIe DMA 直写主机内存**：100~300ns（**单向**）
  - 通过 PCIe DMA 直写主机内存的 Disruptor Ring Buffer
  - 更新 Sequence 用硬件操作替代 CAS
  - 全程无 CPU 介入
  - 注意：往返延迟（RTT）约为 200~600ns，取决于 PCIe 版本和配置
- **优势**：
  - 无软件开销
  - 延迟确定性极高
  - 可直写 Ring Buffer
- **限制**：
  - PCIe 链路延迟（Gen3×8 约 500~800ns 往返）
  - 硬件成本高

### 关键优化点

#### epoll/io_uring 优化清单

- ✅ CPU 亲和性绑定（避免跨 NUMA）
- ✅ 大页内存（减少 TLB miss）
- ✅ 关闭中断（轮询模式）
- ✅ 零拷贝（`SO_ZEROCOPY` / `IORING_OP_RECV_ZC`）
- ✅ 批处理（io_uring 批提交）
- ✅ 实时内核（RT kernel，减少调度抖动）

#### DPDK 优化清单

- ✅ 专用 CPU 核心（隔离系统进程）
- ✅ 大页内存（2MB/1GB pages）
- ✅ Poll-Mode Driver（禁用中断）
- ✅ 零拷贝（避免数据拷贝）
- ✅ NUMA 亲和性（CPU/内存/网卡同 NUMA）
- ✅ Burst size = 1（最小延迟，牺牲吞吐）
- ✅ 关闭 CPU 节能（C-states）

#### FPGA 优化清单

- ✅ 片上 BRAM/URAM 实现 Ring Buffer（最小延迟）
- ✅ PCIe Gen4/Gen5（降低链路延迟）
- ✅ 硬件 Sequence 更新（替代 CAS）
- ✅ 零拷贝 DMA（直写用户空间）
- ✅ 硬件时间戳（精确测量）

### 参考数据来源

- **DPDK 实测**：PerfDB 项目在极致优化下达到 P50 ~0.3μs，P99 ~0.8μs（**单向收包延迟**）
  - 来源：[LinkedIn - True DPDK Implementation](https://www.linkedin.com/pulse/true-dpdk-implementation-how-perfdb-devlopement-achieves-fenil-sonani-hrc5f)
  - 说明：这是从网卡收包到用户态处理的单向延迟，不包含发送返回路径
  
- **FPGA 实测**：CESNET NDK-FPGA 在 PCIe Gen3×8 上测得 ~822ns 往返延迟（64字节包，中位数）
  - 来源：[CESNET NDK-FPGA - DMA Calypte](https://cesnet.github.io/ndk-fpga/devel/comp/dma/dma_calypte/readme.html)
  - 说明：这是 Host → FPGA → Host 的往返延迟（RTT），单向延迟约为 ~400-500ns（假设对称路径）
  - 注意：FPGA 的 822ns 往返延迟与 DPDK 的 0.3-0.8μs 单向延迟不在同一维度，FPGA 单向延迟约为 DPDK 的 50-60%
  
- **io_uring 实测**：相比 epoll 可减少 30~50% 延迟（P99 尾延迟）
  - 来源：
    - [flashQ 基准测试](https://dev.to/egeominotti/iouring-how-flashq-achieves-kernel-level-async-io-performance-15d2) - P99 延迟降低 ~50%
    - [Alibaba Cloud io_uring 分析](https://www.alibabacloud.com/blog/io-uring-vs--epoll-which-is-better-in-network-programming_599544) - 高并发场景下吞吐量提升 ~10%
    - [Swoole 6.2 io_uring 升级](https://medium.com/@mariasocute/swoole-6-2-revolutionary-upgrade-iouring-replaces-epoll-asynchronous-io-performance-soars-to-3-c42ffd76eeba) - 平均延迟从 2.81ms 降至 1.36ms（约 50%）
  
- **epoll 实测**：典型 HTTP 服务器延迟 200~700μs（包含应用处理），纯收包路径 10~50μs
  - 来源：[Linux Kernel vs DPDK HTTP Performance](https://talawah.io/blog/linux-kernel-vs-dpdk-http-performance-showdown/)

## 6.1 百万级连接支持

### 内存估算

单连接内存开销（WebSocket 空闲状态）：
- uWebSockets 连接对象：~200-400 bytes
- 用户数据（UserData）：~100-200 bytes
- TCP 缓冲区：~8-16 KB（系统默认）
- **总计：~10-20 KB/连接**

百万连接内存需求：**20-30 GB**

### 系统配置

#### 文件描述符限制

```bash
# /etc/security/limits.conf
* soft nofile 1048576
* hard nofile 1048576

# /etc/sysctl.conf
fs.file-max = 2097152
```

#### TCP 参数优化

```bash
# /etc/sysctl.conf
# 连接队列大小（SO_REUSEPORT 需要）
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 65535

# TIME_WAIT 优化
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_fin_timeout = 30

# 缓冲区大小
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216

# SO_REUSEPORT 负载均衡（Linux 3.9+）
# 默认已启用，无需配置
# 内核自动将连接分发到不同的监听 socket
```

**SO_REUSEPORT 工作原理**：
1. 多个线程/进程绑定同一端口（`SO_REUSEPORT`）
2. 内核根据 4-tuple (src_ip, src_port, dst_ip, dst_port) 哈希分发连接
3. 相同客户端的连接会路由到同一线程（连接亲和性）
4. 实现无锁负载均衡，性能优于应用层负载均衡

#### 进程内存限制

```bash
# /etc/security/limits.conf
* soft memlock unlimited
* hard memlock unlimited
```

### 多线程架构

**重要**：libuv 事件循环是单线程的，需要多线程架构以充分利用多核 CPU。

```
┌─────────────────────────────────────────────────┐
│  Gateway 进程                                    │
│                                                  │
│  ┌──────────────┐  ┌──────────────┐           │
│  │ Thread 0     │  │ Thread 1     │  ...      │
│  │ libuv loop   │  │ libuv loop   │           │
│  │ Port 8080    │  │ Port 8080    │           │
│  │ (SO_REUSEPORT)│ │ (SO_REUSEPORT)│          │
│  └──────────────┘  └──────────────┘           │
│         │                  │                   │
│         └──────────────────┘                   │
│              │                                  │
│              ↓                                  │
│    ┌─────────────────────┐                     │
│    │ ExchangeCore        │                     │
│    │ (共享，线程安全)     │                     │
│    └─────────────────────┘                     │
└─────────────────────────────────────────────────┘
```

**多线程实现**：

```cpp
#include <thread>
#include <vector>
#include <uWebSockets/App.h>

class MultiThreadGateway {
    std::vector<std::thread> threads_;
    ExchangeCore* exchangeCore_;
    
public:
    void start(int numThreads, int port) {
        for (int i = 0; i < numThreads; ++i) {
            threads_.emplace_back([this, port, i]() {
                // 每个线程创建独立的事件循环
                uWS::App app;
                
                // 配置路由
                setupRoutes(&app);
                
                // 监听端口（SO_REUSEPORT）
                app.listen(port, LIBUS_LISTEN_EXCLUSIVE_PORT, [](auto *token) {
                    if (token) {
                        std::cout << "Thread listening on port " << port << std::endl;
                    }
                });
                
                // 运行事件循环（阻塞）
                app.run();
            });
        }
        
        // 等待所有线程
        for (auto& t : threads_) {
            t.join();
        }
    }
};
```

**SO_REUSEPORT 配置**：

```cpp
// uWebSockets 支持 SO_REUSEPORT
// 使用 LIBUS_LISTEN_EXCLUSIVE_PORT 标志
app.listen(8080, LIBUS_LISTEN_EXCLUSIVE_PORT, [](auto *token) {
    // 多个线程/进程可以绑定同一端口
    // 内核自动负载均衡连接
});
```

**系统配置（SO_REUSEPORT）**：

```bash
# /etc/sysctl.conf
# SO_REUSEPORT 需要 Linux 3.9+
# 默认已支持，无需额外配置

# 验证支持
cat /proc/sys/net/core/somaxconn
# 应该 >= 65535
```

**性能参考**（多线程）：
- **单线程**：100K 连接，~2 GB 内存，10-20% CPU（单核）
- **4 线程**：400K 连接，~8 GB 内存，40-80% CPU（4 核）
- **8 线程**：800K 连接，~16 GB 内存，80-100% CPU（8 核）
- **16 线程**：1M+ 连接，~20-30 GB 内存，充分利用多核

## 7. 多线程实现细节

### 7.1 线程间通信

**ExchangeCore 访问**：
- ExchangeCore 的 RingBuffer 是线程安全的（MPSC 模式）
- 多个 Gateway 线程可以并发调用 `ExchangeApi::placeOrder()`
- 无需额外的锁或同步机制

**WebSocket 推送**：
- 每个线程维护自己的 WebSocket 连接
- 结果推送通过线程本地的 uWebSockets Hub
- 无需跨线程访问其他线程的连接

### 7.2 SO_REUSEPORT 配置

**uWebSockets 配置**：

```cpp
// 方式1：使用 LIBUS_LISTEN_EXCLUSIVE_PORT（推荐）
app.listen(8080, LIBUS_LISTEN_EXCLUSIVE_PORT, [](auto *token) {
    if (token) {
        std::cout << "Listening with SO_REUSEPORT" << std::endl;
    }
});

// 方式2：手动设置 socket 选项
int fd = socket(AF_INET, SOCK_STREAM, 0);
int reuseport = 1;
setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuseport, sizeof(reuseport));
// 然后传递给 uWebSockets
```

**负载均衡机制**：
- 内核根据 4-tuple 哈希分发连接
- 相同客户端的连接会路由到同一线程（连接亲和性）
- 新连接自动负载均衡到不同线程
- 无锁、零开销的负载均衡

**优势**：
- ✅ **内核级负载均衡**：比应用层负载均衡更快
- ✅ **连接亲和性**：同一客户端的连接在同一线程，缓存友好
- ✅ **无锁设计**：内核处理，无需应用层同步
- ✅ **水平扩展**：可以轻松扩展到多进程（多机）

### 7.3 CPU 亲和性（可选）

```cpp
#include <pthread.h>
#include <sched.h>

void setThreadAffinity(int threadId, int cpuCore) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpuCore, &cpuset);
    
    pthread_t thread = pthread_self();
    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}

// 在每个线程中设置
void gatewayThread(int threadId) {
    setThreadAffinity(threadId, threadId % std::thread::hardware_concurrency());
    // ... 运行事件循环
}
```

## 8. 与 ExchangeCore 集成

### 8.1 直接调用

```cpp
// Gateway 直接调用 ExchangeCore C++ API
class Gateway {
    ExchangeCore* exchangeCore_;
    ExchangeApi* api_;
    
public:
    int64_t placeOrder(int64_t uid, int64_t price, int64_t size) {
        CompletableFuture<OrderCommand> future;
        api_->placeNewOrder(uid, price, size, future);
        return future.get()->orderId;
    }
};
```

### 8.2 结果回调

```cpp
// ExchangeCore 处理完成后回调
exchangeCore_->setResultsConsumer([this](OrderCommand* cmd, int64_t seq) {
    // 通过 WebSocket 推送结果
    pushToWebSocket(cmd);
});
```

## 9. 开发计划

### Phase 1: 基础框架
- [ ] 集成 uWebSockets
- [ ] 集成 rapidjson
- [ ] 实现 HTTP REST 基础接口

### Phase 2: 核心功能
- [ ] 实现下单、撤单、查询接口
- [ ] 实现 WebSocket 连接管理
- [ ] 实现结果推送

### Phase 3: 多线程架构
- [ ] 实现多线程事件循环（每个线程一个 libuv loop）
- [ ] 配置 SO_REUSEPORT 支持
- [ ] 实现线程安全的 ExchangeCore 访问
- [ ] 性能测试（单线程 vs 多线程）

### Phase 4: 优化
- [ ] 性能优化（内存池、零拷贝）
- [ ] 集成 mimalloc
- [ ] CPU 亲和性配置
- [ ] 压力测试和调优

## 10. 参考资源

- [uWebSockets](https://github.com/uNetworking/uWebSockets)
- [rapidjson](https://github.com/Tencent/rapidjson)
- [mimalloc](https://github.com/microsoft/mimalloc)
- [exchange-gateway-rest](https://github.com/exchange-core/exchange-gateway-rest) (Java 参考实现)

## 11. 总结

**推荐技术栈**：
- **HTTP/WebSocket**: uWebSockets
- **JSON**: rapidjson
- **内存**: mimalloc

**优势**：
- 性能优秀（业界最佳实践）
- 单头文件，易集成
- 成熟稳定，广泛使用
- 与 C++ ExchangeCore 无缝集成

**无需自己写底层**，专注业务逻辑即可。

