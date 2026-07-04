#include "os_def.h"
#include "os_print_external.h"
#include "os_process_external.h"
#include "os_debug_external.h"
#include "os_syscall_i386.h"
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
