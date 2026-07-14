# pthread_detach 功能设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | pthread_detach（线程分离） |
| 涉及路径 | arch/sys/、kernel/task/、kernel/include/、test/ |
| 文档版本 | V8.0 |
| 编写日期 | 2026-07-13 |

---

## 2 模块概述

### 2.1 目的

实现 POSIX `pthread_detach` 语义：将线程标记为"分离"状态，使其退出后**自动回收资源**（TCB、内核栈），无需 `pthread_join`/`waitpid` 收割。

### 2.2 适用范围

所有通过 `usr_clone`（`OS_SYS_CLONE`）创建的共享地址空间用户线程。也支持线程 detach 自己（self-detach）。

### 2.3 设计约束

- 单核非抢占内核，所有 TCB 操作在关中断下进行
- 最大任务数 32（`OS_TASK_MAX_NUM`），ZOMBIE 任务占用 TCB 槽位，资源宝贵
- clone 线程退出后进入 ZOMBIE，当前**必须** waitpid 收割，否则 TCB 泄漏
- 不影响 fork 子进程和内核线程的既有行为
- **依赖单核非抢占下 `OsTaskDelete` 执行期间 TCB 不会被复用的假设**：`OsTaskDelete` 中关中断后，唯一可能触发调度的是 self-delete 分支的 `OsTaskSchedule()`，此分支在所有遍历/修改 TCB 的逻辑之后。若未来引入抢占，需重新审视 TCB 遍历中的状态一致性
- **OsSysExit 统一设 ZOMBIE**：无论 detached 与否，`OsSysExit` 总是设 ZOMBIE + 调用 `OsTaskDelete`。`OsTaskDelete` 内部根据 detached 决定是否保留 ZOMBIE。此解耦使 `OsSysExit` 无需感知 detach 模块

---

## 3 功能描述

### 3.1 功能列表

| 功能编号 | 功能名称 | 功能描述 |
|----------|----------|----------|
| DET-001 | pthread_detach | 将指定线程标记为 detached，退出时自动回收 |
| DET-002 | detached 线程自动回收 | detached 线程 exit 后跳过 ZOMBIE，直接释放 TCB |
| DET-003 | detached 线程 join 拒绝 | 对已 detached 线程调用 waitpid/join 返回错误 |
| DET-004 | self-detach | 线程可以 detach 自己（pthread_detach(pthread_self())） |
| DET-005 | 先退出后 detach | 对已 ZOMBIE 的线程调用 detach 时立即收割 |

### 3.2 各功能详细描述

#### 3.2.1 DET-001: pthread_detach

- **触发条件**：用户态调用 `pthread_detach(thread)`
- **处理流程**：
  1. 通过新 syscall `OS_SYS_DETACH` 进入内核
  2. 查找目标线程 TCB，校验 pid 合法、status 含 USED、tskType 为 PROCESS
  3. 校验调用者与目标线程在同一地址空间组（`pgShareMaster` 相同）
  4. 校验目标线程是 clone 线程（`pgShareMaster != self`），fork 子进程和独立进程不可 detach
  5. 校验目标线程没有正在被 waitpid 等待（防止 detach 后 waitpid 永久阻塞）
  6. 校验目标线程未已 detached（不可重复 detach）
  7. 如果目标线程**已经是 ZOMBIE**（先退出后 detach），直接收割并返回（不设 detached）
  8. 如果目标线程不是 ZOMBIE，设置 `tskCb->detached = TRUE`
  9. 返回 OS_OK
- **返回值**：0=成功，错误码=失败
- **错误码**：

| 错误码 | 含义 |
|--------|------|
| `OS_TASK_DETACH_INVALID_PID` (0x20) | pid 越界或 status 无效 |
| `OS_TASK_DETACH_NOT_PROCESS` (0x21) | 目标不是用户进程/线程 |
| `OS_TASK_DETACH_NOT_SAME_VM` (0x22) | 不在同一地址空间组 |
| `OS_TASK_DETACH_NOT_CLONE` (0x23) | 目标是独立进程（fork/OsProcessCreate），不可 detach |
| `OS_TASK_DETACH_ALREADY` (0x24) | 目标已 detached |
| `OS_TASK_DETACH_HAS_WAITER` (0x25) | 有线程正在 waitpid 等待目标 |

#### 3.2.2 DET-002: detached 线程自动回收

- **触发条件**：detached 线程调用 `usr_exit`（`OS_SYS_EXIT`）进入 `OsTaskDelete`
- **处理流程**：
  1. `OsSysExit` 中设置 ZOMBIE + 调用 `OsTaskDelete`
  2. `OsTaskDelete` 末尾，检查 `tskCb->detached`
  3. 若 detached，**跳过 ZOMBIE 保留逻辑**，直接将 status 清零并归还空闲链表
- **关键**：detached 线程退出时不唤醒任何 waitpid 等待者（detached 设置时已保证无人等待）
- **与引用计数的交互**：detached 只影响 TCB 回收时机，不影响 `pgDirRefCnt` 递减逻辑

#### 3.2.3 DET-003: detached 线程 join 拒绝

- **触发条件**：对已 detached 的线程调用 `usr_waitpid`
- **处理流程**：
  - 指定 pid 的 waitpid：在确认 parentPid 匹配后检查 detached，返回错误
  - `waitpid(-1)`：扫描时跳过 detached 的 ZOMBIE
  - `hasChild` 检查：区分"有可 join 的子线程"和"只有 detached 子线程"

#### 3.2.4 DET-004: self-detach

- **触发条件**：线程调 `pthread_detach(pthread_self())`
- **处理流程**：`OsSysDetach` 中 `thread == OS_RUNNING_TASK()->pid`，校验通过（同地址空间组、无 waitpid 等待者），设 `detached=TRUE`
- **语义**：线程退出后自动回收，无需 join

