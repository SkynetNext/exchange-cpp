# 分布式撮合系统架构设计

## 整体架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              Client                                      │
│   • 查询 Coordinator 获取分区映射（币对 → 集群节点列表）                  │
│   • 连接任意节点，Follower 返回 REDIRECT 指向 Leader                     │
│   • Leader 变更时收到 NewLeaderEvent，自动切换                           │
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
│  │  • 推送 NewLeaderEvent│     │  • 实时复制          │                   │
│  └─────────────────────┘      └─────────────────────┘                   │
└─────────────────────────────────────────────────────────────────────────┘
```

**职责分离**：
- **Coordinator**：分区映射（币对/交易类型 → 集群），避免盲目 REDIRECT
- **集群自身**：Leader 发现和切换（REDIRECT + NewLeaderEvent）

---

## 核心设计原则

| 原则 | 说明 |
|------|------|
| **按币对分区** | 每个币对由独立集群处理，故障隔离 |
| **单 Leader 写入** | 撮合串行处理，保证公平性 |
| **Gateway 同机** | 省一跳网络 |
| **Raft 共识** | 强一致性，自动选主 |
| **热备秒切** | 故障切换 < 1秒 |

---

## Leader 变更感知

### 正常切换（NewLeaderEvent 推送）

```
旧 Leader 宕机 → Raft 选举 → 新 Leader 推送 NewLeaderEvent → Client 切换
```

### 异常切换（被动发现）

```
Client → 旧 Leader（已宕机）→ 连接失败 → 重连任意节点 → REDIRECT → 新 Leader
```

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
