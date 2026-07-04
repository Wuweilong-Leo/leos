# 任务管理模块功能设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | 任务管理（Task） |
| 模块路径 | kernel/task/ |
| 文档版本 | V1.0 |
| 编写日期 | 2026-07-03 |

## 2 模块概述

### 2.1 目的

管理内核线程的完整生命周期：创建、挂起、恢复、删除、延时。维护任务控制块（TCB）池和延时链表，提供任务自删除的安全回收机制。

### 2.2 适用范围

所有内核线程的创建与状态管理。进程通过本模块创建线程骨架后叠加进程属性。

### 2.3 设计约束

- 最大任务数 32（OS_TASK_MAX_NUM）
- 内核栈大小 4K（OS_TASK_KERNEL_STACK_SIZE）
- 优先级 0~30 可用，31 为 idle 专属
- 任务自删除时不能立即释放栈（当前还在用）

## 3 功能描述

### 3.1 功能列表

| 功能编号 | 功能名称 | 功能描述 |
|----------|----------|----------|
| TASK-001 | TCB 池初始化 | 分配 TCB 数组，初始化空闲链表 |
| TASK-002 | 任务创建 | 分配 TCB 和内核栈，伪造上下文，设置初始参数 |
| TASK-003 | idle 任务创建 | 创建优先级 31 的 idle 任务（不走优先级校验） |
| TASK-004 | 任务恢复 | 清除挂起状态，加入就绪队列 |
| TASK-005 | 任务挂起 | 从就绪队列移除，设置挂起状态 |
| TASK-006 | 任务删除 | 清理所有状态，释放栈和 TCB |
| TASK-007 | 任务延时 | 设置到期时刻，从就绪队列移除，插入延时链 |
| TASK-008 | 任务调度请求 | 检查 needSched 标志，触发上下文切换 |
| TASK-009 | 延时链插入 | 按到期时刻升序插入延时链表 |
| TASK-010 | 栈回收 | 在系统栈上安全回收已删除任务的栈内存 |

## 4 数据设计

### 4.1 任务控制块

```
结构体名称：OsTaskCb
用途：描述一个任务的所有状态信息

┌──────────────────┬────────────────────────┬────────────────────────────────────────┐
│ 字段名           │ 类型                   │ 说明                                   │
├──────────────────┼────────────────────────┼────────────────────────────────────────┤
│ stkPtr           │ uintptr_t              │ 上下文保存点（栈指针）                 │
│ kernelStkTop     │ uintptr_t              │ 内核栈底地址（用于释放）               │
│ freeListNode     │ struct OsList          │ 空闲 TCB 链表 / 回收链表节点           │
│ pid              │ U32                    │ 任务 ID（= TCB 数组下标）              │
│ entry            │ OsTaskEntryFunc        │ 用户入口函数                           │
│ arg[4]           │ void *                 │ 入口参数（最多 4 个）                  │
│ status           │ U32                    │ 任务状态位图                           │
│ prio             │ U32                    │ 当前优先级（可能被 PI 修改）           │
│ oriPrio          │ U32                    │ 原始优先级（PI 恢复用）               │
│ timeSliceTicks   │ U64                    │ 剩余时间片 tick 数                     │
│ expiredTick      │ U64                    │ 延时到期时刻                           │
│ name[16]         │ char                   │ 任务名称                               │
│ rdyListNode      │ struct OsList          │ 就绪队列节点                           │
│ pendListNode     │ struct OsList          │ 信号量等待队列节点                     │
│ timerListNode    │ struct OsList          │ 延时链节点                             │
│ holdSemList      │ struct OsList          │ 持有的互斥信号量链表头                 │
│ msgList          │ struct OsList          │ 消息信箱链表头                         │
│ tskType          │ enum OsTaskType        │ THREAD 或 PROCESS                      │
│ pgDir            │ uintptr_t              │ 进程页目录虚拟地址（线程为 0）         │
│ usrVirMemPool    │ struct OsMemPool       │ 进程用户虚拟内存池                     │
└──────────────────┴────────────────────────┴────────────────────────────────────────┘
```

