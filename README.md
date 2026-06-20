# LEOS — 一个简单的 x86 内核操作系统

LEOS 是一个从零开始编写的 32 位 x86 操作系统内核，支持保护模式、分页、多任务调度和延时唤醒。

## 快速开始

### 环境要求

- Linux（推荐 Ubuntu 22.04，也可通过 WSL 在 Windows 上使用）
- GCC 32 位交叉编译器（`gcc -m32`）
- NASM 或 GNU AS 汇编器
- GNU ld 链接器
- QEMU（测试用）：`qemu-system-i386`

### 编译

```bash
make clean && make && make dis
```

编译产物在 `build/output/` 目录下：
- `os_mbr.bin` — 主引导记录（MBR），512 字节
- `os_loader.bin` — Loader，加载内核并切换到保护模式+分页
- `kernel.bin` — 内核代码+数据
- `os_kernel.elf` — 完整 ELF（含符号表，调试用）
- `os_kernel.dis` — 反汇编文件

### 创建磁盘镜像

```bash
cd build/output

# 创建 32MB 空白硬盘镜像
dd if=/dev/zero of=leos_hdd.img bs=512 count=65536

# 写入 MBR（第 0 扇区）
dd if=os_mbr.bin of=leos_hdd.img bs=512 conv=notrunc

# 写入 Loader（第 2 扇区开始）
dd if=os_loader.bin of=leos_hdd.img bs=512 seek=2 conv=notrunc

# 写入内核（第 9 扇区开始，与 OS_KERNEL_SEC_ID=9 对应）
dd if=kernel.bin of=leos_hdd.img bs=512 seek=9 conv=notrunc
```

### 运行

```bash
qemu-system-i386 -drive format=raw,file=leos_hdd.img,if=ide -boot c -m 32
```

你应该能看到 VGA 屏幕上显示三个线程（A/B/C）在屏幕上显示各自的计数器。第 0 行是 MBR 输出的 "1 MBR"，内核输出从第 2 行开始。

---

## 启动流程（从按下电源到任务运行）

整个过程分三个阶段，每一步都在为下一步做准备：

### 第一阶段：实模式 — MBR（主引导记录）

**什么是实模式？** CPU 刚开机时处于"实模式"，这是一种最原始的工作方式：
- 内存地址 = 段寄存器 × 16 + 偏移量（最大只能访问 1MB 内存）
- 没有内存保护，任何程序都能读写任何地址
- 没有"内核态"和"用户态"的区别

**MBR 做了什么？**

1. BIOS 把硬盘的第 0 个扇区（512 字节）读入内存地址 `0x7C00` 并跳转过去执行
2. MBR 用 BIOS 的 `int 0x13` 中断读硬盘，把 Loader 从第 2 扇区加载到内存 `0x1000`
3. 跳转到 `0x1000` 执行 Loader

```
物理内存此时：
0x00007C00  ← MBR 代码（512 字节）
0x00001000  ← Loader 代码（即将被加载）
```

### 第二阶段：实模式 → 保护模式 — Loader

**Loader 做了什么？** 这是最复杂的一步，要完成从"原始社会"到"现代文明"的跨越。

#### 2.1 加载内核

用 BIOS `int 0x13, AH=42h`（LBA 扩展读盘）把内核从硬盘第 9 扇区读到内存 `0xD000`。

由于 kernel.bin 可能超过 128 扇区（64KB），Loader 采用**循环读盘**策略：每批读 64 扇区，循环直到读完最多 768 扇区（覆盖 KERNEL_MEM 最大 0x60000 字节）。

**⚠️ SeaBIOS DAP 格式陷阱：** DAP（磁盘地址包）偏移 4-7 的 buffer 字段是**段:偏移格式**（2字节 offset + 2字节 segment），不是 32 位线性地址。例如目标地址 `0x1D000` 必须拆成 `segment=0x1000, offset=0xD000`（0x1000×16+0xD000=0x1D000）。直接写 `0x0001D000` 会被 SeaBIOS 解析为 `offset=0xD000, segment=0x0001`，数据写到错误地址。Loader 把 DAP 放在物理地址 `0x500`（BIOS 数据区之后的安全区域），每批读完后 segment 递增 0x800（64×512/16）。

