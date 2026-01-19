# 分布式撮合系统架构设计

## 整体架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              Client                                      │
│   • 查询 Coordinator 获取分区映射（币对 → 集群节点列表）                  │
│   • 连接任意节点，Follower 返回 REDIRECT 指向 Leader                     │
│   • Leader 变更时通过 REDIRECT 发现新 Leader                             │
└───────────┬─────────────────────────────────────────────────────────────┘
            │ ① 查询分区映射                   ② 订单请求
            ▼                                  │
┌───────────────────────┐                      │
│   Coordinator         │                      │
│  (分区元数据服务)      │                      │
│  ┌─────────────────┐  │                      │
│  │ BTC/USDT → 集群A │  │                      │
│  │ ETH/USDT → 集群B │  │                      │
│  │ 期货BTC  → 集群C │  │                      │
│  └─────────────────┘  │                      │
└───────────────────────┘                      │
                                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                        撮合集群 A (BTC/USDT)                             │
│  ┌─────────────────────┐      ┌─────────────────────┐                   │
│  │  Gateway + 撮合      │      │  Gateway + 撮合      │                   │
│  │  (Leader)           │◄────►│  (Follower)         │                   │
│  │  • 处理订单          │ Raft │  • 返回 REDIRECT     │                   │
│  │  • 复制日志          │      │  • 实时执行日志       │                   │
│  └─────────────────────┘      └─────────────────────┘                   │
└─────────────────────────────────────────────────────────────────────────┘
```

**职责分离**：
- **Coordinator**：分区映射（币对/交易类型 → 集群），避免盲目 REDIRECT
- **集群自身**：Leader 发现（REDIRECT）、状态复制（Raft 日志）

---

## Aeron Cluster 状态复制

### Follower 工作机制

Follower 采用 **状态机复制**，不只是存储日志，而是实时执行：

```
┌─────────────────────────────────────────────────────────────────────┐
│  启动恢复阶段                                                        │
│                                                                      │
│  1. 加载 Snapshot（最新快照）                                        │
│     → onStart(cluster, snapshotImage)                               │
│     → loadSnapshot() 恢复订单簿状态                                  │
│                                                                      │
│  2. Replay 日志（从 snapshot 位置到日志末尾）                        │
│     → FOLLOWER_REPLAY 状态                                          │
│     → 逐条执行 onSessionMessage()，重建完整状态                      │
│                                                                      │
│  3. Catchup（如果落后于 Leader）                                    │
│     → 从 Leader 拉取缺失日志并执行                                   │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│  运行时同步阶段                                                      │
│                                                                      │
│  Leader 接收订单 → 写入日志 → 复制到 Follower                        │
│                                    │                                │
│                                    ▼                                │
│  Follower 接收日志 → BoundedLogAdapter → onSessionMessage()         │
│                                    │                                │
│                                    ▼                                │
│  立即执行撮合逻辑（与 Leader 相同）                                   │
│                                    │                                │
│                                    ▼                                │
│  订单簿状态与 Leader 保持同步                                        │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键代码路径

```java
// Follower 读取日志时（BoundedLogAdapter.java）
agent.onSessionMessage(position, sessionId, timestamp, buffer, offset, length, header);

// 转发到业务服务（ClusteredServiceAgent.java）
service.onSessionMessage(clientSession, timestamp, buffer, offset, length, header);

// 业务服务执行（如撮合引擎）
matchingEngine.processOrder(order);  // Leader 和 Follower 执行相同逻辑
```

### 对撮合系统的意义

| 特性 | 说明 |
|------|------|
| **热备状态** | Follower 订单簿与 Leader 同步，可立即接管 |
| **快速切换** | 无需重建状态，故障切换 < 500ms |
| **确定性** | Leader/Follower 执行相同逻辑，结果一致 |
| **Snapshot 加速** | 定期快照减少启动时日志回放量 |

---

## API 认证

### HMAC-SHA256 签名（业界标准）

Binance、Coinbase、OKX 等主流交易所均采用 HMAC-SHA256，延迟 ~1μs，支持每次请求验签。

```
┌─────────┐                              ┌──────────────┐
│ Client  │                              │ Auth Service │
└────┬────┘                              └──────┬───────┘
     │                                          │
     │◄── 1. 注册/登录: API Key + Secret ───────│
     │                                          │
     │  2. 每次请求签名                          │
     │  signature = HMAC(secret, payload)       │
     ▼                                          │
┌─────────────────────────────────────┐         │
│  撮合集群（Gateway + 撮合）          │         │
│                                     │         │
│  HMAC 验签 ~1μs                     │         │
│  • 内存存储 apiKey → secret 映射    │         │
│  • 每次请求验签，性能可接受          │         │
└─────────────────────────────────────┘         │
```

