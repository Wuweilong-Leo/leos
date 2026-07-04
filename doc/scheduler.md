# 调度器模块功能设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | 调度器（Scheduler） |
| 模块路径 | kernel/sched/ |
| 文档版本 | V1.0 |
| 编写日期 | 2026-07-03 |

## 2 模块概述

### 2.1 目的

管理所有就绪任务的调度决策，采用固定优先级策略，同优先级内时间片轮转。负责就绪队列的维护、最高优先级任务选取、任务切换的触发与执行。

### 2.2 适用范围

所有涉及任务状态变化（就绪、阻塞、超时、删除）后需要重新调度的场景。

### 2.3 设计约束

- 优先级范围 0~31，0 为最高，31 为 idle 专属
- 同优先级任务按 FIFO 排队，时间片耗尽后轮转到下一个
- 调度在中断关闭状态下执行
- 不支持优先级动态调整（优先级继承除外）

## 3 功能描述

### 3.1 功能列表

| 功能编号 | 功能名称 | 功能描述 |
|----------|----------|----------|
| SCHED-001 | 调度器初始化 | 初始化运行队列、就绪位图、僵尸任务 |
| SCHED-002 | 就绪入队 | 将任务加入对应优先级就绪队列尾部，更新位图 |
| SCHED-003 | 就绪出队 | 将任务从就绪队列移除，更新位图 |
| SCHED-004 | 最高优先级选取 | 从位图中找到最高优先级，取该队列头部任务 |
| SCHED-005 | 调度主循环 | 判断是否需要调度，切换到下一个任务 |
| SCHED-006 | 首次调度 | 创建 idle 任务，切换到首个用户任务 |
| SCHED-007 | 时间片轮转 | 当前任务时间片耗尽时，出队再入队尾 |

## 4 数据设计

### 4.1 运行队列

```
结构体名称：OsRunQue
用途：全局唯一的运行队列，管理调度所需的所有状态

┌──────────────┬─────────────────────────┬──────────────────────────────────────┐
│ 字段名       │ 类型                    │ 说明                                 │
├──────────────┼─────────────────────────┼──────────────────────────────────────┤
│ runningTsk   │ struct OsTaskCb *       │ 当前运行任务                         │
│ idleTsk      │ struct OsTaskCb *       │ idle 任务（优先级31，永远就绪）       │
│ uniFlag      │ U32                     │ 系统状态位图                         │
│ intCount     │ U32                     │ 中断嵌套深度                         │
│ needSched    │ bool                    │ 需要重新调度标志                     │
│ rdyListMsk   │ U32                     │ 就绪位图：bit[i]=1表示优先级i有任务  │
│ rdyList[32]  │ struct OsList           │ 每个优先级一条就绪链表               │
│ scheduler    │ struct OsScheduler *    │ 调度策略函数指针                     │
└──────────────┴─────────────────────────┴──────────────────────────────────────┘

全局实例：g_runQue
```

### 4.2 调度策略

```
结构体名称：OsScheduler
用途：封装调度策略，可替换

┌──────────────┬─────────────────────────┬──────────────────────────────────────┐
│ 字段名       │ 类型                    │ 说明                                 │
├──────────────┼─────────────────────────┼──────────────────────────────────────┤
│ pickNextTsk  │ OsPickNextTsk           │ 选取下一个运行任务的函数指针         │
└──────────────┴─────────────────────────┴──────────────────────────────────────┘

当前策略：g_mfqsScheduler，pickNextTsk = OsSchedPickHighestPrioTsk
```

### 4.3 僵尸任务

```
全局变量：g_zombieTsk (struct OsTaskCb)
用途：系统启动前的占位 runningTsk
  - prio = 31（最低优先级）
  - 保证 runningTsk 永远非空
  - 保证新创建任务的优先级都低于 zombie，可立即抢占
```

## 5 接口设计

### 5.1 OsSchedConfigInit

```
函数原型：U32 OsSchedConfigInit(void)
功能：初始化调度器
参数：无
返回值：OS_OK
处理逻辑：
  1. 初始化僵尸任务（prio=31）
  2. runningTsk = &g_zombieTsk
  3. rdyListMsk = 0
  4. 初始化 32 条 rdyList
  5. intCount = 0
  6. scheduler = &g_mfqsScheduler
  7. needSched = FALSE
```

### 5.2 OsSchedRdyListEnqueTsk

```
函数原型：void OsSchedRdyListEnqueTsk(struct OsTaskCb *tsk)
功能：将任务加入就绪队列
参数：
  tsk [IN] - 任务控制块指针
前置条件：中断已关闭
处理逻辑：
  1. 加入 rdyList[tsk->prio] 尾部
  2. rdyListMsk |= (1 << tsk->prio)
  3. 若 tsk->prio < runningTsk->prio：needSched = TRUE
  4. tsk->status |= OS_TASK_STATUS_READY
```

