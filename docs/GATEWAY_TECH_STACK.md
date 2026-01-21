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
│  Gateway (C++)                                   │
│  ┌─────────────────┐  ┌─────────────────┐      │
│  │ uWebSockets     │  │ rapidjson       │      │
│  │ (HTTP/WS Server)│  │ (JSON Parse)    │      │
│  └─────────────────┘  └─────────────────┘      │
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
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 65535
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_fin_timeout = 30
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216
```

#### 进程内存限制

```bash
# /etc/security/limits.conf
* soft memlock unlimited
* hard memlock unlimited
```

### 单机架构

```
┌─────────────────────────────────┐
│  Gateway 进程                    │
│  - uWebSockets (libuv)           │
│  - 1M WebSocket 连接            │
│  - 内存：20-30 GB                │
└─────────────────────────────────┘
```

**性能参考**：
- 100K 连接：~2 GB 内存，10-20% CPU
- 500K 连接：~10 GB 内存，30-50% CPU
- 1M 连接：~20 GB 内存，50-70% CPU

## 7. 与 ExchangeCore 集成

### 7.1 直接调用

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

### 7.2 结果回调

```cpp
// ExchangeCore 处理完成后回调
exchangeCore_->setResultsConsumer([this](OrderCommand* cmd, int64_t seq) {
    // 通过 WebSocket 推送结果
    pushToWebSocket(cmd);
});
```

## 8. 开发计划

### Phase 1: 基础框架
- [ ] 集成 uWebSockets
- [ ] 集成 rapidjson
- [ ] 实现 HTTP REST 基础接口

### Phase 2: 核心功能
- [ ] 实现下单、撤单、查询接口
- [ ] 实现 WebSocket 连接管理
- [ ] 实现结果推送

### Phase 3: 优化
- [ ] 性能优化（内存池、零拷贝）
- [ ] 集成 mimalloc
- [ ] 压力测试和调优

## 9. 参考资源

- [uWebSockets](https://github.com/uNetworking/uWebSockets)
- [rapidjson](https://github.com/Tencent/rapidjson)
- [mimalloc](https://github.com/microsoft/mimalloc)
- [exchange-gateway-rest](https://github.com/exchange-core/exchange-gateway-rest) (Java 参考实现)

## 10. 总结

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