```
为什么是第 9 扇区？
  - 第 0 扇区：MBR
  - 第 1 扇区：空
  - 第 2-8 扇区：Loader
  - 第 9 扇区开始：kernel.bin
  这个数字由 OS_KERNEL_SEC_ID=9 决定，dd 的 seek 参数必须与之匹配
```

#### 2.2 设置 GDT（全局描述符表）

**什么是 GDT？** 保护模式下，CPU 不再用"段×16+偏移"算地址了，而是通过"段选择子"查 GDT 表，得到一个"段描述符"，里面记录了基地址、大小和权限。

Loader 设置了一个简单的 GDT：
```
选择子 0x08 → 代码段：基址=0，限长=4GB，可执行
选择子 0x10 → 数据段：基址=0，限长=4GB，可读写
选择子 0x18 → 显存段：基址=0xC00B8000，指向 VGA 文本缓冲区
```

#### 2.3 进入保护模式

1. 执行 `lgdt` 指令加载 GDT
2. 把 CR0 寄存器的 PE 位置 1 → CPU 切换到保护模式
3. 执行长跳转 `ljmp $0x08, $next` 刷新流水线，正式进入 32 位保护模式

**保护模式有什么不同？**
- 地址不再用 段×16+偏移，而是通过 GDT 查段描述符
- 有了"特权级"（Ring 0-3），内核跑在 Ring 0（最高权限）
- 但此时**还没有分页**，虚拟地址 = 物理地址（平坦模型）

#### 2.4 设置页表并开启分页

**为什么需要分页？** 内核代码被链接在高地址 `0xC000xxxx`（3GB 以上），但物理内存在低地址。分页让 CPU 在访问 `0xC000xxxx` 时，自动翻译到真实的物理地址。

**两级页表结构：**

```
虚拟地址 0xC000D000 的翻译过程：

31-22位    21-12位    11-0位
  768        13        000     ← 从虚拟地址拆出来的三个部分

PDE[768] → 指向 g_pgt[0]（第0张页表）
PTE[13]  → 指向物理地址 0xD000（内核代码所在位置）
偏移 000  → 最终物理地址 = 0xD000
```

**页表设置：**

| 虚拟地址范围 | 用途 | PDE 索引 | 指向 |
|---|---|---|---|
| `0x00000000-0x001FFFFF` | 恒等映射（低地址=物理地址） | 0 | g_pgt[0] |
| `0xC0000000-0xC01FFFFF` | 内核高地址映射 | 768 | g_pgt[0]（同一张页表！） |
| `0xC0200000-0xC05FFFFF` | 内核虚拟堆 | 769-770 | g_pgt[1]-g_pgt[2]（动态映射） |
| `0xFFC00000-0xFFFFFFFF` | 递归自映射 | 1023 | g_pgd（指向页目录自己） |

**为什么要恒等映射？** 开启分页的那一瞬间，CPU 还在低地址执行代码。如果低地址没有映射，CPU 一开分页就找不到下一条指令了，会立刻崩溃（triple fault）。

**为什么 PDE 0 和 PDE 768 指向同一张页表？** 这样 `0x0000D000` 和 `0xC000D000` 访问的是同一个物理地址。内核代码在低地址加载，但链接在高地址——恒等映射让"开启分页"这个瞬间不会断掉。

**递归自映射是什么？** PDE 1023 指向页目录自己。这样可以通过虚拟地址 `0xFFFFF000 + index*4` 来读写页目录项，不需要知道页目录的物理地址。这是修改页表的关键技巧。

**开启分页的步骤：**

1. `OsSetupPgt()` 初始化页目录和页表
2. `mov CR3, 0x100000`（把页目录物理地址告诉 CPU）
3. `mov CR0, CR0 | 0x80000000`（开启分页位）
4. 刷新流水线，现在 CPU 走的是高地址 `0xC000xxxx`

#### 2.5 跳入内核

1. 重新 `lgdt` 加载高地址版的 GDT（GDT 里的基地址改为 `0xC0001600`）
2. 长跳转到 `0xC000D000`（内核的 `main` 函数）

---