#### 3.2.5 DET-005: 先退出后 detach

- **触发条件**：目标线程已 ZOMBIE，调用者调 `pthread_detach`
- **处理流程**：`OsSysDetach` 检测到 `status & ZOMBIE` → 调用 `OsTaskReapZombie`
- **【R5-I4】语义警告**：detach 已 ZOMBIE 的线程等于放弃获取退出码的最后机会。收割后 TCB 立即回收，父进程后续 `waitpid` 将返回 ECHILD（目标已不存在）。这是 POSIX detach 语义的固有结果，符合预期。
- **调用链分析**（os_task.c）：
  ```
  OsSysDetach → OsTaskReapZombie(B):
    1. OsTaskRecycleStk()  — 释放 g_tskRecycleList 上的内核栈
       - B 的 freeListNode 可能在 g_tskRecycleList 上（self-delete 时挂入）
       - OsTaskRecycleStk 用 OsListPopHead 弹出，释放内核栈
       - B 的 parentPid != 0（指向调用 detach 的父进程）
       - 所以 OsTaskRecycleStk 保留 ZOMBIE，不归还 TCB 到 g_tskFreeList
    2. B->status = 0, B->pgDir = 0, B->usrFscCtrl = NULL
    3. OsListAddTail(&g_tskFreeList, &B->freeListNode)  — 安全归还
  ```
  **安全性**：`OsTaskRecycleStk` 对有父进程的 ZOMBIE 保留不归还，`OsTaskReapZombie` 随后安全归还。无 double-free。

---

## 4 数据结构

### 4.1 TCB 新增字段

在 `struct OsTaskCb`（`kernel/include/os_task_external.h`）中新增：

```c
bool detached;    /* TRUE=分离线程，退出时自动回收，不需 join
                   * FALSE=joinable（默认），退出后 ZOMBIE 等 waitpid */
```

**初始值**：`OsProcessCreate` 和 `OsProcessFork` 中设为 `FALSE`（默认 joinable）。
`OsSysClone` 中也设为 `FALSE`（clone 线程默认 joinable，需显式 detach）。

**字段位置**：放在 `waitPid` 之后，与进程生命周期管理字段聚簇。

**对齐影响**：`bool` 为 1 字节，放在 `U32 waitPid` 之后会引入 3 字节 padding（如果后续有指针/U64 字段）。32 个 TCB 共增加 128 字节（含 padding），可忽略。若未来 TCB 紧凑性成为问题，可改用位域 `U32 detached : 1`。

**回收时清理**：`OsTaskReapZombie` 和 `OsTaskRecycleStk` 回收 TCB 时，显式设置 `tskCb->detached = FALSE`，防止 TCB 复用时残留 TRUE 导致新任务被误判为 detached。

### 4.2 新增系统调用

```c
#define OS_SYS_DETACH  13    /* ebx=tid; 返回 0=成功, 错误码=失败 */
#define OS_SYS_NUM     14    /* 系统调用号总数更新 */
```

### 4.3 新增错误码

在 `kernel/include/os_task_external.h` 中新增：

```c
#define OS_TASK_DETACH_INVALID_PID   OS_BUILD_ERR_CODE(OS_MID_TASK, 0x20)
#define OS_TASK_DETACH_NOT_PROCESS   OS_BUILD_ERR_CODE(OS_MID_TASK, 0x21)
#define OS_TASK_DETACH_NOT_SAME_VM   OS_BUILD_ERR_CODE(OS_MID_TASK, 0x22)
#define OS_TASK_DETACH_NOT_CLONE     OS_BUILD_ERR_CODE(OS_MID_TASK, 0x23)
#define OS_TASK_DETACH_ALREADY       OS_BUILD_ERR_CODE(OS_MID_TASK, 0x24)
#define OS_TASK_DETACH_HAS_WAITER    OS_BUILD_ERR_CODE(OS_MID_TASK, 0x25)
```

### 4.4 用户态包装

```c
/* 在 arch/sys/os_syscall_i386.h 中新增 */
OS_INLINE int pthread_detach(pthread_t thread)
{
    return (int)OsSyscall1(OS_SYS_DETACH, thread);
}
```

---

## 5 处理流程

### 5.1 pthread_detach 流程（OsSysDetach）

```c
static OS_SEC_KERNEL_TEXT U32 OsSysDetach(U32 tid, U32 arg2, U32 arg3, U32 arg4)
{
    struct OsTaskCb *tskCb;
    struct OsTaskCb *curTsk;
    enum OsIntStatus intSave;
    U32 wi;

    (void)arg2; (void)arg3; (void)arg4;

    if (tid >= g_tskMaxNum) {
        return OS_TASK_DETACH_INVALID_PID;
    }

    intSave = OsIntLock();

    tskCb = OS_TASK_GET_CB(tid);
    curTsk = OS_RUNNING_TASK();

    /* 1. 目标必须存在且是用户进程/线程 */
    if (!(tskCb->status & OS_TASK_STATUS_USED)) {
        OsIntRestore(intSave);
        return OS_TASK_DETACH_INVALID_PID;
    }
    if (tskCb->tskType != OS_TASK_PROCESS) {
        OsIntRestore(intSave);
        return OS_TASK_DETACH_NOT_PROCESS;
    }

    /* 2. 调用者与目标必须在同一地址空间组（POSIX: 同进程内线程可互相 detach） */
    if (tskCb->pgShareMaster != curTsk->pgShareMaster) {
        OsIntRestore(intSave);
        return OS_TASK_DETACH_NOT_SAME_VM;
    }

    /* 3. 只有 clone 线程可以被 detach（fork 子进程和独立进程不可） */
    if (tskCb->pgShareMaster == tskCb) {
        OsIntRestore(intSave);
        return OS_TASK_DETACH_NOT_CLONE;
    }

    /* 4. 不可重复 detach */
    if (tskCb->detached) {
        OsIntRestore(intSave);
        return OS_TASK_DETACH_ALREADY;
    }

    /* 5. 如果目标线程的父进程正在 waitpid 等待目标，不允许 detach（防止永久阻塞）
     *    只检查目标线程的父进程（waiter->pid == tskCb->parentPid），
     *    其他进程的 waitpid(-1) 不影响 detach（它们不是目标的父进程） */
    for (wi = 0; wi < g_tskMaxNum; wi++) {
        struct OsTaskCb *waiter = &g_tskCbArray[wi];
        if (!(waiter->status & OS_TASK_STATUS_PENDING)) continue;
        if (waiter->waitPid == 0) continue;
        if (waiter->pid != tskCb->parentPid) continue;  /* 只检查目标线程的父进程 */
        if (waiter->waitPid == tid || waiter->waitPid == OS_WAIT_ANY_CHILD) {
            OsIntRestore(intSave);
            return OS_TASK_DETACH_HAS_WAITER;
        }
    }

    /* 6-7. ZOMBIE 时直接收割（不设 detached，TCB 已回收）；非 ZOMBIE 时设 detached */
    if (tskCb->status & OS_TASK_STATUS_ZOMBIE) {
        OsTaskReapZombie(tskCb);
    } else {
        tskCb->detached = TRUE;
    }

    OsIntRestore(intSave);
    return OS_OK;
}
```

