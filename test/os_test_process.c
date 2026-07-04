#include "os_def.h"
#include "os_print_external.h"
#include "os_process_external.h"
#include "os_debug_external.h"
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
    if (semId == (U32)-1) {
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
    if (semId == (U32)-1) {
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
    if (semId == (U32)-1) {
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
