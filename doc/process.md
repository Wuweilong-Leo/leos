# 进程管理模块设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | 进程管理（Process） |
| 涉及路径 | kernel/task/process/、arch/cpu/i386/、arch/sys/、arch/exc/i386/ |
| 文档版本 | V3.0 |
| 更新日期 | 2026-07-04 |

---

## 2 一句话说清楚：进程是什么？

**进程 = 一个跑在"自己房间"里的程序。**

内核线程（Thread）大家挤在内核的一个大房间里，互相能看见对方的东西（共享地址空间），权限也一样（Ring 0，想干啥干啥）。

进程（Process）不一样——每个进程有自己的独立房间（独立页目录），别人看不到也改不了你的东西。而且权限被限制了（Ring 3），不能直接碰硬件，想做事必须"敲门"请内核帮忙（syscall）。

| | 线程（Thread） | 进程（Process） |
|---|---|---|
| 比喻 | 大通铺，大家住一起 | 单间，有独立空间 |
| 地址空间 | 共享内核空间 | 自己的页目录 + 自己的用户空间 |
| 运行级别 | Ring 0（内核态，全能） | Ring 3（用户态，受限） |
| 能直接调内核函数吗 | 能 | 不能，必须通过 syscall |
| 出错了 | 整个系统崩 | 只杀该进程，系统没事 |
| 堆内存 | 共享内核堆 | 通过 syscall 使用自己的用户堆 |

---

## 3 进程的一生

```
┌─────────────┐     ┌─────────────┐     ┌──────────────┐     ┌──────────┐
│  OsProcessCreate  │────▶│  SUSPENDED   │────▶│  RUNNING     │────▶│  DEAD       │
│  创建进程         │     │  挂起等待     │     │  在用户态跑   │     │  OsTaskDelete│
└─────────────┘     └─────────────┘     └──────────────┘     └──────────┘
                        OsProcessResume       iret 切到用户态       OS_SYS_EXIT
                                               │                   OsTaskDelete(pid)
                                               │
                                               └─ malloc/free 通过 syscall 操作用户堆
```

### 3.1 创建：OsProcessCreate

创建一个进程，做了这些事：

1. **借用线程骨架**：先调 `OsTaskCreate` 创建一个线程（省去重复代码），此时还是内核态线程
2. **建独立房间**：`OsProcessInitArch` → `OsCreateProcessPgd`，分配一页内存当页目录，把内核的高 1GB 映射复制过来，这样进程也能看见内核代码
3. **画一张地契**：`OsProcessInitVirMemPool`，初始化位图，记录用户空间（0x00804800 ~ 0xBFFFFFFF）哪些地址被用了
4. **铺一张地板**：`OsProcessInitUsrMem`，分配一页物理内存，映射到 0xBFFFF000 作为用户栈
5. **盖上进程章**：`tskType = OS_TASK_PROCESS`，标记它不再是普通线程
6. **用户堆不在这里初始化**：见第 7 节，用户堆 FSC 采用惰性初始化

创建完后进程处于 SUSPENDED 状态，还不会跑。

### 3.2 激活：OsProcessResume

调 `OsProcessResume(pid)`，实际就是 `OsTaskResume(pid)`——把进程加入就绪队列，等调度器选中它。

### 3.3 首次运行：OsProcessEntry

进程被调度器选中后，从 `OsProcessEntry` 开始跑。这个函数的核心任务就一个：**从内核态（Ring 0）切到用户态（Ring 3）**。

怎么切？x86 的标准方法是 `iret` 指令。但 `iret` 不是随便调的——它需要栈上有一套特定的数据。所以 `OsProcessEntry` 做的事就是**在内核栈上伪造一套"中断返回上下文"**，骗 CPU 以为刚才从用户态进来的，现在要回去：

```
内核栈上的布局（OsAllSaveContext）：

低地址 ←─────────────────────────────────────── 高地址
│saveFlag│edi│esi│ebp│espDummy│ebx│edx│ecx│eax│gs│fs│es│ds│errCode│eip│cs│eflags│esp│ss│
                                                  │    │    │      │    │
                                                  │    │    │      │    └─ ss=0x33 用户数据段(RPL=3)
                                                  │    │    │      └─ esp=用户栈顶(0xC0000000)
                                                  │    │    └─ eflags=0x202(IF=1开中断)
                                                  │    └─ cs=0x2B 用户代码段(RPL=3)
                                                  └─ eip=用户入口函数地址
```