### 5.2 detached 线程退出流程（OsTaskDelete 修改）

**修改位置**：`kernel/task/os_task.c` OsTaskDelete 函数第 369-374 行

```
detached 线程调用 usr_exit(result):
  OsSysExit:
    tskCb->exitCode = result
    tskCb->status |= OS_TASK_STATUS_ZOMBIE
    OsTaskDelete(pid)
      │
      ├─ ... (既有的从队列移除、释放消息、C3 杀线程等逻辑不变) ...
      ├─ pgDirRefCnt 递减 (不变)
      ├─ 父子清理 (不变)
      │
      ├─ 【修改点1】status 清零逻辑 (os_task.c:369-374):
      │   /* detached 线程跳过 ZOMBIE：status 清零直接回收。
      │    * 注意：下方唤醒条件用 detached（而非此时的 status）是因为
      │    * status 已被清零，需靠 detached 标志触发唤醒。【R4-M2】 */
      │   if (tskCb->status & OS_TASK_STATUS_ZOMBIE) {
      │       if (tskCb->detached) {
      │           tskCb->status = 0;          // ← detached: 跳过 ZOMBIE，直接清零
      │       } else {
      │           tskCb->status = OS_TASK_STATUS_USED | OS_TASK_STATUS_ZOMBIE;  // joinable: 保留 ZOMBIE 等 waitpid
      │       }
      │   } else {
      │       tskCb->status = 0;
      │   }
      │
      ├─ 【修改点2】唤醒 waitpid 等待者 (os_task.c:376-390):
      │   原逻辑：仅在 status & ZOMBIE 时唤醒，且只唤醒第一个匹配者（break）
      │   新逻辑：status & ZOMBIE **或** detached 时唤醒，唤醒**所有**匹配者（无 break）
      │   if ((tskCb->status & OS_TASK_STATUS_ZOMBIE) || tskCb->detached) {
      │       U32 wi;
      │       for (wi = 0; wi < g_tskMaxNum; wi++) {
      │           struct OsTaskCb *waiter = &g_tskCbArray[wi];
      │           if (!(waiter->status & OS_TASK_STATUS_PENDING)) continue;
      │           if (waiter->waitPid == 0) continue;
      │           if (waiter->waitPid == tskCb->pid || waiter->waitPid == OS_WAIT_ANY_CHILD) {
      │               waiter->status &= ~OS_TASK_STATUS_PENDING;
      │               waiter->status |= OS_TASK_STATUS_READY;
      │               OsSchedRdyListEnqueTsk(waiter);
      │               // 不 break，唤醒所有匹配的等待者
      │           }
      │       }
      │   }
      │   理由：
      │   - detached 线程退出后 status 清零，但 waitpid 等待者需要被唤醒
      │     以便重新扫描发现目标已回收，避免永久阻塞
      │   - 去掉 break 解决多个 waitpid(-1) 等待者死锁：
      │     如果只唤醒一个，它发现无 ZOMBIE 后重新 PENDING，其余等待者无人唤醒
      │   - 单核非抢占下安全：唤醒只是设 READY + 入就绪队列，不立即切换
      │
      ├─ 内核栈回收:
      │   if (tskCb == curTsk) {
      │       // self-delete: 挂入 g_tskRecycleList
      │       // status 保持(USED|ZOMBIE for joinable)，OsTaskRecycleStk 据此保留/回收
      │       OsListAddTail(&g_tskRecycleList, &tskCb->freeListNode);
      │       OsTaskSchedule();
      │   } else {
      │       // 被 C3 杀死或被其他方式删除: 直接归还
      │       if (tskCb->kernelStkTop != 0) {   // 【R5-C1】防御：OsMemFscFree 无 NULL 检查
      │           OsMemKernelFree((void *)tskCb->kernelStkTop);
      │           tskCb->kernelStkTop = 0;
      │       }
      │       tskCb->status = 0;         // 【N6-C1】显式清零，防止 ZOMBIE 残留导致 TCB 复用误判
      │       tskCb->pgDir = 0;          // 【R7-M2】与 OsTaskReapZombie 对齐，防止复用后残留共享 PGD 指针
      │       tskCb->usrFscCtrl = NULL;  // 【R7-C1】清零共享 FSC 指针（不可释放，属 master），
      │                                   //         防止 TCB 复用后新进程继承悬空 FSC 指针
      │       tskCb->detached = FALSE;   // 【R4-C1】防止 TCB 复用残留
      │       tskCb->exitCode = 0;       // 【R4-C1】清理残留
      │       tskCb->parentPid = 0;      // 【R4-I3】防止复用后 waitpid 误匹配
      │       OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
      │   }
      └─ OsIntRestore + return
```

