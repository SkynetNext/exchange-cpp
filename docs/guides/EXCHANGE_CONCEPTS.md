# Exchange Core 核心概念汇总

## 1. 交易模式（Trading Mode）

| 模式 | SymbolType | 说明 | 保证金 | 示例 |
|------|------------|------|--------|------|
| **Exchange（现货）** | `CURRENCY_EXCHANGE_PAIR` (0) | 现货交易，需要全额资金 | ❌ 不需要 | BTC/USD 现货交易 |
| **Margin（保证金）** | `FUTURES_CONTRACT` (1) | 期货合约，使用杠杆 | ✅ 需要 | BTC/USD 期货合约 |
| **Option（期权）** | `OPTION` (2) | 期权合约 | ✅ 需要 | 暂未实现 |

**关键区别**：
- **Exchange 模式**：需要账户中有足够的 baseCurrency 或 quoteCurrency 才能下单
- **Margin 模式**：只需要保证金（margin），可以开多倍杠杆仓位

## 2. Symbol（交易对）结构

| 字段 | 类型 | 说明 | 示例 |
|------|------|------|------|
| `symbolId` | `int32_t` | 交易对唯一标识 | 1 = BTC/USD |
| `type` | `SymbolType` | 交易对类型 | `CURRENCY_EXCHANGE_PAIR` (0), `FUTURES_CONTRACT` (1), `OPTION` (2) |
| `baseCurrency` | `int32_t` | 基础货币 ID | 1 = BTC |
| `quoteCurrency` | `int32_t` | 计价货币 ID | 2 = USD |
| `baseScaleK` | `int64_t` | 基础货币数量倍数（手数） | 1000 = 0.001 BTC/手 |
| `quoteScaleK` | `int64_t` | 计价货币步长倍数 | 100 = 0.01 USD/步 |
| `marginBuy` | `int64_t` | 买入保证金（仅 Margin 模式） | 10000 = 100 USD |
| `marginSell` | `int64_t` | 卖出保证金（仅 Margin 模式） | 10000 = 100 USD |
| `takerFee` | `int64_t` | Taker 手续费（每手，quoteCurrency） | 10 = 0.1 USD/手 |
| `makerFee` | `int64_t` | Maker 手续费（每手，quoteCurrency） | 5 = 0.05 USD/手 |

**示例**：
- **BTC/USD 现货**：`symbolId=1, type=CURRENCY_EXCHANGE_PAIR, baseCurrency=1(BTC), quoteCurrency=2(USD)`
- **BTC/USD 期货**：`symbolId=2, type=FUTURES_CONTRACT, baseCurrency=1(BTC), quoteCurrency=2(USD), marginBuy=10000, marginSell=10000`

## 3. Currency（货币）

| 概念 | 说明 | 存储位置 |
|------|------|----------|
| **Currency ID** | 货币唯一标识（int32_t） | `UserProfile.accounts` 的 key |
| **可用余额（Available Balance）** | 用户在该货币的可用余额（已扣除冻结资金） | `UserProfile.accounts[currency]` |
| **冻结资金机制** | 下单时从余额中直接扣除，撤单/成交后返还 | R1 阶段扣除，R2 阶段结算 |
| **账户类型** | Exchange 模式：baseCurrency + quoteCurrency<br>Margin 模式：quoteCurrency（保证金货币） | `UserProfile.accounts` (map: currency -> balance) |

**重要说明**：
- **不存在 `UserCurrencyAccount` 结构**：账户信息直接存储在 `UserProfile.accounts` 中（`map<int32_t, int64_t>`）
- **冻结资金实现**：在 R1（Risk Pre-hold）阶段，直接从 `accounts[currency]` 中扣除所需金额，所以 `accounts[currency]` 存储的是**可用余额**（总余额 - 冻结资金）
- **撤单/成交后**：在 R2（Risk Release）阶段，根据成交情况调整余额或返还冻结资金

**示例**：
- **Exchange 模式**：用户需要 BTC 和 USD 两个账户
- **Margin 模式**：用户只需要 USD 账户（作为保证金）

## 4. 订单操作（Order Command）

| 操作 | OrderCommandType | 代码 | 说明 | 主要参数 |
|------|------------------|------|------|----------|
| **下单** | `PLACE_ORDER` | 1 | 创建新订单 | `orderId, symbol, price, size, action, orderType, uid, reserveBidPrice` |
| **撤单** | `CANCEL_ORDER` | 2 | 取消现有订单 | `orderId, uid` |
| **改价** | `MOVE_ORDER` | 3 | 修改订单价格 | `orderId, uid, newPrice` |
| **减量** | `REDUCE_ORDER` | 4 | 减少订单数量 | `orderId, uid, reduceSize` |
| **查询订单簿** | `ORDER_BOOK_REQUEST` | 6 | 查询订单簿快照 | `symbol, depth` |

**OrderCommand 完整字段**：
- 基础字段：`command, orderId, symbol, price, size, action, orderType, uid`
- 扩展字段：`reserveBidPrice`（GTC 买单的保留价格，用于快速改价）, `timestamp`, `userCookie`
- 处理字段：`eventsGroup`（分组标识）, `serviceFlags`, `resultCode`（执行结果）
- 事件链：`matcherEvent`（成交事件链）, `marketData`（可选市场数据）

**处理流程**：
```
PLACE_ORDER → R1(风险检查) → ME(撮合) → R2(结算) → ResultsHandler
CANCEL_ORDER → ME(从订单簿移除) → R2(释放冻结资金) → ResultsHandler
MOVE_ORDER → ME(取消旧订单+创建新订单) → R2(调整冻结资金) → ResultsHandler
REDUCE_ORDER → ME(减少订单数量) → R2(释放部分冻结资金) → ResultsHandler
```