伪造完之后，把 esp 指向这个结构，跳到 `OsSwitch2Process` → `OsAllLoad` → `iret`，CPU 就乖乖地切到 Ring 3，开始执行用户入口函数。

> **为什么 iret 能切特权级？**
> CPU 看 cs 段选择子的 RPL 字段。cs=0x2B，低两位是 3（RPL=3），CPU 就知道要切到 Ring 3，于是会从栈上额外读 esp 和 ss（Ring 0 切 Ring 3 必须换栈）。

### 3.4 用户态运行

进入用户态后，进程只能访问自己页目录映射的内存。想打印？想分配内存？想退出？不能直接调内核函数——**必须用 syscall**。

### 3.5 退出：OS_SYS_EXIT

进程通过 `int $0x80`（`OS_SYS_EXIT`）请求退出。内核态的 `OsSysExit` 调用 `OsTaskDelete(pid)` 杀掉当前进程，调度器切换到其他任务，系统继续运行。

### 3.6 出错：用户态异常保护

如果进程在用户态做了坏事（访问空指针、执行非法指令），CPU 会触发异常（GPF、Page Fault 等）。

现在的处理：
- 用户态缺页在堆范围内 → 自动映射物理页，进程继续运行
- 其他用户态异常 → 打印信息，杀掉该进程，系统继续运行
- 内核态异常 → 系统挂死（内核 bug，不可恢复）

---

## 4 Syscall：用户态如何请求内核服务

### 4.1 为什么需要 syscall？

Ring 3 的代码不能调 Ring 0 的函数，那是越权。但进程总得做事——打印、分配内存、退出……怎么办？

**syscall = 敲门。** 用户态执行 `int $0x80`，CPU 自动切到 Ring 0，跳到内核预先注册的处理函数。处理完了，`iret` 回用户态继续执行。

```
用户态 (Ring 3)                    内核态 (Ring 0)
    │                                  │
    │  int $0x80  ──────────────────▶  │  OsSyscallVector80
    │       (eax=syscall号,            │    ├── 保存所有寄存器
    │        ebx/ecx/edx=参数)         │    ├── 切到内核栈
    │                                  │    ├── 调 OsSyscallHandler
    │                                  │    │    └── switch(eax) { ... }
    │  ◀──────────────── iret          │    └── 恢复寄存器, iret
    │   eax=返回值                     │
    │                                  │
```

### 4.2 调用约定

| 寄存器 | 用途 |
|--------|------|
| eax | syscall 号（入） / 返回值（出） |
| ebx | 第 1 个参数 |
| ecx | 第 2 个参数 |
| edx | 第 3 个参数 |
| esi | 第 4 个参数 |

### 4.3 目前支持的 syscall

| 编号 | 名称 | 参数 | 返回值 | 说明 |
|------|------|------|--------|------|
| 1 | OS_SYS_WRITE | ebx=字符串指针, ecx=长度 | 写入字节数 | 向串口写字符串 |
| 2 | OS_SYS_EXIT | ebx=退出码 | 不返回 | 杀掉当前进程 |
| 3 | OS_SYS_MALLOC | ebx=大小 | 分配的地址(0=失败) | 从用户堆分配内存 |
| 4 | OS_SYS_FREE | ebx=地址 | 0 | 释放用户堆内存 |

### 4.4 汇编入口详解：OsSyscallVector80

`int $0x80` 触发后，CPU 跳到这里执行。完整流程：

```
OsSyscallVector80:
  ① pushl $0              ← 假 errCode，平衡栈帧（与 HWI 格式统一）
  ② OS_HWI_SAVE_CONTEXT   ← 全量保存所有寄存器到栈上
  ③ 保存 esp → runningTsk->stkPtr   ← 记住上下文在哪
  ④ 判断 esp 是否在内核栈范围
     ├─ 在 → 直接调 handler
     └─ 不在 → 切到内核栈，再调 handler
  ⑤ call OsSyscallHandler(ctx)
  ⑥ 从 runningTsk->stkPtr 恢复 esp  ← 回到上下文所在的位置
  ⑦ jmp OsHwiRet          ← 恢复寄存器 + iret 回用户态
```

