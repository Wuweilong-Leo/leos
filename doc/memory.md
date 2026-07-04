# 内存管理模块功能设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | 内存管理（Memory） |
| 模块路径 | kernel/mem/、kernel/mem/fsc/ |
| 子模块 | 物理页池管理、FSC 堆分配器（内核堆+用户堆） |
| 文档版本 | V2.0 |
| 编写日期 | 2026-07-04 |

## 2 模块概述

### 2.1 目的

管理物理内存和虚拟内存，提供三级管理：位图管理的物理/虚拟页池、虚实映射、以及基于 FSC（First Split Coalesce）算法的内核堆分配器。

### 2.2 适用范围

所有内核数据结构的动态内存分配（TCB、信号量 Cb、任务栈、消息等），以及进程创建时的页表和用户空间管理。

### 2.3 设计约束

- 页面大小 4K（OS_PG_SIZE）
- 内核虚拟堆 4M（0xC0200000~0xC0600000），按需缺页增长
- 用户虚拟堆 4M（0x08049000~0x08449000），按需缺页增长，FSC 惰性初始化
- FSC 分配器最大块 2^31 字节，最小块约 28 字节
- 物理页池使用位图管理，不支持伙伴系统

## 3 功能描述

### 3.1 功能列表

| 功能编号 | 功能名称 | 功能描述 |
|----------|----------|----------|
| MEM-001 | 物理页池初始化 | 初始化内核/用户物理页池和内核虚拟页池 |
| MEM-002 | 连续页分配 | 从虚拟池取连续页，从物理池逐页取物理页并建立映射 |
| MEM-003 | 按地址分配页 | 为指定虚拟地址分配物理页并建立映射（缺页处理用） |
| MEM-004 | 内核堆分配 | 任意大小、任意对齐的堆内存分配 |
| MEM-005 | 内核堆释放 | 释放堆内存，自动左右合并 |
| MEM-006 | FSC 初始化 | 初始化 FSC 控制块和空闲链表 |
| MEM-007 | FSC 模糊搜索 | 从位图找到 ≥ 所需大小的最小级别空闲块 |
| MEM-008 | FSC 精确搜索 | 在对应级别链表中逐个查找满足条件的块 |
| MEM-009 | FSC 块分裂 | 分配后剩余空间切为左右独立空闲块 |
| MEM-010 | FSC 块合并 | 释放时与相邻空闲块合并 |
| MEM-011 | 用户堆分配 | 通过 syscall 从用户堆 FSC 分配内存 |
| MEM-012 | 用户堆释放 | 通过 syscall 释放用户堆内存 |
| MEM-013 | 用户堆惰性初始化 | 第一次 malloc 时初始化 FSC，缺页自动扩展 |
| MEM-014 | FSC 尾部哨兵 | 初始化时在堆末尾写入哨兵块，防止 free 合并越界 |

## 4 数据设计

### 4.1 内存池

```
结构体名称：OsMemPool
用途：管理一段连续虚拟地址空间的页分配状态

┌──────────────┬─────────────────────────┬────────────────────────────────────────┐
│ 字段名       │ 类型                    │ 说明                                   │
├──────────────┼─────────────────────────┼────────────────────────────────────────┤
│ btmp         │ struct OsBtmp           │ 位图：bit[i]=1 表示第 i 页已分配      │
│ base         │ uintptr_t               │ 内存池起始虚拟地址                     │
│ size         │ size_t                  │ 内存池大小（字节）                     │
│ memCtrlList  │ struct OsList           │ 内存控制块链表                         │
└──────────────┴─────────────────────────┴────────────────────────────────────────┘

全局实例：
  g_kernelPhyMemPool  - 内核物理页池
  g_usrPhyMemPool     - 用户物理页池
  g_kernelVirMemPool  - 内核虚拟页池（0xC0200000, 4M）

每个进程还有一个 usrVirMemPool，管理 0x08048000~0xBFFFFFFF 的用户虚拟空间。
```

### 4.2 FSC 块头

