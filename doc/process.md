# 进程管理模块设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | 进程管理（Process） |
| 涉及路径 | kernel/task/process/、arch/cpu/i386/、arch/sys/、arch/exc/i386/ |
| 文档版本 | V4.0 |
| 更新日期 | 2026-07-12 |

---

## 2 一句话说清楚：进程是什么？

**进程 = 一个跑在"自己房间"里的程序。**

内核线程（Thread）大家挤在内核的一个大房间里，互相能看见对方的东西（共享地址空间），权限也一样（Ring 0，想干啥干啥）。

进程（Process）不一样——每个进程有自己的独立房间（独立页目录），别人看不到也改不了你的东西。而且权限被限制了（Ring 3），不能直接碰硬件，想做事必须"敲门"请内核帮忙（syscall）。

| | 线程（Thread） | 进程（Process） | pthread（共享线程） |
|---|---|---|---|
| 比喻 | 大通铺，大家住一起 | 单间，有独立空间 | 和别人合租一间 |
| 地址空间 | 共享内核空间 | 自己的页目录 + 自己的用户空间 | 共享主进程的页目录 |
| 运行级别 | Ring 0（内核态，全能） | Ring 3（用户态，受限） | Ring 3（用户态，受限） |
| 能直接调内核函数吗 | 能 | 不能，必须通过 syscall | 不能，必须通过 syscall |
| 出错了 | 整个系统崩 | 只杀该进程，系统没事 | 只杀该线程，系统没事 |
| 堆内存 | 共享内核堆 | 通过 syscall 使用自己的用户堆 | 共享主进程的用户堆 |
| 创建方式 | OsTaskCreate | OsProcessCreate / fork | clone (OS_SYS_CLONE) |

---

## 3 进程的一生

```
┌─────────────┐     ┌─────────────┐     ┌──────────────┐     ┌──────────┐
│  OsProcessCreate  │────▶│  SUSPENDED   │────▶│  RUNNING     │────▶│  ZOMBIE     │
│  创建进程         │     │  挂起等待     │     │  在用户态跑   │     │  等待收割    │
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
3. **画一张地契**：`OsProcessInitVirMemPool`，初始化位图，记录用户空间（0x08048000 ~ 0xBFFFFFFF）哪些地址被用了
4. **盖上进程章**：`tskType = OS_TASK_PROCESS`，标记它不再是普通线程
5. **设置地址空间主人和引用计数**：`pgShareMaster = self`, `pgDirRefCnt = 1`（详见第 12 节）
6. **铺一张地板**：`OsProcessInitUsrMem`，分配一页物理内存，映射到 0xBFFFF000 作为用户栈（必须在 pgShareMaster 之后，因为缺页处理依赖正确的 TCB 字段）
7. **用户堆不在这里初始化**：见第 7 节，用户堆 FSC 采用惰性初始化

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
    │        ebx/ecx/edx/esi=参数)     │    ├── 切到内核栈
    │                                  │    ├── 调 OsSyscallHandler
    │                                  │    │    └── g_syscallTab[eax](arg1..4)
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
| 2 | OS_SYS_EXIT | ebx=退出码 | 不返回 | 杀掉当前进程/线程 |
| 3 | OS_SYS_MALLOC | ebx=大小 | 分配的地址(0=失败) | 从用户堆分配内存 |
| 4 | OS_SYS_FREE | ebx=地址 | 0 | 释放用户堆内存 |
| 5 | OS_SYS_SEM_CREATE | ebx=类型, ecx=初始值, edx=最大计数 | semId(≥0x10000为错误码) | 创建信号量 |
| 6 | OS_SYS_SEM_PEND | ebx=semId, ecx=超时 | OS_OK/错误码 | 等待信号量 |
| 7 | OS_SYS_SEM_POST | ebx=semId | OS_OK/错误码 | 释放信号量 |
| 8 | OS_SYS_SEM_DELETE | ebx=semId | OS_OK/错误码 | 删除信号量 |
| 9 | OS_SYS_GETPID | 无 | 当前 pid | 获取进程 ID |
| 10 | OS_SYS_FORK | 无 | 子进程 pid(父) / 0(子) | 创建子进程（独立地址空间） |
| 11 | OS_SYS_WAITPID | ebx=pid, ecx=statusPtr, edx=options | 子进程 pid | 等待子进程退出 |
| 12 | OS_SYS_CLONE | ebx=stackTop, ecx=startRoutine, edx=arg, esi=trampoline | 子线程 tid(父) | 创建共享地址空间的用户线程（子线程 eip 被改为 trampoline，不返回到 clone 调用点） |

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

### 4.5 分发机制：系统调用表

系统调用不再用 switch-case，而是用函数指针数组 `g_syscallTab[OS_SYS_NUM]`。`OsSyscallHandler` 查表分发，`OsSyscallRegister` 注册新 syscall，新增系统调用无需改分发逻辑：

```c
OS_SEC_KERNEL_DATA OsSyscallFunc g_syscallTab[OS_SYS_NUM];

