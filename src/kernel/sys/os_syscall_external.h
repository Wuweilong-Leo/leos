#ifndef OS_SYSCALL_EXTERNAL_H
#define OS_SYSCALL_EXTERNAL_H
#include "os_def.h"

/*
 * 系统调用 ABI（架构无关）
 *
 * 系统调用号是跨架构通用约定，与触发机制（i386 的 int $0x80 / 其它架构的
 * syscall / svc 指令）解耦。本头定义号、处理函数类型、分发与注册接口，
 * 对所有架构共用。架构相关层（如 arch/sys/os_syscall_i386.h）#include 本头
 * 拿号与类型，依赖方向为 arch → kernel。
 *
 * 架构相关层只保留：
 *   - 触发入口（汇编向量 + C 分发的调用方，如 i386 OsSyscallVector80）
 *   - IDT 安装（OsSyscallConfigInit）
 *   - 依赖寄存器上下文的 syscall（OsSysClone 改 AllSaveContext）
 *   - 依赖设备的 syscall（OsSysWrite 走 UART）
 */

/* 系统调用号 */
#define OS_SYS_WRITE  1   /* buf=字符串指针, len=长度; 返回写入字节数 */
#define OS_SYS_EXIT   2   /* exitCode=退出码; 不返回 */
#define OS_SYS_MALLOC 3   /* size=大小; 返回分配地址, 0=失败 */
#define OS_SYS_FREE   4   /* addr=地址; 返回 OS_OK */
#define OS_SYS_SEM_CREATE 5   /* type=信号量类型, initVal=初始值, maxCnt=最大计数; 返回 semId, >=0x10000 为错误码 */
#define OS_SYS_SEM_PEND 6     /* semId=信号量ID, timeout=超时; 返回 OS_OK/错误码 */
#define OS_SYS_SEM_POST 7     /* semId=信号量ID; 返回 OS_OK/错误码 */
#define OS_SYS_SEM_DELETE 8   /* semId=信号量ID; 返回 OS_OK/错误码 */
#define OS_SYS_GETPID    9   /* 返回当前进程 pid */
#define OS_SYS_FORK     10   /* 创建子进程; 返回子进程 pid(父) / 0(子) */
#define OS_SYS_WAITPID  11   /* pid=目标pid, statusPtr=状态指针, options=选项; 返回子进程 pid */
#define OS_SYS_CLONE   12   /* ebx=stack_top, ecx=start_routine, edx=arg, esi=trampoline; 返回 tid(父)/0(子) */
#define OS_SYS_DETACH  13   /* ebx=tid; 将线程标记为 detached，退出时自动回收; 返回 0=成功, 错误码=失败 */

/* 系统调用号总数（必须等于最大系统调用号 + 1） */
#define OS_SYS_NUM    14

/* 系统调用处理函数类型 */
typedef U32 (*OsSyscallFunc)(U32 arg1, U32 arg2, U32 arg3, U32 arg4);

/* C 分发函数（由架构相关汇编入口调用，如 i386 的 OsSyscallVector80） */
extern U32 OsSyscallHandler(U32 sysno, U32 arg1, U32 arg2, U32 arg3, U32 arg4);

/* 注册系统调用处理函数到系统调用表 */
extern U32 OsSyscallRegister(U32 sysno, OsSyscallFunc func);

/* 注册所有架构无关的系统调用（由架构相关 OsSyscallConfigInit 调用）。
 * 架构相关 syscall（OsSysWrite/OsSysClone）由 ConfigInit 自行注册。 */
extern void OsSyscallRegisterAll(void);

#endif /* OS_SYSCALL_EXTERNAL_H */
