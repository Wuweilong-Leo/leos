# 消息 IPC 模块功能设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | 消息 IPC（Message） |
| 模块路径 | kernel/ipc/msg/ |
| 文档版本 | V1.0 |
| 编写日期 | 2026-07-03 |

## 2 模块概述

### 2.1 目的

提供任务间点对点消息通信机制。发送方将消息投入目标任务的信箱，接收方从自己信箱取消息；信箱为空时接收方可阻塞等待（支持超时）。

### 2.2 适用范围

需要任务间传递结构化数据的场景，与信号量的"信号通知"形成互补。

### 2.3 设计约束

- 消息动态分配，无静态控制块池
- 每条消息含 8 字节链表头开销
- 发送目标是进程 ID（pid），不是信号量 ID
- 信箱无容量上限（受内核堆大小限制）

## 3 功能描述

### 3.1 功能列表

| 功能编号 | 功能名称 | 功能描述 |
|----------|----------|----------|
| MSG-001 | 消息分配 | 从内核堆分配指定大小的消息缓冲区 |
| MSG-002 | 消息发送 | 将消息链入目标信箱，唤醒等待中的接收者 |
| MSG-003 | 消息接收 | 从自身信箱取消息，信箱为空时阻塞等待（支持超时） |
| MSG-004 | 消息释放 | 将消息缓冲区（含链表头）释放回内核堆 |

## 4 数据设计

### 4.1 消息内存布局

```
分配大小 = sizeof(OsList) + size

内存布局：
┌──────────────────────┬──────────────────────────────┐
│ OsList node (8 字节) │ payload (size 字节)          │
│ [prev][next]         │ 用户数据                     │
└──────────────────────┴──────────────────────────────┘
↑ 返回给内核链表操作       ↑ 返回给用户的指针
   header = msgBuf - 1       msgBuf = header + 1

反查头部：header = (struct OsList *)msgBuf - 1
```

### 4.2 信箱

```
位置：OsTaskCb.msgList（struct OsList）
初始化：OsTaskConfigInit 中 OsListInit(&tskCb->msgList)
说明：每个任务自带消息信箱，OsMsgSend 将消息链入目标 msgList，
      OsMsgRecv 从自身 msgList 取消息
```

### 4.3 超时常量

| 常量 | 值 | 含义 |
|------|-----|------|
| OS_MSG_WAIT_FOREVER | 0xFFFFFFFF | 永久等待 |
| OS_MSG_NO_WAIT | 0 | 不等待 |

## 5 接口设计

### 5.1 OsMsgAlloc

```
函数原型：void *OsMsgAlloc(size_t size)
功能：分配消息缓冲区
参数：
  size [IN] - 负载数据大小（字节）
返回值：payload 起始地址；NULL=分配失败或 size==0
处理逻辑：
  1. 若 size == 0，返回 NULL
  2. totalSize = sizeof(struct OsList) + size
  3. OsMemKernelAlloc(totalSize, 4) 分配内存
  4. OsListInit(header) 初始化链表节点
  5. 返回 (void *)(header + 1)，即 payload 起始地址
```

### 5.2 OsMsgSend

```
函数原型：U32 OsMsgSend(U32 targetPid, void *msgBuf)
功能：发送消息到目标任务信箱
参数：
  targetPid [IN] - 目标进程 ID
  msgBuf    [IN] - 消息缓冲区（OsMsgAlloc 返回的指针）
返回值：
  OS_OK                 - 成功
  OS_MSG_SEND_PID_INVALID - 目标 ID 越界或目标未创建
  OS_MSG_SEND_BUF_INVALID - msgBuf 为 NULL
处理逻辑：
  1. 校验 targetPid < OS_TASK_MAX_NUM
  2. 校验 msgBuf 非 NULL
  3. 反查消息头部：header = (struct OsList *)msgBuf - 1
  4. 关中断
  5. 校验目标任务 USED 状态
  6. OsListAddTail(&targetTsk->msgList, header) 消息入信箱
  7. 若目标在等消息（PEND_MSG）：
     a. 清除 PEND_MSG 状态
     b. 若带超时（IN_DELAY）：从延时链移除，OsRefreshNearestTick
     c. 若非 SUSPENDED：加入就绪队列，触发调度
  8. 恢复中断
```

### 5.3 OsMsgRecv

