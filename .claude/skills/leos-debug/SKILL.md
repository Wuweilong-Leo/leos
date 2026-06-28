---
name: leos-debug
description: leos 内核 qemu 调试 — 构建、运行、抓 VGA/串口输出、解析测试结果、定位失败原因。当用户说"调试""跑一下""测试一下""看看结果""qemu""验证"时触发。
user-invocable: true
allowed-tools:
  - Bash
  - Read
  - Grep
  - Glob
  - Edit
  - Write
---

# /leos-debug — leos 内核 qemu 调试

一键构建 leos 内核、跑 qemu、抓输出、解析测试结果，定位哪个测试挂了。

Arguments passed: `$ARGUMENTS`

---

## 环境与构建

leos 是 32 位 x86 内核，**必须在 WSL Ubuntu-22.04 里构建和运行**（本机 msys2 的 ld 不支持 elf_i386）。

仓库在 WSL 里是 `/mnt/d/leos/leos`。

### 构建命令

```bash
wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/d/leos/leos && make 2>&1 | tail -20"
```

构建成功标志：最后一行 `Linking build/output/os_kernel.elf`，无 `error`。

### 生成 bin + 重组镜像

```bash
wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/d/leos/leos && make dis >/dev/null 2>&1 && \
  cd build/output && \
  dd if=/dev/zero of=leos_hdd.img bs=1M count=32 2>/dev/null && \
  dd if=os_mbr.bin of=leos_hdd.img bs=512 seek=0 conv=notrunc 2>/dev/null && \
  dd if=os_loader.bin of=leos_hdd.img bs=512 seek=2 conv=notrunc 2>/dev/null && \
  dd if=kernel.bin of=leos_hdd.img bs=512 seek=9 conv=notrunc 2>/dev/null && \
  echo 'image rebuilt'"
```

磁盘布局：LBA0=mbr, LBA2=loader, LBA9=kernel。

### 运行 qemu + 抓 VGA 转储

```bash
wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/d/leos/leos && bash run_qemu.sh 2>&1 | grep -E 'Row [0-9]'"
```

qemu 跑 5 秒后自动 dump VGA 文本模式缓冲区(0xb8000)，输出每行非空内容。

---

## 测试结果解析

VGA 行布局（固定位置）：

| 行 | 内容 | 判定 |
|---|---|---|
| 2 | `A xx` | TaskA 计数递增 → 正常 |
| 3 | `B xx` | TaskB 计数递增 → 正常 |
| 4 | `C xx` | TaskC 计数递增 → 正常 |
| 5 | `D xx` | TaskD 自删除后应停在某值 |
| 6 | `E xx` | TaskRrE 同优先级轮转计数 |
| 7 | `F xx` | TaskRrF 同优先级轮转计数 |
| 8 | `PI: boost=X restore=X lowPrio=X->X->X midRan=X highGot=X susOk=X` | 信号量测试汇总 |
| 10 | `MEM: result=0xFF fails=0 ...` | 内存测试，0xFF = 全过 |
| 11 | `PGF: result=0x7F fails=0 ...` | 缺页测试，0x7F = 全过 |

### 快速判定规则

- **MEM result**: 0xFF = 8 项全过，任何位为 0 表示对应测试失败
- **PGF result**: 0x7F = 7 项全过，任何位为 0 表示对应测试失败
- **PI 行**: boost=1 restore=1 susOk=1 为正常
- **A/B/C**: 十六进制计数 > 0 且持续递增
- **E/F**: 两者都 > 0 且量级相近 = RR 正常
- **PANIC**: 如果 VGA 上出现 `PANIC` 字样，内核崩溃

### MEM result 位图

```
bit0: basic    bit1: align    bit2: split    bit3: coa
bit4: reuse    bit5: oom      bit6: stress   bit7: ma
```

### PGF result 位图

```
bit0: fault   bit1: inv      bit2: phys     bit3: guard
bit4: prim    bit5: xpde     bit6: stress
```

---

## 调试工作流

### 1. 一键全流程（构建 → 镜像 → 运行 → 解析）

当用户说"跑一下""验证""调试"时，按顺序执行：

1. `make` 构建
2. `make dis` + dd 重组镜像
3. `run_qemu.sh` 抓 VGA
4. 解析输出，逐项报告通过/失败

### 2. 构建失败

- 检查 `make` 输出中的 `error:` 行
- 常见原因：语法错误、未声明符号、头文件缺失
- 用 `grep` 在源码中定位问题

### 3. 运行时问题

- **PANIC**: VGA 上出现 PANIC 行 → 读 panic 信息中的文件名和行号 → grep 源码定位
- **测试卡住**: 某行计数为 0 或不递增 → 可能是死锁、调度问题、中断未开
- **MEM/PGF 位图非全 1**: 根据位图确定哪项失败 → 读对应测试代码

### 4. WSL 服务问题

如果 WSL 命令报 `RPC_S_CALL_FAILED` 或 `Exit code 127`：

```powershell
wsl --shutdown
Restart-Service LxssManager
```

然后重试。

---

## 注意事项

- qemu 跑 5 秒后自动退出，不要手动 kill
- `run_qemu.sh` 末尾的 `pkill -9 qemu-system-i386` 返回 exit code 1 是正常的（进程已退出）
- VGA 转储中 `(qemu)` 行是 monitor 回显，可忽略
- 新增测试文件放 `test/` 目录，makefile wildcard 自动收录，无需改构建
- 测试任务优先级选择：不要占用 prio 31（idle 专属），普通任务 prio < 31