U32 OsSyscallHandler(U32 sysno, U32 arg1, U32 arg2, U32 arg3, U32 arg4)
{
    if (sysno >= OS_SYS_NUM) return (U32)-1;
    OsSyscallFunc func = g_syscallTab[sysno];
    if (func == NULL) return (U32)-1;
    return func(arg1, arg2, arg3, arg4);
}
```

### 4.6 IDT 注册

在 `OsSyscallConfigInit()` 中：

```c
OsIdtBuildEntry(0x80, OS_IDT_ENTRY_ATTR3, OsSyscallVector80);
```

`OS_IDT_ENTRY_ATTR3` 把 IDT 描述符的 DPL 设为 3，这是关键——如果 DPL=0，用户态执行 `int $0x80` 会触发 GPF（General Protection Fault），因为权限不够。

### 4.7 用户态怎么调用？

`OsSyscall1`/`OsSyscall2`/`OsSyscall3`/`OsSyscall4` 是 `OS_INLINE` 内联函数，在 `OS_SEC_KERNEL_TEXT` 函数中被内联后代码留在 `.os.kernel.text` 段，用户态可安全执行：

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

也可以用 `usr_malloc`/`usr_free`/`usr_printf`/`usr_fork`/`usr_clone` 等更高层的包装函数。

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
    void *param[2];                      // 入口参数（最多 2 个，通过 OS_PROC_ARG1/2 宏读取）
};
```

### 6.2 OsTaskCb 中的进程相关字段

TCB 是线程和进程共用的。进程额外用到这些字段：

| 字段 | 类型 | 线程 | 独立进程 | 共享线程(pthread) |
|------|------|------|---------|------------------|
| `tskType` | enum | OS_TASK_THREAD | OS_TASK_PROCESS | OS_TASK_PROCESS |
| `pgDir` | uintptr_t | 0 | 进程页目录的虚拟地址 | 共享主进程的 pgDir |
| `usrVirMemPool` | OsMemPool | 未使用 | 用户虚拟内存池（含位图） | 浅拷贝结构体，共享 btmp.base |
| `usrFscCtrl` | OsMemFscCtrl* | NULL | 用户堆 FSC 控制块指针 | 共享主进程的 FSC |
| `pgShareMaster` | OsTaskCb* | 未使用 | 指向自己（= self） | 指向主进程的 TCB |
| `pgDirRefCnt` | U16 | 未使用 | 初始=1 | 不直接使用（递增 master 的计数） |
| `parentPid` | U32 | 未使用 | 父进程 pid | 创建者 pid |
| `exitCode` | U32 | 未使用 | 退出码 | 退出码 |
| `waitPid` | U32 | 未使用 | 等待的目标 pid | 等待的目标 pid |

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

**clone 线程天然正确**：clone 线程的 `tskType` 也是 `OS_TASK_PROCESS`，`pgDir` 共享主进程的页目录。切换进来时 CR3 加载相同的物理地址，TLB 不需要额外刷新。

同时还要更新 TSS 的 esp0，因为用户态进内核时 CPU 自动切到 TSS.esp0 指向的栈：

```c
void OsConfigTssForTskSwitch(struct OsTaskCb *tsk)
{
    OsTssUpdateEsp0(OS_SELECTOR_K_DATA,
                    tsk->kernelStkTop + OS_TASK_KERNEL_STACK_SIZE);
}
```

clone 线程有自己独立的内核栈（`kernelStkTop` 在 `OsSysClone` 中分配），TSS.esp0 指向正确的内核栈顶。

---

## 10 fork：创建独立地址空间的子进程

### 10.1 fork 做了什么？

fork 创建一个与父进程完全独立的子进程——拥有自己的 PGD、页表、位图、堆。子进程从 fork 调用点继续执行，但 `fork()` 返回 0（子进程）或子进程 pid（父进程）。

