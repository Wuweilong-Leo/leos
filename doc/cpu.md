# CPU 抽象层模块功能设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | CPU 抽象层（Arch/CPU） |
| 模块路径 | arch/cpu/i386/ |
| 子模块 | GDT、TSS、页表（Pgt）、上下文切换（Dispatch）、CPU 初始化 |
| 文档版本 | V1.0 |
| 编写日期 | 2026-07-03 |

## 2 模块概述

### 2.1 目的

封装 i386 架构相关细节，为内核提供统一的 CPU 操作接口。包括：段描述符管理、任务状态段管理、二级页表操作、任务上下文保存与恢复。通过条件编译隔离架构差异，内核核心代码不直接依赖 i386 特定指令或寄存器。

### 2.2 适用范围

所有涉及地址空间切换、任务切换、中断/异常处理的内核子系统。

### 2.3 设计约束

- i386 二级页表，4K 页面，32 位地址空间（4G）
- 每个进程独立页目录，线程共享内核页目录
- 上下文切换必须在中断关闭状态下进行

## 3 功能描述

### 3.1 功能列表

| 功能编号 | 功能名称 | 功能描述 |
|----------|----------|----------|
| CPU-001 | GDT 初始化 | 建立内核/用户代码段和数据段描述符，加载 GDTR |
| CPU-002 | TSS 初始化与更新 | 初始化 TSS 段，切换进程时更新 esp0 |
| CPU-003 | 虚实映射建立 | 为指定虚拟地址分配页表并建立到物理地址的映射 |
| CPU-004 | 虚实映射取消 | 取消映射并刷新 TLB，返回物理地址 |
| CPU-005 | 物理地址查询 | 根据虚拟地址查询对应的物理地址 |
| CPU-006 | 进程页目录创建 | 分配页目录并复制内核映射 |
| CPU-007 | 页目录切换 | 将指定页目录物理地址写入 CR3 |
| CPU-008 | 任务上下文初始化 | 在任务栈顶伪造上下文帧，使任务可被首次调度 |
| CPU-009 | 快速上下文保存 | 保存 callee-saved 寄存器（esi/edi/ebx/ebp），用于主动让出 |
| CPU-010 | 全量上下文保存 | 保存所有寄存器+段寄存器，用于中断/异常/进程 |
| CPU-011 | 上下文恢复 | 从栈帧恢复寄存器并跳转到目标指令 |
| CPU-012 | 进程入口构造 | 在内核栈伪造用户态上下文，通过 iret 切到用户态 |

## 4 数据设计

### 4.1 GDT 描述符表

| 选择子 | 值 | 类型 | DPL | 基址 | 粒度/上限 | 用途 |
|--------|-----|------|-----|------|----------|------|
| 0x08 | 内核代码段 | 代码 | 0 | 0 | 4K/4G | 内核代码执行 |
| 0x10 | 内核数据段 | 数据 | 0 | 0 | 4K/4G | 内核数据访问 |
| 0x18 | 用户代码段 | 代码 | 3 | 0 | 4K/4G | 用户态代码执行 |
| 0x20 | 用户数据段 | 数据 | 3 | 0 | 4K/4G | 用户态数据访问 |
| 0x28 | TSS 段 | TSS | 0 | - | - | 任务状态段 |

### 4.2 上下文帧结构

#### 快速保存帧（FastSave，主动让出时使用）

```
栈布局（从低地址到高地址）：
┌──────────────────┐ ← stkPtr
│ saveFlag (0xA5)  │ 识别标记
├──────────────────┤
│ ebp              │
├──────────────────┤
│ ebx              │
├──────────────────┤
│ edi              │
├──────────────────┤
│ esi              │
├──────────────────┤
│ eip              │ 返回地址（=OsTaskCommonEntry 或上次让出点）
└──────────────────┘
```

#### 全量保存帧（AllSave，中断/异常/进程使用）

