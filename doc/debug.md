# 调试日志模块功能设计说明书

## 1 文档信息

| 项目 | 内容 |
|------|------|
| 模块名称 | 调试日志（Debug） |
| 模块路径 | debug/ |
| 文档版本 | V1.0 |
| 编写日期 | 2026-07-03 |

## 2 模块概述

### 2.1 目的

提供分级日志、断言和 Panic 机制。支持运行时动态调整日志级别，在保证生产环境性能的同时为调试阶段提供详尽的运行信息。

### 2.2 适用范围

所有内核模块的日志输出、运行时断言检查和致命错误处理。

### 2.3 设计约束

- 日志宏的 `__func__` 和 `__LINE__` 必须放在 `##__VA_ARGS__` 之前（自定义 va_arg 按栈顺序读参数，必须与格式串中 %s:%d 的顺序一致）
- OS_ASSERT 在条件为假时调用 OsDebugAssertFail，不停机
- OS_PANIC 在输出错误信息后调用 OsPanic 死循环，不可恢复
- DEBUG_ENABLE 为 FALSE 时，旧接口编译为空，零运行时开销

## 3 功能描述

### 3.1 功能列表

| 功能编号 | 功能名称 | 功能描述 |
|----------|----------|----------|
| DBG-001 | 分级日志输出 | 按 ERROR/WARN/INFO/DEBUG 四级输出日志 |
| DBG-002 | 日志级别控制 | 运行时获取和设置日志级别 |
| DBG-003 | 断言检查 | 运行时条件检查，失败时报告文件/行/函数/条件 |
| DBG-004 | Panic 处理 | 致命错误时输出信息并死循环 |
| DBG-005 | 调试信息打印 | 打印链表、就绪队列、任务信息、内存池状态等 |
| DBG-006 | 旧接口兼容 | DEBUG_ENABLE 控制是否编译调试输出代码 |

## 4 数据设计

### 4.1 日志级别枚举

```
枚举名称：OsLogLevel

┌──────────────┬──────┬──────────────────────────────────────┐
│ 枚举值       │ 值   │ 说明                                 │
├──────────────┼──────┼──────────────────────────────────────┤
│ OS_LOG_NONE  │ 0    │ 静默，不输出任何日志                 │
│ OS_LOG_ERROR │ 1    │ 仅错误                               │
│ OS_LOG_WARN  │ 2    │ 错误 + 警告                          │
│ OS_LOG_INFO  │ 3    │ 一般信息                             │
│ OS_LOG_DEBUG │ 4    │ 详细调试                             │
└──────────────┴──────┴──────────────────────────────────────┘

全局变量：g_logLevel（默认值由初始化设置）
判断规则：level <= g_logLevel 时输出
```

## 5 接口设计

### 5.1 日志级别控制

```
函数原型：void OsDebugSetLogLevel(enum OsLogLevel level)
功能：设置日志级别
参数：level [IN] - 目标级别

函数原型：enum OsLogLevel OsDebugGetLogLevel(void)
功能：获取当前日志级别
返回值：当前日志级别
```

### 5.2 日志宏

#### OS_LOG_ERROR / OS_LOG_WARN / OS_LOG_INFO / OS_LOG_DEBUG

```
宏原型：
  OS_LOG_ERROR(fmt, ...)
  OS_LOG_WARN(fmt, ...)
  OS_LOG_INFO(fmt, ...)
  OS_LOG_DEBUG(fmt, ...)

功能：按指定级别输出日志，自动附带函数名和行号
参数：
  fmt - 格式字符串（不含 [E][%s:%d] 前缀，宏自动添加）
  ... - 格式参数

输出格式：
  [E][函数名:行号] 用户格式内容
  [W][函数名:行号] 用户格式内容
  [I][函数名:行号] 用户格式内容
  [D][函数名:行号] 用户格式内容

展开示例：
  OS_LOG_ERROR("alloc failed, size=%u\n", size)
  → OS_LOG(OS_LOG_ERROR, "[E][%s:%d] " "alloc failed, size=%u\n",
            __func__, __LINE__, size)

重要约束：
  fmt 必须显式声明，不能使用 (...) 形式
  __func__ 和 __LINE__ 在 ##__VA_ARGS__ 之前
  原因：自定义 va_arg 按栈顺序读参数，格式串中 %s:%d 在用户格式符之前
```