```
OsProcessFork 完整流程：
  1. 分配子进程 TCB + 内核栈
  2. memcpy 拷贝父进程整个内核栈（INT 0x80 期间中断关闭，内核栈稳定）
  3. 计算 stkPtr 偏移：child->stkPtr = childStk - parentStk + parent->stkPtr
  4. 拷贝 TCB 基本字段（prio、entry、arg 等）
  5. 设置 child->pgShareMaster = child（独立地址空间）
  6. 设置 child->pgDirRefCnt = 1
  7. 分配子进程 PGD（OsCreateProcessPgd）
  8. 深拷贝 usrVirMemPool 位图（新分配位图页，memcpy 内容）
  9. OsListInit(&child->usrVirMemPool.memCtrlList)（独立链表）
  10. child->usrFscCtrl = NULL（惰性初始化，第一次 malloc 时创建）
  11. CR3 交替逐页拷贝用户映射（OsProcessForkCopyPageTables）
  12. 设置 fork 返回值：ctx->eax = 0（子进程看到 0）
  13. OsTaskResume(childPid) → 加入就绪队列
```

### 10.2 CR3 交替拷贝

页表深拷贝需要反复切换 CR3——读父进程页面时用父 PGD，写子进程页面时用子 PGD。流程：

```
for 每个用户 PDE:
  for 每个用户 PTE:
    1. CR3 = 父进程 PGD
    2. 读父进程页面到临时缓冲
    3. 分配子进程物理页
    4. CR3 = 子进程 PGD
    5. 映射并写入子进程页面
    6. 标记子进程虚拟位图
```

### 10.3 fork 返回值

子进程通过 `ctx->eax = 0` 获取返回值 0。父进程的 eax 不变（`OsSyscallHandler` 返回子进程 pid）。

`OsProcessForkSetChildRetval` 由 arch 层实现，设置子进程 AllSaveContext 的 eax 字段。

### 10.4 waitpid：收割子进程

父进程通过 `OS_SYS_WAITPID` 等待子进程退出：

```
OsSysWaitpid 流程：
  1. 非阻塞扫描：遍历 g_tskCbArray 找 ZOMBIE 子进程
     ├─ 找到 → 收割 (OsTaskReapZombie)，返回子进程 pid
     └─ 没找到 → 检查是否有子进程
          ├─ 没有 → 返回 -1 (ECHILD)
          └─ 有但还在跑 →
               ├─ WNOHANG → 返回 0
               └─ 阻塞 → 设 waitPid + PENDING，调度走

  2. 被唤醒后再次扫描（单核非抢占，无竞态）
```

退出码编码：`status = exitCode << 8`（与 Linux 一致，`WEXITSTATUS(status)` 即 `status >> 8`）。

---

## 11 clone：创建共享地址空间的用户线程

### 11.1 为什么需要 clone？

fork 创建的是**独立地址空间**的子进程——拷贝 PGD、页表、位图、堆。但很多场景需要**共享地址空间**的线程：
- 多线程共享全局变量
- 一个线程 malloc 的内存另一个线程可以 free
- 互斥锁保护共享数据

`OS_SYS_CLONE` 类似 Linux 的 `clone(CLONE_VM)`，创建一个共享主进程地址空间的用户线程。

### 11.2 进程 vs 共享线程 对比

| 方面 | 进程 (OsProcessCreate / fork) | 共享线程 (OsSysClone) |
|------|------|------|
| **PGD** | 新分配，独立 | 共享主进程的 pgDir 指针 |
| **usrVirMemPool** | 新位图，独立 | 浅拷贝结构体，共享 btmp.base 物理页 |
| **usrFscCtrl** | 惰性初始化，独立 | 共享主进程的 FSC（同一个堆） |
| **pgShareMaster** | = self（自己是主人） | = parent->pgShareMaster（指向同一个主人） |
| **pgDirRefCnt** | 初始=1 | master 的 refCnt +1 |
| **内核栈** | 新分配 | 新分配（每个线程必须有独立的内核栈） |
| **用户栈** | 内核分配并映射 | 用户态 malloc 提供，内核只设 esp |
| **页表** | 深拷贝（fork） | 共享（同一个 PGD） |
| **parentPid** | 父进程 pid | 创建者 pid |
| **入口方式** | OsProcessEntry → iret 到用户入口 | trampoline → start_routine |
| **退出后** | ZOMBIE → waitpid 收割 → refCnt-1 → 释放 PGD | ZOMBIE → waitpid 收割 → refCnt-1，最后一个才释放 PGD |