```
结构体名称：OsMemFscHead
用途：描述一个内存块（使用中或空闲）

┌──────────────┬─────────────────────────┬────────────────────────────────────────┐
│ 字段名       │ 类型                    │ 说明                                   │
├──────────────┼─────────────────────────┼────────────────────────────────────────┤
│ ctrl         │ struct OsMemFscCtrl *   │ 所属控制块（使用中有效，空闲时=NULL） │
│ next         │ struct OsMemFscHead *   │ 空闲链表后继                           │
│ size         │ size_t                  │ 本块总大小（含头+尾）                  │
│ preSize      │ size_t                  │ 前邻空闲块大小（0=前邻在使用）        │
│ offset/prev  │ U32 / OsMemFscHead *    │ 联合体：                               │
│              │                         │   使用中：offset=用户地址到块头的偏移  │
│              │                         │   空闲中：prev=空闲链表前驱            │
└──────────────┴─────────────────────────┴────────────────────────────────────────┘

状态判断：ctrl != NULL → 使用中；ctrl == NULL → 空闲
```

### 4.3 FSC 控制块

```
结构体名称：OsMemFscCtrl
用途：管理整个 FSC 堆的元数据

┌──────────────┬─────────────────────────┬────────────────────────────────────────┐
│ 字段名       │ 类型                    │ 说明                                   │
├──────────────┼─────────────────────────┼────────────────────────────────────────┤
│ btmp         │ U32                     │ 32 位位图：bit[i]=1 表示第 i 级有空闲 │
│ freeList[32] │ struct OsMemFscHead     │ 32 个空闲链表头                        │
│ totalSize    │ size_t                  │ 堆总大小                               │
│ freeSize     │ size_t                  │ 空闲大小                               │
│ memCtrl      │ void *                  │ 所属内存控制块                         │
└──────────────┴─────────────────────────┴────────────────────────────────────────┘

全局实例：g_kernelMemPtCtrl（内核堆 FSC 控制块）

每个进程还有一个 usrFscCtrl（用户堆 FSC 控制块），通过 TCB 的 usrFscCtrl 指针访问。

级别映射：idx = 31 - clz(size)
  第 i 级管理大小 ∈ [2^i, 2^(i+1)) 的空闲块
  bit 0 永远为 1（2^0 大小不会挂块，但标记存在）
```

### 4.4 内存块完整布局

```
使用中的块：
┌──────────────┬──────────────┬──────────────────┬──────────────┐
│ OsMemFscHead │ offset (4B)  │ payload (size B) │ tail magic   │
│ ctrl≠NULL    │ usrAddr偏移  │ 用户数据         │ 0xCDCDCDCD   │
└──────────────┴──────────────┴──────────────────┴──────────────┘
 ↑ 块头地址                    ↑ 用户地址            ↑ 块尾地址

空闲中的块：
┌──────────────┬──────────────────────────────────────────────────┐
│ OsMemFscHead │ 可用空间                                        │
│ ctrl=NULL    │ (加入对应级别空闲链表)                           │
└──────────────┴──────────────────────────────────────────────────┘
```

### 4.5 常量定义

| 常量 | 值 | 含义 |
|------|-----|------|
| OS_MEM_FSC_SIZE_NUM | 32 | 空闲链表级数 |
| OS_MEM_FSC_TAIL_MAGIC | 0xCDCDCDCD | 尾部魔数 |
| OS_MEM_FSC_MIN_SIZE | 头+4+尾魔数 | 最小可分裂块大小 |
| OS_KERNEL_VIR_HEAP_MEM_BASE | 0xC0200000 | 内核虚拟堆基址 |
| OS_KERNEL_VIR_HEAP_MEM_SIZE | 4M | 内核虚拟堆大小 |
| OS_PROCESS_USR_HEAP_BASE | 0x08049000 | 用户堆基址（代码段1页之后） |
| OS_USR_HEAP_MEM_SIZE | 4M | 用户堆大小 |

## 5 接口设计

### 5.1 页面分配接口