### 请求签名示例（Binance 风格）

```cpp
// Client 端
string payload = "symbol=BTCUSDT&side=BUY&timestamp=1737200000";
string signature = hmac_sha256(api_secret, payload);
// 发送: payload + "&signature=" + signature + "&apiKey=" + api_key

// Gateway 端验签
string stored_secret = secrets[api_key];
string expected = hmac_sha256(stored_secret, payload);
if (signature == expected) { /* 通过 */ }
```

### 验签性能对比

| 算法 | 延迟 | 适用场景 |
|------|------|----------|
| **HMAC-SHA256** | ~1μs | API 请求签名（每次验签） |
| **Ed25519** | ~28μs | Token 签发（一次性） |

数据来源：[lib25519 speed benchmark](https://lib25519.cr.yp.to/speed.html)

### 密钥管理

| 密钥 | 存储位置 | 说明 |
|------|----------|------|
| **API Key** | Client + Gateway | 公开标识符 |
| **API Secret** | Client + Gateway | 共享密钥，用于签名/验签 |

### 安全机制

- **时间戳**：请求携带 timestamp，Gateway 拒绝过期请求（±5s）
- **Nonce**：可选，防重放攻击
- **IP 白名单**：可选，限制 API Key 使用范围

---

## 行情推送

### 数据流

```
撮合引擎 ──► EventsRouter ──► WebSocket/TCP ──► 订阅者
                │
                └──► REST（按需查询）
```

### 推送方式

| 协议 | 延迟 | 说明 |
|------|------|------|
| **WebSocket / TCP** | ~1-10ms | 长连接，主动推送 |
| **REST** | 按需 | Pull 模式，低频查询 |

### 实现方式

- **订阅**：用户打开页面时订阅 `/topic/market/trade/{symbol}`
- **推送**：撮合批次完成后，遍历订阅者发送
- **简单直接**：无 Kafka 等中间件

---

## 核心设计原则

| 原则 | 说明 |
|------|------|
| **按币对分区** | 每个币对由独立集群处理，故障隔离 |
| **单 Leader 写入** | 撮合串行处理，保证公平性 |
| **Gateway 同机** | 省一跳网络 |
| **Raft 共识** | 强一致性，自动选主 |
| **热备秒切** | 故障切换 < 1秒 |
| **Token 本地验签** | 无 Session 同步，完全解耦 |
| **WebSocket 推送** | 订阅模式，撮合后直接推送 |

---

## Leader 变更感知

### 核心机制：REDIRECT（被动发现）

```
Client → 旧 Leader（已宕机）→ 连接失败 → 重连任意节点 → REDIRECT → 新 Leader
```

这是故障切换的核心路径，适用于所有场景。

### 可选优化：NewLeaderEvent（主动推送）

```
Graceful 让位 → Raft 选举 → 新 Leader 推送 NewLeaderEvent → Client 切换
```

**注意**：NewLeaderEvent 仅在 Graceful 让位（旧 Leader 主动下线）时有效。
Leader 宕机时连接已断，无法推送。

---

## 故障切换时序

```
时间 ────────────────────────────────────────────────────────►

Leader          Follower                         Client
  │                │                                │
  X (宕机)         │                                │
  │                │                                │
  │         检测心跳超时 (~200ms)                    │
  │         发起选举，成为新 Leader                  │
  │                │                                │
  │                │──── NewLeaderEvent ──────────►│
  │                │                                │
  │                │◄──────── 请求 ────────────────│
  │                │──────── 响应 ────────────────►│

总耗时: < 500ms
```

---

## 客户端重试

```cpp
for (attempt = 0; attempt < 3; attempt++) {
    target = current_leader ?: any_node;
    response = send(target, request);
    
    if (response.ok) return response;
    
    if (response.not_leader) {
        current_leader = response.leader_hint;
        continue;  // 立即重试
    }
    
    if (connection_failed) {
        current_leader = null;
        backoff();
    }
}
```

---

## 实现计划

| Phase | 内容 |
|-------|------|
| **1. 单机撮合** | ✅ 撮合引擎、ART Tree、对象池 |
| **2. 集群化** | Aeron Cluster（Raft）、快照恢复 |
| **3. 分布式** | Coordinator、客户端 SDK、分区迁移 |
| **4. 生产化** | 监控告警、容量规划 |

---

## 参考

- **Aeron Cluster**: https://github.com/aeron-io/aeron
- **Raft 论文**: https://raft.github.io/raft.pdf