**self-delete 完整数据流**：

```
detached 线程 self-delete:
  1. OsTaskDelete: status=0（跳过 ZOMBIE）
  2. 唤醒 waitpid 等待者（detached==TRUE，即使 status 不含 ZOMBIE 也唤醒）
  3. freeListNode 挂入 g_tskRecycleList
  4. OsTaskSchedule() → 切走
  5. OsHwiTail → OsTaskRecycleStk():
     - OsListPopHead 从 g_tskRecycleList 弹出
     - OsMemKernelFree 内核栈
     - status 不含 ZOMBIE → 走"回收 TCB"分支
     - tskCb->detached = FALSE（清理残留）
     - tskCb->exitCode = 0（清理残留）
     - OsListAddTail(&g_tskFreeList)
```

**C3 杀死共享线程**（joinable 与 detached 统一处理）：

```
主进程退出时杀共享线程:
  0. 【R5-C1】C3 循环前先调 OsTaskRecycleStk()，清空 g_tskRecycleList
     - 防止 self-delete 后挂在 g_tskRecycleList 上的 joinable ZOMBIE 线程
       被后续处理重复挂到 g_tskFreeList，导致链表损坏
     - OsTaskRecycleStk 对 detached 线程(status=0)直接回收 TCB
     - OsTaskRecycleStk 对 joinable ZOMBIE(parentPid=master≠0)摘除 freeListNode、
       free 内核栈(kernelStkTop=0)、保留 ZOMBIE 不归还
  1. C3 循环中按 ZOMBIE 状态分支处理：
     for (si ...) {
         if (shared == master) continue;
         if (shared->pgShareMaster != master) continue;
         if (!(shared->status & USED)) continue;
         shared->parentPid = 0;   // 【R4-C2】防止 master 父子清理重复处理

         if (shared->status & ZOMBIE) {
             /* 【R5-C1 核心】已退出的 ZOMBIE 共享线程：
              *   - pgDirRefCnt 已在自身 self-delete 时递减过，不能再 OsTaskDelete（会 double-decrement）
              *   - kernelStkTop 已被 OsTaskRecycleStk 置 0，不能再 OsMemKernelFree（free(0) 崩溃）
              *   - freeListNode 已从 g_tskRecycleList 摘除，OsTaskReapZombie 可安全归还
              *   直接收割 TCB */
             OsTaskReapZombie(shared);
         } else {
             /* 仍在运行的共享线程：强制终止（首次删除，递减 pgDirRefCnt） */
             if (!shared->detached) {
                 /* 【R7-I4】原 shared->exitCode=(U32)-1 为死代码：parentPid 已被 C3 置 0，
                  *         无人 waitpid 此线程；且 else 分支 exitCode=0 会覆盖之。仅保留 ZOMBIE 标记。*/
                 shared->status |= OS_TASK_STATUS_ZOMBIE;
             }
             OsTaskDelete(shared->pid);   // detached: status=0; joinable: 保留 ZOMBIE
         }
     }
  2. 回到 master 的 OsTaskDelete 父子清理循环：
     - 所有 shared->parentPid == 0（C3 已设）→ 不匹配 master->pid → 跳过
     - 避免 OsTaskReapZombie 重复归还（double-free 修复）
```

**R5-C1 既有 bug 说明（三个互相关联的问题）**：joinable 共享线程 self-delete 后 freeListNode 挂在 g_tskRecycleList 上，status=USED|ZOMBIE，pgDirRefCnt 已递减。若 master 不 waitpid 直接退出，C3 会找到它。原来的 C3 统一调 OsTaskDelete(shared) 会导致三重损坏：
1. **链表损坏**：else 分支 `OsListAddTail(&g_tskFreeList)` 时 freeListNode 仍挂在 g_tskRecycleList 上 → 同一节点挂两条链表
2. **free(0) 崩溃**：若 OsTaskRecycleStk 已先运行（timer tick），kernelStkTop 被置 0，OsTaskDelete else 分支 `OsMemKernelFree(0)` → OsMemFscFree(0) 解引用近 0 地址崩溃（OsMemFscFree 无 NULL 检查，仅 OsSysFree 有）
3. **pgDirRefCnt double-decrement**：OsTaskDelete 中 `master->pgDirRefCnt--` 对已 ZOMBIE 线程重复执行 → 下溢

**修复方案**：C3 前调 `OsTaskRecycleStk()` 清空回收链表，然后 C3 循环按 ZOMBIE 分支——已 ZOMBIE 的用 `OsTaskReapZombie`（不递减 pgDirRefCnt、不 free 栈、OsTaskRecycleStk 已摘除 freeListNode），运行中的用 `OsTaskDelete`。三个问题一并解决。此 bug 既有（不限于 detach），但 detach 使"线程不 join 就退出"成为常见模式，显著增加触发概率。

**R4-C2 既有 bug 说明**：在 detach 实施前，C3 递归 OsTaskDelete 后，shared 的 status 被设为 `USED|ZOMBIE`（保留 USED），else 分支归还 freeList（第一次）。随后 master 的父子清理循环匹配 `parentPid == master->pid` 且 `USED|ZOMBIE`，调用 `OsTaskReapZombie` 再次归还（第二次）→ 链表损坏。当前测试未触发因为主进程总是 join 完所有线程再退出（`pgDirRefCnt == 1`，C3 不运行）。C3 设 `shared->parentPid = 0` 彻底解决此问题，对 joinable 和 detached 都生效。

