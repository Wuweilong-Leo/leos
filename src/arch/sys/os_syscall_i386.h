#ifndef OS_SYSCALL_I386_H
#define OS_SYSCALL_I386_H
#include "os_def.h"
#include "os_syscall_external.h"   /* OS_SYS_* 号、OsSyscallFunc、分发/注册接口（架构无关 ABI） */

/*
 * i386 系统调用机制 (INT 0x80)
 * - 用户态通过 int $0x80 触发系统调用
 * - eax = 系统调用号, ebx/ecx/edx/esi = 最多 4 个参数
 * - eax = 返回值
 *
 * 系统调用号、处理函数类型、分发(OsSyscallHandler)/注册(OsSyscallRegister)
 * 接口定义在架构无关头 os_syscall_external.h。本头只保留 i386 专有部分：
 * 汇编入口、IDT 初始化、int $0x80 内联触发、用户态包装。
 */

/* 汇编入口 */
extern void OsSyscallVector80(void);

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

/* 用户态 clone：创建共享地址空间的用户线程 */
OS_INLINE U32 usr_clone(U32 stackTop, U32 startRoutine, U32 arg, U32 trampoline)
{
    return OsSyscall4(OS_SYS_CLONE, stackTop, startRoutine, arg, trampoline);
}

/* 用户态 detach：将线程标记为分离，退出时自动回收资源 */
OS_INLINE U32 usr_detach(U32 tid)
{
    return OsSyscall1(OS_SYS_DETACH, tid);
}

/* ====== POSIX pthread 接口 ====== */

typedef U32 pthread_t;
typedef struct { U32 semId; } pthread_mutex_t;

/* pthread_detach：分离线程，使其退出后自动回收，无需 join */
OS_INLINE int pthread_detach(pthread_t thread)
{
    return (int)OsSyscall1(OS_SYS_DETACH, thread);
}

#endif /* OS_SYSCALL_I386_H */
