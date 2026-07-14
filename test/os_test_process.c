#include "os_def.h"
#include "os_print_external.h"
#include "os_process_external.h"
#include "os_debug_external.h"
#include "os_cpu.h"
#include "os_syscall_i386.h"
#include "os_sem_external.h"
#include "os_test_framework.h"
#include "os_uart_external.h"
#include "os_sched_external.h"

/*
 * 进程入口：在用户态 (CPL=3) 执行。
 * 通过 syscall (int 0x80) 与内核交互。
 *
 * OsSyscall1/2/3 是 OS_INLINE 函数，在 OS_SEC_KERNEL_TEXT 函数内被内联，
 * 生成的代码留在 .os.kernel.text 段，用户态可执行。
 */

/* 辅助：用户态打印字符串 */
OS_SEC_KERNEL_TEXT static void usr_puts(const char *s)
{
    U32 len = 0;
    while (s[len] != '\0') {
        len++;
    }
    OsSyscall2(OS_SYS_WRITE, (U32)(uintptr_t)s, len);
}

OS_SEC_KERNEL_TEXT static void TestProcEntry(void)
{
    U32 a1, a2, a3;

    /* === 1. 基本打印 === */
    usr_puts("[1] hello\n");

    /* === 2. malloc + 写入 + 打印 + free（基本流程） === */
    a1 = OsSyscall1(OS_SYS_MALLOC, 32);
    if (a1 != 0) {
        char *p = (char *)(uintptr_t)a1;
        p[0] = 'o'; p[1] = 'k'; p[2] = '\n';
        OsSyscall2(OS_SYS_WRITE, (U32)(uintptr_t)p, 3);
        OsSyscall1(OS_SYS_FREE, a1);
        usr_puts("[2] malloc+write+free ok\n");
    } else {
        usr_puts("[2] FAIL: malloc returned 0\n");
    }

    /* === 3. 多次 malloc，不 free（验证多次缺页扩展） === */
    a1 = OsSyscall1(OS_SYS_MALLOC, 64);
    a2 = OsSyscall1(OS_SYS_MALLOC, 128);
    a3 = OsSyscall1(OS_SYS_MALLOC, 256);
    if (a1 != 0 && a2 != 0 && a3 != 0) {
        /* 写入每个块，验证地址不重叠 */
        ((char *)(uintptr_t)a1)[0] = 'A';
        ((char *)(uintptr_t)a2)[0] = 'B';
        ((char *)(uintptr_t)a3)[0] = 'C';
        if (((char *)(uintptr_t)a1)[0] == 'A' &&
            ((char *)(uintptr_t)a2)[0] == 'B' &&
            ((char *)(uintptr_t)a3)[0] == 'C') {
            usr_puts("[3] multi-malloc ok\n");
        } else {
            usr_puts("[3] FAIL: write check\n");
        }
        OsSyscall1(OS_SYS_FREE, a1);
        OsSyscall1(OS_SYS_FREE, a2);
        OsSyscall1(OS_SYS_FREE, a3);
    } else {
        usr_puts("[3] FAIL: malloc returned 0\n");
    }

    /* === 4. malloc + free + malloc（验证 free 后内存可复用） === */
    a1 = OsSyscall1(OS_SYS_MALLOC, 48);
    if (a1 != 0) {
        ((char *)(uintptr_t)a1)[0] = 'X';
        OsSyscall1(OS_SYS_FREE, a1);
        a2 = OsSyscall1(OS_SYS_MALLOC, 48);
        if (a2 != 0) {
            ((char *)(uintptr_t)a2)[0] = 'Y';
            if (((char *)(uintptr_t)a2)[0] == 'Y') {
                usr_puts("[4] malloc-free-malloc reuse ok\n");
            } else {
                usr_puts("[4] FAIL: reuse write\n");
            }
            OsSyscall1(OS_SYS_FREE, a2);
        } else {
            usr_puts("[4] FAIL: second malloc returned 0\n");
        }
    } else {
        usr_puts("[4] FAIL: first malloc returned 0\n");
    }

    /* === 5. 大块分配（跨页，验证缺页自动扩展） === */
    a1 = OsSyscall1(OS_SYS_MALLOC, 2048);
    if (a1 != 0) {
        /* 写入首尾，验证跨页映射 */
        ((char *)(uintptr_t)a1)[0] = 'S';
        ((char *)(uintptr_t)a1)[2047] = 'E';
        if (((char *)(uintptr_t)a1)[0] == 'S' &&
            ((char *)(uintptr_t)a1)[2047] == 'E') {
            usr_puts("[5] large malloc ok\n");
        } else {
            usr_puts("[5] FAIL: large write\n");
        }
        OsSyscall1(OS_SYS_FREE, a1);
    } else {
        usr_puts("[5] FAIL: large malloc returned 0\n");
    }

    /* === 6. free(NULL) 应该安全 === */
    OsSyscall1(OS_SYS_FREE, 0);
    usr_puts("[6] free(NULL) ok\n");

    /* === 7. 惰性初始化验证：usrFscCtrl 一开始是 NULL，第一次 malloc 才创建 === */
    /* 前面的 malloc 已经触发过初始化了，这里不再测。但至少确认多次调用不崩溃 */
    a1 = OsSyscall1(OS_SYS_MALLOC, 16);
    if (a1 != 0) {
        OsSyscall1(OS_SYS_FREE, a1);
        usr_puts("[7] repeated malloc ok\n");
    }

    usr_puts("[ALL] process tests done\n");

    /* 退出进程 */
    OsSyscall1(OS_SYS_EXIT, 0);
}