**关键**：OsTaskRecycleStk 不需要新增 detached 分支——detached 线程 status 在 OsTaskDelete 中已清零，OsTaskRecycleStk 自然走"非 ZOMBIE → 直接回收"分支。

### 5.3 OsSysWaitpid 修改

**修改位置**：`arch/sys/os_syscall_i386.c` OsSysWaitpid

**修改 1**：两个 ZOMBIE 扫描循环中都跳过 detached

```c
/* 第一个扫描循环 (os_syscall_i386.c:148-163) */
for (i = 0; i < g_tskMaxNum; i++) {
    struct OsTaskCb *child = &g_tskCbArray[i];
    if (child == curTsk) continue;
    if (child->parentPid != curTsk->pid) continue;
    if (!(child->status & OS_TASK_STATUS_ZOMBIE)) continue;
    if (child->detached) continue;                          // ← 新增
    if (pid != OS_WAIT_ANY_CHILD && child->pid != pid) continue;
    // 收割 ...
}

/* 第二个扫描循环 (唤醒后再次扫描, os_syscall_i386.c:192-204) */
for (i = 0; i < g_tskMaxNum; i++) {
    struct OsTaskCb *child = &g_tskCbArray[i];
    if (child->parentPid != curTsk->pid) continue;
    if (!(child->status & OS_TASK_STATUS_ZOMBIE)) continue;
    if (child->detached) continue;                          // ← 新增
    if (pid != OS_WAIT_ANY_CHILD && child->pid != pid) continue;
    // 收割 ...
}
```

**修改 2**：指定 pid 的 waitpid，在 parentPid 匹配后检查 detached

```c
/* 在两个循环的 "if (pid != OS_WAIT_ANY_CHILD && child->pid != pid) continue;" 之后，
 * 收割之前，增加 detached 检查 */
if (child->detached) {
    return (U32)-1;  /* 对 detached 线程 join 无效 */
}
```

注意：不在函数开头做 `target->detached` 检查——因为需要先确认 parentPid 匹配（权限校验），再检查 detached。在函数开头检查可能对非子进程返回 -1 而非 ECHILD。

**修改 3**：hasChild 检查区分可 join 和 detached

```c
/* 替换原有 hasChild 检查（入口处和唤醒后都需要使用） */
{
    bool hasJoinable = FALSE;
    for (i = 0; i < g_tskMaxNum; i++) {
        if (g_tskCbArray[i].parentPid == curTsk->pid &&
            (g_tskCbArray[i].status & OS_TASK_STATUS_USED) &&
            !g_tskCbArray[i].detached) {
            hasJoinable = TRUE;
            break;
        }
    }
    if (!hasJoinable) {
        return (U32)-1;  /* ECHILD: 没有可 join 的子进程 */
    }
}
```

**修改 4**：唤醒后第二段扫描找不到 ZOMBIE 时，不再直接返回 -1，而是重新检查 hasJoinable

```c
/* 唤醒后再次扫描（单核非抢占，无竞态） */
for (i = 0; i < g_tskMaxNum; i++) {
    struct OsTaskCb *child = &g_tskCbArray[i];
    if (child->parentPid != curTsk->pid) continue;
    if (!(child->status & OS_TASK_STATUS_ZOMBIE)) continue;
    if (child->detached) continue;
    if (pid != OS_WAIT_ANY_CHILD && child->pid != pid) continue;

    if (statusPtr != 0) {
        *(U32 *)(uintptr_t)statusPtr = (child->exitCode << 8);
    }
    OsTaskReapZombie(child);
    curTsk->waitPid = 0;
    return child->pid;
}

/* 被唤醒但未找到 ZOMBIE（可能是 detached 线程退出唤醒的），
 * 重新检查是否还有可 join 的子进程 */
{
    bool hasJoinable = FALSE;
    for (i = 0; i < g_tskMaxNum; i++) {
        if (g_tskCbArray[i].parentPid == curTsk->pid &&
            (g_tskCbArray[i].status & OS_TASK_STATUS_USED) &&
            !g_tskCbArray[i].detached) {
            hasJoinable = TRUE;
            break;
        }
    }
    if (hasJoinable) {
        /* 还有 joinable 子进程未退出，重新 PENDING 等待 */
        curTsk->waitPid = pid;
        curTsk->status |= OS_TASK_STATUS_PENDING;
        OsSchedRdyListDequeTsk(curTsk);
        OsTaskSchedule();
        /* 被唤醒后再次扫描（goto 或循环重试） */
    } else {
        curTsk->waitPid = 0;
        return (U32)-1;  /* ECHILD: 所有子线程都是 detached 或已退出 */
    }
}
```

### 5.4 OsTaskReapZombie 和 OsTaskRecycleStk 中的 detached 清理

**归还 TCB 到 g_tskFreeList 的完整路径清单**（所有路径都必须清零 `detached`、`exitCode`、`parentPid`，防止 TCB 复用残留）：

| 路径 | 函数 | 触发场景 | 清零字段 |
|------|------|---------|---------|
| 1 | `OsTaskReapZombie` | waitpid 收割 ZOMBIE | detached, exitCode, parentPid |
| 2 | `OsTaskRecycleStk` 孤儿 ZOMBIE | self-delete 后无父进程 | detached, exitCode, parentPid |
| 3 | `OsTaskRecycleStk` 非 ZOMBIE | self-delete 后非 ZOMBIE | detached, exitCode, parentPid |
| 4 | `OsTaskDelete` else 分支 | C3 杀线程 / 非递归删除 | pgDir, usrFscCtrl, detached, exitCode, parentPid |
| 5 | `OsTaskReleaseFreeCb` | fork/clone 失败回滚 | detached, exitCode, parentPid（已有 status=0, pgDir=0） |