#### OS_LOG

```
宏原型：OS_LOG(level, ...)
功能：日志输出底层宏
展开逻辑：
  if (level <= g_logLevel):
    kprintf(__VA_ARGS__)
```

### 5.3 断言宏

#### OS_ASSERT

```
宏原型：OS_ASSERT(cond)
功能：运行时断言检查
参数：cond - 条件表达式
展开逻辑：
  if (!(cond)):
    OsDebugAssertFail(__FILE__, __LINE__, __func__, #cond)

函数原型：void OsDebugAssertFail(const char *filename, U32 line,
                                  const char *func, const char *cond)
功能：断言失败处理
参数：
  filename [IN] - 源文件名
  line     [IN] - 行号
  func     [IN] - 函数名
  cond     [IN] - 失败的条件字符串
行为：输出断言失败信息（不停机）
```

### 5.4 Panic 宏

#### OS_PANIC

```
宏原型：OS_PANIC(fmt, ...)
功能：致命错误处理
参数：
  fmt - 格式字符串
  ... - 格式参数
展开逻辑：
  do {
    kprintf("[PANIC][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__);
    OsPanic();  // 死循环
  } while (0)

行为：输出错误信息后进入死循环，系统不可恢复
```

### 5.5 调试信息函数

| 函数 | 功能 |
|------|------|
| OsDebugPrintList(list) | 打印链表所有节点地址 |
| OsDebugPrintRdyList() | 打印各优先级就绪队列的任务列表 |
| OsDebugPrintTaskInfo(tsk) | 打印指定任务的 TCB 详细信息 |
| OsDebugPrintAllTasks() | 打印所有任务的状态概要 |
| OsDebugPrintMemPool(pool, name) | 打印内存池基址、大小、位图信息 |
| OsDebugSystemStatus() | 打印系统整体状态（运行任务、就绪位图、内存等） |

### 5.6 旧接口兼容

```
条件编译：DEBUG_ENABLE

DEBUG_ENABLE == TRUE:
  OS_DEBUG_PRINT_CHAR(c)  → OsPrintChar(c)
  OS_DEBUG_PRINT_HEX(hex) → OsPrintHex(hex)
  OS_DEBUG_PRINT_STR(str) → OsPrintStr(str)
  OS_DEBUG_KPRINT(...)    → kprintf(...)

DEBUG_ENABLE == FALSE:
  上述宏全部编译为空，零运行时开销
```

## 6 设计决策

| 决策 | 理由 |
|------|------|
| 日志宏参数顺序 | `__func__`, `__LINE__` 必须在 `##__VA_ARGS__` 之前，因为自定义 va_arg 按 C 调用约定的参数入栈顺序读取，必须与格式串中 `%s:%d` 的位置一致 |
| 四级日志 | ERROR/WARN/INFO/DEBUG 覆盖从生产到调试的全场景；NONE 级可完全静默 |
| OS_ASSERT 不停机 | 断言失败报告错误但不死机，允许系统在非致命错误下继续运行以收集更多调试信息 |
| OS_PANIC 死循环 | 致命错误不可恢复，死循环防止错误扩散 |
| DEBUG_ENABLE 条件编译 | 生产环境编译为空，消除所有调试字符串和调用，零代码体积和运行时开销 |

## 7 模块依赖

| 依赖模块 | 依赖内容 |
|----------|---------|
| 输出与打印 | kprintf、OsPrintChar/Hex/Str |
| 中断 | OsPanic（可能关中断后死循环） |
