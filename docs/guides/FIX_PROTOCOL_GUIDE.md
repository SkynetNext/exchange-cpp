# FIX协议教程

## 1. 什么是FIX协议？

**FIX (Financial Information eXchange)** 是金融行业标准的电子交易通信协议，用于交易前、交易中和交易后的信息交换。

**核心特点**：标准化、实时性、完整性、跨平台

**应用场景**：订单路由、市场数据、执行报告、风险控制、清算结算

## 2. FIX消息格式

### 2.1 格式选择

**业界主流：文本格式（Tag=Value）**

| 格式 | 占比 | 主要用途 | 性能 |
|------|------|---------|------|
| **文本格式** | **~92-95%** | 一般交易系统 | ~2μs/消息，~300字节 |
| **FAST** | **~4-6%** | 市场数据推送 | ~0.5μs/消息，~150字节 |
| **SBE** | **~1-3%** | 交易执行路径 | ~0.3μs/消息，~100字节 |

**建议**：默认使用文本格式，只有在延迟要求 < 1ms 或吞吐量 > 10万消息/秒时才考虑二进制格式。

### 2.2 文本格式结构

FIX消息使用**标签-值对（Tag=Value）**格式，字段之间用**SOH（ASCII 0x01）**分隔：

```
8=FIX.4.4|9=123|35=D|49=SENDER|56=TARGET|...|10=123|
```

**消息结构**：`[Header] [Body] [Trailer]`

- **Header**：`8=BeginString`, `9=BodyLength`, `35=MsgType`, `49=SenderCompID`, `56=TargetCompID`, `34=MsgSeqNum`, `52=SendingTime`
- **Body**：业务字段
- **Trailer**：`10=CheckSum`（校验和，模256）

**示例消息（NewOrderSingle）**：
```
8=FIX.4.4|9=178|35=D|49=CLIENT|56=EXCHANGE|34=1|52=20231222-10:30:00|11=ORDER001|55=BTC/USD|54=1|38=100|44=50000|40=2|10=123|
```

## 3. 常用消息类型

| MsgType | 名称 | 说明 |
|---------|------|------|
| **交易前** | | |
| `D` | NewOrderSingle | 新订单 |
| `G` | OrderCancelRequest | 撤单请求 |
| `F` | OrderCancelReplaceRequest | 改单请求 |
| `V` | MarketDataRequest | 市场数据请求 |
| `W` | MarketDataSnapshot | 市场数据快照 |
| `X` | MarketDataIncrementalRefresh | 增量市场数据 |
| **交易中** | | |
| `8` | ExecutionReport | 执行报告（订单状态更新） |
| **基础设施** | | |
| `A` | Logon | 登录 |
| `5` | Logout | 登出 |
| `0` | Heartbeat | 心跳 |
| `1` | TestRequest | 测试请求 |
| `2` | ResendRequest | 重发请求 |
| `3` | Reject | 拒绝消息 |
| `4` | SequenceReset | 序号重置 |

## 4. 关键字段

### 4.1 订单字段

| Tag | 字段名 | 说明 |
|-----|--------|------|
| `11` | ClOrdID | 客户端订单ID |
| `37` | OrderID | 交易所订单ID |
| `55` | Symbol | 交易对 |
| `54` | Side | 方向：1=Buy, 2=Sell |
| `38` | OrderQty | 订单数量 |
| `44` | Price | 价格 |
| `40` | OrdType | 订单类型：1=Market, 2=Limit |
| `59` | TimeInForce | 有效期：1=GTC, 3=IOC, 4=FOK |

### 4.2 执行报告字段

| Tag | 字段名 | 说明 |
|-----|--------|------|
| `150` | ExecType | 执行类型：0=New, 4=Canceled, F=Trade |
| `39` | OrdStatus | 订单状态：0=New, 1=PartiallyFilled, 2=Filled, 4=Canceled |
| `14` | CumQty | 累计成交数量 |
| `31` | LastPx | 最后成交价格 |
| `32` | LastQty | 最后成交数量 |
| `151` | LeavesQty | 剩余数量 |