**OsTaskReapZombie**（os_task.c:94-103）：
```c
tskCb->status = 0;
tskCb->pgDir = 0;
tskCb->usrFscCtrl = NULL;
tskCb->detached = FALSE;   /* 清理残留 */
tskCb->exitCode = 0;       /* 清理残留 */
tskCb->parentPid = 0;      /* 【R4-I3】防止复用后 waitpid 误匹配 */
OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
```

**OsTaskDelete else 分支**（os_task.c:397-400）：
```c
} else {
    /* 防御性检查：kernelStkTop 为 0 时跳过 free。
     * 正常路径（C3 杀运行中的线程）kernelStkTop 非 0；
     * 此检查防御未来代码路径或异常状态。
     * 注意：OsMemFscFree 无 NULL 检查（仅 OsSysFree 有），free(0) 会崩溃 */
    if (tskCb->kernelStkTop != 0) {
        OsMemKernelFree((void *)tskCb->kernelStkTop);
        tskCb->kernelStkTop = 0;
    }
    tskCb->status = 0;         /* 【N6-C1】显式清零，防止 ZOMBIE 残留导致 TCB 复用误判
                                * joinable 线程被 C3 杀时 status=USED|ZOMBIE，必须清零 */
    tskCb->pgDir = 0;          /* 【R7-M2】与 OsTaskReapZombie 对齐，防止复用后残留共享 PGD 指针 */
    tskCb->usrFscCtrl = NULL;  /* 【R7-C1】清零共享 FSC 指针（不可释放，属 master），
                                *         防止 TCB 复用后新进程继承悬空 FSC 指针 */
    tskCb->detached = FALSE;   /* 【R4-C1】防止 TCB 复用残留 */
    tskCb->exitCode = 0;
    tskCb->parentPid = 0;      /* 【R4-I3】 */
    OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
}
```

**OsTaskReleaseFreeCb**（os_task.c:82-87）：
```c
OS_SEC_KERNEL_TEXT void OsTaskReleaseFreeCb(struct OsTaskCb *tskCb)
{
    tskCb->status = 0;
    tskCb->pgDir = 0;
    tskCb->detached = FALSE;   /* 【R5-C2】完整清理 */
    tskCb->exitCode = 0;       /* 【R5-C2】 */
    tskCb->parentPid = 0;      /* 【R5-C2】fork 失败回滚时 parentPid 已被设置，必须清零 */
    tskCb->kernelStkTop = 0;   /* 【N6-M1】与其他回收路径一致（fork 回滚时栈已 free 或未分配） */
    OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
}
```

**OsTaskDelete pgDirRefCnt 递增/递减**（os_task.c:340-347）：
```c
if (tskCb->tskType == OS_TASK_PROCESS && tskCb->pgDir) {
    struct OsTaskCb *master = tskCb->pgShareMaster;
    OS_PANIC_IF(master->pgDirRefCnt == 0, "pgDirRefCnt underflow");  /* 【N6-M2】防止下溢静默绕回 */
    master->pgDirRefCnt--;
    if (master->pgDirRefCnt == 0) {
        OsProcessFreeResources(master);   /* 此后 master->pgDir=0 */
    }
    /* 【N6-I3】此后 tskCb->pgDir 可能为 0（若 refCnt 归零则 OsProcessFreeResources 已清零），
     *         后续代码（父子清理/status清零/唤醒/栈回收）不得再访问 pgDir */
}
```

**OsTaskRecycleStk**（os_task.c:407-436）：
在所有归还 TCB 到 g_tskFreeList 的路径中（孤儿 ZOMBIE 回收、非 ZOMBIE 回收），增加 `tskCb->detached = FALSE; tskCb->exitCode = 0; tskCb->parentPid = 0;`。

**【R5-I5】usrFscCtrl 不清零说明**：OsTaskRecycleStk 非 ZOMBIE 分支（detached 线程 self-delete 后触发）不清零 `usrFscCtrl`。因为进入此分支的只有 detached 线程，其 `usrFscCtrl` 由 `pgShareMaster` 持有（共享），不可在此释放。若误清零会导致 master 的 FSC 指针失效。

---

## 6 与其他模块的交互

### 6.1 依赖关系

| 本模块依赖 | 用途 |
|-----------|------|
| OsTaskDelete | detached 线程退出时跳过 ZOMBIE |
| OsTaskReapZombie | 先退出后 detach 时立即收割 |
| OsSysWaitpid | 跳过 detached ZOMBIE |
| OsSysExit | 线程退出入口，不修改 |
| OsSysClone | 初始化 detached=FALSE |

### 6.2 被依赖关系

无。本功能是终端 API，不被其他模块依赖。

---

## 7 设计决策记录