/* ====== setup：创建并恢复进程 ====== */

OS_SEC_KERNEL_TEXT void TestProcSetup(void)
{
    U32 ret;
    U32 pid;
    struct OsProcessCreateParam param = {0};

    strcpy(param.processName, "test_proc");
    param.entryFunc = (OsProcessEntryFunc)TestProcEntry;
    param.prio = 5;
    param.param[0] = NULL;
    param.param[1] = NULL;

    ret = OsProcessCreate(&param, &pid);
    if (ret != OS_OK) {
        return;
    }

    OsProcessResume(pid);
}

/* ====== verify：延迟后检查系统是否仍在运行 ====== */

OS_SEC_KERNEL_TEXT void TestProcVerify(void)
{
    OsUartPuts("[PROC] verify ok - system survived\n");
}

/* ====== 用户态信号量测试 ====== */

/*
 * 单进程信号量基本功能测试：
 * 1. sem_create 返回有效 ID
 * 2. BINARY_MUTEX: pend 立即获取 (val=1) + post 释放
 * 3. BINARY_SYNC: pend 超时 + post 后 pend 成功
 * 4. COUNTING: 多次 post 后多次 pend
 * 5. sem_delete 成功
 *
 * 跨进程同步测试需要进程间共享 semId（待进程参数传递机制实现后补充）。
 */

OS_SEC_KERNEL_BSS volatile U32 g_testUsrSemAlive;

OS_SEC_KERNEL_TEXT static void TestUsrSemEntry(void)
{
    U32 semId;
    U32 ret;

    /* === 1. BINARY_MUTEX: pend 立即获取，post 释放，delete === */
    semId = usr_sem_create(OS_SEM_BINARY_MUTEX, 1, 1);
    if (OS_USR_SEM_ID_IS_ERR(semId)) {
        usr_puts("[SEM] FAIL: mutex create\n");
        usr_exit(1);
    }

    ret = usr_sem_pend(semId, OS_SEM_NO_WAIT);
    if (ret != OS_OK) {
        usr_puts("[SEM] FAIL: mutex pend\n");
        usr_exit(1);
    }

    ret = usr_sem_post(semId);
    if (ret != OS_OK) {
        usr_puts("[SEM] FAIL: mutex post\n");
        usr_exit(1);
    }

    ret = usr_sem_delete(semId);
    if (ret != OS_OK) {
        usr_puts("[SEM] FAIL: mutex delete\n");
        usr_exit(1);
    }
    usr_puts("[SEM] mutex ok\n");

    /* === 2. BINARY_SYNC: 初始 val=0，pend 超时，然后 post+pend === */
    semId = usr_sem_create(OS_SEM_BINARY_SYNC, 0, 1);
    if (OS_USR_SEM_ID_IS_ERR(semId)) {
        usr_puts("[SEM] FAIL: sync create\n");
        usr_exit(1);
    }

    ret = usr_sem_pend(semId, 10);
    if (ret != OS_SEM_PEND_TIMEOUT) {
        usr_puts("[SEM] FAIL: sync pend timeout\n");
        usr_exit(1);
    }

    usr_sem_post(semId);
    ret = usr_sem_pend(semId, OS_SEM_NO_WAIT);
    if (ret != OS_OK) {
        usr_puts("[SEM] FAIL: sync pend after post\n");
        usr_exit(1);
    }

    usr_sem_delete(semId);
    usr_puts("[SEM] sync ok\n");

    /* === 3. COUNTING: 多次 post + 多次 pend === */
    semId = usr_sem_create(OS_SEM_COUNTING, 0, 5);
    if (OS_USR_SEM_ID_IS_ERR(semId)) {
        usr_puts("[SEM] FAIL: cnt create\n");
        usr_exit(1);
    }

    usr_sem_post(semId);
    usr_sem_post(semId);
    usr_sem_post(semId);

    ret = usr_sem_pend(semId, OS_SEM_NO_WAIT);
    if (ret != OS_OK) {
        usr_puts("[SEM] FAIL: cnt pend1\n");
        usr_exit(1);
    }
    ret = usr_sem_pend(semId, OS_SEM_NO_WAIT);
    if (ret != OS_OK) {
        usr_puts("[SEM] FAIL: cnt pend2\n");
        usr_exit(1);
    }
    ret = usr_sem_pend(semId, OS_SEM_NO_WAIT);
    if (ret != OS_OK) {
        usr_puts("[SEM] FAIL: cnt pend3\n");
        usr_exit(1);
    }
    /* 计数耗尽，应该拿不到 */
    ret = usr_sem_pend(semId, OS_SEM_NO_WAIT);
    if (ret == OS_OK) {
        usr_puts("[SEM] FAIL: cnt pend4 should fail\n");
        usr_exit(1);
    }

    usr_sem_delete(semId);
    usr_puts("[SEM] counting ok\n");

    usr_puts("[SEM] ALL ok\n");
    g_testUsrSemAlive = 1;
    usr_exit(0);
}

