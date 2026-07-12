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
    U32 ret;
    struct OsTaskCreateParam param;
    memset(&param, 0, sizeof(param));
    strcpy(param.name, name);
    param.prio = prio;
    param.entryFunc = entry;
    ret = OsTaskCreate(&param, &tskId);
    if (ret != OS_OK) {
        return g_tskMaxNum;
    }
    return tskId;
}

/* 安全 Resume：tskId 无效时跳过 */
static OS_SEC_KERNEL_TEXT void TestSafeResume2(U32 tskId)
{
    if (tskId < g_tskMaxNum) {
        OsTaskResume(tskId);
    }
}

/* 辅助：删除任务（若暂时持有 mutex 则重试） */
static OS_SEC_KERNEL_TEXT void TestCleanupTask(U32 tskId)
{
    U32 ret;
    U32 retry = 0;
    struct OsTaskCb *tskCb = OS_TASK_GET_CB(tskId);
    if (!(tskCb->status & OS_TASK_STATUS_USED)) {
        return;  /* 任务已自行退出 */
    }
    OsTaskSuspend(tskId);
    while ((ret = OsTaskDelete(tskId)) != OS_OK && retry < 5) {
        tskCb = OS_TASK_GET_CB(tskId);
        if (!(tskCb->status & OS_TASK_STATUS_USED)) {
            return;  /* 重试过程中任务退出 */
        }
        OsTaskResume(tskId);
        OsTaskDelay(2);
        OsTaskSuspend(tskId);
        retry++;
    }
    (void)ret;
}

OS_SEC_KERNEL_TEXT void TestTaskSetup(void)
{
    g_testSelfDeleteDone = 0;
    g_testTaskTskA = TestCreateTask2("TaskA", 5, TestTaskA);
    g_testTaskTskB = TestCreateTask2("TaskB", 5, TestTaskB);
    g_testTaskTskC = TestCreateTask2("TaskC", 5, TestTaskC);
    g_testTaskTskD = TestCreateTask2("TaskD", 6, TestTaskSelfDelete);
    TestSafeResume2(g_testTaskTskA);
    TestSafeResume2(g_testTaskTskB);
    TestSafeResume2(g_testTaskTskC);
    TestSafeResume2(g_testTaskTskD);
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
