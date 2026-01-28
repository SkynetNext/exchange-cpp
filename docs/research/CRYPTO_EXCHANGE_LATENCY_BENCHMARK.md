# 加密货币交易所延迟基准

> **更新日期**: 2026-01-28

---

## 时延数据汇总

| 交易所 | 往返延迟 (RTT) | 吞吐量 | 来源 |
|--------|---------------|--------|------|
| **Coinbase International** | **sub-millisecond** | 100,000 msg/s | [AWS 案例研究](https://aws.amazon.com/solutions/case-studies/coinbase-cryptocurrency-exchange-case-study) |
| **Bybit MMGW** | **5 ms** (当前) / **2.5 ms** (2026计划) | - | [Bybit 新闻稿](https://www.newswire.ca/news-releases/bybit-institutional-head-unveils-new-institutional-credit-architecture-and-ultra-low-latency-execution-layer-860042030.html) |

---

## Coinbase International Exchange

**时延**: sub-millisecond RTT  
**吞吐量**: 100,000 消息/秒  
**部署**: AWS AP-NORTHEAST-1 (东京)，EC2 z1d + cluster placement groups  
**交易量**: $15B+ (截至案例发布时)

**来源**: [AWS 案例研究](https://aws.amazon.com/solutions/case-studies/coinbase-cryptocurrency-exchange-case-study)

---

## Bybit Market Maker Gateway (MMGW)

**时延**: 5 ms RTT (当前，机构 INS 客户) → 2.5 ms (2026计划)  
**改进**: 从 ~30 ms 降至 5 ms (约 83% 降低)

**来源**: [Bybit 机构新闻稿](https://www.newswire.ca/news-releases/bybit-institutional-head-unveils-new-institutional-credit-architecture-and-ultra-low-latency-execution-layer-860042030.html) (2025年12月)

---

*仅包含有公开时延数据的交易所*
