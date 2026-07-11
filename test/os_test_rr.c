#include "os_def.h"
#include "os_print_external.h"
#include "os_task_external.h"
#include "os_test_framework.h"
#include "string.h"

/*
 * 同优先级时间片轮转(Round-Robin)测试
 *
 * 两个永不阻塞的同优先级(30)任务,纯计数,靠时间片轮转在彼此间切换。
 * verify 采样计数后 delay,再检查计数增长 → 证明 RR 生效。
 */

#define OS_TEST_RR_PRIO (OS_TASK_LOWEST_PRIO - 1)

static OS_SEC_KERNEL_TEXT void TestPutChar(int row, int col, char c)
{
    OsPrintSetCursor((U16)(row * 80 + col));
    OsPrintChar(c);
}

static const char g_hexTbl[] = "0123456789ABCDEF";

/* 计数器（供 verify 采样） */
OS_SEC_KERNEL_BSS volatile U32 g_rrCountE;
OS_SEC_KERNEL_BSS volatile U32 g_rrCountF;
OS_SEC_KERNEL_BSS U32 g_rrTskIdE;
OS_SEC_KERNEL_BSS U32 g_rrTskIdF;

/* 任务E:第 6 行,永不阻塞,纯计数 + 显示 */
OS_SEC_KERNEL_TEXT void TestTaskRrE(void *arg1, void *arg2, void *arg3, void *arg4)
{
    U32 count = 0;
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    TestPutChar(6, 0, 'E');
    while (1) {
        TestPutChar(6, 2, g_hexTbl[(count >> 4) & 0xF]);
        TestPutChar(6, 3, g_hexTbl[count & 0xF]);
        count++;
        g_rrCountE = count;
    }
}

/* 任务F:第 7 行,永不阻塞,纯计数 + 显示 */
OS_SEC_KERNEL_TEXT void TestTaskRrF(void *arg1, void *arg2, void *arg3, void *arg4)
{
    U32 count = 0;
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    TestPutChar(7, 0, 'F');
    while (1) {
        TestPutChar(7, 2, g_hexTbl[(count >> 4) & 0xF]);
        TestPutChar(7, 3, g_hexTbl[count & 0xF]);
        count++;
        g_rrCountF = count;
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

/* setup: 创建两个 RR 任务 */
OS_SEC_KERNEL_TEXT void TestRrSetup(void)
{
    struct OsTaskCreateParam param;

    g_rrCountE = 0;
    g_rrCountF = 0;

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TaskRrE");
    param.prio = OS_TEST_RR_PRIO;
    param.entryFunc = TestTaskRrE;
    OsTaskCreate(&param, &g_rrTskIdE);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TaskRrF");
    param.prio = OS_TEST_RR_PRIO;
    param.entryFunc = TestTaskRrF;
    OsTaskCreate(&param, &g_rrTskIdF);

    OsTaskResume(g_rrTskIdE);
    OsTaskResume(g_rrTskIdF);
}

/* verify: 采样 → delay → 检查计数增长 → 清理 */
OS_SEC_KERNEL_TEXT void TestRrVerify(void)
{
    U32 e1 = g_rrCountE;
    U32 f1 = g_rrCountF;
    OsTaskDelay(50);
    OS_TEST_ASSERT(g_rrCountE > e1);
    OS_TEST_ASSERT(g_rrCountF > f1);
    /* 清理 */
    TestCleanupTask(g_rrTskIdE);
    TestCleanupTask(g_rrTskIdF);
}
