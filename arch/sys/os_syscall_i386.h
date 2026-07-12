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

/* 系统调用号总数（必须等于最大系统调用号 + 1） */
#define OS_SYS_NUM    12

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

OS_INLINE U32 OsSyscall0(U32 sysno)
{
    U32 ret;
    OS_EMBED_ASM("int $0x80" : "=a"(ret) : "a"(sysno) : "memory");
    return ret;
}

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
/* sem_create 返回值：[0, OS_SEM_MAX_NUM) 为 semId，>= 0x10000 为错误码 */
#define OS_USR_SEM_ID_IS_ERR(r) ((r) >= 0x10000U)

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

/* 用户态进程接口 */
OS_INLINE U32 usr_getpid(void)
{
    return OsSyscall0(OS_SYS_GETPID);
}

OS_INLINE U32 usr_fork(void)
{
    return OsSyscall0(OS_SYS_FORK);
}

OS_INLINE U32 usr_waitpid(U32 pid, U32 *status, U32 options)
{
    return OsSyscall3(OS_SYS_WAITPID, pid, (U32)(uintptr_t)status, options);
}

#endif /* OS_SYSCALL_I386_H */
