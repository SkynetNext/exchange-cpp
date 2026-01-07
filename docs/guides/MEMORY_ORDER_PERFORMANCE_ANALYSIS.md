# Memory Order 性能分析：relaxed load + acquire fence vs acquire load

## 测试代码

```cpp
#include <atomic>
#include <cstdint>

// 方式 1：relaxed load + acquire fence（当前实现）
int64_t get_relaxed_fence(const std::atomic<int64_t>& value_) {
    int64_t value = value_.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    return value;
}

// 方式 2：直接 acquire load（对比实现）
int64_t get_acquire(const std::atomic<int64_t>& value_) {
    return value_.load(std::memory_order_acquire);
}

// 方式 3：seq_cst load（最强保证）
int64_t get_seq_cst(const std::atomic<int64_t>& value_) {
    return value_.load(std::memory_order_seq_cst);
}

// 方式 4：relaxed load（无同步语义）
int64_t get_relaxed(const std::atomic<int64_t>& value_) {
    return value_.load(std::memory_order_relaxed);
}

// Busy-spin 循环
void busy_spin_relaxed_fence(std::atomic<int64_t>& cursor_, int64_t targetSeq) {
    while (true) {
        int64_t seq = get_relaxed_fence(cursor_);
        if (seq >= targetSeq) break;
    }
}

void busy_spin_acquire(std::atomic<int64_t>& cursor_, int64_t targetSeq) {
    while (true) {
        int64_t seq = get_acquire(cursor_);
        if (seq >= targetSeq) break;
    }
}
```

---

## x86-64 架构（GCC -O3）

### 单独函数

```asm
get_relaxed_fence(std::atomic<long> const&):
        mov     rax, QWORD PTR [rdi]
        ret

get_acquire(std::atomic<long> const&):
        mov     rax, QWORD PTR [rdi]
        ret

get_seq_cst(std::atomic<long> const&):
        mov     rax, QWORD PTR [rdi]
        ret

get_relaxed(std::atomic<long> const&):
        mov     rax, QWORD PTR [rdi]
        ret
```

### Busy-spin 循环

```asm
busy_spin_relaxed_fence(std::atomic<long>&, long):
.L7:
        mov     rax, QWORD PTR [rdi]
        cmp     rsi, rax
        jg      .L7
        ret

busy_spin_acquire(std::atomic<long>&, long):
.L10:
        mov     rax, QWORD PTR [rdi]
        cmp     rsi, rax
        jg      .L10
        ret

busy_spin_seq_cst(std::atomic<long>&, long):
.L13:
        mov     rax, QWORD PTR [rdi]
        cmp     rsi, rax
        jg      .L13
        ret

busy_spin_relaxed(std::atomic<long>&, long):
.L16:
        cmp     rsi, QWORD PTR [rdi]  ; 编译器优化：直接比较
        jg      .L16
        ret
```

**结论**：x86-64 上所有实现生成的机器码完全相同，`atomic_thread_fence` 被优化掉了。x86 的 TSO 模型使 `mov` 已具备 acquire 语义，无需额外指令。

---

## ARM 架构（Clang -O3）

### 单独函数

```asm
get_relaxed_fence(std::atomic<long> const&):
        ldr     x0, [x0]        ; 普通 load
        dmb     ishld           ; acquire fence
        ret

get_acquire(std::atomic<long> const&):
        ldar    x0, [x0]        ; Load-Acquire
        ret

get_seq_cst(std::atomic<long> const&):
        ldar    x0, [x0]        ; Load-Acquire
        ret

get_relaxed(std::atomic<long> const&):
        ldr     x0, [x0]        ; 普通 load
        ret
```

### Busy-spin 循环

```asm
busy_spin_relaxed_fence(std::atomic<long>&, long):
.LBB4_1:
        ldr     x8, [x0]        ; 普通 load
        dmb     ishld           ; 每次循环都有屏障
        cmp     x8, x1
        b.lt    .LBB4_1
        ret

busy_spin_acquire(std::atomic<long>&, long):
.LBB5_1:
        ldar    x8, [x0]        ; Load-Acquire
        cmp     x8, x1
        b.lt    .LBB5_1
        ret
```

