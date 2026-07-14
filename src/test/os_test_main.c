#include "os_def.h"
#include "os_print_external.h"
#include "os_task_external.h"
#include "os_sched_external.h"
#include "os_test.h"
#include "os_uart_external.h"
#include "string.h"

/* 各模块 VGA 详细输出函数 */
extern void TestMemPrintVga(void);
extern void TestPgfPrintVga(void);

OS_SEC_KERNEL_TEXT void OsTestMainTask(void *arg1, void *arg2, void *arg3, void *arg4)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;

    OsTestFrameworkInit();
    OsTestRunAll();

    /* VGA 补充：各模块详细信息行 */
    TestMemPrintVga();
    TestPgfPrintVga();

    OsTestPrintSummary();

    OsTaskDelete(OS_RUNNING_TASK()->pid);
}
