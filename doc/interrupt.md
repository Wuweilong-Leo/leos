# 中断与异常模块功能设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | 中断与异常（HWI/EXC） |
| 模块路径 | arch/hwi/i386/、arch/exc/i386/、arch/idt/i386/、kernel/hwi/ |
| 文档版本 | V1.0 |
| 编写日期 | 2026-07-03 |

## 2 模块概述

### 2.1 目的

管理 i386 的硬件中断（IRQ）和 CPU 异常，提供中断注册、分发、嵌套计数、中断尾部处理（tick 补偿、栈回收、重新调度）以及缺页异常处理。

### 2.2 适用范围

所有硬件中断（定时器、键盘等）和 CPU 异常（缺页、一般保护错误等）的处理。

### 2.3 设计约束

- 中断处理期间关闭中断，不支持中断嵌套
- 中断处理必须尽可能短，实质工作延迟到中断尾部
- 缺页处理程序在内核虚拟堆按需增长路径上，不能使用可能再次缺页的操作

## 3 功能描述

### 3.1 功能列表

| 功能编号 | 功能名称 | 功能描述 |
|----------|----------|----------|
| HWI-001 | IDT 初始化 | 设置 256 项中断描述符表，异常 0x00~0x1F，中断 0x20~0x30 |
| HWI-002 | 中断入口生成 | 为每个中断号生成入口函数，完成上下文保存、栈切换、分发 |
| HWI-003 | 中断注册 | 注册指定中断号的 ISR 处理函数 |
| HWI-004 | 中断分发 | 调用注册的 ISR，维护嵌套计数和中断标志 |
| HWI-005 | 中断尾部处理 | 补偿未响应 tick、回收已删除任务栈、重新调度 |
| HWI-006 | 异常入口生成 | 为每个异常号生成入口函数，含错误码对齐和 CR2 保存 |
| HWI-007 | 异常分发 | 根据异常号分发处理，缺页异常特殊处理 |
| HWI-008 | 缺页处理 | 内核虚拟堆按需映射物理页 |
| HWI-009 | 中断控制 | 提供开关中断和状态恢复接口 |

## 4 数据设计

### 4.1 中断控制块

```
结构体名称：OsHwiForm
用途：每个中断号对应的 ISR 注册表项

┌──────────┬──────────────────┬──────────────────────────────────┐
│ 字段名   │ 类型             │ 说明                             │
├──────────┼──────────────────┼──────────────────────────────────┤
│ isr      │ OsHwiHandlerFunc │ 中断服务程序函数指针             │
└──────────┴──────────────────┴──────────────────────────────────┘

数组：g_hwiForm[OS_HWI_MAX_NUM]
```

### 4.2 运行队列中断相关字段

```
位于 OsRunQue 中：

┌──────────┬────────┬────────────────────────────────────────────┐
│ 字段名   │ 类型   │ 说明                                       │
├──────────┼────────┼────────────────────────────────────────────┤
│ intCount │ U32    │ 中断嵌套深度，>0 表示在中断上下文          │
│ uniFlag  │ U32    │ 系统状态位图                               │
│          │        │   bit: HWI_ACTIVE  - 中断处理中            │
│          │        │   bit: TICK_ACTIVE - tick 处理中            │
│          │        │   bit: BGD_TSK     - 后台调度已启动        │
└──────────┴────────┴────────────────────────────────────────────┘
```

### 4.3 中断栈帧

参见 CPU 模块文档中的 AllSave 帧结构。中断入口额外处理：
- 无错误码的中断：入口压入 0 占位
- 有错误码的中断（0x08, 0x0A~0x0E, 0x11, 0x1E）：CPU 自动压入错误码，入口用 nop 占位

## 5 接口设计

### 5.1 OsHwiCreate

```
函数原型：U32 OsHwiCreate(U32 hwiNum, OsHwiHandlerFunc isr)
功能：注册中断服务程序
参数：
  hwiNum [IN] - 中断号
  isr    [IN] - 中断服务程序函数指针
返回值：OS_OK
```

### 5.2 OsHwiDispatcher

```
函数原型：void OsHwiDispatcher(U32 hwiNum)
功能：中断分发，调用注册的 ISR
参数：
  hwiNum [IN] - 中断号
处理逻辑：
  1. intCount++
  2. uniFlag |= HWI_ACTIVE
  3. 调用 g_hwiForm[hwiNum].isr(hwiNum)
  4. uniFlag &= ~HWI_ACTIVE
  5. intCount--
```

### 5.3 OsHwiTail