```
栈布局（从低地址到高地址）：
┌──────────────────┐
│ ss               │ 用户态栈段（进程使用）
├──────────────────┤
│ esp              │ 用户态栈指针（进程使用）
├──────────────────┤
│ eflags           │ 标志寄存器
├──────────────────┤
│ cs               │ 代码段
├──────────────────┤
│ eip              │ 返回地址
├──────────────────┤
│ errorCode        │ CPU 错误码（无则填 0）
├──────────────────┤
│ saveFlag (0x5A)  │ 识别标记
├──────────────────┤
│ ebp              │
├──────────────────┤
│ espDummy         │ 占位（与 AllSave 对齐）
├──────────────────┤
│ ebx              │
├──────────────────┤
│ edx              │
├──────────────────┤
│ ecx              │
├──────────────────┤
│ eax              │
├──────────────────┤
│ gs, fs, es, ds   │ 段寄存器
└──────────────────┘
```

### 4.3 页表寻址公式

利用自映射机制，通过虚拟地址直接访问页目录项和页表项：

```
PDE 虚拟地址 = 0xFFFFF000 + PDE索引(vaddr) × 4
PTE 虚拟地址 = 0xFFC00000 + (PDE索引(vaddr) × 1024 + PTE索引(vaddr)) × 4

其中：
  PDE索引(vaddr) = (vaddr >> 22) & 0x3FF
  PTE索引(vaddr) = (vaddr >> 12) & 0x3FF
```

## 5 接口设计

### 5.1 页表操作接口

#### OsMapVir2Phy

```
函数原型：bool OsMapVir2Phy(uintptr_t virAddr, uintptr_t phyAddr)
功能：建立虚拟地址到物理地址的映射
参数：
  virAddr [IN] - 虚拟地址（页对齐）
  phyAddr [IN] - 物理地址（页对齐）
返回值：TRUE=成功，FALSE=失败（页表分配失败）
处理逻辑：
  1. 根据 virAddr 计算 PDE 和 PTE 的虚拟地址
  2. 若 PDE 不存在：
     a. 从内核物理页池分配 1 页作为页表
     b. 将页表物理地址写入 PDE
     c. 将整张页表清零
  3. 若 PTE 不存在：将 phyAddr | 属性位 写入 PTE
  4. 若 PTE 已存在：跳过（预映射页）
```

#### OsUnmapVir2Phy

```
函数原型：uintptr_t OsUnmapVir2Phy(uintptr_t virAddr)
功能：取消虚拟地址映射
参数：
  virAddr [IN] - 虚拟地址（页对齐）
返回值：物理地址；0 表示该虚拟地址未映射
副作用：执行 invlpg 刷新该地址的 TLB 项
```

#### OsGetPaddrByVaddr

```
函数原型：uintptr_t OsGetPaddrByVaddr(uintptr_t vaddr)
功能：查询虚拟地址对应的物理地址
参数：
  vaddr [IN] - 虚拟地址
返回值：物理地址（= PTE高20位 + vaddr低12位偏移）
```

#### OsCreateProcessPgd

```
函数原型：uintptr_t OsCreateProcessPgd(void)
功能：为进程创建独立页目录
参数：无
返回值：页目录的虚拟地址；NULL=失败
处理逻辑：
  1. OsMemKernelAllocPgs(1) 分配 1 页作为页目录
  2. 复制内核区页目录项（高 256 项，0xC0000000~0xFFFFFFFF）
  3. 最后一项写入页目录自身物理地址（维持自映射）
```

#### OsLoadPgd

```
函数原型：void OsLoadPgd(uintptr_t pgdPhyAddr)
功能：切换地址空间
参数：
  pgdPhyAddr [IN] - 页目录物理地址
实现：movl pgdPhyAddr, %cr3
```

### 5.2 上下文操作接口

#### OsSetContext

