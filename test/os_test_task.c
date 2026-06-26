#include "os_def.h"
#include "os_print_external.h"
#include "os_task_external.h"
#include "os_sched_external.h"
#include "os_test_framework.h"
#include "string.h"

static OS_SEC_KERNEL_TEXT void TestPutChar(int row, int col, char c)
{
    OsPrintSetCursor((U16)(row * 80 + col));
    OsPrintChar(c);
}

/* ====== 自删除测试 ====== */
OS_SEC_KERNEL_BSS volatile U32 g_testSelfDeleteDone;
OS_SEC_KERNEL_BSS U32 g_testTaskTskA;
OS_SEC_KERNEL_BSS U32 g_testTaskTskB;
OS_SEC_KERNEL_BSS U32 g_testTaskTskC;
OS_SEC_KERNEL_BSS U32 g_testTaskTskD;

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

/* 辅助：创建任务 */
static OS_SEC_KERNEL_TEXT U32 TestCreateTask2(const char *name, U32 prio, OsTaskEntryFunc entry)
{
    U32 tskId;
    struct OsTaskCreateParam param;
    memset(&param, 0, sizeof(param));
    strcpy(param.name, name);
    param.prio = prio;
    param.entryFunc = entry;
    OsTaskCreate(&param, &tskId);
    return tskId;
}

/* 辅助：删除任务 */
static OS_SEC_KERNEL_TEXT void TestCleanupTask(U32 tskId)
{
    OsTaskSuspend(tskId);
    OsTaskDelete(tskId);
}

OS_SEC_KERNEL_TEXT void TestTaskSetup(void)
{
    g_testSelfDeleteDone = 0;
    g_testTaskTskA = TestCreateTask2("TaskA", 5, TestTaskA);
    g_testTaskTskB = TestCreateTask2("TaskB", 5, TestTaskB);
    g_testTaskTskC = TestCreateTask2("TaskC", 5, TestTaskC);
    g_testTaskTskD = TestCreateTask2("TaskD", 6, TestTaskSelfDelete);
    OsTaskResume(g_testTaskTskA);
    OsTaskResume(g_testTaskTskB);
    OsTaskResume(g_testTaskTskC);
    OsTaskResume(g_testTaskTskD);
}

OS_SEC_KERNEL_TEXT void TestTaskVerify(void)
{
    OS_TEST_ASSERT(g_testSelfDeleteDone == 1);
    /* 清理 A/B/C（while(1) 任务） */
    TestCleanupTask(g_testTaskTskA);
    TestCleanupTask(g_testTaskTskB);
    TestCleanupTask(g_testTaskTskC);
    /* D 已自删除，不需要再删 */
}