### 4.2 任务状态位图

| 位 | 常量名 | 值 | 含义 |
|----|--------|-----|------|
| 0 | OS_TASK_STATUS_USED | 0x1 | TCB 已分配 |
| 1 | OS_TASK_STATUS_READY | 0x2 | 在就绪队列中 |
| 2 | OS_TASK_STATUS_RUNNING | 0x4 | 正在运行 |
| 3 | OS_TASK_STATUS_PENDING | 0x8 | 在信号量等待队列中 |
| 4 | OS_TASK_STATUS_IN_DELAY | 0x10 | 在延时链表中 |
| 5 | OS_TASK_STATUS_TIMEOUT | 0x20 | 等待超时被唤醒 |
| 6 | OS_TASK_STATUS_SUSPENDED | 0x40 | 被挂起 |
| 7 | OS_TASK_STATUS_PEND_MSG | 0x80 | 在等待消息 |

### 4.3 任务创建参数

```
结构体名称：OsTaskCreateParam
用途：传递任务创建所需参数

┌──────────────────┬────────────────────────┬────────────────────────────────────────┐
│ 字段名           │ 类型                   │ 说明                                   │
├──────────────────┼────────────────────────┼────────────────────────────────────────┤
│ name[16]         │ char                   │ 任务名称                               │
│ prio             │ U32                    │ 优先级（0~30）                         │
│ entryFunc        │ OsTaskEntryFunc        │ 入口函数                               │
│ arg[4]           │ void *                 │ 入口参数                               │
└──────────────────┴────────────────────────┴────────────────────────────────────────┘
```

### 4.4 全局数据

| 变量 | 类型 | 说明 |
|------|------|------|
| g_tskCbArray | struct OsTaskCb * | TCB 数组（动态分配） |
| g_tskMaxNum | U32 | 最大任务数 = 32 |
| g_tskFreeList | struct OsList | 空闲 TCB 链表 |
| g_tskRecycleList | struct OsList | 待回收栈的 TCB 链表 |

### 4.5 栈顶魔数

```
常量：OS_TASK_STACK_TOP_MAGIC = 0xA5A6A7A8
位置：内核栈起始地址的第一个 U32
用途：检测栈溢出（若该值被改写，说明栈指针越界）
初始化：OsTaskInitKernelStack 中设置
```

## 5 接口设计

### 5.1 OsTaskConfigInit

```
函数原型：U32 OsTaskConfigInit(void)
功能：初始化任务管理模块
参数：无
返回值：OS_OK
处理逻辑：
  1. OsMemKernelAlloc 分配 TCB 数组（32 × sizeof(OsTaskCb)）
  2. memset 清零
  3. 每个 TCB：设置 pid，初始化各链表节点，挂入 g_tskFreeList
```

### 5.2 OsTaskCreate

```
函数原型：U32 OsTaskCreate(struct OsTaskCreateParam *param, U32 *tskId)
功能：创建内核线程
参数：
  param [IN]  - 创建参数
  tskId [OUT] - 返回的任务 ID
返回值：
  OS_OK                    - 成功
  OS_TASK_CREATE_NO_FREE_CB - 无空闲 TCB
  OS_TASK_CREATE_STK_ALLOC_FAIL - 栈分配失败
  OS_TASK_CREATE_PRIO_ILL  - 优先级非法（≥31）
前置条件：param 非 NULL，tskId 非 NULL
处理逻辑：
  1. 校验 prio < OS_TASK_LOWEST_PRIO（31）
  2. OsTaskGetFreeCb() 从空闲链表取 TCB
  3. OsMemKernelAlloc(OS_TASK_KERNEL_STACK_SIZE, 16) 分配内核栈
  4. OsTaskInitKernelStack：memset 填充 0xCA，栈顶写魔数
  5. OsTaskSetCb：复制名称、入口、优先级、参数，设置 USED 状态
  6. OsTaskSetTimeSlice：设置初始时间片
  7. OsSetContext：伪造 FastSave 上下文，eip=OsTaskCommonEntry
  8. 设置 tskType = OS_TASK_THREAD
  9. *tskId = tskCb->pid
```

