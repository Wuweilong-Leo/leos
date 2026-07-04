# 双向链表模块功能设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | 双向链表（OsList） |
| 模块路径 | kernel/include/os_list_external.h |
| 文档版本 | V1.0 |
| 编写日期 | 2026-07-03 |

## 2 模块概述

### 2.1 目的

为内核各子系统提供通用的侵入式双向循环链表数据结构及操作接口。链表本身不持有业务数据，而是将链表节点嵌入业务结构体中，实现零额外内存分配的多链表复用。

### 2.2 适用范围

本模块是内核基础数据结构，被调度器、任务管理、信号量、消息 IPC、内存管理等模块直接依赖。

### 2.3 设计约束

- 不使用动态内存分配，链表节点由宿主结构体静态包含
- 所有操作在中断关闭前提下使用，自身不加锁
- 节点移除后自动重初始化为孤立状态，防止野指针

## 3 功能描述

### 3.1 功能列表

| 功能编号 | 功能名称 | 功能描述 |
|----------|----------|----------|
| LIST-001 | 链表初始化 | 将链表头节点初始化为空循环链表（prev=next=self） |
| LIST-002 | 节点插入 | 在指定节点前方插入新节点 |
| LIST-003 | 尾部插入 | 将节点插入链表尾部 |
| LIST-004 | 头部插入 | 将节点插入链表头部 |
| LIST-005 | 节点移除 | 将节点从链表中摘除并重初始化 |
| LIST-006 | 判空 | 判断链表是否为空 |
| LIST-007 | 弹出首节点 | 取出链表首节点并从链表中移除 |
| LIST-008 | 节点查找 | 在链表中查找指定节点是否存在 |
| LIST-009 | 遍历 | 提供宏遍历链表所有节点 |

## 4 数据设计

### 4.1 数据结构

```
结构体名称：OsList
用途：双向循环链表节点，既可作为链表头，也可嵌入宿主结构体作为成员

字段说明：
┌──────────┬──────────────┬──────────────────────────────────┐
│ 字段名   │ 类型         │ 说明                             │
├──────────┼──────────────┼──────────────────────────────────┤
│ prev     │ struct OsList * │ 前驱指针                     │
│ next     │ struct OsList * │ 后继指针                     │
└──────────┴──────────────┴──────────────────────────────────┘

空链表状态：prev == next == 自身地址
```

### 4.2 宿主结构反查

```
宏名称：OS_GET_STRUCT_ENTRY(type, member, listNode)
功能：从嵌入的链表节点地址反查宿主结构体首地址
原理：宿主地址 = 节点地址 - offsetof(type, member)

示例：
  struct OsTaskCb *tsk = OS_GET_STRUCT_ENTRY(struct OsTaskCb, pendListNode, node);
  含义：从 pendListNode 地址反查所属的 OsTaskCb
```

## 5 接口设计

### 5.1 OsListInit

```
函数原型：void OsListInit(struct OsList *list)
功能：初始化链表为空
参数：
  list [IN] - 待初始化的链表头指针
返回值：无
前置条件：list 非 NULL
后置条件：list->prev == list->next == list
```

### 5.2 OsListInsertPrev

```
函数原型：void OsListInsertPrev(struct OsList *target, struct OsList *nextNode)
功能：在 nextNode 前方插入 target
参数：
  target   [IN] - 待插入的节点
  nextNode [IN] - 目标位置节点，target 将插入其前方
返回值：无
前置条件：target 和 nextNode 均 非 NULL，nextNode 在某链表中或为链表头
后置条件：target 位于 nextNode 的前驱位置
```

### 5.3 OsListAddTail

```
函数原型：void OsListAddTail(struct OsList *list, struct OsList *node)
功能：将 node 插入链表尾部（list->prev 之后）
参数：
  list [IN] - 链表头
  node [IN] - 待插入节点
返回值：无
实现：OsListInsertPrev(node, list)
```