#### OsMemKernelAllocPgs

```
函数原型：uintptr_t OsMemKernelAllocPgs(size_t cnt)
功能：分配 cnt 页内核内存
参数：cnt [IN] - 页数
返回值：起始虚拟地址；NULL=失败
实现：OsMemAllocPgs(OS_MEM_KERNEL, cnt)
```

#### OsMemUsrAllocPgs

```
函数原型：uintptr_t OsMemUsrAllocPgs(size_t cnt)
功能：分配 cnt 页用户内存
参数：cnt [IN] - 页数
返回值：起始虚拟地址；NULL=失败
实现：OsMemAllocPgs(OS_MEM_USR, cnt)
```

#### OsMemKernelAllocPgByAddr

```
函数原型：uintptr_t OsMemKernelAllocPgByAddr(uintptr_t virAddr)
功能：为指定内核虚拟地址分配物理页并建立映射
参数：virAddr [IN] - 虚拟地址（缺页地址）
返回值：virAddr；NULL=失败
用途：缺页异常处理程序调用
```

#### OsMemUsrAllocPgByAddr

```
函数原型：uintptr_t OsMemUsrAllocPgByAddr(uintptr_t virAddr)
功能：为指定用户虚拟地址分配物理页并建立映射
参数：virAddr [IN] - 虚拟地址
返回值：virAddr；NULL=失败
用途：用户栈分配、用户堆缺页扩展
注意：内部通过 OS_RUNNING_TASK()->usrVirMemPool 获取虚拟池，
      因此必须在进程运行上下文中调用（不能在 OsProcessCreate 期间调用）
```

### 5.2 堆分配接口

#### OsMemKernelAlloc

```
函数原型：void *OsMemKernelAlloc(size_t size, U32 align)
功能：从内核堆分配内存
参数：
  size  [IN] - 请求大小（字节）
  align [IN] - 对齐要求（字节，4/8/16 等）
返回值：分配的内存地址；NULL=失败
实现：OsMemFscAlloc(g_kernelMemPtCtrl, size, align)
```

#### OsMemKernelFree

```
函数原型：void OsMemKernelFree(void *addr)
功能：释放内核堆内存
参数：addr [IN] - OsMemKernelAlloc 返回的地址
实现：OsMemFscFree(addr)
```

### 5.3 FSC 分配器接口

#### OsMemFscAlloc

```
函数原型：void *OsMemFscAlloc(struct OsMemFscCtrl *ctrl, size_t size, U32 align)
功能：FSC 堆内存分配
参数：
  ctrl  [IN] - FSC 控制块
  size  [IN] - 请求大小
  align [IN] - 对齐要求
返回值：用户地址；NULL=无合适块
处理逻辑：
  1. alignSize = ROUND_UP(size, 4)
  2. allocSize = alignSize + (align-4) + HEAD_SIZE + TAIL_MAGIC_SIZE
  3. 模糊搜索：从位图找 ≥ allocSize 的最小级别
  4. 若失败，精确搜索：在对应级别链表逐个检查
  5. 从空闲链表摘除该块
  6. 从后往前切割（保证用户地址按 align 对齐）：
     realBlk = ROUND_DOWN(尾部-alignSize, align) - HEAD_SIZE
  7. 尝试右分裂：剩余 ≥ MIN_SIZE 则独立成块
  8. 尝试左分裂：剩余 ≥ MIN_SIZE 则独立成块
  9. 设置 ctrl、尾魔数、偏移量
  10. 更新 freeSize
```

#### OsMemFscFree

```
函数原型：void OsMemFscFree(void *addr)
功能：释放 FSC 堆内存
参数：addr [IN] - 用户地址
处理逻辑：
  1. OsMemFscGetHead(addr)：通过偏移量反查块头
  2. 保存 ctrl 和 size（左合并后 memHead 指向变化）
  3. OsMemFscTryMergeRight：若右邻空闲（ctrl==NULL），合并
  4. OsMemFscTryMergeLeft：若前邻空闲（preSize≠0），合并
  5. 更新下一块的 preSize
  6. 挂入对应级别空闲链表
  7. 更新 freeSize（用步骤 2 保存的 ctrl）
  8. 设置 ctrl=NULL（标记空闲）
```

