# Java/C++ 性能对比测试指南

## 运行方式

### 方法1：直接运行可执行文件（推荐）

#### C++ 版本

```bash
# 基础对比测试（快速）
./build/tests/test_perf_latency --gtest_filter="PerfLatency.TestLatencyMargin:PerfLatency.TestLatencyExchange"
./build/tests/test_perf_throughput --gtest_filter="PerfThroughput.TestThroughputMargin:PerfThroughput.TestThroughputExchange"

# 标准对比测试（推荐）
./build/tests/test_perf_latency --gtest_filter="PerfLatency.TestLatencyMargin:PerfLatency.TestLatencyExchange:PerfLatency.TestLatencyMultiSymbolMedium"
./build/tests/test_perf_throughput --gtest_filter="PerfThroughput.TestThroughputMargin:PerfThroughput.TestThroughputExchange:PerfThroughput.TestThroughputMultiSymbolMedium"
```

#### Java 版本

```bash
# 基础对比测试
mvn test -Dtest=PerfLatency#testLatencyMargin,PerfLatency#testLatencyExchange
mvn test -Dtest=PerfThroughput#testThroughputMargin,PerfThroughput#testThroughputExchange

# 标准对比测试
mvn test -Dtest=PerfLatency#testLatencyMargin,PerfLatency#testLatencyExchange,PerfLatency#testLatencyMultiSymbolMedium
mvn test -Dtest=PerfThroughput#testThroughputMargin,PerfThroughput#testThroughputExchange,PerfThroughput#testThroughputMultiSymbolMedium
```

### 方法2：使用 ctest（运行整个测试套件）

```bash
# 运行所有性能测试
ctest -R "^Perf"

# 运行特定测试套件
ctest -R "^PerfLatency"
ctest -R "^PerfThroughput"
```

## 推荐的性能对比测试组合

### 方案A：快速对比（~15-20分钟）

**C++:**
```bash
./build/tests/test_perf_latency --gtest_filter="PerfLatency.TestLatencyMargin:PerfLatency.TestLatencyExchange"
./build/tests/test_perf_throughput --gtest_filter="PerfThroughput.TestThroughputMargin:PerfThroughput.TestThroughputExchange"
```

**Java:**
```bash
mvn test -Dtest=PerfLatency#testLatencyMargin,PerfLatency#testLatencyExchange
mvn test -Dtest=PerfThroughput#testThroughputMargin,PerfThroughput#testThroughputExchange
```

### 方案B：标准对比（~30-40分钟）- **推荐**

**C++:**
```bash
./build/tests/test_perf_latency --gtest_filter="PerfLatency.TestLatencyMargin:PerfLatency.TestLatencyExchange:PerfLatency.TestLatencyMultiSymbolMedium"
./build/tests/test_perf_throughput --gtest_filter="PerfThroughput.TestThroughputMargin:PerfThroughput.TestThroughputExchange:PerfThroughput.TestThroughputMultiSymbolMedium"
```

**Java:**
```bash
mvn test -Dtest=PerfLatency#testLatencyMargin,PerfLatency#testLatencyExchange,PerfLatency#testLatencyMultiSymbolMedium
mvn test -Dtest=PerfThroughput#testThroughputMargin,PerfThroughput#testThroughputExchange,PerfThroughput#testThroughputMultiSymbolMedium
```

## 测试用例说明

| 测试用例 | 规模 | 预计时间 | 说明 |
|---------|------|----------|------|
| `TestLatencyMargin` | 单符号，3M交易 | ~1-2分钟 | Margin模式延迟 |
| `TestLatencyExchange` | 单符号，1M交易 | ~1分钟 | Exchange模式延迟 |
| `TestThroughputMargin` | 单符号，3M交易 | ~5-10分钟 | Margin模式吞吐量 |
| `TestThroughputExchange` | 单符号，1M交易 | ~3-5分钟 | Exchange模式吞吐量 |
| `TestLatencyMultiSymbolMedium` | 10K符号，3M交易 | ~5-10分钟 | 中等负载延迟 |
| `TestThroughputMultiSymbolMedium` | 10K符号，3M交易 | ~10-20分钟 | 中等负载吞吐量 |

## 性能指标对比

### Latency 测试输出
- **P50**: 中位数延迟
- **P99**: 99%延迟
- **P99.9**: 99.9%延迟
- **平均延迟**: 平均响应时间

### Throughput 测试输出
- **MT/s**: 百万交易/秒
- **平均吞吐量**: 多次迭代的平均值
- **每次迭代**: 单次运行的吞吐量

## 注意事项

1. **环境一致性**：确保 Java 和 C++ 在相同硬件上运行
2. **JVM 参数**：Java 测试建议使用相同的 JVM 参数
3. **多次运行**：建议运行3-5次取平均值
4. **系统资源**：记录 CPU、内存使用情况
5. **禁用 Huge 测试**：默认已禁用，避免运行时间过长