**关键细节**：第 ⑥ 步不能省！handler 可能在内核栈上操作，调用完 esp 指向内核栈，但 `OsHwiRet` 期望 esp 指向任务栈上的上下文。不恢复的话，`pop %gs` 会从内核栈上弹出垃圾数据，直接 Page Fault。

### 4.5 IDT 注册

在 `OsSyscallConfigInit()` 中：

```c
OsIdtBuildEntry(0x80, OS_IDT_ENTRY_ATTR3, OsSyscallVector80);
```

`OS_IDT_ENTRY_ATTR3` 把 IDT 描述符的 DPL 设为 3，这是关键——如果 DPL=0，用户态执行 `int $0x80` 会触发 GPF（General Protection Fault），因为权限不够。

### 4.6 用户态怎么调用？

`OsSyscall1`/`OsSyscall2`/`OsSyscall3` 是 `OS_INLINE` 内联函数，在 `OS_SEC_KERNEL_TEXT` 函数中被内联后代码留在 `.os.kernel.text` 段，用户态可安全执行：

```c
/* 分配 32 字节 */
U32 addr = OsSyscall1(OS_SYS_MALLOC, 32);
if (addr != 0) {
    char *buf = (char *)(uintptr_t)addr;
    buf[0] = 'o'; buf[1] = 'k';
    OsSyscall2(OS_SYS_WRITE, (U32)(uintptr_t)buf, 2);
    OsSyscall1(OS_SYS_FREE, addr);
}
```

也可以用 `usr_malloc`/`usr_free`/`usr_printf` 等更高层的包装函数。

> **为什么之前不用 OsSyscall2() 包装函数？**
> 早期担心 `OS_INLINE` 未内联时在 `.text` 段生成 out-of-line 副本（不映射，触发 PF）。经验证，在 `OS_SEC_KERNEL_TEXT` 函数内调用时编译器总是内联，不会生成 `.text` 副本，所以可以放心使用。

---

## 5 进程地址空间布局

每个进程有自己的页目录，地址空间被分成两半：

```
0x00000000 ┌──────────────────────────────┐
           │                              │
0x08048000 ├──────────────────────────────┤ ← 用户空间起点（预留1页给代码段）
0x08049000 ├──────────────────────────────┤ ← 用户堆起始 (OS_PROCESS_USR_HEAP_BASE)
           │                              │
           │     用户堆 4MB                │ ← FSC 管理，缺页自动扩展
           │     (0x08049000~0x08449000)   │
           │                              │
0x08449000 ├──────────────────────────────┤ ← 用户堆上限
           │                              │
           │     ...空闲...                │
           │                              │
0xBFFFF000 ├──────────────────────────────┤ ← 用户栈（1 页，4KB）
0xC0000000 ├──────────────────────────────┤ ← 3GB 分界线
           │                              │
           │     内核空间（所有进程共享）     │
           │     从内核页目录复制高 256 项   │
           │                              │
0xFFFFFFFF └──────────────────────────────┘
```

### 5.1 用户栈

- 虚拟地址：`0xBFFFF000`（`OS_PROCESS_USR_STACK_BASE`）
- 大小：1 页 = 4KB
- 栈顶：`0xC0000000`（`esp` 初始值，x86 栈向下增长）
- 分配时机：`OsProcessInitUsrMem`，在进程创建时分配物理页并映射

### 5.2 用户堆

- 虚拟地址：`0x08049000`（`OS_PROCESS_USR_HEAP_BASE`）
- 大小：4MB（`OS_USR_HEAP_MEM_SIZE`）
- 管理方式：FSC 分配器，缺页自动扩展
- 初始化时机：惰性初始化，第一次 `OS_SYS_MALLOC` 时创建（详见第 7 节）

### 5.3 内核映射的复制

`OsCreateProcessPgd` 创建页目录时，把内核页目录的高 256 项（对应 0xC0000000 ~ 0xFFFFFFFF）逐项复制过来。这样每个进程都能看到相同的内核空间——这是 syscall 能工作的前提，否则切到 Ring 0 后找不到内核代码。

### 5.4 自映射

页目录的最后一项（第 1023 项）指向页目录自身的物理地址，这是 x86 页表自映射的经典技巧。通过自映射，内核可以用虚拟地址 0xFFC00000 ~ 0xFFFFFFFF 访问所有页表，无需临时映射。

---

## 6 数据结构

### 6.1 OsProcessCreateParam