### 5.3 OsSchedRdyListDequeTsk

```
函数原型：void OsSchedRdyListDequeTsk(struct OsTaskCb *tsk)
功能：将任务从就绪队列移除
参数：
  tsk [IN] - 任务控制块指针
前置条件：中断已关闭
处理逻辑：
  1. 从 rdyList[tsk->prio] 移除 tsk->rdyListNode
  2. 若 rdyList[tsk->prio] 为空：rdyListMsk &= ~(1 << tsk->prio)
  3. 若 tsk == runningTsk：needSched = TRUE
  4. tsk->status &= ~OS_TASK_STATUS_READY
```

### 5.4 OsSchedPickHighestPrioTsk

```
函数原型：struct OsTaskCb *OsSchedPickHighestPrioTsk(void)
功能：选取最高优先级的就绪任务
参数：无
返回值：任务控制块指针
处理逻辑：
  1. OsSchedGetHighestPrio()：从 rdyListMsk 找最低非零位
  2. 取 rdyList[highestPrio] 的首节点
  3. 通过 OS_GET_STRUCT_ENTRY 反查 OsTaskCb
```

### 5.5 OsSchedMain

```
函数原型：void OsSchedMain(void)
功能：调度主函数，在系统栈上执行
参数：无
处理逻辑：
  1. 若 needSched == FALSE：跳到步骤 4
  2. needSched = FALSE
  3. nextTsk = scheduler->pickNextTsk()
     若 nextTsk != curTsk：
       a. curTsk 清除 RUNNING 状态
       b. nextTsk 设置 RUNNING 状态
       c. OsConfigArchForTskSwitch(nextTsk)  // 切页目录+TSS
       d. runningTsk = nextTsk
  4. OsLoadTsk(nextTsk)  // 恢复上下文
```

### 5.6 OsSchedSwitchFirstTsk

```
函数原型：void OsSchedSwitchFirstTsk(void)
功能：系统启动后切换到首个任务
参数：无
处理逻辑：
  1. OsTaskCreateIdle() 创建 idle 任务
  2. 将 idle 任务加入就绪队列（不走 EnqueTsk，避免 runningTsk 判空）
  3. 选出最高优先级任务
  4. 设为 runningTsk，设置 RUNNING 状态
  5. uniFlag |= BGD_TSK_MSK（允许后续 OsTaskResume 触发调度）
  6. OsLoadTsk(tskCb) 首次跳转
```

## 6 处理逻辑

### 6.1 调度触发时机

| 触发场景 | 触发路径 | needSched 设置者 |
|----------|---------|-----------------|
| 更高优先级任务就绪 | OsTaskResume → EnqueTsk | EnqueTsk（发现优先级更高） |
| 当前任务 Pend 阻塞 | OsSemPend → DequeTsk | DequeTsk（发现是 runningTsk） |
| 当前任务 Delay | OsTaskDelay → DequeTsk | DequeTsk |
| 当前任务 Suspend | OsTaskSuspend → DequeTsk | DequeTsk |
| 当前任务删除自身 | OsTaskDelete → OsTaskSchedule | DequeTsk |
| 时间片耗尽 | OsTickHandleTimeSlice → Deque+Enque | DequeTsk + EnqueTsk |
| 中断尾部 | OsHwiTail → OsSchedMain | 上述各场景在中断中设的标志 |

### 6.2 就绪位图加速查找

```
rdyListMsk: 32 位无符号整数
  bit[i] = 1 表示优先级 i 的就绪队列非空

查找最高优先级（OsSchedGetHighestPrio）：
  从 bit 0 向 bit 31 扫描，找到第一个为 1 的位
  idle 任务（优先级 31）永远就绪，保证至少 bit31=1

等效于：__builtin_ctz(rdyListMsk)
```

## 7 设计决策

| 决策 | 理由 |
|------|------|
| 固定优先级 + 同优先级轮转 | 简单可靠，适合实时性要求高的嵌入式场景；MFQS 的动态优先级调整增加了复杂度但收益不大 |
| 位图加速 | 32 优先级用 32 位位图，一次位运算定位最高优先级，O(1) 选取 |
| needSched 延迟调度 | 不在中断中直接切换上下文，而是设标志后在 OsHwiTail 统一处理，减少中断延迟 |
| idle 任务永远就绪 | 保证调度器不会选出空任务，简化判空逻辑 |

## 8 模块依赖

| 依赖模块 | 依赖内容 |
|----------|---------|
| CPU 抽象层 | OsConfigArchForTskSwitch、OsLoadTsk |
| 任务管理 | OsTaskCreateIdle、OsTaskCb |
| 中断 | OsIntLock/OsIntRestore |