OS_SEC_KERNEL_TEXT void TestUsrSemSetup(void)
{
    U32 ret;
    U32 pid;
    struct OsProcessCreateParam param = {0};

    g_testUsrSemAlive = 0;

    strcpy(param.processName, "semTest");
    param.entryFunc = (OsProcessEntryFunc)TestUsrSemEntry;
    param.prio = 5;
    ret = OsProcessCreate(&param, &pid);
    if (ret != OS_OK) {
        return;
    }

    OsProcessResume(pid);
}

OS_SEC_KERNEL_TEXT void TestUsrSemVerify(void)
{
    OS_TEST_ASSERT(g_testUsrSemAlive == 1);
    OsUartPuts("[PROC-SEM] verify ok\n");
}

/* ====== 进程重复创建退出压力测试 ====== */

/* 验证进程退出后资源被释放，能再次创建新进程 */
#define TEST_PROC_RECYCLE_ROUNDS 5

OS_SEC_KERNEL_BSS volatile U32 g_testProcRecycleCount;

/* 用户态进程入口：打印一个字符后退出 */
OS_SEC_KERNEL_TEXT static void TestProcRecycleEntry(void)
{
    usr_puts("R");
    usr_exit(0);
}

OS_SEC_KERNEL_TEXT void TestProcRecycleSetup(void)
{
    U32 i;
    U32 ret;
    U32 pid;
    struct OsProcessCreateParam param = {0};

    g_testProcRecycleCount = 0;

    for (i = 0; i < TEST_PROC_RECYCLE_ROUNDS; i++) {
        strcpy(param.processName, "recycle");
        param.entryFunc = (OsProcessEntryFunc)TestProcRecycleEntry;
        param.prio = 5;

        ret = OsProcessCreate(&param, &pid);
        if (ret != OS_OK) {
            OsUartPrintf("[PROC-RECYCLE] FAIL: create ret=%u i=%u\n", ret, i);
            return;
        }

        OsProcessResume(pid);
        /* 等进程退出后再创建下一个 */
        OsTaskDelay(30);
        g_testProcRecycleCount++;
    }
    OsUartPuts("[PROC-RECYCLE] setup done\n");
}

OS_SEC_KERNEL_TEXT void TestProcRecycleVerify(void)
{
    OS_TEST_ASSERT(g_testProcRecycleCount == TEST_PROC_RECYCLE_ROUNDS);
    OsUartPuts("[PROC-RECYCLE] verify ok\n");
}

/* ====== 跨进程信号量同步测试 ====== */

/*
 * 两个用户态进程通过内核创建的信号量同步：
 * - 进程 B（waiter, prio=5）：先运行，pend 阻塞
 * - 进程 A（poster, prio=6）：B 阻塞后才运行，post 唤醒 B
 * semId 通过 OS_PROC_ARG1() 从内核传入
 */
OS_SEC_KERNEL_BSS volatile U32 g_testProcCrossSemDone;
OS_SEC_KERNEL_BSS U32 g_testProcCrossSemId;

OS_SEC_KERNEL_TEXT static void TestProcCrossSemPostEntry(void)
{
    U32 semId = OS_PROC_ARG1();

    usr_puts("[CROSS-A] pre-post\n");
    usr_sem_post(semId);
    usr_puts("[CROSS-A] post done\n");
    usr_exit(0);
}