### 5.3 OsTaskResume

```
函数原型：U32 OsTaskResume(U32 tskId)
功能：恢复挂起的任务
参数：
  tskId [IN] - 任务 ID
返回值：
  OS_OK                      - 成功
  OS_TASK_TSK_ID_INVALID     - ID 越界
  OS_TASK_RESUME_TSK_STATUS_ILL - 任务未创建
处理逻辑：
  1. 校验 tskId < g_tskMaxNum 且 status 含 USED
  2. 清除 SUSPENDED 状态
  3. 若无其他阻塞（PEND/IN_DELAY/PEND_MSG）：加入就绪队列
  4. 若系统已进入后台调度（BGD_TSK）：触发调度
```

### 5.4 OsTaskSuspend

```
函数原型：U32 OsTaskSuspend(U32 tskId)
功能：挂起任务
参数：
  tskId [IN] - 任务 ID
返回值：
  OS_OK                         - 成功或已挂起
  OS_TASK_TSK_ID_INVALID        - ID 越界
  OS_TASK_SUSPEND_TSK_STATUS_ILL - 任务未创建
  OS_TASK_DELETE_HOLD_SEM       - 持有互斥信号量，不允许挂起
处理逻辑：
  1. 校验 tskId 和 USED 状态
  2. 若已挂起，直接返回 OK
  3. 若持有互斥信号量，拒绝挂起
  4. 若在就绪队列，移除
  5. 设置 SUSPENDED 状态
  6. 若挂起自身，触发调度
```

### 5.5 OsTaskDelete

```
函数原型：U32 OsTaskDelete(U32 tskId)
功能：删除任务
参数：
  tskId [IN] - 任务 ID
返回值：
  OS_OK                         - 成功
  OS_TASK_TSK_ID_INVALID        - ID 越界
  OS_TASK_DELETE_TSK_STATUS_ILL - 任务未创建
  OS_TASK_DELETE_HOLD_SEM       - 持有互斥信号量，不允许删除
处理逻辑：
  1. 校验 USED 状态
  2. 若持有互斥信号量，拒绝删除
  3. 从等待队列移除（若 PENDING）
  4. 从延时链移除（若 IN_DELAY），OsRefreshNearestTick
  5. 从就绪队列移除（若 READY）
  6. 释放消息信箱中所有未读消息
  7. 清除 status 和 pgDir
  8. 若删除自身：
     a. 挂入 g_tskRecycleList（栈还在用，不能释放）
     b. 触发调度
  9. 若删除其他任务：
     a. OsMemKernelFree 释放栈
     b. 归还 TCB 到 g_tskFreeList
```

### 5.6 OsTaskDelay

```
函数原型：U32 OsTaskDelay(U32 ticks)
功能：当前任务延时指定 tick 数
参数：
  ticks [IN] - 延时 tick 数，必须 > 0
返回值：
  OS_OK                    - 延时结束，被唤醒
  OS_TASK_DELAY_PARAM_ILL  - ticks == 0
处理逻辑：
  1. 校验 ticks > 0
  2. expiredTick = g_uniTicks + ticks
  3. 从就绪队列移除
  4. OsTaskTimerListInsert 插入延时链
  5. 设置 IN_DELAY 状态
  6. 触发调度（切到其他任务）
  7. 被唤醒后：清除 IN_DELAY
```

### 5.7 OsTaskRecycleStk