### 11.3 OsSysClone 完整流程

```c
OsSysClone(stackTop, startRoutine, arg, trampoline):
  1. 仅用户进程可调用（tskType == OS_TASK_PROCESS）
  2. 分配子线程 TCB + 内核栈
  3. memcpy 拷贝父进程整个内核栈
  4. 计算 stkPtr 偏移
  5. 修改子线程的 AllSaveContext：
     - ctx->eax = 0（不会被跳板代码读取，安全默认值）
     - ctx->eip = trampoline（iret 后跳到跳板函数）
     - ctx->esp = stackTop（用户传入的栈顶）
     - ctx->ebx = startRoutine（跳板通过 ebx 读取 start_routine）
     - ctx->ecx = arg（跳板通过 ecx 读取 arg）
     - ctx->eflags = OS_PROCESS_EFLAGS（IF=1, IOPL=0，确保开中断启动）
  6. 设置 TCB 字段：
     - 共享 pgDir、usrVirMemPool（浅拷贝 + OsListInit memCtrlList）、usrFscCtrl
     - pgShareMaster = parent->pgShareMaster
     - parent->pgShareMaster->pgDirRefCnt++
  7. OsTaskResume(childPid)
```

### 11.4 入口方式：trampoline 跳板

clone 子线程不返回到 clone 调用点，而是 iret 后跳到用户提供的 trampoline 函数（`__pthread_entry`）。为什么？

- fork 子进程继续执行 fork 之后的代码，通过 eax==0 判断自己是子进程
- clone 子线程应从 `start_routine` 开始，而非 clone 调用点——语义更清晰
- trampoline 方案避免了用户态需要检查 eax 返回值的混乱

```
fork:  parent INT 0x80 → OsSysFork → child ctx->eax=0 → iret → fork 返回点 → if(pid==0) 子进程代码
clone: parent INT 0x80 → OsSysClone → child ctx->eip=trampoline, ebx=start, ecx=arg
       → iret → trampoline → movl %ebx, start_routine; movl %ecx, arg → start_routine(arg) → usr_exit
```

### 11.5 __pthread_entry 跳板函数

```c
static OS_SEC_KERNEL_TEXT void __pthread_entry(void)
{
    void *(*start_routine)(void *);
    void *arg;
    void *result;
    OS_EMBED_ASM("movl %%ebx, %0" : "=r"(start_routine));  /* 内核在 ctx->ebx 写入 */
    OS_EMBED_ASM("movl %%ecx, %0" : "=r"(arg));             /* 内核在 ctx->ecx 写入 */
    result = start_routine(arg);
    usr_exit((U32)(uintptr_t)result);
}
```

**为什么用内联汇编读取 ebx/ecx？** 编译器不知道这些寄存器已被内核修改，必须用内联汇编强制读取。

### 11.6 clone 子线程的 eflags

INT 0x80 是中断门，硬件自动清除 IF。如果子线程继承父进程的 eflags（IF=0），会以中断禁用状态启动——可能饿死其他任务。

**修复**：`OsSysClone` 中显式设置 `ctx->eflags = OS_PROCESS_EFLAGS`（IF=1, IOPL=0），与 `OsProcessEntry` 一致。

### 11.7 为什么不调用 OsTaskInitKernelStack？

`OsTaskInitKernelStack` 是 `os_task.c` 中的 `OS_INLINE static` 函数，不可从 `os_syscall_i386.c` 调用。实际上，memcpy 会覆盖整个栈内容，`OsTaskInitKernelStack` 的填充被完全覆盖。fork 实现中也不调用它。

---

## 12 地址空间引用计数

### 12.1 问题：共享地址空间何时释放？

多个 clone 线程共享同一个 PGD。如果某个线程退出就释放 PGD，其他线程立刻崩溃——它们还在用这个 PGD。

### 12.2 解决方案：pgShareMaster + pgDirRefCnt

每个 TCB 新增两个字段：

```c
struct OsTaskCb *pgShareMaster;    /* 地址空间组的主人 TCB
                                    * - 独立进程：指向自己
                                    * - clone 线程：指向主进程的 TCB */
U16 pgDirRefCnt;                   /* 页目录引用计数
                                    * - 仅在 master TCB 中有效
                                    * - 退出时 master->pgDirRefCnt--
                                    * - 降到 0 时释放地址空间资源 */
```