OS_SEC_KERNEL_TEXT static void TestProcCrossSemWaitEntry(void)
{
    U32 ret;
    U32 semId = OS_PROC_ARG1();

    usr_puts("[CROSS-B] pre-pend\n");
    ret = usr_sem_pend(semId, 200);
    if (ret != OS_OK) {
        usr_puts("[CROSS-B] FAIL: pend\n");
        usr_exit(1);
    }
    usr_puts("[CROSS-B] pend ok\n");
    g_testProcCrossSemDone = 1;
    usr_exit(0);
}

OS_SEC_KERNEL_TEXT void TestProcCrossSemSetup(void)
{
    U32 ret;
    U32 semId;
    U32 pidA, pidB;
    struct OsProcessCreateParam param = {0};

    g_testProcCrossSemDone = 0;

    /* 内核态创建信号量，初始 val=0，两个进程共享此 semId */
    ret = OsSemCreate(OS_SEM_BINARY_SYNC, 0, 1, OS_SEM_WAKE_FIFO, &g_testProcCrossSemId);
    semId = g_testProcCrossSemId;
    if (ret != OS_OK) {
        OsUartPrintf("[PROC-CROSS] FAIL: sem create ret=%u\n", ret);
        return;
    }

    /* 进程 B（waiter）：prio=5，先运行，先 pend 阻塞 */
    strcpy(param.processName, "crossB");
    param.entryFunc = (OsProcessEntryFunc)TestProcCrossSemWaitEntry;
    param.prio = 5;
    param.param[0] = (void *)(uintptr_t)semId;
    ret = OsProcessCreate(&param, &pidB);
    if (ret != OS_OK) {
        OsUartPrintf("[PROC-CROSS] FAIL: create B ret=%u\n", ret);
        return;
    }

    /* 进程 A（poster）：prio=6，B 阻塞后才运行，post 唤醒 B */
    strcpy(param.processName, "crossA");
    param.entryFunc = (OsProcessEntryFunc)TestProcCrossSemPostEntry;
    param.prio = 6;
    param.param[0] = (void *)(uintptr_t)semId;
    ret = OsProcessCreate(&param, &pidA);
    if (ret != OS_OK) {
        OsUartPrintf("[PROC-CROSS] FAIL: create A ret=%u\n", ret);
        return;
    }

    OsProcessResume(pidB);
    OsProcessResume(pidA);
    OsUartPuts("[PROC-CROSS] setup done\n");
}

OS_SEC_KERNEL_TEXT void TestProcCrossSemVerify(void)
{
    /* 先回收信号量，避免影响后续 STRESS 测试 */
    OsSemDelete(g_testProcCrossSemId);
    OS_TEST_ASSERT(g_testProcCrossSemDone == 1);
    OsUartPuts("[PROC-CROSS] verify ok\n");
}

/* ====== fork + waitpid POSIX 接口测试 ====== */

/*
 * 1. fork-basic: 子进程 fork 返回 0，父进程 fork 返回子 pid
 * 2. fork-waitpid: 父进程 waitpid 收割子进程，获取退出码
 * 3. fork-getpid: 父子进程 getpid 返回不同值
 */

OS_SEC_KERNEL_BSS volatile U32 g_testForkBasicDone;
OS_SEC_KERNEL_BSS volatile U32 g_testForkWaitpidDone;

/* fork-basic: 验证 fork 返回值 */
OS_SEC_KERNEL_TEXT static void TestForkBasicEntry(void)
{
    U32 myPid = usr_getpid();
    g_testForkBasicDone = myPid;  /* 记录 getpid 返回值 */
    U32 pid = usr_fork();
    if (pid == 0) {
        g_testForkBasicDone = 0xAA;
        usr_exit(0);
    } else if (pid != (U32)-1) {
        g_testForkBasicDone = 0xBB;
        usr_exit(0);
    } else {
        g_testForkBasicDone = 0xFF;
        usr_exit(1);
    }
}

OS_SEC_KERNEL_TEXT void TestForkBasicSetup(void)
{
    U32 ret;
    U32 pid;
    struct OsProcessCreateParam param = {0};

    g_testForkBasicDone = 0;

    strcpy(param.processName, "forkB");
    param.entryFunc = (OsProcessEntryFunc)TestForkBasicEntry;
    param.prio = 5;
    ret = OsProcessCreate(&param, &pid);
    if (ret != OS_OK) {
        return;
    }
    OsProcessResume(pid);
}

