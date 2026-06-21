#include "os_def.h"
#include "os_print_external.h"
#include "os_task_external.h"
#include "os_test.h"
#include "string.h"

/*
 * 同优先级时间片轮转(Round-Robin)测试
 *
 * 验证点:两个优先级相同且"永不阻塞"的任务,必须在时间片驱动下严格交替获得 CPU,
 * 各自计数都持续增长(不会出现一个把另一个饿死)。
 *
 * 为什么放 prio = OS_TASK_LOWEST_PRIO - 1 (30):
 *   idle 独占最低层 31,普通任务不得占用(OsTaskCreate 会拒绝)。把 RR 测试任务放到
 *   30 这一层,它们只在所有更高优先级任务(TaskA/B/C/D、sem 各任务)全部阻塞时才运行,
 *   即占用系统本会交给 idle 的"空闲窗口"。这样:
 *     1) 不会抢占/饿死任何现有测试;
 *     2) 两者始终就绪,仅靠时间片轮转在彼此间切换 → 两个计数同步爬升即为 RR 正常。
 *   若 RR 失效(例如旧版 OsTaskAdjustPrio 改 prio 导致一个被甩到别的层),会出现
 *   一个计数飞涨、另一个卡住不动。
 *
 * 观察:第 6 行 'E' 与第 7 行 'F' 的两位十六进制计数应同时不断翻转。
 */

#define OS_TEST_RR_PRIO (OS_TASK_LOWEST_PRIO - 1)

static OS_SEC_KERNEL_TEXT void TestPutChar(int row, int col, char c)
{
    OsPrintSetCursor((U16)(row * 80 + col));
    OsPrintChar(c);
}

static const char g_hexTbl[] = "0123456789ABCDEF";

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
    }
}

OS_SEC_KERNEL_TEXT U32 OsTestRrInit(void)
{
    U32 tskIdE, tskIdF;
    struct OsTaskCreateParam param;

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TaskRrE");
    param.prio = OS_TEST_RR_PRIO;
    param.entryFunc = TestTaskRrE;
    OsTaskCreate(&param, &tskIdE);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TaskRrF");
    param.prio = OS_TEST_RR_PRIO;
    param.entryFunc = TestTaskRrF;
    OsTaskCreate(&param, &tskIdF);

    OsTaskResume(tskIdE);
    OsTaskResume(tskIdF);

    return OS_OK;
}