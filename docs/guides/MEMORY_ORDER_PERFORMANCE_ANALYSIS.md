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

## 总结

| 架构 | `relaxed_fence` | `acquire` | 性能差异 |
|------|----------------|-----------|---------|
| **x86-64** | `mov` | `mov` | 无差异（相同代码） |
| **ARM** | `ldr + dmb ishld` | `ldar` | 有差异（`acquire` 可能更快） |


