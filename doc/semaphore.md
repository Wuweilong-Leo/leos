# 信号量模块功能设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | 信号量（Semaphore） |
| 模块路径 | kernel/ipc/sem/ |
| 文档版本 | V1.0 |
| 编写日期 | 2026-07-03 |

## 2 模块概述

### 2.1 目的

提供任务间同步与互斥机制。支持三种信号量类型（二值同步、二值互斥、计数）和两种唤醒策略（FIFO、优先级）。互斥信号量支持优先级继承（Priority Inheritance）防止优先级反转，支持递归持有。

### 2.2 适用范围

所有需要任务间同步、互斥保护或资源计数的场景。

### 2.3 设计约束

- 最大信号量数 16（OS_SEM_MAX_NUM）
- 仅 BINARY_MUTEX 支持优先级继承和递归持有
- 持有互斥信号量的任务不允许被挂起或删除
- Pend 侧消费资源（val--），Post 侧只唤醒等待者

## 3 功能描述

### 3.1 功能列表

| 功能编号 | 功能名称 | 功能描述 |
|----------|----------|----------|
| SEM-001 | 信号量创建 | 分配信号量控制块，设置类型、初始值、唤醒策略 |
| SEM-002 | 信号量 Pend | 请求资源，无资源时阻塞等待（支持超时） |
| SEM-003 | 信号量 Post | 释放资源，唤醒等待者 |
| SEM-004 | 信号量删除 | 释放信号量控制块 |
| SEM-005 | 优先级继承 | 高优先级任务 Pend 被阻塞时提升持有者优先级 |
| SEM-006 | 优先级恢复 | 释放 mutex 后根据剩余持有的 mutex 恢复优先级 |
| SEM-007 | 递归持有 | 同一任务多次 Pend 同一 mutex，嵌套计数管理 |
| SEM-008 | 按优先级插入等待队列 | 高优先级任务排在等待队列前面 |

## 4 数据设计

### 4.1 信号量控制块

```
结构体名称：OsSemCb
用途：描述一个信号量的完整状态

┌──────────────┬─────────────────────────┬────────────────────────────────────────┐
│ 字段名       │ 类型                    │ 说明                                   │
├──────────────┼─────────────────────────┼────────────────────────────────────────┤
│ semId        │ U32                     │ 信号量 ID（= CbArray 下标）            │
│ val          │ U32                     │ 当前值                                 │
│ maxCnt       │ U32                     │ 最大值（二值=1，计数=用户指定）        │
│ type         │ enum OsSemType          │ 信号量类型                             │
│ wakePolicy   │ enum OsSemWakePolicy    │ 唤醒策略                               │
│ freeListNode │ struct OsList           │ 空闲信号量链表节点                     │
│ pendList     │ struct OsList           │ 等待队列                               │
│ holdNode     │ struct OsList           │ 挂入持有者 TCB 的 holdSemList          │
│ holder       │ struct OsTaskCb *       │ BINARY_MUTEX 持有者（其他类型为 NULL） │
│ nestCnt      │ U32                     │ 递归嵌套计数（仅 BINARY_MUTEX）        │
└──────────────┴─────────────────────────┴────────────────────────────────────────┘
```

### 4.2 信号量类型

| 枚举值 | 名称 | 初始值 | 最大值 | 用途 |
|--------|------|--------|--------|------|
| 0 | OS_SEM_BINARY_SYNC | 必须为 0 | 1 | 事件通知/同步 |
| 1 | OS_SEM_BINARY_MUTEX | 必须为 1 | 1 | 互斥保护 |
| 2 | OS_SEM_COUNTING | 用户指定 | 用户指定 | 资源计数 |

### 4.3 唤醒策略

| 枚举值 | 名称 | 说明 |
|--------|------|------|
| 0 | OS_SEM_WAKE_FIFO | 先等先唤醒 |
| 1 | OS_SEM_WAKE_PRIO | 高优先级先唤醒（按优先级插入等待队列） |

### 4.4 超时常量

| 常量 | 值 | 含义 |
|------|-----|------|
| OS_SEM_WAIT_FOREVER | 0xFFFFFFFF | 永久等待 |
| OS_SEM_NO_WAIT | 0 | 不等待 |

## 5 接口设计

### 5.1 OsSemCreate

