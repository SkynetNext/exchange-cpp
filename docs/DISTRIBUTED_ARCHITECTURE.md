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

## 用户认证（Token 缓存模式）

```
┌─────────┐  账号 + 密码 + 2FA      ┌──────────────┐
│ Client  │────────────────────────►│ Auth Service │
└────┬────┘                         └──────┬───────┘
     │                                     │
     │◄─── Access Token + Refresh Token ───┤
     │                                     │
     │                               写入 Redis
     │                               token → {uid, perms, exp}
     │
     │  Authorization: Bearer {token}
     ▼
┌─────────────────────────────────────────────────┐
│  撮合 Gateway                                    │
│                                                 │
│  ┌─────────────────┐      ┌─────────────────┐  │
│  │ 本地 LRU 缓存    │ 未命中│     Redis       │  │
│  │ token → uid     │─────►│ token → {uid,   │  │
│  │ TTL: 10s        │◄─────│   perms, exp}   │  │
│  └─────────────────┘ 回填  └─────────────────┘  │
│                                                 │
└─────────────────────────────────────────────────┘
     │
     ▼
┌───────────────┐
│ Matching      │
│ Engine        │
└───────────────┘
```

### 流程说明

1. **登录**：用户通过 Auth Service 认证，获得 Token
2. **写 Redis**：Auth Service 将 token → {uid, perms, exp} 写入 Redis
3. **请求**：Client 携带 `Authorization: Bearer {token}`
4. **Gateway 验证**：
   - 先查本地 LRU 缓存（~100ns）
   - 未命中 → 直连 Redis 查询（~100μs）
   - 回填本地缓存（TTL 较短，如 10s）
5. **续期**：Token 过期前用 Refresh Token 换新

### 认证延迟

| 操作 | 延迟 |
|------|------|
| **本地 LRU 命中** | ~100ns |
| **Redis 查询** | ~100μs-1ms |

### 设计要点

| 要点 | 说明 |
|------|------|
| **Gateway 直连 Redis** | 无需经过 Auth Service，延迟最低 |
| **本地 LRU 缓存** | 热点用户命中率高，减少 Redis 压力 |
| **短 TTL（10s）** | 平衡性能与撤销时效 |
| **Redis 集群** | 水平扩展，高可用 |

### 安全机制

| 机制 | 说明 |
|------|------|
| **2FA** | 登录时强制验证 |
| **Token 短期有效** | Access Token 15-30 分钟过期 |
| **即时撤销** | 登出/改密时删除 Redis，最多 10s 延迟 |
| **Refresh Token** | 长期有效，用于续期 |
| **权限分离** | READ / TRADE / WITHDRAW |

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

---

## 清算与风险控制架构

### 问题背景

**分区维度冲突**：
- **撮合层**：按币对分区（8台撮合服务器）
- **账户层**：按用户分片（16台账户服务器）
- **矛盾**：用户可能同时操作多个币对，但账户状态需要统一管理

### 架构设计：账户服务分离

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                        撮合服务器集群（按币对分区，8台）                        │
│                                                                              │
│  ┌──────────────────────┐  ┌──────────────────────┐  ┌──────────────────┐  │
│  │ 撮合服务器1           │  │ 撮合服务器2           │  │ 撮合服务器N      │  │
│  │ (BTC/USDT)           │  │ (ETH/USDT)           │  │ (其他币对)       │  │
│  │                      │  │                      │  │                  │  │
│  │ Gateway → R1 → ME → R2│  │ Gateway → R1 → ME → R2│  │ Gateway → R1 → ME│  │
│  │                      │  │                      │  │                  │  │
│  │ R1: 同步调用         │  │ R1: 同步调用         │  │ R1: 同步调用     │  │
│  │ R2: 异步通知         │  │ R2: 异步通知         │  │ R2: 异步通知     │  │
│  └──────────┬───────────┘  └──────────┬───────────┘  └──────────┬───────┘  │
│             │                         │                         │          │
│             │ R1: 同步调用            │ R1: 同步调用            │ R1: 同步调用│
│             │ R2: 异步通知            │ R2: 异步通知            │ R2: 异步通知│
│             │                         │                         │          │
│             └─────────────┬───────────┴───────────┬─────────────┘          │
│                           │                       │                        │
│                           │   并行交互            │                        │
│                           ▼                       ▼                        │
└───────────────────────────┼───────────────────────┼────────────────────────┘
                            │                       │
                            │   按用户ID路由         │
                            │   (uid % 16)          │
                            │                       │
┌───────────────────────────┴───────────────────────┴────────────────────────┐
│                    Account Service 集群（按用户分片，16台）                    │
│                                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ Shard 0      │  │ Shard 1      │  │ Shard 2      │  │ ... Shard 15 │  │
│  │ (uid%16=0)   │  │ (uid%16=1)   │  │ (uid%16=2)   │  │ (uid%16=15)  │  │
│  │              │  │              │  │              │  │              │  │
│  │ 全局视图：    │  │ 全局视图：    │  │ 全局视图：    │  │ 全局视图：    │  │
│  │ - 账户余额    │  │ - 账户余额    │  │ - 账户余额    │  │ - 账户余额    │  │
│  │ - 各币对冻结  │  │ - 各币对冻结  │  │ - 各币对冻结  │  │ - 各币对冻结  │  │
│  │ - 持仓状态    │  │ - 持仓状态    │  │ - 持仓状态    │  │ - 持仓状态    │  │
│  │              │  │              │  │              │  │              │  │
│  │ R1: 检查/冻结 │  │ R1: 检查/冻结 │  │ R1: 检查/冻结 │  │ R1: 检查/冻结 │  │
│  │ R2: 清算/风险 │  │ R2: 清算/风险 │  │ R2: 清算/风险 │  │ R2: 清算/风险 │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘  │
└──────────────────────────────────────────────────────────────────────────────┘