## 6 处理逻辑

### 6.1 页面分配完整流程

```
OsMemAllocPgs(flag, cnt):
  │
  ├─ 选择虚拟池和物理池
  │    KERNEL: kernelVirMemPool + kernelPhyMemPool
  │    USR:    task->usrVirMemPool + usrPhyMemPool
  │
  ├─ OsMemPoolGetFreePgs(virMemPool, cnt)
  │    └─ 位图扫描连续 cnt 个空闲位，返回起始虚拟地址
  │
  ├─ 逐页分配物理页并映射：
  │    for i = 0 to cnt-1:
  │      ├─ OsMemPoolGetFreePgs(phyMemPool, 1) 取 1 页物理页
  │      ├─ OsMapVir2Phy(virAddr, phyAddr) 建立映射
  │      └─ 任一步失败：OsMemAllocPgsRollback 回滚
  │
  └─ 返回 virAddrBase
```

### 6.2 回滚流程

```
OsMemAllocPgsRollback(virMemPool, phyMemPool, virAddrBase, cnt, allocated):
  │
  ├─ 对已映射的 allocated 页：
  │    ├─ OsUnmapVir2Phy(virAddr) 取消映射，获取物理地址
  │    └─ OsBtmpClear(phyMemPool, phyIdx) 归还物理页
  │
  └─ 对所有 cnt 页：
       └─ OsBtmpClear(virMemPool, virIdx) 归还虚拟页位图
```

### 6.3 按需缺页增长流程

```
FSC 分配器写入未映射地址
  │
  ├─ 触发缺页异常（int 0x0E）
  ├─ OsExcDispatcher 判断缺页来源
  │
  ├─ [内核态缺页] OsExcHandleKernelPgFault:
  │    ├─ errAddr 在内核堆范围 (0xC0200000~0xC0600000)?
  │    │    └─ OsMemKernelAllocPgByAddr → 映射 → 返回
  │    └─ errAddr 在用户堆范围 (0x08049000~0x08449000)?
  │         └─ OsMemUsrAllocPgByAddr → 映射 → 返回
  │              （内核代为访问用户堆，如 syscall 拷贝数据时触发）
  │
  └─ [用户态缺页] OsExcDispatcher 用户态分支:
       ├─ cr2 在用户堆范围?
       │    └─ OsMemUsrAllocPgByAddr → 映射 → iret 回用户态继续
       └─ 不在堆范围 → 杀进程
```

### 6.4 用户堆惰性初始化流程

```
第一次 OS_SYS_MALLOC:
  │
  ├─ OsSysMalloc 发现 usrFscCtrl == NULL
  │
  ├─ OsMemFscInitPt(0x08049000, 4MB)
  │    ├─ 在堆首写入 FSC ctrl 结构体 → 缺页（页未映射）
  │    │    └─ OsExcHandleKernelPgFault → OsMemUsrAllocPgByAddr
  │    │         └─ OS_RUNNING_TASK() 是进程自己 ✓ → 位图正确 → 映射成功
  │    ├─ 初始化空闲链表
  │    ├─ 创建大空闲块
  │    └─ 初始化尾部哨兵块（ctrl=ptCtrl，非 NULL）
  │         └─ 写入堆末尾 → 可能缺页 → 同上处理
  │
  ├─ usrFscCtrl 赋值给 tsk->usrFscCtrl
  │
  └─ OsMemFscAlloc(usrFscCtrl, size, 4) → 分配内存
       └─ 写入触发缺页 → 自动映射 → 返回
```

### 6.5 FSC 尾部哨兵

FSC 初始化时在堆末尾预留 `OS_MEM_FSC_HEAD_SIZE` 大小的占位。这个占位被初始化为一个"使用中"的哨兵块：