## 5. 会话管理

**登录流程**：
```
Client → Logon (A) [Username, Password, HeartBtInt]
Exchange → Logon (A) [确认登录]
双方开始发送 Heartbeat (0) 保持连接
```

**消息序号管理**：
- `MsgSeqNum (34)`：每条消息都有序号，从1开始递增
- 序号不连续时，使用 `SequenceReset (4)` 填充或 `ResendRequest (2)` 请求重发

## 6. 典型流程

**下单流程**：
```
Client → NewOrderSingle (D)
Exchange → ExecutionReport (8) [New]
Exchange → ExecutionReport (8) [PartiallyFilled]
Exchange → ExecutionReport (8) [Filled]
```

**撤单流程**：
```
Client → OrderCancelRequest (G)
Exchange → ExecutionReport (8) [Canceled]
```

## 7. 与内部系统映射

| FIX消息/字段 | 内部命令 | 说明 |
|-------------|---------|------|
| NewOrderSingle | PLACE_ORDER | 下单 |
| OrderCancelRequest | CANCEL_ORDER | 撤单 |
| OrderCancelReplaceRequest | MOVE_ORDER | 改单 |
| ExecutionReport | ResultsHandler | 执行结果 |
| Side: 1=Buy | OrderAction: BID | 买入 |
| Side: 2=Sell | OrderAction: ASK | 卖出 |
| TimeInForce: 1=GTC | OrderType: GTC | 限价单 |
| TimeInForce: 3=IOC | OrderType: IOC | 立即或取消 |
| TimeInForce: 4=FOK | OrderType: FOK | 全部成交或取消 |

## 8. 消息示例

### 8.1 登录（Logon）

**客户端发送**：
```
8=FIX.4.4|9=123|35=A|49=CLIENT001|56=EXCHANGE|34=1|52=20231222-10:00:00|108=30|553=USER001|554=PASSWORD123|10=234|
```

字段：`35=A`(Logon), `108=30`(心跳间隔), `553=USER001`(用户名), `554=PASSWORD123`(密码)

### 8.2 新订单（NewOrderSingle）

```
8=FIX.4.4|9=178|35=D|49=CLIENT001|56=EXCHANGE|34=2|52=20231222-10:30:00|11=ORD001|55=BTC/USD|54=1|38=100|44=50000|40=2|59=1|10=123|
```

字段：`35=D`(NewOrderSingle), `11=ORD001`(订单ID), `55=BTC/USD`(交易对), `54=1`(Buy), `38=100`(数量), `44=50000`(价格), `40=2`(Limit), `59=1`(GTC)

### 8.3 执行报告（ExecutionReport）

**订单已接受**：
```
8=FIX.4.4|9=245|35=8|49=EXCHANGE|56=CLIENT001|34=1|37=EXCH001|11=ORD001|150=0|39=0|55=BTC/USD|54=1|38=100|44=50000|151=100|10=125|
```

字段：`35=8`(ExecutionReport), `150=0`(ExecType=New), `39=0`(OrdStatus=New), `151=100`(剩余数量)

**部分成交**：
```
8=FIX.4.4|9=245|35=8|...|150=F|39=1|32=30|31=50000|14=30|151=70|10=126|
```

字段：`150=F`(Trade), `39=1`(PartiallyFilled), `32=30`(本次成交), `31=50000`(成交价), `14=30`(累计成交), `151=70`(剩余)

**全部成交**：
```
8=FIX.4.4|9=245|35=8|...|150=F|39=2|32=70|14=100|151=0|10=127|
```

字段：`39=2`(Filled), `14=100`(累计成交), `151=0`(剩余0)

## 9. 实现要点

### 9.1 消息解析

- **分隔符**：使用 SOH (0x01) 分隔字段
- **校验**：验证 CheckSum (10) 和 BodyLength (9)
- **性能优化**：使用字符串视图避免复制，对象池重用消息对象

### 9.2 会话管理

