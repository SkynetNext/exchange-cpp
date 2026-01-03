# std::memory_order 参考翻译

本文档是 [cppreference - std::memory_order](https://en.cppreference.com/w/cpp/atomic/memory_order) 的中文翻译。

---

## 概述

`std::memory_order` 指定内存访问，包括常规的非原子内存访问，如何围绕原子操作进行排序。在没有任何约束的多处理器系统上，多个线程同时读取或写入多个变量时，一个线程能观察到的值的变化顺序可能与另一个线程写入它们的顺序不同。实际上，甚至对于多个线程读取的单个变量的更改，如果没有显式的同步，更改的顺序也可能因观察者而异。

所有原子操作的默认行为是**Sequential Consistency 排序**（见下文）。该默认值可能会损害性能，但可以给库的原子操作添加 `std::memory_order` 参数，以指定除 `seq_cst` 之外的确切约束。

---

## 常量

定义于头文件 `<atomic>`

| 名称 | 含义 |
|------|------|
| `memory_order_relaxed` | 宽松操作：不对其他读写操作施加同步或排序约束，只保证此操作的原子性（见下方 Relaxed 排序） |
| `memory_order_consume`<br/>(C++26 中弃用) | 具有此内存序的加载操作对受影响的内存位置执行 consume 操作：当前线程中依赖于当前加载值的读写操作不能在此加载之前重排。在其他线程中 release 同一原子变量的数据依赖写入在当前线程中可见。在大多数平台上，这仅影响编译器优化（见下方 Release-Consume 排序） |
| `memory_order_acquire` | 具有此内存序的加载操作对受影响的内存位置执行 acquire 操作：当前线程中的读写操作不能在此加载之前重排。在其他线程中 release 同一原子变量的所有写入在当前线程中可见（见下方 Release-Acquire 排序） |
| `memory_order_release` | 具有此内存序的存储操作执行 release 操作：当前线程中的读写操作不能在此存储之后重排。当前线程中的所有写入在其他线程中 acquire 同一原子变量时可见，并且携带依赖进入原子变量的写入在其他线程中 consume 同一原子变量时可见（见下方 Release-Acquire 排序和 Release-Consume 排序） |
| `memory_order_acq_rel` | 具有此内存序的 Read-Modify-Write 操作既是 acquire 操作也是 release 操作。当前线程中的内存读写操作不能在加载之前重排，也不能在存储之后重排。在其他线程中 release 同一原子变量的所有写入在修改之前可见，并且修改在其他线程中 acquire 同一原子变量时可见 |
| `memory_order_seq_cst` | 具有此内存序的加载操作执行 acquire 操作，存储执行 release 操作，Read-Modify-Write 执行 acquire 和 release 操作，此外存在单一的总顺序，其中所有线程以相同顺序观察所有修改（见下方 Sequential Consistency 排序） |

---

## Fence-Atomic 同步

线程 A 中的释放栅栏（release fence）F 与线程 B 中的原子 acquire 操作 Y 同步（synchronizes-with），如果：

- 存在一个原子 store 操作 X（任意内存序），
- Y 读取由 X 写入的值（或如果 X 是 release 操作，则读取由 X 为首的 release sequence 写入的值），
- F 在线程 A 中 sequenced-before X。

在这种情况下，所有在线程 A 中 sequenced-before F 的非原子和 relaxed 原子存储操作，都会 happens-before 所有在线程 B 中在 Y 之后从相同位置进行的非原子和 relaxed 原子加载操作。

---

## Atomic-Fence 同步

线程 A 中的原子 release 操作 X 与线程 B 中的获取栅栏（acquire fence）F 同步（synchronizes-with），如果：

- 存在一个原子读取 Y（任意内存序），
- Y 读取由 X 写入的值（或由 X 为首的 release sequence 写入的值），
- Y 在线程 B 中 sequenced-before F。

在这种情况下，所有在线程 A 中 sequenced-before X 的非原子和 relaxed 原子存储操作，都会 happens-before 所有在线程 B 中在 F 之后从相同位置进行的非原子和 relaxed 原子加载操作。

---

## Fence-Fence 同步

线程 A 中的释放栅栏（release fence）FA 与线程 B 中的获取栅栏（acquire fence）FB 同步（synchronizes-with），如果：

- 存在一个原子对象 M，
- 存在一个在线程 A 中修改 M 的原子写操作 X（任意内存序），
- FA 在线程 A 中 sequenced-before X，
- 存在一个在线程 B 中的原子读操作 Y（任意内存序），
- Y 读取由 X 写入的值（或如果 X 是 release 操作，则读取由 X 为首的 release sequence 写入的值），
- Y 在线程 B 中 sequenced-before FB。

在这种情况下，所有在线程 A 中 sequenced-before FA 的非原子和 relaxed 原子存储操作，都会 happens-before 所有在线程 B 中在 FB 之后从相同位置进行的非原子和 relaxed 原子加载操作。

---

## Release-Acquire 排序

如果线程 A 中的原子 store 操作标记为 `memory_order_release`，线程 B 中从同一变量的原子 load 操作标记为 `memory_order_acquire`，并且线程 B 中的 load 读取由线程 A 中的 store 写入的值，则线程 A 中的 store 与线程 B 中的 load 同步（synchronizes-with）。这建立了 release-acquire 同步关系。

从线程 A 的角度来看，所有在原子 store 之前发生的内存写入（包括非原子和 relaxed 原子写入），都会在线程 B 中可见（作为副作用）。也就是说，一旦原子 load 完成，线程 B 保证能看到线程 A 写入内存的所有内容。此承诺仅在 B 实际返回 A store 的值，或返回 release sequence 中较晚的值时才成立。

同步仅在 release 和 acquire 同一原子变量的线程之间建立。其他线程可能看到与任一或两个同步线程不同的内存访问顺序。

在强有序系统上（x86、SPARC TSO、IBM 大型机等），对于大多数操作，Release-Acquire 排序是自动的。不会为此同步模式发出额外的 CPU 指令；只有某些编译器优化会受到影响（例如，禁止编译器将非原子 store 移到 release 原子 store 之后，或禁止在 acquire 原子 load 之前执行非原子 load）。在弱有序系统上（ARM、Itanium、PowerPC），使用特殊的 CPU load 或内存栅栏指令。

互斥锁（如 `std::mutex` 或原子自旋锁）是 Release-Acquire 同步的一个例子：当线程 A release 锁而线程 B acquire 锁时，在线程 A 的临界区（release 之前）中发生的一切都必须对线程 B（acquire 之后）可见，线程 B 正在执行相同的临界区。

**示例代码**：

```cpp
#include <atomic>
#include <cassert>
#include <string>
#include <thread>

std::atomic<std::string*> ptr;
int data;

void producer()
{
    std::string* p = new std::string("Hello");
    data = 42;
    ptr.store(p, std::memory_order_release);
}

void consumer()
{
    std::string* p2;
    while (!(p2 = ptr.load(std::memory_order_acquire)))
        ;
    assert(*p2 == "Hello"); // 永远不会失败
    assert(data == 42);     // 永远不会失败
}

int main()
{
    std::thread t1(producer);
    std::thread t2(consumer);
    t1.join();
    t2.join();
}
```

---

## Sequential Consistency 排序

顺序一致性排序（sequential consistency ordering）可能对多生产者-多消费者场景是必要的，其中所有消费者必须观察到所有生产者以相同顺序发生的操作。

完全的 Sequential Consistency 排序需要在所有多核系统上使用完整的内存栅栏 CPU 指令。这可能成为性能瓶颈，因为它强制受影响的内存访问传播到每个核心。

此示例演示了 Sequential Consistency 排序必要的情况。任何其他排序都可能触发断言，因为线程 `c` 和 `d` 可能以相反的顺序观察到原子变量 `x` 和 `y` 的变化。

**示例代码**：

```cpp
#include <atomic>
#include <cassert>
#include <thread>

std::atomic<bool> x = {false};
std::atomic<bool> y = {false};
std::atomic<int> z = {0};

void write_x()
{
    x.store(true, std::memory_order_seq_cst);
}

void write_y()
{
    y.store(true, std::memory_order_seq_cst);
}

void read_x_then_y()
{
    while (!x.load(std::memory_order_seq_cst))
        ;
    if (y.load(std::memory_order_seq_cst))
        ++z;
}

void read_y_then_x()
{
    while (!y.load(std::memory_order_seq_cst))
        ;
    if (x.load(std::memory_order_seq_cst))
        ++z;
}

int main()
{
    std::thread a(write_x);
    std::thread b(write_y);
    std::thread c(read_x_then_y);
    std::thread d(read_y_then_x);
    a.join();
    b.join();
    c.join();
    d.join();
    assert(z.load() != 0); // 永远不会失败
}
```

---

## Release-Consume 排序

如果线程 A 中的原子 store 操作标记为 `memory_order_release`，线程 B 中从同一变量的原子 load 操作标记为 `memory_order_consume`，并且线程 B 中的 load 读取由线程 A 中的 store 写入的值，则线程 A 中的 store 与线程 B 中的 load 同步（synchronizes-with）。这建立了 release-consume 同步关系。

从线程 A 的角度来看，所有在原子 store 之前发生的内存写入（包括非原子和 relaxed 原子写入），都会在线程 B 中可见（作为副作用），但仅限于线程 B 中依赖于该原子 load 值的操作。也就是说，一旦原子 load 完成，线程 B 保证能看到线程 A 写入内存的内容，但仅限于那些依赖于 load 值的操作。

此承诺仅在 B 实际返回 A 存储的值，或返回 release sequence 中较晚的值时才成立。

**注意**：`memory_order_consume` 在 C++26 中已弃用。

---

## Relaxed 排序

标记为 `memory_order_relaxed` 的原子操作不是同步操作；它们不会在并发内存访问之间强加顺序。它们只保证原子性和修改顺序（modification order）的一致性。

例如，对于 `x` 和 `y` 初始为零：

```cpp
// 线程 1:
r1 = y.load(std::memory_order_relaxed); // A
x.store(r1, std::memory_order_relaxed); // B

// 线程 2:
r2 = x.load(std::memory_order_relaxed); // C
y.store(42, std::memory_order_relaxed); // D
```

允许产生 `r1 == 42 && r2 == 42`。即使线程 1 中的 A sequenced-before B，线程 2 中的 C sequenced-before D，但没有任何约束阻止 D 在 A 之前出现（从修改顺序（modification order）的角度），而 B 在 C 之前出现，因此最终结果可能是 `y == 42 && x == 42`。

Relaxed 内存排序的典型用途是增加计数器，例如 `std::shared_ptr` 的引用计数器，因为这只需要原子性，而不需要排序或同步（注意 `std::shared_ptr` 计数器的递减需要与析构函数进行 acquire-release 同步，但增量只需要 relaxed）。

---

## 与 volatile 的关系

在执行线程内，通过 volatile 左值进行的访问（读取和写入）不能重排到在同一线程内 sequenced-before 或 sequenced-after 的可观察附带作用（observable side effect，包括其他 volatile 访问）之后，但此顺序不保证被另一个线程观察到，因为 volatile 访问不建立线程间同步。

此外，volatile 访问不是原子的（并发读写是数据竞争（data race）），并且不对内存排序（非 volatile 内存访问可以在 volatile 访问周围自由重排）。

一个值得注意的例外是 Visual Studio，在默认设置下，每个 volatile 写入都具有 release 语义，每个 volatile 读取都具有 acquire 语义（Microsoft Docs），因此 volatile 可以用于线程间同步。标准 volatile 语义不适用于多线程编程，尽管它们对于例如与在同一线程中运行的 `std::signal` 处理程序通信（当应用于 `sig_atomic_t` 变量时）是足够的。编译器选项 `/volatile:iso` 可用于恢复与标准一致的行为，这是目标平台为 ARM 时的默认设置。

---

## 形式化描述

### Sequenced-before

在同一线程内，求值 A 可能在求值 B 之前顺序执行（sequenced-before），如[求值顺序](https://en.cppreference.com/w/cpp/language/eval_order)中所述。

### Carries dependency

在同一线程内，如果以下任一条件为真，则求值 A 可能**携带依赖到**（carries dependency into）求值 B（即，B 依赖于 A）：

1. A 的值用作 B 的操作数，**除了**：
   - 如果 B 是对 `std::kill_dependency` 的调用，
   - 如果 A 是内置 `&&`、`||`、`?:` 或 `,` 运算符的左操作数。
2. A 写入标量对象 M，B 从 M 读取。
3. A 携带依赖到另一个求值 X，且 X 携带依赖到 B。

### 修改顺序（Modification order）

所有对任何特定原子变量的修改都以某种顺序发生，这个顺序称为该变量的**修改顺序**（modification order）。修改顺序对所有原子操作都是全局的。

以下四个要求保证所有原子操作都满足：

1. **Write-Write 一致性**：如果原子操作 A 修改了原子对象 M，而操作 B 在 M 的修改顺序中位于 A 之后，则 B 也修改 M。
2. **Read-Read 一致性**：如果原子操作 A 读取了原子对象 M 的值，而操作 B 在 M 的修改顺序中位于 A 之后，则 B 要么读取 A 写入的值，要么读取在修改顺序中位于 A 之后的某个操作写入的值。
3. **Read-Write 一致性**：如果原子操作 A 读取了原子对象 M 的值，而操作 B 在 M 的修改顺序中位于 A 之后，则 B 读取在修改顺序中位于 A 之后的某个操作写入的值。
4. **Write-Read 一致性**：如果原子操作 A 修改了原子对象 M，而操作 B 在 M 的修改顺序中位于 A 之后，则 B 要么读取 A 写入的值，要么读取在修改顺序中位于 A 之后的某个操作写入的值。

### Release sequence

在以 `memory_order_release`、`memory_order_acq_rel` 或 `memory_order_seq_cst` 标记的原子 store 操作 A 之后，由同一线程执行的对同一原子变量的所有修改，或由任何线程执行的对同一原子变量的原子 Read-Modify-Write 操作，构成一个 **release sequence**，其中：

- 同一线程中的所有操作在修改顺序中位于 A 之后
- 所有 Read-Modify-Write 操作在修改顺序中位于 A 之后
- 所有 Read-Modify-Write 操作在修改顺序中位于前一个 Read-Modify-Write 操作之后

### Dependency ordering

在线程间，如果以下之一为真，则操作 A **依赖排序于**（dependency-ordered before）操作 B：

1. A 对原子对象 M 执行 release 操作，而 B 在同一原子对象 M 上执行 consume 操作，并读取由 A（或由 A 为首的 release sequence）写入的值
2. A 在线程 X 中依赖排序于 B，而 B 在线程 X 中依赖排序于 C，则 A 在线程 X 中依赖排序于 C

### Happens-before

在线程间，如果以下之一为真，则操作 A **先发生于**（happens-before）操作 B：

1. A 在程序顺序中位于 B 之前（sequenced-before）
2. A 与 B 同步（synchronizes-with）
3. A 依赖排序于 B
4. A 先发生于 X，而 X 先发生于 B（传递性）

### Synchronizes-with

如果以下之一为真，则操作 A **与**操作 B **同步**（synchronizes-with）：

1. A 是对原子对象 M 执行 release 操作的原子操作，而 B 是对同一原子对象 M 执行 acquire 操作的原子操作，并且 B 读取由 A（或由 A 为首的 release sequence）写入的值
2. A 是 release fence，而 B 是 acquire fence，并且存在某个原子对象 M，使得 A 在某个对 M 的原子操作之前，B 在某个对 M 的原子操作之后
3. A 是 release fence，而 B 是对某个原子对象 M 执行 acquire 操作的原子操作，并且存在某个对 M 的原子操作 X，使得 A 在 X 之前，B 读取由 X（或由 X 为首的 release sequence）写入的值
4. A 是对某个原子对象 M 执行 release 操作的原子操作，而 B 是 acquire fence，并且存在某个对 M 的原子操作 X，使得 X 在 B 之前，X 读取由 A（或由 A 为首的 release sequence）写入的值

### Sequential Consistency 排序的形式化定义

对于标记为 `memory_order_seq_cst` 的操作，除了满足 Release-Acquire 排序的约束外，还有一个额外的约束：所有 `memory_order_seq_cst` 操作，包括栅栏，必须有一个单一的总顺序 S，满足以下约束：

1. 如果 A 和 B 是 `memory_order_seq_cst` 操作，且 A strongly happens-before B，则 A 在 S 中位于 B 之前
2. 对于原子对象 M 上的每对原子操作 A 和 B，其中 A coherence-ordered-before B：
   - a) 如果 A 和 B 都是 `memory_order_seq_cst` 操作，则 A 在 S 中位于 B 之前
   - b) 如果 A 是 `memory_order_seq_cst` 操作，且 B happens-before `memory_order_seq_cst` 栅栏 Y，则 A 在 S 中位于 Y 之前
   - c) 如果 `memory_order_seq_cst` 栅栏 X happens-before A，且 B 是 `memory_order_seq_cst` 操作，则 X 在 S 中位于 B 之前
   - d) 如果 `memory_order_seq_cst` 栅栏 X happens-before A，且 B happens-before `memory_order_seq_cst` 栅栏 Y，则 X 在 S 中位于 Y 之前