```
堆内存布局：
0x08049000  [FSC ctrl 结构体]
            [空闲块 HEAD]  size = blkSize
            [空闲块数据 ~4MB]
            [尾部哨兵 HEAD]  ctrl = ptCtrl (非NULL), size = 0   ← 阻止合并越界
0x08449000  （堆外）
```

free 时 `OsMemFscTryMergeRight` 检查 `rightBlk->ctrl`：
- 哨兵块 `ctrl != NULL` → 判定使用中 → 不合并 → 正常
- 若未初始化（全零页），`ctrl == NULL` → 误判空闲 → 链表操作解引用 NULL → 崩溃

### 6.6 FSC 块合并流程

```
OsMemFscFree(addr):
  │
  ├─ 反查块头 memHead
  ├─ 保存 ctrl = memHead->ctrl, size = memHead->size
  │
  ├─ 右合并 OsMemFscTryMergeRight:
  │    ├─ rightBlk = memHead + memHead->size
  │    └─ if (rightBlk->ctrl == NULL):  // 右邻空闲
  │         ├─ OsMemFscFreeListRemoveBlk(rightBlk)
  │         └─ memHead->size += rightBlk->size
  │
  ├─ 左合并 OsMemFscTryMergeLeft:
  │    ├─ if (memHead->preSize != 0):  // 前邻空闲
  │         ├─ leftBlk = memHead - memHead->preSize
  │         ├─ OsMemFscFreeListRemoveBlk(leftBlk)
  │         └─ leftBlk->size += memHead->size
  │              mergedLeft = leftBlk
  │
  ├─ 更新下一块的 preSize = memHead->size（合并后的大小）
  ├─ OsMemFscFreeListInsertBlk(ctrl, memHead)
  ├─ ctrl->freeSize += size（用入口处保存的 ctrl）
  └─ memHead->ctrl = NULL（标记空闲）
```

## 7 设计决策

| 决策 | 理由 |
|------|------|
| 按需缺页增长 | 不预先映射 4M 虚拟堆，只映射 FSC 控制块所在的第一页；后续分配时缺页自动补映射，节省物理内存 |
| FSC 位图+链表 | 位图 O(1) 定位有空间的级别，链表管理同级别内的多个空闲块 |
| 从后往前切割 | 保证用户地址按 align 对齐：从块尾部向前推算对齐地址，左余料和右余料各自分裂 |
| 偏移量反查块头 | 释放时 O(1) 找到块头，无需全量扫描；用户地址前 4 字节记录偏移 |
| 尾魔数检测 | 块末尾 4 字节 0xCDCDCDCD，可检测用户越界写（覆写时魔数改变） |
| 左合并依赖 preSize | 释放时通过 preSize 判断前邻是否空闲，无需遍历；preSize 由合并时更新 |
| 回滚机制 | 页面分配部分成功部分失败时，自动回滚已完成的映射和分配，保证一致性 |
| 用户堆 FSC 惰性初始化 | OsProcessCreate 期间 OS_RUNNING_TASK() 是创建者线程，缺页处理取不到新进程的位图；第一次 malloc 时进程已在运行，一切正常 |
| FSC 尾部哨兵 | 新映射物理页全零，ctrl=NULL 被误判为空闲块导致 free 崩溃；初始化 ctrl 非 NULL 阻止合并越界 |
| 用户堆复用 FSC 分配器 | 内核堆和用户堆用同一个分配器，通过 syscall 调用（用户态不能直接调内核函数） |

## 8 模块依赖

| 依赖模块 | 依赖内容 |
|----------|---------|
| CPU 抽象层 | OsMapVir2Phy、OsUnmapVir2Phy |
| 中断 | OsIntLock/OsIntRestore |
| 调试 | OS_LOG_ERROR/OS_LOG_WARN |
| 位图 | OsBtmp 系列操作 |
| 调度 | OS_RUNNING_TASK()（OsMemUsrAllocPgByAddr 取用户虚拟池） |
| Syscall | OS_SYS_MALLOC/OS_SYS_FREE（用户堆分配/释放入口） |
