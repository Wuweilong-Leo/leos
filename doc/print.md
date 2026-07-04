# 输出与打印模块功能设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | 输出与打印（Print） |
| 模块路径 | kernel/print/ |
| 文档版本 | V1.0 |
| 编写日期 | 2026-07-03 |

## 2 模块概述

### 2.1 目的

提供内核格式化输出（kprintf）和字符/字符串输出，支持 VGA 文本模式和串口镜像输出。通过可插拔后端（OsPrintOps）将输出逻辑与硬件解耦。

### 2.2 适用范围

所有内核日志输出、调试信息输出和用户可见的文本显示。

### 2.3 设计约束

- kprintf 缓冲区 512 字节，超出截断
- kprintf 内部关中断，保证输出原子性
- vsprintf 自定义实现，支持 %d %u %x %s %c %%，不支持 %f %p 等
- VGA 文本模式 80×25，光标位置由硬件寄存器管理

## 3 功能描述

### 3.1 功能列表

| 功能编号 | 功能名称 | 功能描述 |
|----------|----------|----------|
| PRINT-001 | 格式化输出 | kprintf：将格式字符串和参数输出到 VGA 和串口 |
| PRINT-002 | 格式化到缓冲区 | vsprintf：将格式字符串和参数写入缓冲区（带边界保护） |
| PRINT-003 | 字符输出 | OsPrintChar：输出单个字符，处理换行/退格/滚屏 |
| PRINT-004 | 字符串输出 | OsPrintStr：逐字符输出字符串 |
| PRINT-005 | 十六进制输出 | OsPrintHex：将 U32 以十六进制形式输出 |
| PRINT-006 | 后端注册 | OsPrintRegisterOps：注册输出后端操作函数集 |
| PRINT-007 | 光标管理 | OsPrintSetCursor/OsPrintGetCursor：管理 VGA 光标位置 |
| PRINT-008 | 滚屏 | OsPrintRollScreen：VGA 文本模式向上滚一行 |

## 4 数据设计

### 4.1 输出后端操作集

```
结构体名称：OsPrintOps
用途：封装硬件相关的输出操作，实现可插拔后端

┌──────────────┬─────────────────────────┬────────────────────────────────────────┐
│ 字段名       │ 类型                    │ 说明                                   │
├──────────────┼─────────────────────────┼────────────────────────────────────────┤
│ colNum       │ U16                     │ 每行字符数（VGA=80）                  │
│ posNum       │ U16                     │ 屏幕总字符数（VGA=80×25=2000）        │
│ attrDefault  │ U16                     │ 默认字符属性（前景色+背景色）         │
│ setCursor    │ void (*)(U16)           │ 设置光标位置                           │
│ getCursor    │ U16 (*)(void)           │ 获取光标位置                           │
│ writeChar    │ void (*)(U16, char, U16)│ 在指定位置写字符                       │
│ scrollUp     │ void (*)(void)          │ 向上滚一行                             │
│ clearLine    │ void (*)(U16)           │ 清除指定行                             │
│ mirrorChar   │ void (*)(char)          │ 镜像输出字符（串口）                   │
└──────────────┴─────────────────────────┴────────────────────────────────────────┘

全局实例：g_printOps
注册方式：OsPrintRegisterOps(&ops)
```

### 4.2 vsprintf 支持的格式符

| 格式符 | 参数类型 | 输出 |
|--------|---------|------|
| %d | int | 有符号十进制 |
| %u | unsigned int | 无符号十进制 |
| %x | unsigned int | 十六进制（小写） |
| %s | char * | 字符串（NULL 输出 "(null)"） |
| %c | char | 单字符 |
| %% | 无 | 百分号字面量 |
| 其他 | - | 原样输出 "%" + 字符 |

### 4.3 缓冲区大小

| 常量 | 值 | 说明 |
|------|-----|------|
| KPRINTF_BUF_SIZE | 512 | kprintf 内部缓冲区 |

## 5 接口设计

### 5.1 kprintf

```
函数原型：size_t kprintf(const char *fmt, ...)
功能：内核格式化输出
参数：
  fmt [IN] - 格式字符串
  ...      - 可变参数
返回值：写入的字符数（不含结尾 '\0'）
处理逻辑：
  1. 关中断
  2. vsprintf(buf, sizeof(buf), fmt, args) 格式化到缓冲区
  3. OsPrintStr(buf) 逐字符输出
  4. 恢复中断
```

### 5.2 vsprintf

```
函数原型：size_t vsprintf(char *str, size_t bufSize, const char *fmt, void *ap)
功能：格式化字符串到缓冲区
参数：
  str     [OUT] - 目标缓冲区
  bufSize [IN]  - 缓冲区大小（含结尾 '\0'）
  fmt     [IN]  - 格式字符串
  ap      [IN]  - 可变参数指针
返回值：写入的字符数（不含结尾 '\0'）
处理逻辑：
  1. bufEnd = str + bufSize - 1（预留 '\0' 位置）
  2. 逐字符扫描 fmt：
     - 非 '%'：直接写入
     - '%'：解析格式符，从 ap 取参数，转换写入
  3. 数字格式化：先写入 numBuf[11]，再 memcpy 带长度检查拷贝
  4. 字符串格式化：strlen + memcpy 带长度检查拷贝
  5. 末尾写 '\0'
安全特性：
  - 所有写入检查 bufPtr < bufEnd，不会越界
  - 超出部分截断，保证缓冲区始终以 '\0' 结尾
```

### 5.3 OsPrintChar

```
函数原型：void OsPrintChar(char c)
功能：输出单个字符到 VGA 和串口
参数：
  c [IN] - 待输出字符
处理逻辑：
  1. 镜像到串口：
     - '\n' → 输出 '\r' + '\n'
     - 其他 → 直接输出
  2. VGA 输出：
     - '\r'/'\n' → 光标移到下一行行首，超屏则滚屏
     - '\b' → 光标回退一格，清除字符
     - 其他字符 → 写入当前光标位置，光标前进，超屏则滚屏
```

### 5.4 OsPrintRegisterOps

```
函数原型：void OsPrintRegisterOps(const struct OsPrintOps *ops)
功能：注册输出后端操作函数集
参数：
  ops [IN] - 操作函数集指针
处理逻辑：
  g_printOps = *ops（结构体赋值）
```

## 6 设计决策

| 决策 | 理由 |
|------|------|
| 可插拔后端 | 通过 OsPrintOps 注册，VGA/串口/其他设备可灵活替换，内核核心不依赖具体硬件 |
| 串口镜像 | 每次输出同时镜像到串口，QEMU -serial stdio 可直接看到输出，方便调试 |
| kprintf 关中断 | 防止多任务交叉打印导致输出混乱 |
| vsprintf 带 bufSize | 防止缓冲区溢出，所有写入带边界检查 |
| 数字格式化用 numBuf | 先写入 11 字节临时缓冲区，再用 memcpy 带长度检查拷贝到目标，避免逐字符写入时越界 |

## 7 模块依赖

| 依赖模块 | 依赖内容 |
|----------|---------|
| 中断 | OsIntLock/OsIntRestore |
| VGA 驱动 | OsPrintOps 中注册的 writeChar/setCursor/scrollUp 等 |
| 串口驱动 | OsPrintOps 中注册的 mirrorChar |