### 12.3 引用计数工作流程

```
主进程创建:       pgShareMaster = self, pgDirRefCnt = 1
  └─ clone 线程:  pgShareMaster = master, master->pgDirRefCnt++ → 2

线程退出:         master->pgDirRefCnt-- → 1, 不释放
主进程退出:       master->pgDirRefCnt-- → 0, 释放全部资源
```

### 12.4 OsTaskDelete 中的释放逻辑

```c
if (tskCb->tskType == OS_TASK_PROCESS && tskCb->pgDir) {
    struct OsTaskCb *master = tskCb->pgShareMaster;
    master->pgDirRefCnt--;
    if (master->pgDirRefCnt == 0) {
        OsProcessFreeResources(master);   /* 注意：传入 master，不是 tskCb */
    }
}
```

**为什么传入 master？** PGD、位图页等资源都挂在 master TCB 的字段上（`pgDir`、`usrVirMemPool.btmp.base`），必须通过 master 释放。

### 12.5 主进程退出时强制终止共享线程

POSIX 语义：`exit()` 终止进程内所有线程。如果主进程退出但还有共享线程在跑，必须先杀掉它们，否则：
1. 共享线程还在用 master 的 PGD → 不能释放
2. master 被 waitpid 收割后 pgDir=0 → 共享线程退出时 `OsGetPaddrByVaddr(0)` 崩溃

```c
/* OsTaskDelete 中，pgDirRefCnt 递减之前 */
if (tskCb->pgShareMaster == tskCb && tskCb->pgDirRefCnt > 1) {
    /* 当前退出的是 master，还有共享线程活着 → 全部杀掉 */
    for (si = 0; si < g_tskMaxNum; si++) {
        struct OsTaskCb *shared = &g_tskCbArray[si];
        if (shared == tskCb) continue;
        if (shared->pgShareMaster != tskCb) continue;
        if (!(shared->status & OS_TASK_STATUS_USED)) continue;
        shared->exitCode = (U32)-1;
        shared->status |= OS_TASK_STATUS_ZOMBIE;
        OsTaskDelete(shared->pid);       /* 递归，每个共享线程的 refCnt-- */
    }
}
```

**递归安全性**：`OsTaskDelete` 内部调用 `OsTaskDelete(shared->pid)` 形成递归。但 `OsTaskDelete` 不修改调用栈上的局部变量（只操作全局 TCB 数组），且中断已关闭，递归深度等于共享线程数量（通常很小），不会栈溢出。递归 `OsTaskDelete` 中会尝试唤醒 waitpid 中的父进程，但单核非抢占下只是标记 READY 不会抢占，当前 OsTaskDelete 流程会继续完成。

**与父子清理的交互**：C3 循环杀掉共享线程后，它们已被 `OsTaskDelete` 释放（status=0，归还空闲链表）。后续的父子进程清理循环（见 §12.6）遍历时这些线程的 `status & OS_TASK_STATUS_USED` 为假，会被跳过，不存在重复收割的问题。

### 12.6 父子进程清理与孤儿回收

`OsTaskDelete` 在引用计数逻辑之后，还有一段父子进程清理代码：

```c
/* 父进程退出时，处理子进程 */
if (tskCb->tskType == OS_TASK_PROCESS) {
    for (ci = 0; ci < g_tskMaxNum; ci++) {
        struct OsTaskCb *child = &g_tskCbArray[ci];
        if (child->parentPid != tskCb->pid) continue;
        if (!(child->status & OS_TASK_STATUS_USED)) continue;
        if (child->status & OS_TASK_STATUS_ZOMBIE) {
            OsTaskReapZombie(child);    /* 收割 zombie 子进程 */
        } else {
            child->parentPid = 0;       /* 子进程还在运行，设为孤儿 */
        }
    }
}
```

**两条规则**：
1. **ZOMBIE 子进程**：直接收割 TCB（完全释放）。父进程已退出，没人会 waitpid 它们了
2. **活着的子进程**：`parentPid` 设为 0（变为孤儿），退出时自行完全释放资源

