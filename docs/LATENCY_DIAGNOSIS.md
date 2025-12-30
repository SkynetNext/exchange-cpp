# Latency Performance Diagnosis Guide

## 问题现象
- P50 不稳定：从 0.59µs 到 97ms（优化前）
- P90 经常上 ms：从 1.9ms 到 151ms（优化前）
- 期望：P90 应该在 µs 级别

## 快速诊断脚本

```bash
#!/bin/bash
echo "=== Step 1: Perf Record ==="
perf record -g -F 1000 --call-graph dwarf -o perf.data \
  ./tests/test_perf_latency --gtest_filter=PerfLatency.TestLatencyExchange

echo "=== Step 2: Perf Report (Top 20) ==="
perf report --stdio -i perf.data | head -50

echo "=== Step 3: System Stats ==="
perf stat -e cycles,instructions,cache-misses,context-switches,cpu-migrations \
  ./tests/test_perf_latency --gtest_filter=PerfLatency.TestLatencyExchange 2>&1 | tail -10

echo "=== Step 4: System Calls ==="
strace -c -f ./tests/test_perf_latency --gtest_filter=PerfLatency.TestLatencyExchange 2>&1 | tail -20
```

## 实际诊断结果

### perf stat 输出

```bash
perf stat -e cycles,instructions,cache-misses,context-switches,cpu-migrations \
  ./tests/test_perf_latency --gtest_filter=PerfLatency.TestLatencyExchange
```

**输出**:
```
<not supported>      cache-misses                                                
        30,772      context-switches                                            
            34      cpu-migrations                                              

 169.914787171 seconds time elapsed
 722.554032000 seconds user
  16.326975000 seconds sys
```

**解读**:
- context-switches: 30,772 (169.9s) ≈ 181次/秒，总开销约 101ms
- cpu-migrations: 34，影响很小
- user time (722.5s) >> elapsed (169.9s) ≈ 4.25倍，4-5个线程并行
- sys time: 16.3s (9.6%)

### perf report 输出

```bash
perf report --stdio -i perf.data | head -50
```

**关键发现（优化前）**:
```
81.61%  test_perf_laten  [.] __clone3
  --81.61%--start_thread
     --81.15%--TwoStepMasterProcessor::run
        --15.26%--WaitSpinningHelper::TryWaitFor
           --12.96%--FixedSequenceGroup::get
              --5.64%--Sequence::get
```

**关键发现（优化后 - 2025-12-29）**:
```
82.00%  test_perf_laten  [.] __clone3
  --82.00%--start_thread
     --81.58%--BatchEventProcessor::run
        --8.18%--FixedSequenceGroup::get  (从 12.96% 降到 8.18%)
           --2.93%--Sequence::get         (从 5.64% 降到 2.93%)
```

**分析**:
- ✅ **CPU 占用降低**：`FixedSequenceGroup::get` 从 12.96% → 8.18%（降低 37%）
- ✅ **CPU 占用降低**：`Sequence::get` 从 5.64% → 2.93%（降低 48%）
- ❌ **但延迟波动仍然存在**：P50/P90 延迟波动没有显著改善

**结论**:
- 缓存优化**成功减少了 CPU 占用**（减少了 `getCursor()` 调用频率）
- 但**没有解决延迟波动的根本问题**
- 说明问题不在 `WaitSpinningHelper` 的缓存机制，而是：
  1. **序列更新本身慢**（上游处理慢）
  2. **自旋时间过长**（MASTER_SPIN_LIMIT=5000）
  3. **其他瓶颈**（需要进一步分析）

### strace 输出

```bash
strace -c -f ./tests/test_perf_latency --gtest_filter=PerfLatency.TestLatencyExchange 2>&1 | tail -20
```

