# 时钟与定时模块功能设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | 时钟与定时（Tick） |
| 模块路径 | kernel/tick/、arch/timer/i386/ |
| 文档版本 | V1.0 |
| 编写日期 | 2026-07-03 |

## 2 模块概述

### 2.1 目的

提供系统滴答计数、任务延时管理和同优先级时间片轮转。时钟中断是最频繁的硬件中断，ISR 必须尽可能简短，所有实质处理延迟到中断尾部完成。

### 2.2 适用范围

所有基于时间的任务调度：任务延时到期唤醒、信号量/消息等待超时、时间片轮转。

### 2.3 设计约束

- 时钟频率由 PIT（8253/8254）设定，默认 100Hz（10ms/tick）
- tick 计数器为 64 位（U64），约 5849 亿年溢出
- 延时链按到期时刻升序排列，插入操作 O(n)
- 时钟中断期间关闭中断，ISR 仅做计数器递增

## 3 功能描述

### 3.1 功能列表

| 功能编号 | 功能名称 | 功能描述 |
|----------|----------|----------|
| TICK-001 | 时钟 ISR | 递增系统滴答计数器和未响应 tick 计数器 |
| TICK-002 | tick 分发 | 处理一个 tick：时间片轮转 + 延时链扫描 |
| TICK-003 | 时间片轮转 | 当前任务时间片耗尽时，出队再入队尾 |
| TICK-004 | 延时链扫描 | 扫描到期任务，唤醒并加入就绪队列 |
| TICK-005 | 到期任务处理 | 弹出到期任务，处理超时与等待队列联动 |
| TICK-006 | 最近到期时刻更新 | 延时链变化后更新 g_nearestTick |

## 4 数据设计

### 4.1 全局变量

| 变量 | 类型 | 说明 |
|------|------|------|
| g_uniTicks | U64 | 系统启动以来的滴答总数，永不递减 |
| g_noRespondTicks | U32 | 累积未处理的 tick 数（中断嵌套时递增） |
| g_timerList | struct OsList | 延时任务链表（按 expiredTick 升序） |
| g_nearestTick | U64 | 最近一个到期时刻，加速到期判断 |

### 4.2 延时链排序规则

```
g_timerList 按任务 expiredTick 升序排列：
  链首 = 最早到期任务
  链尾 = 最晚到期任务

插入时遍历找到第一个 expiredTick > tsk->expiredTick 的位置
保证 g_nearestTick 始终等于链首任务的 expiredTick
```

## 5 接口设计

### 5.1 OsTickIsr

```
函数原型：void OsTickIsr(void)
功能：时钟中断服务程序
参数：无
处理逻辑：
  1. g_uniTicks++
  2. g_noRespondTicks++
说明：仅做计数器递增，所有实质处理延迟到 OsHwiTail → OsTickDispatcher
```

### 5.2 OsTickDispatcher

```
函数原型：void OsTickDispatcher(void)
功能：处理一个 tick 的时间片和延时
参数：无
处理逻辑：
  1. OsTickHandleTimeSlice()  // 时间片轮转
  2. OsTickScanTsks()         // 延时链扫描
```

### 5.3 OsTickHandleTimeSlice

```
函数原型：void OsTickHandleTimeSlice(void)
功能：处理当前任务的时间片递减和轮转
参数：无
处理逻辑：
  1. curTsk->timeSliceTicks--
  2. 若 timeSliceTicks == 0（冷分支，UNLIKELY）：
     a. OsSchedRdyListDequeTsk(curTsk)  // 出就绪队列
     b. OsSchedRdyListEnqueTsk(curTsk)  // 入队尾（轮转到同优先级下一个）
     c. OsTaskSetTimeSlice(curTsk, OsTaskCalTimeSlice(curTsk))  // 重置时间片
  3. 此时 needSched 已设，在 OsHwiTail → OsSchedMain 中切换
```

### 5.4 OsTickScanTsks

```
函数原型：void OsTickScanTsks(void)
功能：扫描延时链，处理所有到期任务
参数：无
处理逻辑：
  while (OsTickTryHandleExpiredTsk()):
    OsRefreshNearestTick()
```