```c
struct OsProcessCreateParam {
    OsProcessEntryFunc entryFunc;        // 用户入口函数
    U32 prio;                            // 优先级
    char processName[16];                // 进程名
    void *param[2];                      // 入口参数（最多 2 个）
};
```

### 6.2 OsTaskCb 中的进程相关字段

TCB 是线程和进程共用的。进程额外用到这些字段：

| 字段 | 类型 | 线程 | 进程 |
|------|------|------|------|
| `tskType` | enum | OS_TASK_THREAD | OS_TASK_PROCESS |
| `pgDir` | uintptr_t | 0 | 进程页目录的虚拟地址 |
| `usrVirMemPool` | OsMemPool | 未使用 | 用户虚拟内存池（含位图） |
| `usrFscCtrl` | OsMemFscCtrl* | NULL | 用户堆 FSC 控制块指针 |

### 6.3 OsAllSaveContext

中断/异常/syscall 入口保存的完整上下文，共 19 个 U32 字段：

```
偏移  字段        说明
0x00  saveFlag   OS_ALL_SAVE_FLAG(1) 或 OS_FAST_SAVE_FLAG(0)
0x04  edi        ─┐
0x08  esi         │ 通用寄存器（OS_HWI_SAVE_CONTEXT 宏压入）
0x0C  ebp         │
0x10  espDummy    │ pushl %esp 时的占位
0x14  ebx         │
0x18  edx         │
0x1C  ecx         │
0x20  eax        ─┘
0x24  gs         ─┐
0x28  fs          │ 段寄存器
0x2C  es          │
0x30  ds         ─┘
0x34  errCode    假 errCode（HWI/Syscall 压入的 $0）或 CPU 真实 errCode
0x38  eip        ─┐ CPU 自动压入（iret 时弹出）
0x3C  cs          │
0x40  eflags      │
0x44  esp         │ 特权级变化时额外压入
0x48  ss         ─┘
```

---

## 7 用户堆 malloc/free

### 7.1 设计思路

用户态不能直接调内核函数（`cli` 是特权指令），所以 malloc/free 必须走 syscall。内核态的 syscall handler 调用 FSC 分配器，复用和内核堆完全一样的算法。

### 7.2 惰性初始化

用户堆 FSC 不在 `OsProcessCreate` 时初始化，而是**第一次 `OS_SYS_MALLOC` 时才创建**。原因：

`OsProcessCreate` 执行期间，`OS_RUNNING_TASK()` 返回的是创建者线程（比如 pid=0 的内核线程），不是新进程。如果 `OsMemFscInitPt` 写入触发缺页，缺页处理中的 `OsMemUsrAllocPgByAddr` 通过 `OS_RUNNING_TASK()->usrVirMemPool` 取位图，拿到的是创建者的位图（未初始化，崩溃）。

第一次 malloc 时，进程已经在运行，`OS_RUNNING_TASK()` 正确返回进程自身，缺页处理一切正常。

```
OsProcessCreate
  └─ （堆相关什么都不做，usrFscCtrl = NULL）

第一次 OS_SYS_MALLOC
  └─ OsSysMalloc
       └─ 发现 usrFscCtrl == NULL
            └─ OsMemFscInitPt(0x8049000, 4MB)
                 └─ 写入触发缺页 → OsExcHandleKernelPgFault
                      └─ OsMemUsrAllocPgByAddr
                           └─ OS_RUNNING_TASK()->usrVirMemPool  ← 进程自己 ✓
```

### 7.3 缺页自动扩展

malloc 分配的内存可能跨越未映射的页面。写入时触发缺页，两条路径都能正确处理：

**用户态缺页**（进程在 Ring 3 执行时触发）：
```
OsExcDispatcher → 用户态缺页分支 → cr2 在堆范围内？
  └─ OsMemUsrAllocPgByAddr(cr2 & ~0xFFF) → 分配物理页+映射 → iret 回用户态继续
```

**内核态缺页**（内核代为访问用户堆时触发，如 syscall 拷贝数据）：
```
OsExcHandleKernelPgFault → errAddr 在用户堆范围内？
  └─ OsMemUsrAllocPgByAddr(errAddr & ~0xFFF) → 分配物理页+映射 → 返回
```

两条路径都不需要切 CR3：用户态缺页时 CR3 已经是进程页目录，内核态代为访问时 CR3 也是进程页目录（syscall 入口没切 CR3）。`OsMapVir2Phy` 通过自映射直接操作进程页表。