```
函数原型：void OsSetContext(uintptr_t stkMemBase, size_t stkSize, struct OsTaskCb *tskCb)
功能：在任务栈顶伪造 FastSave 上下文，使任务可被调度器首次调度
参数：
  stkMemBase [IN] - 栈底地址
  stkSize    [IN] - 栈大小
  tskCb      [IN] - 任务控制块
处理逻辑：
  1. 计算栈顶 stkBot = stkMemBase + stkSize
  2. 预留 AllSaveContext + FastSaveContext 大小
  3. 设置 saveFlag = OS_FAST_SAVE_FLAG
  4. 设置 eip = OsTaskCommonEntry
  5. 设置 tskId = tskCb->pid
  6. 将 stkBot 写入 tskCb->stkPtr
```

#### OsProcessEntry

```
函数原型：void OsProcessEntry(OsProcessEntryFunc entry, void *param1, void *param2)
功能：进程首次运行入口，构造用户态上下文并 iret 切到用户态
参数：
  entry  [IN] - 用户入口函数
  param1 [IN] - 参数1（当前未使用）
  param2 [IN] - 参数2（当前未使用）
处理逻辑：
  1. 关中断
  2. 在内核栈顶伪造 AllSaveContext
     - ds/es/fs/gs/ss = 0x20（用户数据段，RPL=3）
     - cs = 0x18（用户代码段，RPL=3）
     - eip = entry
     - eflags = 0x202（IF=1，开中断）
  3. OsMemUsrAllocPgByAddr 分配 1 页用户栈
  4. esp = 用户栈顶
  5. jmp OsSwitch2Process → iret 切到用户态
```

### 5.3 架构配置接口

#### OsConfigArchForTskSwitch

```
函数原型：void OsConfigArchForTskSwitch(struct OsTaskCb *tsk)
功能：任务切换时更新架构相关寄存器
参数：
  tsk [IN] - 即将运行的任务
处理逻辑：
  1. OsConfigPgdForTskSwitch(tsk)
     - PROCESS: 载入进程页目录（CR3 ← pgDir 物理地址）
     - THREAD:  载入内核全局页目录
  2. OsConfigTssForTskSwitch(tsk)
     - PROCESS: TSS.esp0 = 内核栈顶（用户态陷入时用）
     - THREAD:  不更新（始终在内核态）
```

## 6 处理逻辑

### 6.1 任务切换完整流程

```
OsTaskSchedule()
  │
  ├─ OsTrapTsk(curTsk)
  │    ├─ push esi, edi, ebx, ebp        // 快速保存 callee-saved
  │    ├─ push OS_FAST_SAVE_FLAG          // 保存标记
  │    ├─ curTsk->stkPtr = esp            // 栈指针存入 TCB
  │    ├─ esp = g_kernelStackHigh         // 切到系统栈
  │    └─ call OsSchedMain
  │
  ├─ OsSchedMain()
  │    ├─ nextTsk = scheduler->pickNextTsk()
  │    ├─ OsConfigArchForTskSwitch(nextTsk) // 切页目录+TSS
  │    └─ OsLoadTsk(nextTsk)
  │
  └─ OsLoadTsk(nextTsk)
       ├─ esp = nextTsk->stkPtr           // 切到目标任务栈
       ├─ pop saveFlag
       ├─ if FAST_SAVE_FLAG → OsFastLoad: pop ebp,ebx,edi,esi; ret
       └─ if ALL_SAVE_FLAG  → OsAllLoad:  pop 全部寄存器; iret
```

## 7 错误处理

| 错误场景 | 处理方式 |
|----------|---------|
| 页表物理页分配失败 | OsMapVir2Phy 返回 FALSE，调用方执行回滚 |
| 进程页目录分配失败 | OsCreateProcessPgd 返回 NULL，调用方 OsPanic |
| 用户栈分配失败 | OsProcessEntry 调用 OsPanic |

## 8 模块依赖

| 依赖模块 | 依赖内容 |
|----------|---------|
| 内存管理 | OsMemKernelAllocPgs、OsMemPoolGetFreePgs |
| 调度器 | OsSchedMain、g_runQue |
| 任务管理 | OsTaskCb 结构体 |
