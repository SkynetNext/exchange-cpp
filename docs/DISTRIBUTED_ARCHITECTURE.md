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

## 登录认证

### 双 Token 机制（Access + Refresh）

```
┌─────────┐                              ┌──────────────┐
│ Client  │                              │ Auth Service │
└────┬────┘                              └──────┬───────┘
     │                                          │
     │◄── 1. Login: Access Token (15min) ───────│
     │             + Refresh Token (7d)         │
     │                                          │
     │  2. 直连撮合 Gateway（带 Access Token）   │
     ▼                                          │
┌─────────────────────────────────────┐         │
│  撮合集群（Gateway + 撮合）          │         │
│                                     │         │
│  本地验签（Ed25519），~100ns        │         │
│  • 启动时加载公钥                    │         │
│  • 无需查外部服务                    │         │
└─────────────────────────────────────┘         │
     │                                          │
     │  3. Access Token 过期                    │
     │                                          │
     │── 4. Refresh Token ─────────────────────►│
     │                                          │
     │◄── 5. 新 Access Token ───────────────────│
```

### Token 设计

| Token | 有效期 | 用途 |
|-------|--------|------|
| **Access Token** | 15-30 min | 每次请求携带，撮合集群本地验签 |
| **Refresh Token** | 7-30 d | 仅用于换取新 Access Token，不发给撮合集群 |

**Access Token Payload**:
```
{ uid, perms, exp, jti, signature }
```

### 吊销机制

- Auth Service 维护 Revoke List（仅被吊销的 `jti`）
- 定期推送到撮合集群（增量，通常 < 1000 条）
- Access Token 短期有效，即使泄露影响有限

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
| **账户 LRU 缓存** | 热点用户纳秒级访问 |

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