**clone 线程与父子清理的交互**：clone 线程的 `parentPid == master->pid`，也是 master 的"子进程"。但 C3 修复在父子清理之前已杀掉所有共享线程（`OsTaskDelete` 递归调用后，共享线程的 status 被清零归还空闲链表），所以父子清理循环遍历时 `child->status & OS_TASK_STATUS_USED` 为假，跳过它们。不存在重复收割的问题。

**孤儿 ZOMBIE 的最终回收**：孤儿进程（`parentPid == 0`）退出后进入 ZOMBIE 状态，没有父进程可 waitpid。`OsTaskRecycleStk` 在每次 `OsTaskReapZombie` 或 `OsTaskDelete` 时被调用，其中检测到 `parentPid == 0` 的 ZOMBIE 任务，直接回收 TCB，避免资源泄漏。

---

## 13 POSIX pthread 用户态包装

### 13.1 接口列表

| 接口 | 实现 | 后端 |
|------|------|------|
| `pthread_t` | `typedef U32` | TCB pid |
| `pthread_mutex_t` | `struct { U32 semId; }` | 信号量 |
| `usr_clone(stackTop, startRoutine, arg, trampoline)` | `OsSyscall4(OS_SYS_CLONE, ...)` | 内核 clone |
| `usr_getpid()` | `OsSyscall0(OS_SYS_GETPID)` | 内核 getpid |
| `usr_fork()` | `OsSyscall0(OS_SYS_FORK)` | 内核 fork |
| `usr_waitpid(pid, &status, options)` | `OsSyscall3(OS_SYS_WAITPID, ...)` | 内核 waitpid |
| `usr_sem_create(type, initVal, maxCnt)` | `OsSyscall3(OS_SYS_SEM_CREATE, ...)` | 内核信号量 |
| `usr_sem_pend(semId, timeout)` | `OsSyscall2(OS_SYS_SEM_PEND, ...)` | 内核信号量 |
| `usr_sem_post(semId)` | `OsSyscall1(OS_SYS_SEM_POST, ...)` | 内核信号量 |
| `usr_sem_delete(semId)` | `OsSyscall1(OS_SYS_SEM_DELETE, ...)` | 内核信号量 |

### 13.2 pthread_create 语义

用户态通过 `usr_clone` 实现 `pthread_create` 语义：

```c
/* 简化版 pthread_create */
int pthread_create(pthread_t *thread, void *attr,
                   void *(*start_routine)(void *), void *arg)
{
    void *stack = usr_malloc(4096);
    U32 stackTop = (U32)(uintptr_t)stack + 4096;
    U32 tid = usr_clone(stackTop, (U32)(uintptr_t)start_routine,
                        (U32)(uintptr_t)arg, (U32)(uintptr_t)__pthread_entry);
    if (tid == (U32)-1) {
        usr_free(stack);
        return -1;
    }
    *thread = tid;
    return 0;
}
```

### 13.3 pthread_join 语义

`pthread_join(thread, &retval)` 直接复用 `usr_waitpid`：

```c
int pthread_join(pthread_t thread, void **retval)
{
    U32 status;
    U32 ret = usr_waitpid(thread, &status, 0);
    if (ret != thread) return -1;
    if (retval) *retval = (void *)(uintptr_t)(status >> 8);   /* exitCode << 8 */
    return 0;
}
```

### 13.4 pthread_mutex 语义

`pthread_mutex_t` 内含一个 `semId`，后端复用内核 BINARY_MUTEX 信号量：

| pthread_mutex 接口 | 内核后端 | 说明 |
|----|----|----|
| `pthread_mutex_init` | `usr_sem_create(OS_SEM_BINARY_MUTEX, 1, 1)` | 初始值=1（可用） |
| `pthread_mutex_lock` | `usr_sem_pend(semId, OS_SEM_WAIT_FOREVER)` | 获取（值→0） |
| `pthread_mutex_unlock` | `usr_sem_post(semId)` | 释放（值→1） |
| `pthread_mutex_destroy` | `usr_sem_delete(semId)` | 删除信号量 |

**为什么复用信号量？** BINARY_MUTEX 行为等同互斥锁，且支持优先级继承（priority inversion prevention）。不需要新增内核原语。

### 13.5 进程入口参数传递

`OsProcessCreate` 的 `param[0]`/`param[1]` 通过 `OsProcessEntry` 的 AllSaveContext 传到用户态：

```c
/* OsProcessEntry 中 */
allSaveContext->ebx = (U32)(uintptr_t)param1;   /* OS_PROC_ARG1() */
allSaveContext->ecx = (U32)(uintptr_t)param2;   /* OS_PROC_ARG2() */
```