```
函数原型：U32 OsSemCreate(enum OsSemType type, U32 initVal, U32 maxCnt,
                          enum OsSemWakePolicy policy, U32 *semId)
功能：创建信号量
参数：
  type     [IN]  - 信号量类型
  initVal  [IN]  - 初始值
  maxCnt   [IN]  - 最大值（计数型使用）
  policy   [IN]  - 唤醒策略
  semId    [OUT] - 返回的信号量 ID
返回值：
  OS_OK               - 成功
  OS_SEM_PARAM_INVALID - 参数非法
  OS_SEM_CREATE_NO_FREE_CB - 无空闲控制块
参数校验规则：
  BINARY_SYNC:  initVal 必须为 0，maxCnt 强制为 1
  BINARY_MUTEX: initVal 必须为 1，maxCnt 强制为 1
  COUNTING:     maxCnt > 0 且 initVal ≤ maxCnt
```

### 5.2 OsSemPend

```
函数原型：U32 OsSemPend(U32 semId, U32 timeout)
功能：请求信号量资源
参数：
  semId   [IN] - 信号量 ID
  timeout [IN] - 超时 tick 数（WAIT_FOREVER/NO_WAIT/具体值）
返回值：
  OS_OK                  - 成功获取资源
  OS_SEM_PEND_TIMEOUT    - 等待超时
  OS_SEM_PEND_UNAVAILABLE - 无资源且 NO_WAIT
  OS_SEM_SEM_ID_INVALID  - ID 越界
处理逻辑：
  1. 校验 semId
  2. 递归持有检查（BINARY_MUTEX + 当前任务 == holder）：
     nestCnt++，直接返回 OK
  3. while (val == 0)：
     a. NO_WAIT → 返回 UNAVAILABLE
     b. 按策略插入 pendList（FIFO 尾插 / PRIO 按优先级插入）
     c. 优先级继承（BINARY_MUTEX）
     d. 从就绪队列移除，设置 PENDING 状态
     e. 若有超时：设置 IN_DELAY，挂入延时链
     f. OsTaskSchedule() 切出去
     g. 被唤醒后：清除 PENDING，检查 TIMEOUT
  4. val--（在 Pend 侧消费资源）
  5. BINARY_MUTEX：记录持有者，挂入 holdSemList
```

### 5.3 OsSemPost

```
函数原型：U32 OsSemPost(U32 semId)
功能：释放信号量资源
参数：
  semId [IN] - 信号量 ID
返回值：
  OS_OK                - 成功
  OS_SEM_SEM_ID_INVALID - ID 越界
  OS_SEM_POST_NOT_HOLDER - 非持有者 Post（仅 BINARY_MUTEX）
  OS_SEM_POST_IS_FULL   - 计数型信号量已满
处理逻辑：
  1. 校验 semId
  2. BINARY_MUTEX 持有者检查：非持有者返回错误
  3. 递归释放：nestCnt > 0 则递减，直接返回
  4. BINARY_MUTEX 释放：清除持有者，从 holdSemList 摘除
  5. 优先级恢复（OsSemPrioRestore）
  6. val 递增：
     - 二值型：val == 1 则直接返回（防重复 Post），否则 val = 1
     - 计数型：val >= maxCnt 返回 FULL，否则 val++
  7. 唤醒等待者：
     while (pendList 非空):
       a. 弹出首节点
       b. 清除 PENDING 状态
       c. 若带超时：从延时链移除，OsRefreshNearestTick
       d. 若非 SUSPENDED：加入就绪队列，触发调度，break
       e. 若 SUSPENDED：跳过，继续弹下一个（接不住资源）
```

### 5.4 OsSemDelete

```
函数原型：U32 OsSemDelete(U32 semId)
功能：删除信号量
参数：
  semId [IN] - 信号量 ID
返回值：
  OS_OK                   - 成功
  OS_SEM_SEM_ID_INVALID    - ID 越界
  OS_SEM_DELETE_HAS_PENDER - 有任务在等，不允许删除
  OS_SEM_DELETE_HAS_HOLDER - BINARY_MUTEX 被持有，不允许删除
处理逻辑：
  1. 校验 semId
  2. 若 pendList 非空，拒绝删除
  3. 若 BINARY_MUTEX 被持有，拒绝删除
  4. 从 holdSemList 移除 holdNode（安全操作，已在持有者链表上则移除）
  5. 重置 Cb 各字段
  6. 归还空闲链表（LIFO：刚释放的 ID 优先复用）
```