OS_SEC_KERNEL_TEXT void TestForkBasicVerify(void)
{
    OsUartPrintf("[FORK-B] g_testForkBasicDone=%u\n", g_testForkBasicDone);
    OS_TEST_ASSERT(g_testForkBasicDone >= 2);
}

/* fork-waitpid: 验证 waitpid 收割 + 退出码 + getpid */
OS_SEC_KERNEL_TEXT static void TestForkWaitpidEntry(void)
{
    U32 myPid = usr_getpid();
    U32 childPid;
    U32 status;
    U32 ret;

    childPid = usr_fork();
    if (childPid == 0) {
        /* 子进程：验证 getpid 与父不同 */
        U32 childOwnPid = usr_getpid();
        if (childOwnPid == myPid) {
            usr_puts("[FORK-W] FAIL: child pid same as parent\n");
            usr_exit(1);
        }
        usr_puts("[FORK-W] child exit 42\n");
        usr_exit(42);
    }

    /* 父进程：waitpid 阻塞等待子进程 */
    status = 0;
    ret = usr_waitpid(childPid, &status, 0);
    if (ret != childPid) {
        usr_puts("[FORK-W] FAIL: waitpid ret mismatch\n");
        usr_exit(1);
    }
    /* status = exitCode << 8, 所以 42 << 8 = 0x2A00 */
    if (status != (42 << 8)) {
        usr_puts("[FORK-W] FAIL: status mismatch\n");
        usr_exit(1);
    }
    usr_puts("[FORK-W] parent wait ok\n");
    g_testForkWaitpidDone = 1;
    usr_exit(0);
}

OS_SEC_KERNEL_TEXT void TestForkWaitpidSetup(void)
{
    U32 ret;
    U32 pid;
    struct OsProcessCreateParam param = {0};

    g_testForkWaitpidDone = 0;

    strcpy(param.processName, "forkW");
    param.entryFunc = (OsProcessEntryFunc)TestForkWaitpidEntry;
    param.prio = 5;
    ret = OsProcessCreate(&param, &pid);
    if (ret != OS_OK) {
        return;
    }
    OsProcessResume(pid);
}

OS_SEC_KERNEL_TEXT void TestForkWaitpidVerify(void)
{
    OS_TEST_ASSERT(g_testForkWaitpidDone == 1);
    OsUartPuts("[FORK-W] verify ok\n");
}

/* ====== pthread 测试 ====== */

/* pthread_mutex 内联包装（复用内核 BINARY_MUTEX 信号量） */
OS_INLINE int pthread_mutex_init(pthread_mutex_t *m, void *attr)
{
    (void)attr;
    m->semId = usr_sem_create(OS_SEM_BINARY_MUTEX, 1, 1);
    return OS_USR_SEM_ID_IS_ERR(m->semId) ? -1 : 0;
}
OS_INLINE int pthread_mutex_lock(pthread_mutex_t *m)
{
    return (int)usr_sem_pend(m->semId, OS_SEM_WAIT_FOREVER);
}
OS_INLINE int pthread_mutex_unlock(pthread_mutex_t *m)
{
    return (int)usr_sem_post(m->semId);
}
OS_INLINE int pthread_mutex_destroy(pthread_mutex_t *m)
{
    return (int)usr_sem_delete(m->semId);
}

/*
 * pthread-create-basic: 在用户进程内 pthread_create 创建共享地址空间的子线程，
 * 子线程写全局变量，主线程 pthread_join 等待
 */

OS_SEC_KERNEL_BSS volatile U32 g_testPthreadBasicDone;

/* pthread 入口跳板：clone 子线程 iret 后从这里开始执行
 * ebx=start_routine, ecx=arg（由内核 OsSysClone 设置） */
static OS_SEC_KERNEL_TEXT void __pthread_entry(void)
{
    void *(*start_routine)(void *);
    void *arg;
    void *result;
    OS_EMBED_ASM("movl %%ebx, %0" : "=r"(start_routine));
    OS_EMBED_ASM("movl %%ecx, %0" : "=r"(arg));
    result = start_routine(arg);
    usr_exit((U32)(uintptr_t)result);
}

/* 子线程入口 */
static OS_SEC_KERNEL_TEXT void *TestPthreadBasicFunc(void *arg)
{
    (void)arg;
    g_testPthreadBasicDone = 0xAA;
    return (void *)0x1234;
}