| 决策 | 理由 |
|------|------|
| 新增 `bool detached` 字段而非复用 status 位 | status 是位图，detached 是独立布尔语义；用位图需要加 `OS_TASK_STATUS_DETACHED`，与 ZOMBIE 等互斥逻辑混在一起更复杂 |
| 新增 OS_SYS_DETACH 系统调用 | 需要修改内核 TCB 字段，必须通过 syscall 进入内核态；不能在用户态设置 |
| detached 线程退出时 status 直接清零（跳过 ZOMBIE） | ZOMBIE 的唯一目的是等 waitpid 收割，detached 不需要等，所以不保留 ZOMBIE |
| 先退出后 detach 时立即收割 | POSIX 允许此场景，如果不收割则 TCB 泄漏 |
| 对 detached 线程 waitpid 返回 -1 | POSIX 未定义行为，返回错误最安全 |
| waitpid(-1) 跳过 detached ZOMBIE | detached ZOMBIE 不属于任何人的"可收割"集 |
| hasJoinable 检查替代 hasChild | 防止 waitpid(-1) 对全 detached 子线程永久阻塞 |
| OsSysDetach 检查 pgShareMaster | 同一地址空间组才能互相 detach，符合 POSIX 语义（同进程内） |
| OsSysDetach 检查 waitpid 等待者 | 防止 detach 后 waitpid 永久阻塞（有线程在等就不许 detach） |
| 重复 detach 返回错误（非幂等） | 与 Linux glibc（返回 0）不同；更严格，防止编程错误被掩盖 |
| 允许 self-detach | self-detach 在当前设计中自然可用，无需禁止 |
| 回收 TCB 时显式清零 detached | 防止 TCB 复用时残留 detached=TRUE 导致新任务被误判 |
| 只有 clone 线程可 detach（pgShareMaster != self） | fork 子进程和独立进程不可 detach，否则父进程 waitpid 永久阻塞 |
| detached 线程退出也唤醒 waitpid 等待者 | 解决 WNOHANG 轮询间隙 detach 的 ABA 竞态，等待者被唤醒后重新扫描发现目标已回收 |
| ZOMBIE 时直接收割不设 detached | OsSysDetach 中如果目标已是 ZOMBIE，直接 OsTaskReapZombie 返回，不需要设 detached（TCB 已回收） |
| 唤醒所有匹配 waitpid 等待者（去掉 break） | 如果只唤醒一个，它发现无 ZOMBIE 后重新 PENDING，其余等待者无人唤醒，导致死锁。唤醒所有让它们竞争收割 |
| C3 杀 detached 线程不设 ZOMBIE | detached 线程不需要 ZOMBIE，C3 中统一设 ZOMBIE 是多余的。不设 ZOMBIE 使 OsTaskDelete 走非 ZOMBIE 分支，逻辑更清晰 |
| 唤醒后第二段扫描找不到 ZOMBIE 时重新检查 hasJoinable | detached 线程退出可能唤醒等待者，但无 ZOMBIE 可收割。hasJoinable 检查决定是重新 PENDING 还是返回 ECHILD |
| OsSysDetach 等待者检查只扫描目标线程的父进程 | 不同进程的 waitpid(-1) 不应阻止 detach，只有目标的父进程才有权 join |
| 所有归还 freeList 路径清零 detached/exitCode/parentPid | 【R4-C1/R4-I3】TCB 复用时残留字段会导致新任务被误判为 detached 或 waitpid 误匹配。完整路径清单见 5.4 |
| C3 设 shared->parentPid=0 | 【R4-C2】修复既有 double-free：C3 递归 OsTaskDelete 后，父子清理循环不再重复处理 shared 线程 |
| C3 循环前调 OsTaskRecycleStk + ZOMBIE 分支处理 | 【R5-C1】修复既有三重损坏（链表损坏/free(0)崩溃/pgDirRefCnt double-decrement）：C3 前清空回收链表，已 ZOMBIE 的共享线程用 OsTaskReapZombie（不递减 pgDirRefCnt、不 free 栈），运行中的用 OsTaskDelete |
| C3 仅对非 ZOMBIE 线程设 exitCode=-1 | 【R5-I1】保留自愿退出（OsSysExit）线程的有效退出码，仅对被强制终止的线程设 -1 |
| else 分支显式 status=0 | 【N6-C1】消除 TCB 复用后 ZOMBIE 残留：joinable 线程被 C3 杀时 status=USED|ZOMBIE，归还 freeList 前必须清零，否则 OsTaskSetCb(`\|=USED`) 不清 ZOMBIE，新任务携带虚假 ZOMBIE |
| pgDirRefCnt 递减前 OS_PANIC_IF(==0) | 【N6-M2】U16 下溢会静默绕回 65535，断言使 double-decrement bug 立即暴露 |
| OsSysClone 显式初始化 detached=FALSE | 防御性编程，不依赖 TCB 回收时的清零 |
| 错误码从 0x20 开始 | 为既有错误码 0x00-0x0B 保留间隔，避免未来冲突 |

---

## 8 已知限制与未来工作

| 限制 | 计划 |
|------|------|
| 无 pthread_attr_setdetachstate | 创建时指定 DETACHED 属性，需扩展 OsSysClone 参数 |
| detached 不影响 pgDirRefCnt | 地址空间仍按引用计数释放，detach 只管 TCB 回收 |
| self-detach 后线程退出时无法获取退出码 | POSIX 规定 detached 线程的返回值被丢弃，这是正确行为 |
| 线程栈未在 detached 退出时释放 | 用户态 malloc 的栈需要在 start_routine 中手动 free，或提供 pthread_attr_setstack 机制 |
| 对 detached 线程 waitpid 返回 -1（ECHILD 语义），非 POSIX 的 EINVAL | 错误码体系待完善 |
| 只有 clone 线程可 detach，fork 子进程和独立进程不可 | 语义限制：fork 子进程有独立地址空间，detach 语义不适用 |
| 唤醒所有匹配等待者引入冗余唤醒 | 【R4-I1】joinable 线程退出时多个 waitpid(-1) 等待者被唤醒，只有一个能收割。32 任务规模下可接受，精确唤醒方案会与 ABA 修复冲突 |
| detached 频繁退出导致 waitpid(-1) 抖振 | 【R4-I2】detached 子线程退出唤醒等待 joinable 子线程的父进程，扫描无 ZOMBIE 后重新 PENDING。性能退化非功能错误 |
| 全 detached 子线程时 waitpid 返回 ECHILD | 【R5-I2/R7-C2】detached 线程不是 waitpid 合法目标，全 detached 时返回 ECHILD 语义合理，但与 POSIX WNOHANG"有子进程运行返回 0"有差异。**共享地址空间安全风险**：master 收到 ECHILD 后若认为"无子任务"而直接退出/释放资源，仍存活的 detached 共享线程会被 C3 终止——功能正确，但调用方须明白 ECHILD ≠ "无共享者运行"，detached 线程在 master 退出前仍持有共享地址空间 |
| detach 已 ZOMBIE 线程后父进程丢失退出码 | 【R5-I4】POSIX detach 语义固有结果，DET-005 已警告 |
| C3 杀持有互斥信号量的 shared 线程失败 | 【N6-C2】既有 bug：OsTaskDelete 持有信号量时返回 HOLD_SEM 不清理，C3 不检查返回值 → pgDirRefCnt 泄漏。彻底修复需强制释放信号量机制，超出 detach 范围 |
| waitpid 多等待者频繁唤醒可能活锁 | 【N6-I4】detached 线程退出唤醒所有匹配等待者，性能问题非功能错误 |