### 7.4 FSC 尾部哨兵

`OsMemFscInitPt` 初始化时，在堆末尾预留 `OS_MEM_FSC_HEAD_SIZE` 大小的尾部占位。**这个占位必须被初始化为有效的哨兵块**（`ctrl` 非 NULL），否则 free 合并时会越界。

原因：新映射的物理页全零。如果尾部占位未被初始化，`ctrl == NULL` 会被 `OsMemFscTryMergeRight` 误判为空闲块，导致链表操作解引用 NULL 指针崩溃。

```c
/* OsMemFscInitPt 中 */
tailSentinel = (struct OsMemFscHead *)((uintptr_t)blk + blkSize);
tailSentinel->ctrl = ptCtrl;   /* 非 NULL = 使用中，free 不会合并 */
tailSentinel->size = 0;
tailSentinel->preSize = 0;
```

---

## 8 中断/异常中的栈切换

当进程在用户态运行时，中断/异常/syscall 都会让 CPU 从用户栈切到内核栈。但内核代码可能在不同栈上操作，需要正确处理。

### 8.1 硬件中断（定时器、键盘等）

```
OsHwiVector 宏：
  1. CPU 自动切到内核栈（TSS.esp0 指向的栈）
  2. OS_HWI_SAVE_CONTEXT 保存寄存器
  3. 检查 esp 是否在 [g_kernelStackLow, g_kernelStackHigh)
     ├─ 在 → 直接 OsHwiCommonHandler
     └─ 不在 → 保存 esp→stkPtr，切到 g_kernelStackHigh，再 OsHwiCommonHandler
```

### 8.2 Syscall（INT 0x80）

与硬件中断类似，但有两个区别：
- **不需要 EOI**：`int $0x80` 是软件中断，不经过 8259A PIC
- **handler 返回后要恢复 esp**：见第 4.4 节第 ⑥ 步

### 8.3 异常

异常走 `OsExcRet` 恢复路径，与 `OsHwiRet` 类似但多了 cr2 的处理。

---

## 9 任务切换时的 CR3 切换

调度器切换到进程时，必须把页目录换成进程的，否则进程看到的还是内核的地址空间：

```c
void OsConfigPgdForTskSwitch(struct OsTaskCb *tsk)
{
    if (tsk->tskType == OS_TASK_PROCESS) {
        OsLoadPgd(OsGetPaddrByVaddr(tsk->pgDir));  // CR3 = 进程页目录物理地址
    } else {
        OsLoadPgd(OS_KERNEL_PGD_BASE);              // CR3 = 内核页目录
    }
}
```

同时还要更新 TSS 的 esp0，因为用户态进内核时 CPU 自动切到 TSS.esp0 指向的栈：

```c
void OsConfigTssForTskSwitch(struct OsTaskCb *tsk)
{
    if (tsk->tskType == OS_TASK_PROCESS) {
        OsTssUpdateEsp0(OS_SELECTOR_K_DATA,
                        tsk->kernelStkTop + OS_TASK_KERNEL_STACK_SIZE);
    }
}
```

---

## 10 设计决策记录

| 决策 | 理由 |
|------|------|
| 进程复用线程骨架（OsTaskCreate） | 避免重复实现 TCB 分配、栈分配、上下文伪造 |
| iret 切用户态 | x86 标准方法，iret 从栈上恢复 cs/eip/ss/esp/eflags，自然完成特权级切换 |
| 用户入口函数由 OsProcessEntry 间接调用 | OsProcessEntry 负责伪造 AllSaveContext + iret，用户入口只管业务逻辑 |
| int $0x80 做 syscall | 简单可靠，IDT 设 DPL=3 即可，无需 sysenter/sysexit 的兼容性问题 |
| syscall 走 OsHwiRet 而非自定义恢复路径 | 寄存器恢复逻辑与硬件中断完全一致，复用代码 |
| 用户态异常杀进程 | 隔离故障：一个进程出错不影响其他进程和内核 |
| 内核映射在创建页目录时复制 | 所有进程共享内核空间，syscall 切到 Ring 0 后能找到内核代码 |
| malloc/free 走 syscall | 用户态不能执行 `cli`（FSC 内部用 OsIntLock），必须通过 int 0x80 切到内核态 |
| 用户堆 FSC 惰性初始化 | OsProcessCreate 期间 OS_RUNNING_TASK() 是创建者线程，缺页处理取不到新进程的位图；第一次 malloc 时进程已在运行，一切正常 |
| FSC 尾部哨兵块 | 新映射物理页全零，ctrl=NULL 被误判为空闲块导致 free 崩溃；初始化 ctrl 非 NULL 阻止合并越界 |