关键特性：
• 撮合服务器并行运行：8台撮合服务器按币对分区，独立处理不同交易对
• 并行交互：所有撮合服务器同时与 Account Service 交互（R1同步，R2异步）
• 用户分片：Account Service 按 uid % 16 路由到对应 Shard，保证用户操作串行化
• 全局视图：每个 Account Service Shard 维护所负责用户的所有币对状态
```

### 数据流

**并行交互示例**：用户A同时在BTC/USDT和ETH/USDT下单

```
时间 T1: 用户A在撮合服务器1 (BTC/USDT) 下单
  │
  ├─ R1阶段：下单检查
  │   ├─ 接收订单：{uid: A, symbol: "BTC/USDT", size: 10, price: 50000}
  │   └─ 同步调用 Account Service Shard 4 (uid % 16 = 4)
  │       ├─ 查询全局状态：账户余额 100,000 USDT，可用保证金 85,000 USDT
  │       ├─ 检查：需要 10,000 USDT，85,000 >= 10,000 ✅
  │       └─ 冻结 10,000 USDT，返回成功
  │
  ├─ 撮合阶段：撮合订单，生成交易
  │
  └─ R2阶段：清算通知
      └─ 异步发送交易事件到 Account Service Shard 4
          └─ {symbol: "BTC/USDT", price: 50000, size: 10, uid: A}

时间 T2: 用户A在撮合服务器2 (ETH/USDT) 下单（并行）
  │
  ├─ R1阶段：下单检查
  │   ├─ 接收订单：{uid: A, symbol: "ETH/USDT", size: 20, price: 3000}
  │   └─ 同步调用 Account Service Shard 4 (uid % 16 = 4)  ← 同一Shard
  │       ├─ 查询全局状态：账户余额 100,000 USDT
  │       │                  BTC/USDT冻结：10,000 USDT（来自T1）
  │       │                  ETH/USDT冻结：0 USDT
  │       │                  可用保证金：75,000 USDT（已扣除BTC冻结）
  │       ├─ 检查：需要 6,000 USDT，75,000 >= 6,000 ✅
  │       └─ 冻结 6,000 USDT，返回成功
  │
  ├─ 撮合阶段：撮合订单，生成交易
  │
  └─ R2阶段：清算通知
      └─ 异步发送交易事件到 Account Service Shard 4
          └─ {symbol: "ETH/USDT", price: 3000, size: 20, uid: A}

Account Service Shard 4 处理（串行化，按用户A）：
  │
  ├─ 接收 BTC/USDT 交易事件（来自撮合服务器1）
  │   ├─ 更新持仓：position["BTC/USDT"] += 10
  │   ├─ 更新价格：lastPrice["BTC/USDT"] = 50000
  │   └─ 释放冻结：frozenAmount -= 10,000
  │
  ├─ 接收 ETH/USDT 交易事件（来自撮合服务器2）
  │   ├─ 更新持仓：position["ETH/USDT"] += 20
  │   ├─ 更新价格：lastPrice["ETH/USDT"] = 3000
  │   └─ 释放冻结：frozenAmount -= 6,000
  │
  └─ 计算全局风险
      ├─ totalFrozen = Σ(pendingOrders.frozenAmount)
      ├─ totalPositionValue = Σ(position × lastPrice)
      ├─ totalMargin = totalPositionValue + totalFrozen
      ├─ marginRatio = totalMargin / accountBalance
      └─ 风险检查：if (marginRatio > threshold) → 强制平仓
```

**关键点**：
- **并行性**：多个撮合服务器同时与 Account Service 交互
- **串行化**：同一用户的请求路由到同一 Shard，保证串行处理
- **全局视图**：每个 Shard 维护所负责用户的所有币对状态

### 关键设计

| 设计点 | 说明 |
|--------|------|
| **撮合服务器并行** | 8台撮合服务器按币对分区，独立并行运行 |
| **并行交互** | 所有撮合服务器同时与 Account Service 交互，互不阻塞 |
| **R1同步调用** | 调用Account Service检查保证金，保证准确性，阻塞当前订单处理 |
| **R2异步通知** | 发送交易事件，不阻塞撮合流程，多个撮合服务器可同时发送 |
| **按用户分片** | Account Service按 `uid % 16` 分片，保证同一用户操作串行化 |
| **全局视图** | 每个Shard维护所负责用户的所有币对状态，支持跨币对风险计算 |
| **风险前置** | R1阶段拒绝超额订单，避免无效撮合，减少系统负载 |

### 延迟分析

- **R1阶段**：Account Service调用 ~100μs-1ms（可接受）
- **R2阶段**：异步通知，不阻塞撮合
- **优势**：准确性优先，避免超额下单

### 业界采用

Binance、Coinbase、OKX等大型交易所的主流方案

---

## 参考

- **Aeron Cluster**: https://github.com/aeron-io/aeron
- **Raft 论文**: https://raft.github.io/raft.pdf