```cpp
class FIXSession {
    uint32_t nextOutgoingSeqNum_;  // 发送序号
    uint32_t nextIncomingSeqNum_;  // 接收序号
    
    bool validateSequenceNumber(uint32_t seqNum) {
        if (seqNum == nextIncomingSeqNum_) {
            nextIncomingSeqNum_++;
            return true;
        } else if (seqNum < nextIncomingSeqNum_) {
            return false;  // 重复消息
        } else {
            sendResendRequest(nextIncomingSeqNum_, seqNum - 1);
            return false;  // 序号跳跃
        }
    }
};
```

### 9.3 字段转换

**FIX → 内部系统**：
```cpp
OrderCommand cmd;
cmd.command = PLACE_ORDER;
cmd.orderId = parseClOrdID(fixMsg);      // Tag 11
cmd.symbol = symbolMap[fixMsg.getSymbol()]; // Tag 55
cmd.action = (fixMsg.getSide() == 1) ? BID : ASK; // Tag 54
cmd.size = fixMsg.getOrderQty();         // Tag 38
cmd.price = fixMsg.getPrice();          // Tag 44
cmd.orderType = mapTimeInForce(fixMsg.getTimeInForce()); // Tag 59
```

**内部系统 → FIX**：
```cpp
ExecutionReport execRpt;
execRpt.setOrderID(cmd.orderId);        // Tag 37
execRpt.setClOrdID(cmd.orderId);        // Tag 11
execRpt.setOrdStatus(mapOrderStatus(cmd.resultCode)); // Tag 39
execRpt.setCumQty(cmd.matcherEvent->cumQty); // Tag 14
execRpt.setLastPx(cmd.matcherEvent->lastPrice); // Tag 31
execRpt.setLastQty(cmd.matcherEvent->lastQty); // Tag 32
```

## 10. 常见问题

**Q1: 如何处理消息序号跳跃？**
- 发送 `ResendRequest (2)` 请求重发
- 使用 `SequenceReset (4)` 填充间隙

**Q2: 心跳超时怎么办？**
- 发送 `TestRequest (1)` 测试连接
- 无响应则断开重连

**Q3: 消息格式错误怎么办？**
- 发送 `Reject (3)` 告知对方
- 记录错误日志，继续处理后续消息

## 11. 最佳实践

1. **会话管理**：验证消息序号，定期发送心跳，实现自动重连
2. **错误处理**：所有错误发送Reject消息，记录详细日志
3. **性能优化**：使用对象池，批量处理消息，异步I/O
4. **安全性**：
   - **传输加密**：生产环境必须使用TLS 1.2+加密（FIX文本格式默认明文，存在安全风险）
   - **身份认证**：验证用户名密码，支持双因素认证
   - **访问控制**：IP白名单，限制访问来源
   - **消息审计**：记录所有消息用于审计和合规
   - **会话安全**：消息序号防止重放攻击，超时自动断开

## 12. 二进制格式补充

### 12.1 FAST（市场数据推送）

- **特点**：压缩率70-90%，支持增量更新
- **典型用户**：CME等大型交易所
- **性能**：~0.5μs/消息，~150字节

### 12.2 SBE（交易执行路径）

- **特点**：零拷贝，固定长度字段，极低延迟
- **典型用户**：HFT公司、做市商
- **性能**：~0.3μs/消息，~100字节

**格式选择决策**：
```
延迟要求 < 1ms？
├─ 否 → 文本格式 ✅
└─ 是 → 
    ├─ 市场数据推送？ → FAST
    └─ 交易执行？ → SBE
```

## 13. 参考资料

- [FIX协议官方规范](https://www.fixtrading.org/online-specification/)
- [FIXimate](https://fiximate.fixtrading.org/)：在线字段查询工具
- [FAST规范](https://www.fixtrading.org/standards/fast/)
- [SBE规范](https://github.com/real-logic/simple-binary-encoding)

**常用FIX引擎库**：
- **QuickFIX**（C++/Java/Python）：最流行的开源FIX引擎
- **FIX Antenna**（C++/Java）：商业FIX引擎，支持FAST
- **OpenFAST**（Java/C++）：FAST协议实现
- **Real Logic SBE**（Java/C++）：SBE协议实现