### 5.4 OsListAddHead

```
函数原型：void OsListAddHead(struct OsList *list, struct OsList *node)
功能：将 node 插入链表头部（list->next 之前）
参数：
  list [IN] - 链表头
  node [IN] - 待插入节点
返回值：无
实现：OsListInsertPrev(node, list->next)
```

### 5.5 OsListRemoveNode

```
函数原型：void OsListRemoveNode(struct OsList *node)
功能：将 node 从所在链表中摘除
参数：
  node [IN] - 待移除节点
返回值：无
后置条件：node 的 prev/next 重置为指向自身（孤立状态），原链表前后节点重新链接
副作用：对已孤立的节点调用是安全的（仅重置自身）
```

### 5.6 OsListIsEmpty

```
函数原型：bool OsListIsEmpty(struct OsList *list)
功能：判断链表是否为空
参数：
  list [IN] - 链表头
返回值：TRUE = 空，FALSE = 非空
判断条件：list->next == list
```

### 5.7 OsListPopHead

```
函数原型：struct OsList *OsListPopHead(struct OsList *list)
功能：弹出链表首节点并移除
参数：
  list [IN] - 链表头
返回值：首节点指针（已从链表中移除）
前置条件：链表非空
```

### 5.8 OsListFindNode

```
函数原型：bool OsListFindNode(struct OsList *list, struct OsList *node)
功能：在链表中查找指定节点
参数：
  list [IN] - 链表头
  node [IN] - 待查找节点
返回值：TRUE = 存在，FALSE = 不存在
时间复杂度：O(n)
```

### 5.9 OS_LIST_FOR_EACH

```
宏原型：OS_LIST_FOR_EACH(list, tmpNode)
功能：正向遍历链表所有节点
参数：
  list    [IN] - 链表头
  tmpNode [OUT] - 循环变量，每次迭代指向当前节点
注意：遍历过程中不可对当前节点执行 RemoveNode，否则迭代失效
```

## 6 处理逻辑

### 6.1 InsertPrev 处理流程

```
输入：target 节点, nextNode 节点

1. target->prev = nextNode->prev       // target 的前驱指向 nextNode 原前驱
2. target->next = nextNode             // target 的后继指向 nextNode
3. nextNode->prev->next = target       // 原前驱的后继改为 target
4. nextNode->prev = target             // nextNode 的前驱改为 target

结果：[原前驱] <-> [target] <-> [nextNode]
```

### 6.2 RemoveNode 处理流程

```
输入：node 节点

1. node->prev->next = node->next       // 前驱的后继跳过 node
2. node->next->prev = node->prev       // 后继的前驱跳过 node
3. node->prev = node                   // 重置为孤立状态
4. node->next = node

结果：node 从链表中摘除，链表重新链接
```

## 7 使用场景

| 使用方 | 链表头 | 节点字段 | 用途 |
|--------|--------|---------|------|
| 任务管理 | g_tskFreeList | OsTaskCb.freeListNode | 空闲 TCB 池 |
| 调度器 | g_runQue.rdyList[prio] | OsTaskCb.rdyListNode | 就绪队列（32 个优先级各一条） |
| 时钟 | g_timerList | OsTaskCb.timerListNode | 延时任务链（按到期时刻排序） |
| 信号量 | OsSemCb.pendList | OsTaskCb.pendListNode | 等待队列 |
| 信号量 | g_semFreeList | OsSemCb.freeListNode | 空闲信号量池 |
| 互斥量 | OsTaskCb.holdSemList | OsSemCb.holdNode | 任务持有的互斥信号量 |
| 消息 | OsTaskCb.msgList | OsList(消息头) | 消息信箱 |
| 内存 | OsMemPool.memCtrlList | OsMemCtrl.listNode | 内存控制块链 |

## 8 模块依赖

本模块无外部依赖，仅依赖 os_def.h 中的基本类型定义（bool、OS_INLINE）。