---

## 9 修改文件清单

| 文件 | 改动内容 | 行数估计 |
|------|---------|---------|
| `kernel/include/os_task_external.h` | TCB 新增 `bool detached` 字段; 新增 6 个错误码(0x20-0x25) | +8 |
| `arch/sys/os_syscall_i386.h` | `OS_SYS_DETACH=13`, `OS_SYS_NUM=14`; `pthread_detach` 包装 | +5 |
| `arch/sys/os_syscall_i386.c` | 新增 `OsSysDetach` 函数; `OsSysWaitpid` 增加 detached 跳过和 hasJoinable 检查; `OsSysClone` 初始化 detached=FALSE; `OsSyscallConfigInit` 注册 `OS_SYS_DETACH` | +60 |
| `kernel/task/os_task.c` | `OsTaskDelete` 中 detached 跳过 ZOMBIE + 唤醒所有匹配等待者 + else 分支 kernelStkTop 防御检查 + status=0 + 清零 detached/exitCode/parentPid; C3 前调 OsTaskRecycleStk + ZOMBIE 分支(OsTaskReapZombie/OsTaskDelete) + parentPid=0 + exitCode 仅对非 ZOMBIE; pgDirRefCnt 下溢断言; `OsTaskReapZombie`/`OsTaskRecycleStk`/`OsTaskReleaseFreeCb` 清零 pgDir/usrFscCtrl/detached/exitCode/parentPid/kernelStkTop | +34 |
| `kernel/task/process/os_process.c` | `OsProcessCreate`/`OsProcessFork` 初始化 `detached=FALSE` | +2 |
| `test/os_test_process.c` | pthread-detach 测试（含 self-detach、先退出后 detach、detach+join 错误、TCB 复用后 detached 不残留） | +110 |
| `test/os_test_registry.c` | 注册 pthread-detach | +2 |

**总改动量**：~230 行，7 个文件（含 R4/R5/R6/R7 既有 bug 修复）。

---

## 10 多轮对抗式审查记录

本设计经与 Haiku（高级 OS 内核架构师）多轮对抗式审查迭代，逐轮修复 CRITICAL/IMPORTANT 问题。收敛判据：0 CRITICAL、所有 IMPORTANT 已处理或建档延后、连续 2 轮无新增 CRITICAL/IMPORTANT。

| 轮次 | 关键发现 | 处置 |
|------|---------|------|
| R3 | 多 waitpid(-1) 等待者死锁；C3 唤醒时机；回退路径 | 改为唤醒所有匹配等待者；C3 在 refCnt 递减前杀共享线程；统一回滚 |
| R4 | else 分支 detached 残留；C3 double-free | else 分支清零 detached/exitCode；C3 设 `shared->parentPid=0` |
| R5 | g_tskRecycleList 损坏；OsTaskReleaseFreeCb parentPid 残留 | C3 前调 OsTaskRecycleStk + ZOMBIE 分支；回滚路径清零 parentPid |
| R6 | TCB ZOMBIE 残留（OsTaskSetCb 用 `\|=` 不清 status）；持信号量线程 C3 失败 | else 分支显式 `status=0`；持信号量问题建档延后（N6-C2） |
| R7 | else 分支未清零 `usrFscCtrl`（既有 bug，detach 放大）；ECHILD 共享地址空间安全措辞；C3 `exitCode=-1` 死代码 | else 分支补 `usrFscCtrl=NULL`+`pgDir=0`；ECHILD 措辞升级为"共享地址空间安全风险"；移除死代码 |

### 10.1 R7 结论

**detach 设计已收敛。** 第 7 轮未发现任何 detach 独有的 CRITICAL 问题。R7-C1 为既有 bug（`OsProcessCreate` 不初始化 `usrFscCtrl`，依赖 TCB 出 freeList 时已清零），detach 的 else 分支（共享线程退出、master 仍存活、refCnt 未降到 0 → 不调 `OsProcessFreeResources` → `usrFscCtrl` 不被清零）使其暴露为 TCB 复用后新进程继承悬空 FSC 指针。本轮已用一行 `tskCb->usrFscCtrl = NULL;` 修复（与 `OsTaskReapZombie` 对齐），并顺手补 `pgDir = 0`（R7-M2）。

### 10.2 延后项（建档，非本轮范围）

| 编号 | 问题 | 延后理由 |
|------|------|---------|
| N6-C2 | C3 杀持有互斥信号量的 shared 线程失败 → pgDirRefCnt 泄漏 | 需强制释放信号量机制，超出 detach 范围 |
| I2 | `waitpid(-1)` 不区分 fork 子进程与 clone 线程 | 需 TCB 新增 `isSharedThread` 标志，当前测试不触发 |
| I3 | `usrFscCtrl` 控制块本身未释放 | 既有缺陷，教学 OS 进程生命周期短，泄漏有限 |
| I4 | `pthread_cancel` 缺失 | 需 `OS_SYS_TKILL` + cancel point，独立特性 |
| R7-M1 | `OsTaskReapZombie` 内部 `OsTaskRecycleStk()` 在 C3 路径下冗余（列表已空，为 no-op） | 该调用对正常 waitpid 路径是必需的（释放 self-delete 内核栈）；C3 路径下为无害空操作，移除会破坏 OsTaskReapZombie 契约，保留 |
