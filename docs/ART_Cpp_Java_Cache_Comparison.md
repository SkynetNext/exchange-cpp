# ART Tree: C++ Java-aligned vs Java Cache 对比

仅对比 **C++ Java-aligned**（`perf_long_adaptive_radix_tree_map_java_aligned`）与 **Java**（`PerfLongAdaptiveRadixTreeMap.shouldLoadManyItems()`），二者流程与输出对齐，便于同轮数、同机、同 perf 事件横向对比。

Perf 事件：`cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,L1-icache-load-misses`。

---

## 环境与命令

| 项 | C++ Java-aligned | Java |
|----|------------------|------|
| 命令 | `perf stat -e ... -- ./benchmarks/perf_long_adaptive_radix_tree_map_java_aligned` | `perf stat -e ... -- mvn test -Dtest=tests.pref.PerfLongAdaptiveRadixTreeMap` |
| CPU | WSL2，4 × 3600 MHz | 同机 |
| 说明 | 默认 3 轮，流程/输出与 Java 一致 | 约 3 轮（曾 Ctrl+C）；perf 覆盖整进程（含 mvn/JVM） |

---

## 原始数据与派生

| 指标 | C++ Java-aligned | Java |
|------|------------------|------|
| cache-references:u | 15,881,100,676 | 52,313,903,382 |
| cache-misses:u | 10,613,075,469 | 30,168,606,557 |
| **cache 命中率** | **≈33.17%**（miss 66.83%） | **≈42.33%**（miss 57.67%） |
| L1-dcache-loads:u | 16,142,130,886 | 143,387,193,873 |
| L1-dcache-load-misses:u | 6,229,047,077 | 21,133,520,635 |
| **L1-dcache 命中率** | **≈61.41%**（miss 38.59%） | **≈85.26%**（miss 14.74%） |
| L1-dcache-stores:u | 4,853,076,012 | 38,617,399,003 |
| L1-icache-load-misses:u | 31,161,392 | 898,967,595 |
| time elapsed | 146.1 s | 299.2 s |
| user / sys | 144.8 s / 1.2 s | 6.5 s / 1.1 s（进程统计含 mvn/JVM） |

---

## 简要结论

- **整体 cache 命中率**：Java ≈42%，C++ Java-aligned ≈33%（同机 3 轮单次数据，可作参考）。
- **L1-dcache 命中率**：Java 明显更高（≈85% vs ≈61%）；Java 的 L1-dcache-loads 约为 C++ 的 8.9×，说明 Java 侧更多 L1 数据访问（含 JVM/GC 等），形态不同。
- **L1-icache-load-misses**：Java 约为 C++ 的 29×，与 JVM/GC/类加载等指令路径一致。
- **定性说明**：Java 侧 L1-dcache-loads 远高于 C++，主要由 JVM/运行时占用；对象头与元数据等会增大工作集、降低局部性，理论上可使 ART 相关访问的缓存命中率低于 C++，但当前 perf 无法单独剥离 ART 与 JVM 的贡献。