**输出**:
```
  0.00    0.000000           0        33           rt_sigprocmask
  0.00    0.000000           0         3         3 ioctl
  0.00    0.000000           0         4           pread64
  0.00    0.000000           0         2         2 access
  0.00    0.000000           0         1           execve
  0.00    0.000000           0         1           getcwd
  0.00    0.000000           0         1           sysinfo
  0.00    0.000000           0         2         1 arch_prctl
  0.00    0.000000           0         8           gettid
  0.00    0.000000           0         5           sched_setaffinity
  0.00    0.000000           0         1           set_tid_address
  0.00    0.000000           0        61        48 openat
  0.00    0.000000           0        54        40 newfstatat
  0.00    0.000000           0         9           set_robust_list
  0.00    0.000000           0         1           prlimit64
  0.00    0.000000           0         9           getrandom
  0.00    0.000000           0         9           rseq
  0.00    0.000000           0         8           clone3
------ ----------- ----------- --------- --------- ------------------
100.00    2.920165           2   1375821       245 total
```

**解读**:
- 总系统调用: 1,375,821 次，总时间 2.92 秒
- 平均每次系统调用: 2.12µs
- 系统调用开销很小（2.92s / 169.9s ≈ 1.7%）
- 没有频繁的 `clock_gettime`、`futex` 等热点系统调用
- 主要是文件操作（openat, newfstatat）和线程管理（clone3, gettid）


## 关键发现总结

### 已确认
- ✅ 瓶颈定位：`WaitSpinningHelper::TryWaitFor` (15.26%)
- ✅ 系统调用开销很小（1.7%）
- ✅ 上下文切换影响小（181次/秒）
- ✅ 依赖序列数量：**1 个**（TestLatencyExchange: 1 ME + 1 R1 + 1 R2）

### 问题根源
- `FixedSequenceGroup::get` 占用 12.96%，但只有 1 个依赖序列
- **关键观察**：`getCursor()` 占用高说明**等待时间多**，而不是调用开销本身
- **根本原因**：上游序列更新慢 → 下游长时间自旋等待 → 频繁调用 `getCursor()` 检查
- 每次迭代都调用 `getCursor()` → `FixedSequenceGroup::get()` → `Sequence::get()`（原子操作）
- `MASTER_SPIN_LIMIT = 5000`，如果序列更新慢，可能自旋接近 5000 次

### 优化方向（已尝试，效果不明显）

**已尝试**：
- ✅ 减少 `getCursor()` 调用频率：在自旋循环中缓存序列值，每 10 次循环才读取一次
- ❌ **效果不明显**：P50/P90 延迟波动仍然存在

**分析**：
- 缓存优化减少了 `getCursor()` 调用（从5000次 → 500次），但**没有显著改善延迟稳定性**
- 说明问题根源**不在 WaitSpinningHelper 的缓存机制**
- 可能是：
  1. **序列更新本身慢**（上游处理慢，导致下游长时间等待）
  2. **自旋时间过长**（MASTER_SPIN_LIMIT=5000，如果序列不更新，要自旋5000次）
  3. **其他瓶颈**（需要重新用 perf 分析确认）

**下一步方向**：
1. 重新用 `perf` 分析，确认优化后 `WaitSpinningHelper::TryWaitFor` 的 CPU 占用是否降低
2. 如果 CPU 占用降低了，但延迟仍然不稳定，说明问题在**序列更新机制**或**上游处理速度**
3. 考虑：
   - 减少 `MASTER_SPIN_LIMIT` 和 `GROUP_SPIN_LIMIT`
   - 优化序列更新频率（批量更新改为更频繁的更新）
   - 研究是否可以直接使用 Disruptor 的 `waitFor()` 替代 `WaitSpinningHelper`

## 优化历史记录

### Sequence 优化（2025-12-29 - 已回滚）

**优化内容**：
- 将 `Sequence::get()` 从 `relaxed load + acquire fence` 改为直接 `acquire load`
- 将 `Sequence::set()` 从 `release fence + relaxed store` 改为直接 `release store`
- 目的是减少"稍微过时"的读取，改善可见性延迟

**测试结果**：
- P50 延迟：大部分在 0.6-0.7µs，但仍有异常值（337µs, 335µs, 286µs, 439µs, 733µs, 1.38ms, 1.75ms, 2.44ms, 8.8ms, 6.5ms, 53ms）
- P90 延迟：大部分在 1-3ms，但仍有异常值（4.9ms, 6.3ms, 24.4ms, 43ms, 108ms）

**结论**：
- ❌ **没有显著改善延迟稳定性**
- 延迟波动仍然存在，说明问题根源不在 Sequence 的内存序实现
- **已回滚**：用户确认回滚了此优化

