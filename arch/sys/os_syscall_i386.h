#ifndef OS_SYSCALL_I386_H
#define OS_SYSCALL_I386_H
#include "os_def.h"

/*
 * i386 系统调用机制 (INT 0x80)
 * - 用户态通过 int $0x80 触发系统调用
 * - eax = 系统调用号, ebx/ecx/edx/esi = 最多 4 个参数
 * - eax = 返回值
 */

/* 系统调用号 */
#define OS_SYS_WRITE  1   /* arg1=字符串指针, arg2=长度; 返回写入字节数 */
#define OS_SYS_EXIT   2   /* arg1=退出码; 不返回 */

/* 系统调用号总数（必须等于最大系统调用号 + 1） */
#define OS_SYS_NUM    3

/* 系统调用处理函数类型 */
typedef U32 (*OsSyscallFunc)(U32 arg1, U32 arg2, U32 arg3, U32 arg4);

/* 汇编入口 */
extern void OsSyscallVector80(void);

/* C 分发函数（由汇编入口 OsSyscallVector80 调用） */
extern U32 OsSyscallHandler(U32 sysno, U32 arg1, U32 arg2, U32 arg3, U32 arg4);

/* 注册系统调用处理函数到系统调用表 */
extern U32 OsSyscallRegister(U32 sysno, OsSyscallFunc func);

/* 初始化：注册 INT 0x80 到 IDT + 填充系统调用表 */
extern U32 OsSyscallConfigInit(void);

/* ====== 用户态系统调用内联包装 ====== */

OS_INLINE U32 OsSyscall1(U32 sysno, U32 arg1)
{
    U32 ret;
    OS_EMBED_ASM("int $0x80" : "=a"(ret) : "a"(sysno), "b"(arg1) : "memory");
    return ret;
}

OS_INLINE U32 OsSyscall2(U32 sysno, U32 arg1, U32 arg2)
{
    U32 ret;
    OS_EMBED_ASM("int $0x80" : "=a"(ret) : "a"(sysno), "b"(arg1), "c"(arg2) : "memory");
    return ret;
}

OS_INLINE U32 OsSyscall3(U32 sysno, U32 arg1, U32 arg2, U32 arg3)
{
    U32 ret;
    OS_EMBED_ASM("int $0x80" : "=a"(ret) : "a"(sysno), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return ret;
}

OS_INLINE U32 OsSyscall4(U32 sysno, U32 arg1, U32 arg2, U32 arg3, U32 arg4)
{
    U32 ret;
    OS_EMBED_ASM("int $0x80" : "=a"(ret) : "a"(sysno), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4) : "memory");
    return ret;
}

/* 用户态打印 */
OS_INLINE U32 usr_printf(const char *str, U32 len)
{
    return OsSyscall2(OS_SYS_WRITE, (U32)str, len);
}

/* 用户态退出 */
OS_INLINE void usr_exit(U32 exitCode)
{
    OsSyscall1(OS_SYS_EXIT, exitCode);
}

#endif /* OS_SYSCALL_I386_H */