---

## 11 已知限制与后续计划

| 限制 | 计划 |
|------|------|
| 用户栈只有 1 页（4KB），没有栈增长机制 | 实现缺页异常自动扩展用户栈 |
| 进程退出不回收页目录/物理页 | 实现 OsProcessDestroy 释放页目录/物理页/位图/堆 |
| 进程入口参数尚未传递到用户态 | 通过 AllSaveContext 的 ebx/ecx 寄存器传递 |
| 只支持 4 个 syscall | 逐步添加 read/mmap/waitpid 等 |
| 进程出错后只杀进程不回收资源 | 在 OsTaskDelete 中检测 tskType=PROCESS 后调 OsProcessDestroy |

---

## 12 模块依赖

```
                 ┌──────────────────┐
                 │  OsProcessCreate │
                 └────────┬─────────┘
                          │
            ┌─────────────┼──────────────┐
            ▼             ▼              ▼
     OsTaskCreate    OsProcessInitArch  OsProcessInitVirMemPool
     (任务管理)       (CPU抽象层)        (内存管理)
            │             │              │
            │        OsCreateProcessPgd  OsMemKernelAllocPgs
            │        (页表管理)          (内存管理)
            │             │
            │        OsProcessInitUsrMem
            │        (CPU抽象层 + 内存管理，只分配栈)
            │
            ▼
     OsProcessResume (= OsTaskResume)
            │
            ▼
     OsProcessEntry ──── OsSwitch2Process ──── iret 到用户态
            │
            ▼
     用户态代码 ──── int $0x80 ──── OsSyscallVector80
                                       │
                                       ▼
                                  OsSyscallHandler
                                  ├─ OS_SYS_WRITE  → OsUartPutc
                                  ├─ OS_SYS_EXIT   → OsTaskDelete
                                  ├─ OS_SYS_MALLOC → OsMemFscAlloc (惰性初始化)
                                  └─ OS_SYS_FREE   → OsMemFscFree
```

---

## 13 相关文件索引

| 文件 | 内容 |
|------|------|
| kernel/task/process/os_process.c | OsProcessCreate、OsProcessInitUsrMem、OsProcessInitVirMemPool |
| kernel/include/os_process_external.h | OsProcessCreateParam、OsProcessEntryFunc、接口声明 |
| kernel/include/os_process_internal.h | 进程内部函数声明 |
| arch/cpu/i386/os_cpu_i386.c | OsProcessEntry（伪造上下文+iret）、OsProcessInitArch |
| arch/cpu/i386/os_cpu_i386.h | 用户栈/堆/选择子/EFLAGS 常量 |
| arch/cpu/i386/os_context_i386.h | OsAllSaveContext/OsFastSaveContext/OsExcSaveContext |
| arch/cpu/i386/os_dispatch.S | OsSyscallVector80（syscall 汇编入口）、OsHwiRet/OsAllLoad |
| arch/sys/os_syscall_i386.c | OsSyscallHandler（syscall C 处理）、OsSysMalloc/Free、OsSyscallConfigInit |
| arch/sys/os_syscall_i386.h | syscall 号定义、OsSyscall1/2/3 内联包装、usr_malloc/usr_free |
| arch/hwi/i386/os_hwi_i386.c | OsHwiConfigInit 中调 OsSyscallConfigInit 注册 INT 0x80 |
| arch/exc/i386/os_exc_i386.c | OsExcDispatcher 中用户态缺页堆扩展+异常杀进程 |
| arch/cpu/i386/pgt/os_pgt.c | OsCreateProcessPgd（创建独立页目录） |
| kernel/mem/fsc/os_mem_fsc.c | OsMemFscInitPt（含尾部哨兵）、OsMemFscAlloc/Free |
| kernel/sched/os_sched.c | OsSchedMain（调度，含 CR3/TSS 切换） |
| test/os_test_process.c | 进程测试（syscall 打印/malloc/free/大块分配/复用） |