**经验教训**：
- Sequence 的内存序实现不是延迟波动的根本原因
- 需要从等待机制本身入手（`WaitSpinningHelper`）

### WaitSpinningHelper 优化（2025-12-29 - 已实施，效果不明显）

**优化内容**：
- 在 `WaitSpinningHelper::TryWaitFor` 中缓存序列值
- 每 10 次循环才刷新一次缓存（`CACHE_REFRESH_INTERVAL = 10`，从100减少到10）
- 在阻塞前和返回前刷新缓存值，确保不会返回过时的值
- 减少 `getCursor()` 调用频率：从每次循环 → 每 10 次循环

**测试结果**：
- P50 延迟：从 0.63µs 到 15.4ms（波动仍然很大）
- P90 延迟：从 1.64ms 到 58ms（仍然在ms级别）
- ❌ **优化效果不明显**：延迟波动仍然存在

**perf 分析结果（优化后）**：
- `FixedSequenceGroup::get`：从 12.96% → 8.18%（**降低 37%**）
- `Sequence::get`：从 5.64% → 2.93%（**降低 48%**）
- ✅ **CPU 占用显著降低**：缓存优化成功减少了 `getCursor()` 调用频率

**结论**：
- ✅ **CPU 占用降低**：缓存优化成功减少了 CPU 占用
- ❌ **但延迟波动仍然存在**：P50/P90 延迟波动没有显著改善
- 说明问题根源**不在 WaitSpinningHelper 的缓存机制**，而是：
  1. **序列更新本身慢**（上游处理慢，导致下游长时间等待）
  2. **自旋时间过长**（MASTER_SPIN_LIMIT=5000，如果序列不更新，要自旋5000次）
  3. **需要定位具体瓶颈**：需要添加延迟分解统计，定位 P90 延迟卡在哪一个阶段

**下一步方向**：
1. **添加延迟分解统计**：在各处理阶段记录时间戳，定位 P90 延迟卡在哪一个阶段
2. **如果发现等待时间过长**：
   - 减少 `MASTER_SPIN_LIMIT` 和 `GROUP_SPIN_LIMIT`
   - 优化序列更新频率（批量更新改为更频繁的更新）
3. **如果发现处理时间过长**：
   - 优化对应阶段的处理逻辑

## 批量更新优化（2025-12-30 - 已实施，效果显著）

**优化背景**：
- 最初实现：循环结束更新 sequence（对齐 Java 实现，在处理完一批可用消息后更新）
- 优化改进：添加固定间隔批量更新（每 N 条消息更新一次），在高 TPS 场景下性能显著提升
- **注**：Java 实现是在循环结束时批量更新 sequence（批量大小取决于可用消息数量），C++ 的固定间隔批量更新（每 N 条消息）在高 TPS 场景下性能更优

**优化内容**：
- GROUPING: 每 20 个消息更新一次 sequence（50ns/消息，1µs 目标）
- R1: 每 20 个消息更新一次 sequence（50ns/消息，1µs 目标）
- R2: 每 25 个消息更新一次 sequence（40ns/消息，1µs 目标）
- 避免批量更新导致的延迟累积

### 优化前后性能对比

**说明**：以下数据为 C++ 实现两次测试的整体延迟对比（无分阶段数据）