```
函数原型：void OsHwiTail(void)
功能：中断尾部处理
参数：无
处理逻辑：
  1. 若 g_noRespondTicks > 0：
     a. 若 TICK_ACTIVE 已置位，直接返回（防止递归处理 tick）
     b. 置位 TICK_ACTIVE
     c. 循环：开中断 → OsTickDispatcher() → 关中断 → g_noRespondTicks--
     d. 清除 TICK_ACTIVE
  2. OsTaskRecycleStk()  // 回收已删除任务栈
  3. OsSchedMain()       // 重新调度
```

### 5.4 中断控制接口

```
函数原型：enum OsIntStatus OsIntLock(void)
功能：关闭中断，返回之前的中断状态

函数原型：void OsIntRestore(enum OsIntStatus status)
功能：恢复中断状态（开或关）

函数原型：enum OsIntStatus OsIntUnlock(void)
功能：开启中断

说明：OsIntStatus 枚举：OS_INT_OFF=已关中断，OS_INT_ON=已开中断
      架构相关实现，i386 下使用 cli/sti/pushf/popf 指令
```

## 6 处理逻辑

### 6.1 硬件中断处理完整流程

```
IRQ 信号 → 8259A → CPU 响应 → IDT 查表 → 跳转到 OsHwiVectorN
  │
  ├─ [入口] OsHwiVectorN:
  │    ├─ 压入 0（或 nop，对齐错误码）
  │    ├─ OS_HWI_SAVE_CONTEXT（全量保存寄存器）
  │    ├─ 判断当前栈是否在系统栈范围
  │    │    ├─ 不在：保存任务栈指针到 runningTsk->stkPtr，切到系统栈
  │    │    └─ 在：继续使用系统栈
  │    ├─ 发送 EOI（8259A 从片 0xA0 + 主片 0x20）
  │    ├─ push hwiNum
  │    ├─ call OsHwiDispatcher
  │    ├─ call OsHwiTail
  │    └─ jmp OsHwiRet（恢复寄存器，iret）
```

### 6.2 异常处理完整流程

```
CPU 异常 → IDT 查表 → 跳转到 OsExcVectorN
  │
  ├─ [入口] OsExcVectorN:
  │    ├─ 压入 0（或 nop，对齐错误码）
  │    ├─ OS_EXC_SAVE_CONTEXT（全量保存 + CR2）
  │    ├─ 发送 EOI
  │    ├─ push esp（栈指针作为参数）
  │    ├─ push excNum
  │    ├─ call OsExcDispatcher
  │    └─ jmp OsExcRet（恢复寄存器，跳过 errorCode，iret）
```

### 6.3 缺页异常处理流程

```
OsExcDispatcher(EXC_PAGE_FAULT, sp):
  │
  ├─ 从栈帧获取 CR2（缺页虚拟地址）和错误码
  │
  ├─ 判断缺页地址是否在内核虚拟堆范围
  │    ├─ 是：OsMemKernelAllocPgByAddr(cr2)
  │    │       为该虚拟地址分配物理页并建立映射
  │    │       返回，缺页解决
  │    └─ 否：OsPanic()，内核崩溃
```

## 7 错误处理

| 错误场景 | 处理方式 |
|----------|---------|
| 内核非堆区缺页 | OsPanic 崩溃，打印缺页地址和错误码 |
| 堆区缺页但物理页耗尽 | OsMemKernelAllocPgByAddr 返回 NULL，OsPanic 崩溃 |
| 未注册 ISR 的中断 | OsHwiDefHandler 空函数，静默忽略 |

## 8 设计决策

| 决策 | 理由 |
|------|------|
| ISR 极简 + 尾部处理 | 中断处理期间关中断，ISR 必须尽可能短，实质工作延迟到 OsHwiTail |
| tick 补偿机制 | 多个 tick 中断积压时，ISR 只递增计数器，OsHwiTail 中逐个补处理，保证延时精度 |
| 缺页即分配 | 内核虚拟堆不预先映射，写未映射地址触发缺页，缺页处理程序自动补映射，实现按需增长 |
| 中断时栈切换 | 中断可能发生在任务栈上，需要切到系统栈才能安全执行调度和栈回收 |

## 9 模块依赖

| 依赖模块 | 依赖内容 |
|----------|---------|
| 调度器 | OsSchedMain |
| 任务管理 | OsTaskRecycleStk |
| 时钟 | OsTickDispatcher |
| 内存管理 | OsMemKernelAllocPgByAddr |
| 调试 | OsPanic |