```
函数原型：U32 OsMsgRecv(U32 timeout, void **msgBuf)
功能：接收消息
参数：
  timeout [IN]  - 超时 tick 数（WAIT_FOREVER/NO_WAIT/具体值）
  msgBuf  [OUT] - 返回的消息缓冲区指针
返回值：
  OS_OK                  - 成功收到消息
  OS_MSG_RECV_TIMEOUT    - 等待超时
  OS_MSG_RECV_UNAVAILABLE - 无消息且 NO_WAIT
  OS_MSG_PARAM_INVALID   - msgBuf 为 NULL
处理逻辑：
  1. 校验 msgBuf 非 NULL
  2. 关中断
  3. while (msgList 为空)：
     a. NO_WAIT → 返回 UNAVAILABLE
     b. 从就绪队列移除，设置 PEND_MSG 状态
     c. 若有超时：设置 IN_DELAY，挂入延时链
     d. OsTaskSchedule() 切出去
     e. 被唤醒后：若 TIMEOUT → 清除 PEND_MSG/TIMEOUT，返回超时
  4. 从 msgList 取第一个消息：header = OsListPopHead
  5. *msgBuf = (void *)(header + 1)，返回 payload 地址
  6. 恢复中断
```

### 5.4 OsMsgFree

```
函数原型：U32 OsMsgFree(void *msgBuf)
功能：释放消息缓冲区
参数：
  msgBuf [IN] - 消息缓冲区（OsMsgAlloc/OsMsgRecv 返回的指针）
返回值：
  OS_OK                - 成功
  OS_MSG_FREE_BUF_INVALID - msgBuf 为 NULL
处理逻辑：
  1. 校验 msgBuf 非 NULL
  2. 反查头部：header = (struct OsList *)msgBuf - 1
  3. OsMemKernelFree(header) 释放整块（含 8 字节头）
```

## 6 处理逻辑

### 6.1 消息收发完整流程

```
发送方：
  OsMsgAlloc(size) → 得到 msgBuf
  填充 msgBuf 中的数据
  OsMsgSend(targetPid, msgBuf) → 消息入目标信箱

接收方：
  OsMsgRecv(timeout, &msgBuf) → 从信箱取消息
  处理 msgBuf 中的数据
  OsMsgFree(msgBuf) → 释放消息
```

### 6.2 阻塞等待流程

```
OsMsgRecv 发现信箱为空：
  │
  ├─ 从就绪队列移除
  ├─ 设置 PEND_MSG 状态
  ├─ 若有超时：设置 IN_DELAY，挂入延时链
  ├─ OsTaskSchedule() 切到其他任务
  │
  │   ... 被以下两种情况之一唤醒 ...
  │
  ├─ [情况1] OsMsgSend 唤醒：
  │    ├─ Send 清除 PEND_MSG
  │    ├─ Send 加入就绪队列
  │    └─ 被调度后：信箱有消息，取出返回
  │
  └─ [情况2] 超时唤醒（OsTickTryHandleExpiredTsk）：
       ├─ 清除 PEND_MSG，标记 TIMEOUT
       └─ 返回 OS_MSG_RECV_TIMEOUT
```

## 7 错误码汇总

| 错误码 | 含义 |
|--------|------|
| OS_MSG_ALLOC_FAIL | 内核堆分配失败 |
| OS_MSG_SEND_PID_INVALID | 目标 ID 越界或目标未创建 |
| OS_MSG_SEND_BUF_INVALID | 消息缓冲区为 NULL |
| OS_MSG_RECV_TIMEOUT | 接收超时 |
| OS_MSG_RECV_UNAVAILABLE | 信箱为空且 NO_WAIT |
| OS_MSG_FREE_BUF_INVALID | 释放的缓冲区为 NULL |
| OS_MSG_PARAM_INVALID | msgBuf 参数为 NULL |

## 8 设计决策

| 决策 | 理由 |
|------|------|
| 动态分配消息 | 消息大小不固定，静态池无法适配；用内核堆按需分配 |
| 8 字节链表头开销 | 侵入式设计，避免额外分配链表节点；header 与 payload 连续存放，缓存友好 |
| 发送目标是 pid | 与信号量的 semId 区分，pid 是任务的全局标识 |
| 信箱无上限 | 简化实现；内核堆耗尽时 OsMsgAlloc 自然返回 NULL，由发送方处理 |
| 任务删除时清理信箱 | OsTaskDelete 中遍历 msgList 释放所有未读消息，防止内存泄漏 |

## 9 模块依赖

| 依赖模块 | 依赖内容 |
|----------|---------|
| 内存管理 | OsMemKernelAlloc/Free（消息分配） |
| 调度器 | OsSchedRdyListEnqueTsk/DequeTsk |
| 任务管理 | OsTaskCb、OS_RUNNING_TASK |
| 时钟 | OsTaskTimerListInsert、OsRefreshNearestTick |
| 中断 | OsIntLock/OsIntRestore |
| 双向链表 | OsList 系列操作 |