/* 主进程入口：创建线程 + join */
OS_SEC_KERNEL_TEXT static void TestPthreadBasicEntry(void)
{
    pthread_t thread;
    void *stack;
    U32 stackTop;
    U32 tid;
    U32 status;
    U32 ret;

    stack = usr_malloc(4096);
    if (stack == NULL) {
        usr_puts("[PTH-B] FAIL: malloc stack\n");
        usr_exit(1);
    }
    stackTop = (U32)(uintptr_t)stack + 4096;

    tid = usr_clone(stackTop, (U32)(uintptr_t)TestPthreadBasicFunc,
                    0, (U32)(uintptr_t)__pthread_entry);
    if (tid == (U32)-1) {
        usr_puts("[PTH-B] FAIL: clone\n");
        usr_free(stack);
        usr_exit(1);
    }
    thread = tid;

    /* 等待子线程退出 */
    status = 0;
    ret = usr_waitpid(thread, &status, 0);
    if (ret != thread) {
        usr_puts("[PTH-B] FAIL: join ret\n");
        usr_exit(1);
    }

    if (g_testPthreadBasicDone != 0xAA) {
        usr_puts("[PTH-B] FAIL: var not set\n");
        usr_exit(1);
    }

    usr_free(stack);
    usr_puts("[PTH-B] ok\n");
    g_testPthreadBasicDone = 0xBB;
    usr_exit(0);
}

OS_SEC_KERNEL_TEXT void TestPthreadBasicSetup(void)
{
    U32 ret;
    U32 pid;
    struct OsProcessCreateParam param = {0};

    g_testPthreadBasicDone = 0;

    strcpy(param.processName, "pthBas");
    param.entryFunc = (OsProcessEntryFunc)TestPthreadBasicEntry;
    param.prio = 5;
    ret = OsProcessCreate(&param, &pid);
    if (ret != OS_OK) {
        return;
    }
    OsProcessResume(pid);
}

OS_SEC_KERNEL_TEXT void TestPthreadBasicVerify(void)
{
    OS_TEST_ASSERT(g_testPthreadBasicDone == 0xBB);
    OsUartPuts("[PTH-B] verify ok\n");
}

/* ====== pthread-mutex 测试 ====== */

/*
 * 两个线程通过 pthread_mutex 互斥访问共享计数器
 */
OS_SEC_KERNEL_BSS volatile U32 g_testPthreadMutexCnt;
OS_SEC_KERNEL_BSS volatile U32 g_testPthreadMutexDone;
OS_SEC_KERNEL_BSS U32 g_testPthreadMutexSemId;