### 第三阶段：内核初始化 — main()

内核的 `main()` 函数依次调用各模块的初始化：

```
main()
 ├── OsConfigInit()          ← 调用所有模块的 Init 函数
 │    ├── OsTaskConfigInit()  ← 分配任务控制块数组
 │    ├── OsSchedConfigInit() ← 初始化就绪队列
 │    ├── OsMemConfigInit()   ← 初始化内存管理 + 映射虚拟堆
 │    ├── OsHwiConfigInit()   ← 安装中断处理程序
 │    ├── OsUsrConfigInit()   ← 用户态配置
 │    ├── OsSemConfigInit()   ← 初始化信号量
    └── OsAppConfigInit()    ← APP测试模块（创建TaskA/B/C）
 ├── 创建测试线程 TaskA/B/C
 ├── OsSchedSwitchFirstTsk() ← 切换到最高优先级就绪任务
 └── （永远不会返回）
```

---

## 物理内存布局

从低地址到高地址：

```
地址              内容                    大小
─────────────────────────────────────────────────────
0x00001000       Loader 代码+数据        ~1.6KB
0x000015E0       Loader 数据               ~0x200
0x00001600       GDT 表 (g_gdt)            ~128 字节
0x00001680       GDT 描述符 (g_gdtInfo)    6 字节
0x00002000       启动栈                   16KB
0x00007C00       MBR                     512 字节
0x0000D000       kernel.bin              ~65KB
0x00100000       页目录 (g_pgd)          4KB
0x00101000       页表 (g_pgt[0-255])     1MB (256张页表)
0x00201000       空闲物理内存起点         ← g_kernelPhyMemPool
```

## 虚拟地址空间布局

```
地址范围                        用途
─────────────────────────────────────────────────────
0x00000000 - 0x001FFFFF       恒等映射（2MB，PDE 0）
0xC0000000 - 0xC01FFFFF       内核高地址映射（2MB，PDE 768）
0xC0200000 - 0xC05FFFFF       内核虚拟堆（4MB，PDE 769-770，动态映射）
0xFFC00000 - 0xFFFFFFFF       递归自映射（4MB，PDE 1023）
```

**⚠️ 重要：PDE 768 的 PTE 0x100 映射了物理地址 0x100000（页目录本身）。任何写入 `0xC0100000` 的操作都会覆盖页目录，导致系统崩溃。这就是为什么虚拟堆基址从 `0xC0100000` 改为了 `0xC0200000`。**

## 递归页表映射

通过 PDE 1023 指向页目录自身，可以用虚拟地址直接读写页表项：

```
读/写 PDE[i]：  访问虚拟地址 0xFFFFF000 + i * 4
读/写 PTE[i]：  访问虚拟地址 0xFFC00000 + (PDE_index << 12) + PTE_index * 4
```

例如，修改 `0xC0200000` 对应的 PTE：
- PDE index = 769, PTE index = 0
- PTE 虚拟地址 = `0xFFC00000 + (769 << 12) + 0*4` = `0xFFC03000`

---

## 多任务调度

### 任务控制块 (TCB)

每个任务有一个 `OsTaskCb` 结构体，记录：
- `stkPtr`：上下文保存点的栈指针
- `entry`：任务入口函数
- `prio`：当前优先级（0 最高，31 最低，可能被优先级继承临时提升）
- `oriPrio`：创建时的原始优先级（优先级继承恢复时用）
- `status`：状态（USED / READY / RUNNING / PENDING / IN_DELAY / SUSPENDED）
- `timeSliceTicks`：剩余时间片
- `expiredTick`：延时到期时刻
- `holdSemList`：该任务持有的所有互斥信号量链表（优先级继承用）

### 任务状态机

```
OsTaskCreate → USED（已创建，未就绪）
OsTaskResume → USED | READY（已就绪，可被调度）
调度器选中   → USED | READY | RUNNING（正在运行）
OsTaskDelay  → USED | IN_DELAY（延时中，等待唤醒）
延时到期     → USED | READY（重新就绪）
```

### 调度算法

