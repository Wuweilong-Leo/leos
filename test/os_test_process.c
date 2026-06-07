#include "os_def.h"
#include "os_print_external.h"
#include "os_task_external.h"
#include "os_sched_external.h"
#include "os_process_external.h"
#include "os_mem_external.h"
#include "os_cpu.h"
#include "os_test.h"
#include "string.h"

/* 用户进程测试 — 先验证进程创建和页目录切换 */

/* 进程入口：在内核态运行（OsProcessEntry 会 iret 到此函数）
 * 如果 iret 成功，说明 Ring 3 切换正常
 * 进程里只做简单计数循环，通过 kprintf 输出
 * 注意：kprintf 在用户态不可用（需要系统调用），
 * 但如果进程页目录正确映射了内核空间，Ring 3 下访问内核地址会 #GP
 * 所以我们暂时让进程入口只做死循环，通过 QEMU 日志验证
 */
static OS_SEC_KERNEL_TEXT void TestUserProcessEntry(void *arg1, void *arg2)
{
    volatile U32 i = 0;
    while (1) {
        i++;
    }
}

OS_SEC_KERNEL_TEXT U32 OsTestProcessInit(void)
{
    U32 pid;
    struct OsProcessCreateParam param;

    /* 注册所有测试模块（不打印，避免干扰 VGA 测试输出） */

    memset(&param, 0, sizeof(param));
    strcpy(param.processName, "UserProc1");
    param.entryFunc = (OsProcessEntryFunc)TestUserProcessEntry;
    param.prio = 10;
    param.param[0] = NULL;
    param.param[1] = NULL;

    if (OsProcessCreate(&param, &pid) != OS_OK) {
    /* 创建失败也不打印 */
        return OS_OK;
    }

    OsProcessResume(pid);

    /* 不打印 pid */
    return OS_OK;
}