参数传递链：`OsProcessCreateParam.param[0]` → `tskParam.arg[1]` → `OsTaskCommonEntry(arg[1])` → `OsProcessEntry(param1)` → `allSaveContext->ebx` → 用户态 `OS_PROC_ARG1()` 读取 ebx。

用户态通过 `OS_PROC_ARG1()` / `OS_PROC_ARG2()` 宏读取，这些宏展开为内联汇编读取 ebx/ecx。

---

## 14 usrVirMemPool 浅拷贝细节

### 14.1 共享位图页

clone 时 `child->usrVirMemPool = parent->usrVirMemPool` 是结构体赋值（浅拷贝）。两者的 `btmp.base` 指向同一个位图物理页。任何一方对位图的 set/clear 操作，另一方立即可见。

```
主进程 TCB                          clone 线程 TCB
┌───────────────────┐               ┌───────────────────┐
│ usrVirMemPool     │               │ usrVirMemPool     │
│ ├─ btmp.base ─────┼───┐           │ ├─ btmp.base ─────┼───┐
│ ├─ base           │   │  memcpy   │ ├─ base           │   │
│ ├─ size           │   │           │ ├─ size           │   │
│ └─ memCtrlList    │   │           │ └─ memCtrlList    │   │
└───────────────────┘   │           └───────────────────┘   │
                        ▼                                    ▼
                   ┌──────────┐                         ┌──────────┐
                   │ 位图页    │ ◄── 同一个物理页 ──────► │ 位图页    │
                   └──────────┘                         └──────────┘
```

### 14.2 memCtrlList 必须重新初始化

浅拷贝后，父线程和子线程有**独立的链表头**，但头节点的 `next/prev` 指向相同的链表节点。任何一方对链表的增删操作，另一方的链表头不会反映变化。

**修复**：浅拷贝后重新初始化子线程的 memCtrlList：

```c
child->usrVirMemPool = parent->usrVirMemPool;        /* 浅拷贝 */
OsListInit(&child->usrVirMemPool.memCtrlList);       /* 独立链表头，与 fork 一致 */
```

这与 fork 的处理方式完全一致（os_process.c:244-246）。clone 线程的初始状态没有独立的 memCtrl 记录——所有映射已由主进程建立，clone 线程通过共享 PGD 自然可见。

### 14.3 FSC 堆共享

clone 时 `child->usrFscCtrl = parent->usrFscCtrl`，共享同一个 FSC 控制块。共享地址空间必须共享同一个堆，否则 malloc 返回的地址可能重叠。一个线程 malloc 的内存，另一个线程可以访问和释放。

---

## 15 设计决策记录

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
| 用户堆 FSC 惰性初始化 | OsProcessCreate 期间 OS_RUNNING_TASK() 是创建者线程，缺页处理取不到新进程的位图 |
| FSC 尾部哨兵块 | 新映射物理页全零，ctrl=NULL 被误判为空闲块导致 free 崩溃 |
| 不新增 OS_TASK_USR_THREAD 枚举 | clone 线程与独立进程在特权级、CR3 切换等方面完全相同，pgShareMaster 区分即可 |
| pgDirRefCnt 放在 master TCB 中 | 引用计数与 PGD 生命周期绑定，单核关中断天然原子，O(1) 完成引用计数操作 |
| usrVirMemPool 浅拷贝 + 重新初始化 memCtrlList | 共享 btmp.base 但独立链表头，与 fork 处理方式一致 |
| 用户线程栈由用户态 malloc 提供 | POSIX 允许自定义栈，内核无需决定大小/映射页表，减少内核复杂度 |
| trampoline 跳板入口 | clone 子线程从 start_routine 开始，语义清晰，避免返回值检查的混乱 |
| clone 子线程 eflags 显式设 OS_PROCESS_EFLAGS | INT 0x80 中断门清除 IF，子线程必须以开中断状态启动 |
| 主进程退出时强制终止共享线程 | POSIX 语义：exit() 终止进程内所有线程；避免 pgShareMaster 悬空崩溃 |
| pthread_mutex 后端复用 BINARY_MUTEX 信号量 | 行为等同互斥锁，支持优先级继承，不需要新增内核原语 |
| pthread_join 复用 waitpid | 线程退出后同样 ZOMBIE，waitpid 扫描逻辑通用 |