```
函数原型：void OsTaskRecycleStk(void)
功能：回收已删除任务的栈和 TCB
参数：无
调用时机：OsHwiTail 中，在系统栈上执行
处理逻辑：
  while (g_tskRecycleList 非空):
    1. 弹出 TCB
    2. OsMemKernelFree 释放栈
    3. 清除 status 和 pgDir
    4. 归还 TCB 到 g_tskFreeList
```

### 5.8 OsTaskTimerListInsert

```
函数原型：void OsTaskTimerListInsert(struct OsTaskCb *tsk)
功能：按到期时刻升序插入延时链
参数：
  tsk [IN] - 任务控制块（expiredTick 已设置）
处理逻辑：
  1. 若 g_timerList 为空：直接尾插
  2. 否则遍历链表，找到第一个 expiredTick > tsk->expiredTick 的节点
  3. 在该节点前插入
  4. OsRefreshNearestTick 更新最近到期时刻
```

## 6 处理逻辑

### 6.1 任务入口执行流程

```
OsTaskCommonEntry(tskId)
  │
  ├─ 从 TCB 取 entry 和 arg[]
  ├─ OsIntUnlock()          // 强制开中断
  ├─ entry(arg[0..3])       // 执行用户入口
  ├─ OsIntLock()            // 强制关中断
  └─ OsTaskExit()
       └─ OsTaskDelete(runningTsk->pid)  // 自删除
```

### 6.2 自删除安全回收流程

```
OsTaskDelete(自身)
  │
  ├─ 清理所有状态和队列
  ├─ 挂入 g_tskRecycleList（栈不能释放，还在用）
  ├─ OsTaskSchedule() → 切到其他任务
  │
  │   ... 其他任务运行 ...
  │
  └─ 下次中断尾部 OsHwiTail
       └─ OsTaskRecycleStk()
            ├─ OsMemKernelFree(栈)    // 在系统栈上安全释放
            └─ 归还 TCB 到 g_tskFreeList
```

## 7 错误码汇总

| 错误码 | 含义 |
|--------|------|
| OS_TASK_CREATE_NO_FREE_CB | 无空闲 TCB |
| OS_TASK_CREATE_STK_ALLOC_FAIL | 栈分配失败 |
| OS_TASK_CREATE_PRIO_ILL | 优先级 ≥ 31 |
| OS_TASK_RESUME_TSK_STATUS_ILL | 任务未创建 |
| OS_TASK_SUSPEND_TSK_STATUS_ILL | 任务未创建 |
| OS_TASK_TSK_ID_INVALID | 任务 ID 越界 |
| OS_TASK_DELAY_PARAM_ILL | ticks == 0 |
| OS_TASK_DELETE_TSK_STATUS_ILL | 任务未创建 |
| OS_TASK_DELETE_HOLD_SEM | 持有互斥信号量 |

## 8 设计决策

| 决策 | 理由 |
|------|------|
| 自删除延迟回收 | 删除自身时栈仍在使用，不能立即释放；挂入回收队列，在系统栈上安全回收 |
| 栈顶魔数检测 | 0xA5A6A7A8 写在栈底，若被改写说明栈溢出 |
| 统一入口 OsTaskCommonEntry | 用户入口返回后自动自删除，避免任务"跑飞" |
| 强制开关中断 | 入口开中断保证任务可被调度；退出关中断保证自删除原子性 |

## 9 模块依赖

| 依赖模块 | 依赖内容 |
|----------|---------|
| 内存管理 | OsMemKernelAlloc/Free（栈和 TCB 分配） |
| CPU 抽象层 | OsSetContext（伪造上下文） |
| 调度器 | OsSchedRdyListEnqueTsk/DequeTsk |
| 时钟 | g_uniTicks、OsTaskTimerListInsert、OsRefreshNearestTick |
| 中断 | OsIntLock/OsIntRestore |
| 信号量 | holdSemList 判断 |
| 消息 | msgList 清理 |