## 5. 订单类型（Order Type）

| 类型 | 代码 | 说明 | 执行方式 | 适用场景 |
|------|------|------|----------|----------|
| **GTC** | 0 | Good Till Cancel<br>限价单，直到取消 | 挂单到订单簿，等待成交 | 普通限价单 |
| **IOC** | 1 | Immediate or Cancel<br>立即成交或取消（价格上限） | 立即撮合，未成交部分取消 | 市价单（带价格保护） |
| **IOC_BUDGET** | 2 | IOC with Budget<br>立即成交或取消（总金额上限） | 立即撮合，未成交部分取消 | 市价单（带总金额限制） |
| **FOK** | 3 | Fill or Kill<br>全部成交或全部取消（价格上限） | 全部成交才接受，否则全部取消 | 大单，要求全部成交 |
| **FOK_BUDGET** | 4 | FOK with Budget<br>全部成交或全部取消（总金额上限） | 全部成交才接受，否则全部取消 | 大单，要求全部成交（带总金额限制） |

**执行逻辑**：
- **GTC**：挂单，等待对手方成交
- **IOC/IOC_BUDGET**：立即撮合，部分成交后剩余部分取消
- **FOK/FOK_BUDGET**：必须全部成交，否则全部取消

## 6. 订单方向（Order Action）

| 方向 | 代码 | 说明 | 对应操作 |
|------|------|------|----------|
| **ASK** | 0 | 卖出/做空 | 卖出 baseCurrency，获得 quoteCurrency |
| **BID** | 1 | 买入/做多 | 买入 baseCurrency，支付 quoteCurrency |

**Margin 模式下的含义**：
- **BID（买入）**：开多仓（Long Position）
- **ASK（卖出）**：开空仓（Short Position）

## 7. 完整示例

### Exchange 模式示例（BTC/USD 现货）

```
用户下单：买入 0.1 BTC，价格 50000 USD
- symbolId: 1 (BTC/USD)
- type: CURRENCY_EXCHANGE_PAIR
- action: BID (买入)
- orderType: GTC (限价单)
- price: 5000000 (50000 USD，精度 0.01)
- size: 100 (0.1 BTC，精度 0.001)
- 需要资金：5000 USD（全额）
```

### Margin 模式示例（BTC/USD 期货）

```
用户下单：买入 1 BTC，价格 50000 USD，杠杆 10x
- symbolId: 2 (BTC/USD 期货)
- type: FUTURES_CONTRACT
- action: BID (买入/开多仓)
- orderType: GTC (限价单)
- price: 5000000 (50000 USD)
- size: 1000 (1 BTC)
- 需要保证金：500 USD（5000 USD / 10x 杠杆）
- marginBuy: 50000 (500 USD，精度 0.01)
```

## 8. 数据流示例

```
1. 用户下单（PLACE_ORDER）
   ↓
2. GroupingProcessor：分组（eventsGroup）
   ↓
3. R1（Risk Pre-hold）：
   - Exchange 模式：检查账户余额是否足够
   - Margin 模式：检查保证金是否足够
   - 冻结资金/保证金
   ↓
4. ME（Matching Engine）：
   - 撮合订单
   - 生成 TradeEvent 链
   ↓
5. R2（Risk Release）：
   - Exchange 模式：扣除/增加余额
   - Margin 模式：更新仓位，结算盈亏
   ↓
6. ResultsHandler：返回结果给用户
```

## 9. 关键数据结构

| 结构 | 说明 | 关键字段 |
|------|------|----------|
| `OrderCommand` | Disruptor 事件结构 | `command, orderId, symbol, price, size, action, orderType, uid, reserveBidPrice, timestamp, eventsGroup, resultCode, matcherEvent` |
| `CoreSymbolSpecification` | 交易对配置 | `symbolId, type, baseCurrency, quoteCurrency, baseScaleK, quoteScaleK, takerFee, makerFee, marginBuy, marginSell` |
| `UserProfile` | 用户账户信息 | `uid, accounts[currency]` (map: currency -> balance), `positions[symbolId]` (map: symbolId -> SymbolPositionRecord*), `adjustmentsCounter, userStatus` |
| `SymbolPositionRecord` | 仓位记录（仅 Margin） | `uid, symbol, currency, direction, openVolume, openPriceSum, profit, pendingSellSize, pendingBuySize` |

**重要说明**：
- **不存在 `UserCurrencyAccount` 结构**：账户余额直接存储在 `UserProfile.accounts` 中（`map<int32_t, int64_t>`），其中 value 是可用余额（已扣除冻结资金）
- **冻结资金**：通过直接从 `accounts[currency]` 扣除实现，无需单独字段
- **每个用户可以有多个 `SymbolPositionRecord`**：`UserProfile.positions` 是一个 map（`map<int32_t, SymbolPositionRecord *>`），key 是 `symbolId`，value 是指向仓位记录的指针
- **每个交易对一个仓位记录**：用户在某个交易对（symbol）上有仓位时，会有一个对应的 `SymbolPositionRecord`
- **仅 Margin 模式**：只有在 `FUTURES_CONTRACT` 类型的交易对中才会有仓位记录，`CURRENCY_EXCHANGE_PAIR` 类型不需要
- **延迟创建**：仓位记录在首次下单时按需创建，当仓位为空（`IsEmpty()`）时可以被移除