## 6 处理逻辑

### 6.1 优先级继承处理流程

```
OsSemPrioInherit(semCb, pendTsk)      // 在 Pend 中调用
  │
  ├─ holder = semCb->holder
  ├─ if (holder->prio <= pendTsk->prio): return  // 持有者优先级已不低于 Pend 者
  │
  ├─ if (holder 在就绪队列):
  │    ├─ OsSchedRdyListDequeTsk(holder)
  │    ├─ holder->prio = pendTsk->prio
  │    └─ OsSchedRdyListEnqueTsk(holder)     // 按新优先级重新入队
  │
  └─ else:
       └─ holder->prio = pendTsk->prio       // 不在就绪队列，直接改优先级
```

### 6.2 优先级恢复处理流程

```
OsSemPrioRestore(tskCb)               // 在 Post 中调用
  │
  ├─ highestPrio = tskCb->oriPrio     // 默认恢复到原始优先级
  │
  ├─ 遍历 tskCb->holdSemList:
  │    └─ 对每个持有的 semCb，遍历其 pendList:
  │         └─ highestPrio = min(highestPrio, pendTsk->prio)
  │
  ├─ if (tskCb->prio == highestPrio): return  // 无需恢复
  │
  ├─ if (tskCb 在就绪队列):
  │    ├─ OsSchedRdyListDequeTsk(tskCb)
  │    ├─ tskCb->prio = highestPrio
  │    └─ OsSchedRdyListEnqueTsk(tskCb)
  │
  └─ else:
       └─ tskCb->prio = highestPrio
```

### 6.3 按优先级插入等待队列

```
OsSemPendListInsertByPrio(pendList, tsk)
  │
  ├─ 遍历 pendList
  │    └─ 找到第一个 prio > tsk->prio 的节点 pos
  │
  ├─ 若找到: OsListInsertPrev(tsk->pendListNode, pos)  // 插入 pos 前方
  └─ 若未找到: OsListAddTail(pendList, tsk->pendListNode) // 优先级最低，插尾部
```

## 7 错误码汇总

| 错误码 | 含义 |
|--------|------|
| OS_SEM_CREATE_NO_FREE_CB | 无空闲信号量控制块 |
| OS_SEM_PARAM_INVALID | 参数非法（初始值/最大值不匹配类型要求） |
| OS_SEM_SEM_ID_INVALID | 信号量 ID 越界 |
| OS_SEM_PEND_TIMEOUT | 等待超时 |
| OS_SEM_PEND_UNAVAILABLE | 无资源且 NO_WAIT |
| OS_SEM_POST_NOT_HOLDER | 非持有者 Post（仅 BINARY_MUTEX） |
| OS_SEM_POST_IS_FULL | 计数型信号量已满 |
| OS_SEM_DELETE_HAS_PENDER | 有任务在等待，不允许删除 |
| OS_SEM_DELETE_HAS_HOLDER | 互斥量被持有，不允许删除 |

## 8 设计决策

| 决策 | 理由 |
|------|------|
| Pend 侧消费资源 | Post 只唤醒等待者，由被唤醒者在 Pend 循环中 val--；若唤醒 SUSPENDED 任务则跳过，资源不丢失 |
| 优先级继承仅限 BINARY_MUTEX | 同步信号量和计数信号量无持有者概念，无法做 PI |
| 持有互斥量不允许挂起/删除 | 防止持有互斥量的任务被挂起后，其他等待任务永久阻塞 |
| 优先级恢复遍历所有持有的 mutex | 一个任务可能持有多个 mutex，恢复时需综合考虑所有 mutex 的等待者 |
| 删除时 LIFO 归还空闲链表 | 刚释放的信号量 ID 优先复用，有利于缓存局部性 |

## 9 模块依赖

| 依赖模块 | 依赖内容 |
|----------|---------|
| 调度器 | OsSchedRdyListEnqueTsk/DequeTsk、OsTaskSchedule |
| 任务管理 | OsTaskCb、OS_RUNNING_TASK |
| 时钟 | OsTaskTimerListInsert、OsRefreshNearestTick、g_uniTicks |
| 中断 | OsIntLock/OsIntRestore |
| 双向链表 | OsList 系列操作 |
