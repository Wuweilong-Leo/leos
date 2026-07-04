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
#define OS_SYS_MALLOC 3   /* arg1=大小; 返回分配的地址(0=失败) */
#define OS_SYS_FREE   4   /* arg1=地址; 无返回值 */
#define OS_SYS_SEM_CREATE 5   /* arg1=type, arg2=initVal, arg3=maxCnt; 返回semId(-1=失败) */
#define OS_SYS_SEM_PEND 6     /* arg1=semId, arg2=timeout; 返回错误码 */
#define OS_SYS_SEM_POST 7     /* arg1=semId; 返回错误码 */
#define OS_SYS_SEM_DELETE 8   /* arg1=semId; 返回错误码 */

/* 系统调用号总数（必须等于最大系统调用号 + 1） */
#define OS_SYS_NUM    9

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

/* 用户态 malloc */
OS_INLINE void *usr_malloc(U32 size)
{
    return (void *)(uintptr_t)OsSyscall1(OS_SYS_MALLOC, size);
}

/* 用户态 free */
OS_INLINE void usr_free(void *addr)
{
    OsSyscall1(OS_SYS_FREE, (U32)(uintptr_t)addr);
}

/* 用户态信号量 */
OS_INLINE U32 usr_sem_create(U32 type, U32 initVal, U32 maxCnt)
{
    return OsSyscall3(OS_SYS_SEM_CREATE, type, initVal, maxCnt);
}

OS_INLINE U32 usr_sem_pend(U32 semId, U32 timeout)
{
    return OsSyscall2(OS_SYS_SEM_PEND, semId, timeout);
}

OS_INLINE U32 usr_sem_post(U32 semId)
{
    return OsSyscall1(OS_SYS_SEM_POST, semId);
}

OS_INLINE U32 usr_sem_delete(U32 semId)
{
    return OsSyscall1(OS_SYS_SEM_DELETE, semId);
}

#endif /* OS_SYSCALL_I386_H */
