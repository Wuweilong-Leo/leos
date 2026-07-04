# LeOS 内核模块功能设计说明书

## 概述

LeOS 是一个 32 位 x86 教学内核，运行在保护模式下，支持分页、多任务（线程+进程）、信号量、消息 IPC 等功能。

### 地址空间

| 区域 | 虚拟地址范围 | 说明 |
|------|-------------|------|
| 内核代码/数据 | 0xC0000000 ~ 0xC0200000 | 1M~2M 物理内存恒等映射+高端映射 |
| 内核虚拟堆 | 0xC0200000 ~ 0xC0600000 | 4M，FSC 分配器管理，按需缺页增长 |
| 用户空间 | 0x00804800 ~ 0xBFFFFFFF | 进程独占，独立页目录 |

### 系统启动流程

```
MBR (LBA0) → Loader (LBA2) → 保护模式+开启分页 → 内核 (LBA9)
                                                    ↓
                                              OsMain 初始化各模块
                                                    ↓
                                              OsSchedSwitchFirstTsk
                                                    ↓
                                              idle 任务运行，系统就绪
```

## 模块索引

| 序号 | 模块 | 设计说明书 | 路径 | 说明 |
|------|------|-----------|------|------|
| 1 | 双向链表 | [list.md](list.md) | kernel/include | 侵入式双向循环链表，内核基础数据结构 |
| 2 | 启动与引导 | [boot.md](boot.md) | arch/boot | MBR + Loader，实模式→保护模式→分页模式 |
| 3 | CPU 抽象层 | [cpu.md](cpu.md) | arch/cpu | GDT、页表、TSS、上下文切换 |
| 4 | 中断与异常 | [interrupt.md](interrupt.md) | arch/hwi, arch/exc, arch/idt | 硬件中断分发与异常处理 |
| 5 | 调度器 | [scheduler.md](scheduler.md) | kernel/sched | 固定优先级+同优先级轮转 |
| 6 | 任务管理 | [task.md](task.md) | kernel/task | 线程创建、挂起、删除、延时 |
| 7 | 进程管理 | [process.md](process.md) | kernel/task/process | 进程创建，独立地址空间 |
| 8 | 信号量 | [semaphore.md](semaphore.md) | kernel/ipc/sem | 二值同步/互斥/计数信号量+优先级继承 |
| 9 | 消息 IPC | [message.md](message.md) | kernel/ipc/msg | 点对点消息通信 |
| 10 | 内存管理 | [memory.md](memory.md) | kernel/mem | 物理页池+FSC 堆分配器 |
| 11 | 时钟与定时 | [tick.md](tick.md) | kernel/tick | 系统滴答、延时链、时间片轮转 |
| 12 | 输出与打印 | [print.md](print.md) | kernel/print | VGA/串口输出、kprintf |
| 13 | 调试日志 | [debug.md](debug.md) | debug | 分级日志、断言、Panic |