- **优先级调度**：总是选择就绪队列中优先级最高的任务
- **时间片轮转**：同优先级任务轮流执行，时间片 = 优先级 + 1
- **时钟中断**（IRQ 0，每 ~10ms）：减少当前任务时间片，到期后重新调度
- **延时唤醒**：`OsTaskDelay(ticks)` 把任务挂起到延时链表，当 `g_uniTicks >= expiredTick` 时自动唤醒

### 上下文切换

切换任务时保存/恢复的寄存器：
```
OsFastSaveContext:
  saveFlag, ebp, ebx, edi, esi, eip, rsvd, tskId
```

`OsLoadTsk` 把栈指针切到新任务的 `stkPtr`，然后 `OsFastLoad` 弹出寄存器，`ret` 跳到新任务的 `eip`。

### 优先级继承 (Priority Inheritance)

BINARY_MUTEX 信号量支持优先级继承，防止优先级反转：

1. **Pend 时提升**：高优先级任务 pend 一个被低优先级任务持有的 mutex 时，`OsSemPrioInherit` 把持有者的 `prio` 提升到 pend 者的优先级，并重新插入就绪队列。
2. **Post 时恢复**：持有者释放 mutex 时，`OsSemPrioRestore` 遍历该任务仍持有的所有 mutex 的 pend 队列，找到最高优先级作为恢复值；若没有其他 pend，恢复为 `oriPrio`。

测试场景（`os_test_sem.c`）：
- PILow(prio=20) 获取 mutex → delay 60 让出 CPU
- PIMid(prio=15) 开始运行 → delay 10 后运行
- PIHigh(prio=5) pend mutex → 触发 PI，PILow 被提升到 prio=5
- PILow 恢复运行，post mutex → 优先级恢复为 20，PIHigh 被唤醒

### BGD 调度状态

系统初始化阶段（`OsConfigInit`）调用 `OsTaskResume` 入就绪队列时不应触发调度。通过 `uniFlag` 的 `OS_BGD_TSK_MSK` 位控制：
- `OsSchedConfigInit` 初始化时设僵尸线程为 `runningTsk`（prio=31），防止 `OsSchedRdyListEnqueTsk` 空指针
- `OsTaskResume` 在 BGD 未置位时只入就绪队列，不调 `OsTaskSchedule`
- `OsSchedSwitchFirstTsk` 执行第一次调度后置位 BGD，后续 `OsTaskResume` 可正常触发调度

---

## 项目结构

```
leos/
├── arch/                  ← 架构相关代码
│   ├── boot/i386/         ← MBR、Loader（汇编）
│   ├── cpu/i386/          ← CPU 初始化、GDT、页表、TSS、上下文切换
│   ├── hwi/i386/          ← 中断控制器（8259A）、中断处理
│   ├── timer/i386/        ← PIT 定时器
│   ├── io/i386/           ← I/O 端口操作
│   └── sys/               ← 系统栈注册
├── kernel/                ← 架构无关的内核逻辑
│   ├── task/              ← 任务管理、进程
│   ├── sched/             ← 调度器
│   ├── tick/              ← 时钟滴答处理
│   ├── mem/               ← 内存管理（物理池 + FSC 虚拟堆）
│   └── ipc/sem/           ← 信号量
├── dev/                   ← 设备驱动
│   └── print/             ← kprintf 内核打印
├── test/                  ← 测试模块
│   ├── os_test.h          ← 测试框架头文件
│   ├── os_test_app.c      ← APP 初始化入口（configInit 表调用）
│   ├── os_test_task.c     ← 任务调度测试（TaskA/B/C）
│   └── os_test_sem.c      ← 信号量测试（计数/二值/互斥/优先级继承）
├── debug/                 ← 调试打印宏
├── lib/                   ← C 库函数（memset, strcpy 等）
├── ld_script/             ← 链接脚本
├── main.c                 ← 内核入口
├── os_config.c            ← 配置初始化注册
└── Makefile
```

---

## 已知限制

- 虚拟堆固定 4MB，需要实现按需映射才能扩大
- 只支持内核线程（Ring 0），用户态进程（Ring 3）尚未测试
- 没有文件系统
- 没有键盘/网络驱动
- 单核 only

---

## 调试技巧

### 串口输出（推荐）