### 5.5 OsTickTryHandleExpiredTsk

```
函数原型：bool OsTickTryHandleExpiredTsk(void)
功能：尝试处理一个到期任务
参数：无
返回值：TRUE=处理了一个到期任务，FALSE=无到期任务
处理逻辑：
  1. if (g_timerList 为空 || g_nearestTick > g_uniTicks): return FALSE
  2. 弹出链首到期任务 expiredTsk
  3. 清除 IN_DELAY 状态
  4. 若 PENDING（在等信号量）：
     a. OsListRemoveNode(&expiredTsk->pendListNode)
     b. 清除 PENDING 状态
     c. 设置 TIMEOUT 状态
  5. 若 PEND_MSG（在等消息）：
     a. 清除 PEND_MSG 状态
     b. 设置 TIMEOUT 状态
  6. 若非 SUSPENDED：
     a. OsSchedRdyListEnqueTsk(expiredTsk)  // 加入就绪队列
  7. return TRUE
```

### 5.6 OsRefreshNearestTick

```
函数原型：void OsRefreshNearestTick(void)
功能：更新最近到期时刻
参数：无
处理逻辑：
  1. if (g_timerList 为空): return
  2. firstTsk = 链首任务
  3. g_nearestTick = firstTsk->expiredTick
```

## 6 处理逻辑

### 6.1 时钟中断完整处理流程

```
PIT 中断 (IRQ0 → int 0x20)
  │
  ├─ [ISR] OsTickIsr()
  │    ├─ g_uniTicks++
  │    └─ g_noRespondTicks++
  │
  ├─ [中断尾部] OsHwiTail()
  │    ├─ while (g_noRespondTicks > 0):
  │    │    ├─ OsTickDispatcher()
  │    │    │    ├─ OsTickHandleTimeSlice()
  │    │    │    │    └─ 时间片耗尽 → 出队+入队尾+重置时间片
  │    │    │    └─ OsTickScanTsks()
  │    │    │         └─ while OsTickTryHandleExpiredTsk():
  │    │    │              └─ 弹出到期任务 → 处理超时 → 加入就绪队列
  │    │    └─ g_noRespondTicks--
  │    ├─ OsTaskRecycleStk()
  │    └─ OsSchedMain()
```

### 6.2 超时与等待联动

```
任务同时处于两个队列：
  - 信号量 pendList（等资源）
  - g_timerList（等超时）

超时到期时（OsTickTryHandleExpiredTsk）：
  1. 从 g_timerList 弹出（已由 OsListPopHead 完成）
  2. 从 pendList 移除（OsListRemoveNode）
  3. 设置 TIMEOUT 状态

被 OsSemPost 正常唤醒时：
  1. 从 pendList 弹出（已由 OsSemPost 完成）
  2. 从 g_timerList 移除（OsListRemoveNode）
  3. 清除 IN_DELAY

两条路径互斥，确保任务不会同时被超时和正常唤醒
```

## 7 设计决策

| 决策 | 理由 |
|------|------|
| ISR 极简 | 时钟是最高频中断，ISR 仅递增计数器，所有实质工作延迟到 OsHwiTail，减少中断禁用时间 |
| tick 补偿 | 多个 tick 中断嵌套时，ISR 多次执行但 Dispatcher 未执行；g_noRespondTicks 累积差值，OsHwiTail 逐个补处理 |
| TICK_ACTIVE 防递归 | 补处理 tick 时可能触发新的中断，TICK_ACTIVE 标志防止递归进入 tick 处理 |
| 延时链升序排列 | 保证 g_nearestTick 始终为链首，到期判断 O(1)；代价是插入 O(n) |
| UNLIKELY 标记时间片耗尽 | 时间片默认 10 tick，耗尽是冷分支，UNLIKELY 提示 CPU 分支预测 |

## 8 模块依赖

| 依赖模块 | 依赖内容 |
|----------|---------|
| 调度器 | OsSchedRdyListEnqueTsk/DequeTsk |
| 任务管理 | OsTaskCb、OsTaskCalTimeSlice、OsTaskSetTimeSlice |
| 中断 | OsIntLock/OsIntRestore |
| 双向链表 | OsList 系列操作 |