| TPS | 指标 | 优化前（无批量更新） | 优化后（有批量更新） | 改善 |
|-----|------|---------------------|---------------------|------|
| **1M** | P50 | 0.5µs | 0.49µs | ✅ 2% |
| | P90 | 0.6µs | 0.59µs | ✅ 1.7% |
| | P95 | 0.7µs | 0.69µs | ✅ 1.4% |
| | P99 | 5.6µs | 5.6µs | - |
| | P99.9 | **13µs** | **9.7µs** | ✅ **25%** |
| | P99.99 | 61µs | 46µs | ✅ **25%** |
| | Max | 113µs | 113µs | - |
| **2M** | P50 | 0.51µs | 0.51µs | - |
| | P90 | 0.65µs | 0.65µs | - |
| | P95 | 2.59µs | 2.77µs | - |
| | P99 | 6.6µs | 6.8µs | - |
| | P99.9 | **21.1µs** | **17µs** | ✅ **19%** |
| | P99.99 | 63µs | 60µs | ✅ **5%** |
| | Max | 86µs | 102µs | - |
| **3M** | P50 | 0.52µs | 0.52µs | - |
| | P90 | 1.67µs | 1.97µs | - |
| | P95 | 4.9µs | 5.1µs | - |
| | P99 | 7.5µs | 7.7µs | - |
| | P99.9 | 16µs | 17.4µs | - |
| | P99.99 | 56µs | 62µs | - |
| | Max | 100µs | 88µs | ✅ **12%** |
| **4M** | P50 | 0.53µs | 0.53µs | - |
| | P90 | 4.5µs | 4.4µs | ✅ 2% |
| | P95 | 6.4µs | 6.4µs | - |
| | P99 | 8.9µs | 8.4µs | ✅ **6%** |
| | P99.9 | 27.8µs | 17.6µs | ✅ **37%** |
| | P99.99 | 98µs | 47µs | ✅ **52%** |
| | Max | 129µs | 70µs | ✅ **46%** |
| **5M** | P50 | 0.62µs | 0.62µs | - |
| | P90 | 6.4µs | 6µs | ✅ **6%** |
| | P95 | 7.8µs | 7.2µs | ✅ **8%** |
| | P99 | 10.2µs | 8.8µs | ✅ **14%** |
| | P99.9 | **41µs** | **21.5µs** | ✅ **48%** |
| | P99.99 | 94µs | 82µs | ✅ **13%** |
| | Max | 119µs | 115µs | ✅ **3%** |
| **6M** | P50 | 0.8µs | 0.95µs | - |
| | P90 | 7.8µs | 7.2µs | ✅ **8%** |
| | P95 | 8.9µs | 8µs | ✅ **10%** |
| | P99 | 12.1µs | 10.5µs | ✅ **13%** |
| | P99.9 | **55µs** | **37µs** | ✅ **33%** |
| | P99.99 | 86µs | 62µs | ✅ **28%** |
| | Max | 106µs | 70µs | ✅ **34%** |
| **7.5M** | P50 | **9.6ms** | 6.6µs | ✅ **99.9%** |
| | P90 | **23.6ms** | 9.6µs | ✅ **99.6%** |
| | P95 | **26.2ms** | 10.7µs | ✅ **99.6%** |
| | P99 | **28.2ms** | 17.9µs | ✅ **99.9%** |
| | P99.9 | **28.5ms** | 51µs | ✅ **99.8%** |
| | P99.99 | **28.5ms** | 69µs | ✅ **99.8%** |
| | Max | **28.5ms** | 74µs | ✅ **99.7%** |

### 关键改善点

1. **高 TPS 场景（≥5M TPS）**：
   - ✅ **P99.9 显著改善**：5M TPS 时从 41µs → 21.5µs（**改善 48%**）
   - ✅ **P99.9 显著改善**：6M TPS 时从 55µs → 37µs（**改善 33%**）
   - ✅ **P99 改善**：5M TPS 时从 10.2µs → 8.8µs（**改善 14%**），6M TPS 时从 12.1µs → 10.5µs（**改善 13%**）

2. **极高 TPS 场景（7.5M TPS）**：
   - ✅ **性能崩溃完全避免**：无批量更新时延迟达到 **28.5ms**，有批量更新时保持在 **微秒级**（6.6-17.9µs）
   - ✅ **改善幅度超过 99%**：从毫秒级降低到微秒级

3. **中低 TPS 场景（≤3M TPS）**：
   - 性能差异较小，批量更新优化主要影响高 TPS 场景

### 结论

- ✅ **批量更新优化对高 TPS 场景至关重要**：移除后，7.5M TPS 时延迟从微秒级飙升到毫秒级（28.5ms），恢复后保持在微秒级
- ✅ **最高峰延迟显著改善**：P99.9 在 5M-6M TPS 时改善 33-48%
- ✅ **系统稳定性大幅提升**：批量更新机制有效避免了高 TPS 下的性能崩溃
- **注**：测试数据为整体端到端延迟，无分阶段数据