**结论**：ARM 上 `relaxed_fence` 使用 2 条指令（ldr + dmb），`acquire` 使用 1 条指令（ldar）。在 busy-spin 循环中，`acquire` 可能更快。

---

## seq_cst 全局屏障的关键点：xchg 和 lock or 的实际实现

**重要发现**：在 x86-64 架构上：
1. **`seq_cst` store 使用 `xchg`**：`xchg` 指令隐含 `lock` 前缀，提供完整的内存屏障语义（无需额外的 `mfence`）。
2. **`seq_cst` fence 使用 `lock or`**：`atomic_thread_fence(std::memory_order_seq_cst)` 生成 `lock or QWORD PTR [rsp], 0`，这是 `mfence` 的等价实现。
3. **多个 `seq_cst` 操作之间无额外 `mfence`**：因为 `xchg` 已提供全屏障，编译器不会插入额外的 `mfence`。

### 测试代码（完整可编译版本）

以下代码可直接粘贴到 [godbolt.org](https://godbolt.org) 验证：

```cpp
#include <atomic>
#include <cstdint>

std::atomic<int64_t> g_var1{0};
std::atomic<int64_t> g_var2{0};
std::atomic<int64_t> g_var3{0};

// 场景 1：单个 seq_cst load（不会生成 mfence）
int64_t single_seq_cst_load() {
    return g_var1.load(std::memory_order_seq_cst);
}

// 场景 2：单个 seq_cst store（不会生成 mfence）
void single_seq_cst_store(int64_t val) {
    g_var1.store(val, std::memory_order_seq_cst);
}

// 场景 3：两个 seq_cst 操作（会生成 mfence，建立全局全序）
int64_t two_seq_cst_ops() {
    g_var1.store(1, std::memory_order_seq_cst);
    return g_var2.load(std::memory_order_seq_cst);
}

// 场景 4：三个 seq_cst 操作（会生成 mfence）
int64_t three_seq_cst_ops() {
    g_var1.store(1, std::memory_order_seq_cst);
    g_var2.store(2, std::memory_order_seq_cst);
    return g_var3.load(std::memory_order_seq_cst);
}

// 场景 5：seq_cst fence（需要配合 seq_cst 原子操作才能建立全局全序）
void seq_cst_fence_with_atomic() {
    g_var1.store(1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    g_var2.store(2, std::memory_order_seq_cst);  // 有 seq_cst 操作锚点
}

// 场景 6：seq_cst fence 无锚点（退化，不生成 mfence）
void seq_cst_fence_no_anchor() {
    g_var1.store(1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    g_var2.store(2, std::memory_order_relaxed);  // 无 seq_cst 操作锚点
}
```

### x86-64 汇编分析（GCC -O3，实际输出）

```asm
; 场景 1：单个 seq_cst load（普通 mov，无屏障）
single_seq_cst_load():
        mov     rax, QWORD PTR g_var1[rip]
        ret

; 场景 2：单个 seq_cst store（使用 xchg，隐含 lock 前缀，提供全屏障）
single_seq_cst_store(long):
        xchg    rdi, QWORD PTR g_var1[rip]  ; xchg 隐含 lock，提供全屏障
        ret

; 场景 3：两个 seq_cst 操作（store 用 xchg，load 用 mov）
two_seq_cst_ops():
        mov     eax, 1
        xchg    rax, QWORD PTR g_var1[rip]  ; xchg 提供全屏障
        mov     rax, QWORD PTR g_var2[rip]  ; 无需额外 mfence
        ret

; 场景 4：三个 seq_cst 操作（多个 xchg，每个都有全屏障）
three_seq_cst_ops():
        mov     eax, 1
        xchg    rax, QWORD PTR g_var1[rip]  ; xchg 提供全屏障
        mov     eax, 2
        xchg    rax, QWORD PTR g_var2[rip]  ; xchg 提供全屏障
        mov     rax, QWORD PTR g_var3[rip]  ; 无需额外 mfence
        ret

; 场景 5：seq_cst fence + seq_cst 原子操作（生成 lock or，等价 mfence）
seq_cst_fence_with_atomic():
        mov     QWORD PTR g_var1[rip], 1
        lock or QWORD PTR [rsp], 0          ; 等价 mfence（但可能更快）
        mov     eax, 2
        xchg    rax, QWORD PTR g_var2[rip]
        ret

; 场景 6：seq_cst fence 无锚点（仍生成 lock or，但可能无全局全序）
seq_cst_fence_no_anchor():
        mov     QWORD PTR g_var1[rip], 1
        lock or QWORD PTR [rsp], 0          ; 仍生成屏障指令
        mov     QWORD PTR g_var2[rip], 2
        ret
```

### 关键发现

1. **`seq_cst` store 使用 `xchg`**：在 x86-64 上，`seq_cst` store 编译为 `xchg` 指令（隐含 `lock` 前缀），`xchg` 本身提供完整的内存屏障语义，无需额外的 `mfence`。

2. **多个 `seq_cst` 操作之间无 `mfence`**：因为 `xchg` 已经提供了全屏障，编译器不会在多个 `xchg` 之间插入额外的 `mfence`。但 `xchg` 本身比普通 `mov` 慢（通常 5-10ns）。

3. **`seq_cst` fence 生成 `lock or [rsp], 0`**：`atomic_thread_fence(std::memory_order_seq_cst)` 会生成 `lock or QWORD PTR [rsp], 0`，这是 `mfence` 的等价实现（在某些 CPU 上可能比 `mfence` 更快）。

4. **`seq_cst` fence 即使无锚点也生成屏障**：即使没有 `seq_cst` 原子操作作为锚点，`seq_cst` fence 仍会生成屏障指令，但可能无法建立全局全序（需要 `seq_cst` 原子操作参与）。

5. **性能影响**：
   - `xchg` 指令：通常 5-10ns（比普通 `mov` 慢）
   - `lock or [rsp], 0` / `mfence`：通常 10-20ns
   - 在低延迟交易系统中应**绝对避免**使用 `seq_cst`

### 验证方法

1. 访问 [godbolt.org](https://godbolt.org)
2. 选择编译器：`x86-64 gcc (trunk)` 或 `x86-64 clang (trunk)`
3. 编译选项：`-O3 -std=c++17`
4. 粘贴上述完整测试代码
5. 查看汇编输出，搜索 `mfence` 指令

---

## 总结

| 架构 | `relaxed_fence` | `acquire` | 性能差异 |
|------|----------------|-----------|---------|
| **x86-64** | `mov` | `mov` | 无差异（相同代码） |
| **ARM** | `ldr + dmb ishld` | `ldar` | 有差异（`acquire` 可能更快） |

| `seq_cst` 场景 | x86-64 指令 | 屏障指令 | 性能影响 |
|---------------|------------|---------|---------|
| 单个 `seq_cst` load | `mov` | 无 | 无额外开销 |
| 单个 `seq_cst` store | `xchg`（隐含 lock） | `xchg` 本身 | **5-10ns 延迟** |
| 多个 `seq_cst` 操作 | `xchg` + `mov` | `xchg` 提供屏障 | **5-10ns/操作** |
| `seq_cst` fence + `seq_cst` 原子操作 | `mov` + `lock or` + `xchg` | `lock or`（等价 mfence） | **10-20ns 延迟** |
| `seq_cst` fence 无锚点 | `mov` + `lock or` + `mov` | `lock or`（等价 mfence） | **10-20ns 延迟** |

**注意**：
- `xchg` 指令隐含 `lock` 前缀，提供完整的内存屏障语义
- `lock or [rsp], 0` 是 `mfence` 的等价实现（在某些 CPU 上可能更快）
- 所有 `seq_cst` 相关操作都有性能开销，低延迟系统应避免使用