内核测试模块通过 COM1（0x3F8）输出结果，QEMU 可捕获到文件：

```bash
# 编译+创建镜像
make clean && make && make dis
cd build/output
dd if=/dev/zero of=leos_hdd.img bs=512 count=65536
dd if=os_mbr.bin of=leos_hdd.img bs=512 conv=notrunc
dd if=os_loader.bin of=leos_hdd.img bs=512 seek=2 conv=notrunc
dd if=kernel.bin of=leos_hdd.img bs=512 seek=9 conv=notrunc

# 运行，串口输出到文件
qemu-system-i386 -drive format=raw,file=leos_hdd.img,if=ide -boot c -m 32 \
    -serial file:/tmp/leos_serial.log -display none

# 等内核跑一会儿后查看结果（另开终端）
sleep 30
cat /tmp/leos_serial.log
killall qemu-system-i386
```

**注意：** 串口输出由 `TestSemResultCollector` 任务产生（周期性打印 `[SEM_RESULT] 0x...`）。如果内核在信号量测试完成前 crash，串口不会有输出。如果串口无输出，改用下面的 QMP 方式检查内核状态。

### GDB 远程调试

```bash
# 终端 1：启动 QEMU，等待 GDB 连接
qemu-system-i386 -drive format=raw,file=leos_hdd.img,if=ide -boot c -m 32 -s -S

# 终端 2：启动 GDB
 gdb build/output/os_kernel.elf
(gdb) set architecture i386
(gdb) target remote :1234
(gdb) break main
(gdb) continue
```

**注意：** GDB attach 时会暂停 CPU。如果需要读运行中的内核变量，应使用串口输出而非 GDB。

### QEMU 中断日志

```bash
qemu-system-i386 ... -d int -D qemu_int.log
# v=0e → page fault, v=20 → 时钟中断, v=0d → general protection fault
```

### 读 VGA 文本缓冲区

VGA 文本模式缓冲区虚拟地址 `0xC00B8000`，物理地址 `0xB8000`。GDB `x` 命令只能读虚拟地址：

```bash
# 在 GDB 中读 VGA（分页开启后用虚拟地址）
(gdb) x/s 0xc00b8000

# dump 整个 VGA 缓冲区到文件
(gdb) dump binary memory /tmp/vga.bin 0xc00b8000 0xc00b8fa0
```

### QMP 读物理内存（不暂停 CPU）

GDB 会暂停 CPU，串口需要内核跑起来才有输出。QMP 可以在不暂停 CPU 的情况下读物理内存，适合检查内核是否在运行、VGA 输出内容等。

```bash
# 启动 QEMU 并开 QMP 端口
qemu-system-i386 -drive format=raw,file=leos_hdd.img,if=ide -boot c -m 32 \
    -qmp tcp:127.0.0.1:4444,server,nowait -display none &

# 等几秒后用 Python 读物理内存
sleep 5
python3 -c "
import socket, time
s = socket.socket(); s.settimeout(5)
s.connect(('127.0.0.1', 4444)); s.recv(4096)
s.send(b'{\"execute\":\"qmp_capabilities\"}'); time.sleep(0.1); s.recv(4096)
# 读物理地址 0xB8000 开始的 VGA 内容（前5行）
for row in range(5):
    addr = 0xB8000 + row * 160
    s.send(('{\"execute\":\"human-monitor-command\",\"arguments\":{\"command-line\":\"xp /80bx 0x%x\"}}' % addr).encode())
    time.sleep(0.15)
    r = s.recv(8192).decode()
    print('Row', row, r.strip())
s.close()
"
killall qemu-system-i386
```

**读内核变量：** 用 `nm os_kernel.elf` 查符号虚拟地址，物理地址 = 虚拟地址 - 0xC0000000。用 `xp /4bx <物理地址>` 读 4 字节，小端拼成 U32。

```bash
# 例：读 g_uniTicks（内核运行 tick 数）
nm build/output/os_kernel.elf | grep g_uniTicks
# 输出: c0018068 D g_uniTicks  → 物理地址 0x18068
# QMP: xp /4bx 0x18068  → 0xf2 0x00 0x00 0x00 = 242 (内核跑了242 tick)
```