---

## 16 已知限制与后续计划

| 限制 | 计划 |
|------|------|
| 用户栈只有 1 页（4KB），没有栈增长机制 | 实现缺页异常自动扩展用户栈 |
| 无 pthread_attr 支持（栈大小等不可配置） | 提供 pthread_attr_setstacksize |
| 无 pthread_key / TLS | 在 TCB 中新增 tls 指针，实现 pthread_key_create/set/getspecific |
| 无 pthread_cond | 新增 pthread_cond_t，后端复用信号量或新增等待队列 |
| 无 pthread_cancel | 新增 OS_SYS_TKILL 系统调用 + cancel point 机制 |
| 无 pthread_detach | 在 TCB 中新增 detached 标志，退出时跳过 ZOMBIE 直接回收 |
| 共享 FSC 在抢占内核中不安全 | 为 FSC 和位图添加自旋锁/互斥锁保护 |
| 没有 PTHREAD_MUTEX_INITIALIZER 静态初始化 | 需要延迟初始化机制 |
| 线程栈无 guard page | clone 时在用户栈底映射不可访问页，栈溢出触发 page fault |
| waitpid(-1) 不区分 fork 子进程和 clone 线程 | 在 TCB 中新增 isSharedThread 标志 |
| usrFscCtrl 内存从未释放（既有缺陷） | 新增 OsMemFscDestroy 函数 |

---

## 17 模块依赖

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
                                  ├─ OS_SYS_WRITE     → OsUartPutc
                                  ├─ OS_SYS_EXIT      → OsTaskDelete
                                  ├─ OS_SYS_MALLOC    → OsMemFscAlloc (惰性初始化)
                                  ├─ OS_SYS_FREE      → OsMemFscFree
                                  ├─ OS_SYS_SEM_*     → OsSemCreate/Pend/Post/Delete
                                  ├─ OS_SYS_GETPID    → OS_RUNNING_TASK()->pid
                                  ├─ OS_SYS_FORK      → OsProcessFork
                                  ├─ OS_SYS_WAITPID   → 扫描 ZOMBIE + OsTaskReapZombie
                                  └─ OS_SYS_CLONE     → 共享地址空间线程创建
```

---

## 18 相关文件索引

| 文件 | 内容 |
|------|------|
| kernel/task/process/os_process.c | OsProcessCreate、OsProcessFork、OsProcessFreeResources |
| kernel/task/os_task.c | OsTaskDelete（引用计数 + 主进程退出杀线程）、OsTaskReapZombie |
| kernel/include/os_process_external.h | OsProcessCreateParam、OsProcessEntryFunc、接口声明 |
| kernel/task/process/os_process_internal.h | 进程内部函数声明 |
| kernel/include/os_task_external.h | TCB 结构（pgShareMaster、pgDirRefCnt） |
| arch/cpu/i386/os_cpu_i386.c | OsProcessEntry（伪造上下文+iret）、OsConfigPgdForTskSwitch |
| arch/cpu/i386/os_cpu_i386.h | 用户栈/堆/选择子/EFLAGS 常量 |
| arch/cpu/i386/os_context_i386.h | OsAllSaveContext/OsFastSaveContext/OsExcSaveContext |
| arch/cpu/i386/os_dispatch.S | OsSyscallVector80（syscall 汇编入口）、OsHwiRet/OsAllLoad |
| arch/sys/os_syscall_i386.c | OsSyscallHandler、OsSysClone、OsSysFork、OsSysWaitpid |
| arch/sys/os_syscall_i386.h | syscall 号定义、用户态内联包装、pthread_t/pthread_mutex_t |
| arch/hwi/i386/os_hwi_i386.c | OsHwiConfigInit 中调 OsSyscallConfigInit 注册 INT 0x80 |
| arch/exc/i386/os_exc_i386.c | OsExcDispatcher 中用户态缺页堆扩展+异常杀进程 |
| arch/cpu/i386/pgt/os_pgt.c | OsCreateProcessPgd（创建独立页目录） |
| kernel/mem/fsc/os_mem_fsc.c | OsMemFscInitPt（含尾部哨兵）、OsMemFscAlloc/Free |
| kernel/sched/os_sched.c | OsSchedMain（调度，含 CR3/TSS 切换） |
| test/os_test_process.c | 进程测试（fork/waitpid/pthread-basic/pthread-mutex） |
