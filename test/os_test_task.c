#include "os_def.h"
#include "os_print_external.h"
#include "os_task_external.h"
#include "os_sched_external.h"
#include "os_test_framework.h"
#include "string.h"

/* 通过打印模块在指定行列写字符 */
static OS_SEC_KERNEL_TEXT void TestPutChar(int row, int col, char c)
{
    OsPrintSetCursor((U16)(row * 80 + col));
    OsPrintChar(c);
}

/* ====== 自删除测试 ====== */
OS_SEC_KERNEL_BSS volatile U32 g_testSelfDeleteDone;

/* 任务D：跑几圈后自己删自己 */
OS_SEC_KERNEL_TEXT void TestTaskSelfDelete(void *para1, void *param2, void *param3, void *param4)
{
    U32 count = 0;
    TestPutChar(5, 0, 'D');
    while (1) {
        TestPutChar(5, 2, "0123456789ABCDEF"[(count >> 4) & 0xF]);
        TestPutChar(5, 3, "0123456789ABCDEF"[count & 0xF]);
        count++;
        if (count >= 5) {
            g_testSelfDeleteDone = 1;
            OsTaskDelete(OS_RUNNING_TASK()->pid);
        }
        OsTaskDelay(10);
    }
}

/* 线程A: 在第2行显示 A 和计数 */
OS_SEC_KERNEL_TEXT void TestTaskA(void *para1, void *param2, void *param3, void *param4)
{
    U32 count = 0;
    TestPutChar(2, 0, 'A');
    while (1) {
        TestPutChar(2, 2, "0123456789ABCDEF"[(count >> 4) & 0xF]);
        TestPutChar(2, 3, "0123456789ABCDEF"[count & 0xF]);
        count++;
        OsTaskDelay(10);
    }
}

/* 线程B: 在第3行显示 B 和计数 */
OS_SEC_KERNEL_TEXT void TestTaskB(void *para1, void *param2, void *param3, void *param4)
{
    U32 count = 0;
    TestPutChar(3, 0, 'B');
    while (1) {
        TestPutChar(3, 2, "0123456789ABCDEF"[(count >> 4) & 0xF]);
        TestPutChar(3, 3, "0123456789ABCDEF"[count & 0xF]);
        count++;
        OsTaskDelay(20);
    }
}

/* 线程C: 在第4行显示 C 和计数 */
OS_SEC_KERNEL_TEXT void TestTaskC(void *para1, void *param2, void *param3, void *param4)
{
    U32 count = 0;
    TestPutChar(4, 0, 'C');
    while (1) {
        TestPutChar(4, 2, "0123456789ABCDEF"[(count >> 4) & 0xF]);
        TestPutChar(4, 3, "0123456789ABCDEF"[count & 0xF]);
        count++;
        OsTaskDelay(30);
    }
}

/* setup: 创建 A/B/C/D 四个任务 */
OS_SEC_KERNEL_TEXT void TestTaskSetup(void)
{
    U32 tskIdA, tskIdB, tskIdC, tskIdD;
    struct OsTaskCreateParam param;

    g_testSelfDeleteDone = 0;

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TaskA");
    param.prio = 5;
    param.entryFunc = TestTaskA;
    OsTaskCreate(&param, &tskIdA);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TaskB");
    param.prio = 5;
    param.entryFunc = TestTaskB;
    OsTaskCreate(&param, &tskIdB);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TaskC");
    param.prio = 5;
    param.entryFunc = TestTaskC;
    OsTaskCreate(&param, &tskIdC);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TaskD");
    param.prio = 6;
    param.entryFunc = TestTaskSelfDelete;
    OsTaskCreate(&param, &tskIdD);

    OsTaskResume(tskIdA);
    OsTaskResume(tskIdB);
    OsTaskResume(tskIdC);
    OsTaskResume(tskIdD);
}

/* verify: 检查 TaskD 自删除成功 */
OS_SEC_KERNEL_TEXT void TestTaskVerify(void)
{
    OS_TEST_ASSERT(g_testSelfDeleteDone == 1);
}