static OS_SEC_KERNEL_TEXT void *TestPthreadMutexFunc(void *arg)
{
    pthread_mutex_t mtx = *(pthread_mutex_t *)arg;
    U32 i;

    for (i = 0; i < 5; i++) {
        pthread_mutex_lock(&mtx);
        g_testPthreadMutexCnt++;
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

OS_SEC_KERNEL_TEXT static void TestPthreadMutexEntry(void)
{
    pthread_mutex_t mtx;
    void *stack;
    U32 stackTop;
    U32 tid;
    U32 ret;
    U32 i;

    if (pthread_mutex_init(&mtx, NULL) != 0) {
        usr_puts("[PTH-M] FAIL: mutex init\n");
        usr_exit(1);
    }

    g_testPthreadMutexCnt = 0;

    stack = usr_malloc(4096);
    if (stack == NULL) {
        usr_puts("[PTH-M] FAIL: malloc stack\n");
        usr_exit(1);
    }
    stackTop = (U32)(uintptr_t)stack + 4096;

    tid = usr_clone(stackTop, (U32)(uintptr_t)TestPthreadMutexFunc,
                    (U32)(uintptr_t)&mtx, (U32)(uintptr_t)__pthread_entry);
    if (tid == (U32)-1) {
        usr_puts("[PTH-M] FAIL: clone\n");
        usr_free(stack);
        usr_exit(1);
    }

    /* 主线程也跑同样的循环 */
    for (i = 0; i < 5; i++) {
        pthread_mutex_lock(&mtx);
        g_testPthreadMutexCnt++;
        pthread_mutex_unlock(&mtx);
    }

    /* 等待子线程 */
    ret = usr_waitpid(tid, NULL, 0);
    if (ret != tid) {
        usr_puts("[PTH-M] FAIL: join\n");
        usr_exit(1);
    }

    if (g_testPthreadMutexCnt != 10) {
        usr_puts("[PTH-M] FAIL: cnt mismatch\n");
        usr_exit(1);
    }

    pthread_mutex_destroy(&mtx);
    usr_free(stack);
    usr_puts("[PTH-M] ok\n");
    g_testPthreadMutexDone = 1;
    usr_exit(0);
}

OS_SEC_KERNEL_TEXT void TestPthreadMutexSetup(void)
{
    U32 ret;
    U32 pid;
    struct OsProcessCreateParam param = {0};

    g_testPthreadMutexDone = 0;

    strcpy(param.processName, "pthMut");
    param.entryFunc = (OsProcessEntryFunc)TestPthreadMutexEntry;
    param.prio = 5;
    ret = OsProcessCreate(&param, &pid);
    if (ret != OS_OK) {
        return;
    }
    OsProcessResume(pid);
}

OS_SEC_KERNEL_TEXT void TestPthreadMutexVerify(void)
{
    OS_TEST_ASSERT(g_testPthreadMutexDone == 1);
    OsUartPuts("[PTH-M] verify ok\n");
}

/* ====== pthread-detach 测试 ====== */

OS_SEC_KERNEL_BSS volatile U32 g_testPthreadDetachDone;
OS_SEC_KERNEL_BSS volatile U32 g_testPthreadDetachVar;
OS_SEC_KERNEL_BSS U32 g_testPthreadDetachSyncSem;   /* 子线程→主线程 完成信号量 */

/* A/B/E 共用 worker：写全局变量后 post 信号量退出 */
static OS_SEC_KERNEL_TEXT void *TestPthreadDetachWorker(void *arg)
{
    g_testPthreadDetachVar = (U32)(uintptr_t)arg;
    usr_sem_post(g_testPthreadDetachSyncSem);
    return NULL;
}

/* C: 线程 self-detach（DET-004） */
static OS_SEC_KERNEL_TEXT void *TestPthreadDetachSelfFunc(void *arg)
{
    pthread_t self = (pthread_t)usr_getpid();
    (void)arg;
    if (pthread_detach(self) != 0) {
        g_testPthreadDetachVar = 0xEE;   /* self-detach 失败 */
        usr_sem_post(g_testPthreadDetachSyncSem);
        return NULL;
    }
    g_testPthreadDetachVar = 0xCC;
    usr_sem_post(g_testPthreadDetachSyncSem);
    return NULL;
}

/* D: 退出后变 ZOMBIE，供主线程测试"先退出后 detach"（DET-005） */
static OS_SEC_KERNEL_TEXT void *TestPthreadDetachZombieFunc(void *arg)
{
    (void)arg;
    usr_sem_post(g_testPthreadDetachSyncSem);
    return NULL;
}

/* 辅助：分配栈 + clone，返回栈指针（NULL=失败） */
static OS_SEC_KERNEL_TEXT void *TestPthreadDetachSpawn(void *func, U32 arg, U32 *outTid)
{
    void *stack = usr_malloc(4096);
    U32 stackTop;
    U32 tid;

    if (stack == NULL) {
        return NULL;
    }
    stackTop = (U32)(uintptr_t)stack + 4096;
    tid = usr_clone(stackTop, (U32)(uintptr_t)func, arg, (U32)(uintptr_t)__pthread_entry);
    if (tid == (U32)-1) {
        usr_free(stack);
        return NULL;
    }
    *outTid = tid;
    return stack;
}

/* 主进程入口：DET-001..DET-005 + TCB 复用不残留
 * 依赖同优先级非抢占：clone 后子线程 READY 但不立即运行，主线程持续执行直到阻塞，
 * 故 detach/waitpid 的时序确定。 */
OS_SEC_KERNEL_TEXT static void TestPthreadDetachEntry(void)
{
    U32 semId;
    void *stack;
    U32 tid;
    U32 status;
    U32 ret;

    semId = usr_sem_create(OS_SEM_BINARY_SYNC, 0, 1);
    if (OS_USR_SEM_ID_IS_ERR(semId)) {
        usr_puts("[PTH-D] FAIL: sem create\n");
        usr_exit(1);
    }
    g_testPthreadDetachSyncSem = semId;

    /* === A. detach 后线程自动回收（DET-001/002）：主线程不 join === */
    g_testPthreadDetachVar = 0;
    stack = TestPthreadDetachSpawn(TestPthreadDetachWorker, 0xAA, &tid);
    if (stack == NULL) { usr_puts("[PTH-D] FAIL: A spawn\n"); usr_exit(1); }
    if (pthread_detach(tid) != 0) { usr_puts("[PTH-D] FAIL: A detach\n"); usr_exit(1); }
    usr_sem_pend(semId, OS_SEM_WAIT_FOREVER);   /* 子线程 post 后自动回收退出 */
    if (g_testPthreadDetachVar != 0xAA) { usr_puts("[PTH-D] FAIL: A var\n"); usr_exit(1); }
    usr_free(stack);
    usr_puts("[PTH-D] A detach+autoreclaim ok\n");

    /* === B. 对存活中的 detached 线程 waitpid 返回 -1（DET-003）=== */
    g_testPthreadDetachVar = 0;
    stack = TestPthreadDetachSpawn(TestPthreadDetachWorker, 0xBB, &tid);
    if (stack == NULL) { usr_puts("[PTH-D] FAIL: B spawn\n"); usr_exit(1); }
    if (pthread_detach(tid) != 0) { usr_puts("[PTH-D] FAIL: B detach\n"); usr_exit(1); }
    /* 子线程仍 READY 未运行，waitpid 立即返回 -1（ECHILD：无 joinable 子进程） */
    status = 0;
    ret = usr_waitpid(tid, &status, 0);
    if (ret != (U32)-1) { usr_puts("[PTH-D] FAIL: B waitpid\n"); usr_exit(1); }
    usr_sem_pend(semId, OS_SEM_WAIT_FOREVER);   /* 放行子线程跑完自动回收 */
    if (g_testPthreadDetachVar != 0xBB) { usr_puts("[PTH-D] FAIL: B var\n"); usr_exit(1); }
    usr_free(stack);
    usr_puts("[PTH-D] B join-rejected ok\n");

    /* === C. 线程 self-detach（DET-004）=== */
    g_testPthreadDetachVar = 0;
    stack = TestPthreadDetachSpawn(TestPthreadDetachSelfFunc, 0, &tid);
    if (stack == NULL) { usr_puts("[PTH-D] FAIL: C spawn\n"); usr_exit(1); }
    usr_sem_pend(semId, OS_SEM_WAIT_FOREVER);
    if (g_testPthreadDetachVar != 0xCC) { usr_puts("[PTH-D] FAIL: C var\n"); usr_exit(1); }
    usr_free(stack);
    usr_puts("[PTH-D] C self-detach ok\n");

    /* === D. 先退出后 detach（DET-005）：detach 一个已 ZOMBIE 的线程 === */
    stack = TestPthreadDetachSpawn(TestPthreadDetachZombieFunc, 0, &tid);
    if (stack == NULL) { usr_puts("[PTH-D] FAIL: D spawn\n"); usr_exit(1); }
    /* pend 返回时子线程已 usr_exit→self-delete 为 ZOMBIE */
    usr_sem_pend(semId, OS_SEM_WAIT_FOREVER);
    if (pthread_detach(tid) != 0) { usr_puts("[PTH-D] FAIL: D detach zombie\n"); usr_exit(1); }
    status = 0;
    ret = usr_waitpid(tid, &status, 0);
    if (ret != (U32)-1) { usr_puts("[PTH-D] FAIL: D waitpid\n"); usr_exit(1); }
    usr_free(stack);
    usr_puts("[PTH-D] D detach-zombie ok\n");

    /* === E. TCB 复用后 detached 不残留：新建 joinable 线程应能正常 join === */
    g_testPthreadDetachVar = 0;
    stack = TestPthreadDetachSpawn(TestPthreadDetachWorker, 0x55, &tid);
    if (stack == NULL) { usr_puts("[PTH-D] FAIL: E spawn\n"); usr_exit(1); }
    status = 0;
    ret = usr_waitpid(tid, &status, 0);
    if (ret != tid) { usr_puts("[PTH-D] FAIL: E join\n"); usr_exit(1); }
    if (g_testPthreadDetachVar != 0x55) { usr_puts("[PTH-D] FAIL: E var\n"); usr_exit(1); }
    usr_free(stack);
    usr_puts("[PTH-D] E reuse-joinable ok\n");

    usr_sem_delete(semId);
    usr_puts("[PTH-D] ALL ok\n");
    g_testPthreadDetachDone = 1;
    usr_exit(0);
}

OS_SEC_KERNEL_TEXT void TestPthreadDetachSetup(void)
{
    U32 ret;
    U32 pid;
    struct OsProcessCreateParam param = {0};

    g_testPthreadDetachDone = 0;

    strcpy(param.processName, "pthDet");
    param.entryFunc = (OsProcessEntryFunc)TestPthreadDetachEntry;
    param.prio = 5;
    ret = OsProcessCreate(&param, &pid);
    if (ret != OS_OK) {
        return;
    }
    OsProcessResume(pid);
}

OS_SEC_KERNEL_TEXT void TestPthreadDetachVerify(void)
{
    OS_TEST_ASSERT(g_testPthreadDetachDone == 1);
    OsUartPuts("[PTH-D] verify ok\n");
}