形式化定义确保：
1. 单一总顺序与任何原子对象的修改顺序（modification order）一致
2. `memory_order_seq_cst` 加载从其值要么来自最后一个 `memory_order_seq_cst` 修改，要么来自某个非 `memory_order_seq_cst` 修改，且该修改不 happens-before 前面的 `memory_order_seq_cst` 修改

单一总顺序可能与 happens-before 不一致。这允许在某些 CPU 上更高效地实现 `memory_order_acquire` 和 `memory_order_release`。当 `memory_order_acquire` 和 `memory_order_release` 与 `memory_order_seq_cst` 混合使用时，可能产生令人惊讶的结果。

例如，`x` 和 `y` 初始为零：

```cpp
// 线程 1:
x.store(1, std::memory_order_seq_cst); // A
y.store(1, std::memory_order_release); // B

// 线程 2:
r1 = y.fetch_add(1, std::memory_order_seq_cst); // C
r2 = y.load(std::memory_order_relaxed); // D

// 线程 3:
y.store(3, std::memory_order_seq_cst); // E
r3 = x.load(std::memory_order_seq_cst); // F
```

允许产生 `r1 == 1 && r2 == 3 && r3 == 0`，其中 A happens-before C，但 C 在 `memory_order_seq_cst` 的单一总顺序 C-E-F-A 中位于 A 之前（见 [Lahav et al](https://plv.mpi-sws.org/scfix/paper.pdf)）。

注意：
1. 一旦非 `memory_order_seq_cst` 标记的原子操作参与进来，程序的 Sequential Consistency 保证就会丢失
2. 在许多情况下，`memory_order_seq_cst` 原子操作相对于同一线程执行的其他原子操作是可重排的

---

## 外部链接

1. [MOESI protocol](https://en.wikipedia.org/wiki/MOESI_protocol) - 维基百科
2. [x86-TSO: A Rigorous and Usable Programmer's Model for x86 Multiprocessors](https://www.cl.cam.ac.uk/~pes20/weakmemory/cacm.pdf) - P. Sewell et. al., 2010
3. [A Tutorial Introduction to the ARM and POWER Relaxed Memory Models](https://www.cl.cam.ac.uk/~pes20/ppc-supplemental/test7.pdf) - P. Sewell et al, 2012
4. [MESIF: A Two-Hop Cache Coherency Protocol for Point-to-Point Interconnects](https://researchspace.auckland.ac.nz/bitstream/handle/2292/11594/MESIF-2009.pdf?sequence=6) - J.R. Goodman, H.H.J. Hum, 2009
5. [Memory Models](https://research.swtch.com/mm) - Russ Cox, 2021

---

## 参见

- [C documentation for memory order](../../c/atomic/memory_order.html) - C 语言的内存序文档

---

**参考**：[cppreference - std::memory_order](https://en.cppreference.com/w/cpp/atomic/memory_order)

**最后修改时间**：2026年1月3日

