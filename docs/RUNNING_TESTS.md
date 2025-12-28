# 命令行运行测试指南

## 构建测试

```bash
# 在项目根目录
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build .
```

## 运行测试的方法

### 方法1: 使用 ctest (推荐)

```bash
cd build

# 运行所有测试
ctest --output-on-failure

# 运行特定测试套件
ctest -R PerfJournaling --output-on-failure
ctest -R PerfLatency --output-on-failure
ctest -R PerfThroughput --output-on-failure

# 排除性能测试（跳过耗时测试）
ctest -E "Perf.*" --output-on-failure

# 并行运行（多核加速）
ctest --parallel $(nproc) --output-on-failure

# 显示详细输出
ctest --verbose --output-on-failure
```

### 方法2: 直接运行可执行文件

```bash
cd build

# 运行Journaling测试
./tests/test_perf_journaling

# 运行Latency测试
./tests/test_perf_latency

# 运行Throughput测试
./tests/test_perf_throughput

# 运行Persistence测试
./tests/test_perf_persistence
```

### 方法3: 使用 gtest 过滤器运行特定测试

```bash
cd build

# 运行PerfJournaling中的特定测试
./tests/test_perf_journaling --gtest_filter=PerfJournaling.TestJournalingExchange

# 运行PerfLatency中的特定测试
./tests/test_perf_latency --gtest_filter=PerfLatency.TestLatencyExchange

# 运行所有PerfJournaling测试
./tests/test_perf_journaling --gtest_filter=PerfJournaling.*

# 列出所有可用测试
./tests/test_perf_journaling --gtest_list_tests
```

### 方法4: 运行被禁用的测试

```bash
cd build

# 运行包括DISABLED的测试（如TestJournalingMultiSymbolHuge）
./tests/test_perf_journaling --gtest_also_run_disabled_tests
```

## 测试可执行文件列表

| 测试套件 | 可执行文件 | 说明 |
|---------|-----------|------|
| **PerfJournaling** | `test_perf_journaling` | Journaling功能测试 |
| **PerfLatency** | `test_perf_latency` | 延迟性能测试 |
| **PerfThroughput** | `test_perf_throughput` | 吞吐量性能测试 |
| **PerfPersistence** | `test_perf_persistence` | 持久化测试 |
| **PerfLatencyJournaling** | `test_perf_latency_journaling` | 延迟+Journaling测试 |
| **PerfThroughputJournaling** | `test_perf_throughput_journaling` | 吞吐量+Journaling测试 |

## 常用测试命令示例

### 运行所有Journaling测试

```bash
cd build
./tests/test_perf_journaling
```

### 运行特定Journaling测试

```bash
cd build
./tests/test_perf_journaling --gtest_filter=PerfJournaling.TestJournalingExchange
```

### 运行延迟测试（包括被禁用的）

```bash
cd build
./tests/test_perf_latency --gtest_also_run_disabled_tests
```

### 运行所有非性能测试

```bash
cd build
ctest -E "Perf.*" --output-on-failure
```

### 查看测试输出（详细模式）

```bash
cd build
./tests/test_perf_journaling --gtest_color=yes
```

## Windows PowerShell 命令

```powershell
# 进入build目录
cd build

# 运行测试
.\tests\test_perf_journaling.exe

# 使用ctest
ctest --output-on-failure

# 运行特定测试
.\tests\test_perf_journaling.exe --gtest_filter=PerfJournaling.TestJournalingExchange
```

## 性能测试注意事项

- **PerfJournaling**: 基本功能测试，运行时间较短
- **PerfLatency**: 延迟测试，可能需要几分钟
- **PerfThroughput**: 吞吐量测试，可能需要较长时间
- **DISABLED测试**: 需要大量资源（12+线程CPU，32GB RAM），运行时间可能数小时

## 故障排除

### 测试找不到可执行文件

```bash
# 确保已构建测试
cd build
cmake --build . --target test_perf_journaling
```

### 查看测试详细输出

```bash
cd build
ctest -V -R PerfJournaling
```

### 清理并重新构建

```bash
cd build
rm -rf *
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build .
```

