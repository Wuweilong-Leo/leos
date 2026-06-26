#include "os_def.h"
#include "os_print_external.h"
#include "os_task_external.h"
#include "os_uart_external.h"
#include "os_test_framework.h"
#include "string.h"

/* ====== 内部状态 ====== */

#define OS_TEST_MAX_SUITES 8

struct OsTestSuiteResult {
    const char *name;
    U32 passCnt;
    U32 failCnt;
};

OS_SEC_KERNEL_BSS struct OsTestSuiteResult g_testSuites[OS_TEST_MAX_SUITES];
OS_SEC_KERNEL_BSS U32 g_testSuiteCnt;
OS_SEC_KERNEL_BSS U32 g_testTotalPass;
OS_SEC_KERNEL_BSS U32 g_testTotalFail;

/* 当前用例的 suite/name（由 OsTestRunCase 设置，供 Record 函数使用） */
OS_SEC_KERNEL_BSS const char *g_testCurSuite;
OS_SEC_KERNEL_BSS const char *g_testCurName;
OS_SEC_KERNEL_BSS U32 g_testCurPass;
OS_SEC_KERNEL_BSS U32 g_testCurFail;

/* ====== 记录函数 ====== */

OS_SEC_KERNEL_TEXT void OsTestRecordPass(void)
{
    g_testCurPass++;
}

OS_SEC_KERNEL_TEXT void OsTestRecordFail(const char *file, U32 line, const char *cond)
{
    g_testCurFail++;
    OsUartPrintf("[FAIL] %s:%s %s:%d cond=\"%s\"\n",
                g_testCurSuite, g_testCurName, file, line, cond);
}

OS_SEC_KERNEL_TEXT void OsTestRecordFailEq(const char *file, U32 line, U32 actual, U32 expected)
{
    g_testCurFail++;
    OsUartPrintf("[FAIL] %s:%s %s:%d got=0x%x expected=0x%x\n",
                g_testCurSuite, g_testCurName, file, line, actual, expected);
}

/* ====== 框架初始化 ====== */

OS_SEC_KERNEL_TEXT void OsTestFrameworkInit(void)
{
    memset(g_testSuites, 0, sizeof(g_testSuites));
    g_testSuiteCnt = 0;
    g_testTotalPass = 0;
    g_testTotalFail = 0;
    g_testCurSuite = "";
    g_testCurName = "";
    g_testCurPass = 0;
    g_testCurFail = 0;

    OsUartPuts("\n=== LEOS TEST START ===\n");
}

/* ====== 查找/创建 suite 条目 ====== */

static OS_SEC_KERNEL_TEXT struct OsTestSuiteResult *OsTestFindSuite(const char *name)
{
    U32 i;
    for (i = 0; i < g_testSuiteCnt; i++) {
        if (strcmp(g_testSuites[i].name, name) == 0) {
            return &g_testSuites[i];
        }
    }
    /* 新建 */
    if (g_testSuiteCnt < OS_TEST_MAX_SUITES) {
        struct OsTestSuiteResult *s = &g_testSuites[g_testSuiteCnt++];
        s->name = name;
        s->passCnt = 0;
        s->failCnt = 0;
        return s;
    }
    /* 溢出：返回最后一个，累加进去 */
    return &g_testSuites[OS_TEST_MAX_SUITES - 1];
}

/* ====== 运行单个用例 ====== */

OS_SEC_KERNEL_TEXT void OsTestRunCase(const struct OsTestCase *tc)
{
    struct OsTestSuiteResult *suite;

    g_testCurSuite = tc->suite;
    g_testCurName = tc->name;
    g_testCurPass = 0;
    g_testCurFail = 0;

    /* 异步用例：先 setup（创建子任务），再 delay，再 verify */
    if (tc->setup != NULL) {
        tc->setup();
    }
    if (tc->asyncDelay > 0) {
        OsTaskDelay(tc->asyncDelay);
    }

    /* 运行测试体 / verify */
    tc->func();

    /* 累加到 suite 和全局 */
    suite = OsTestFindSuite(tc->suite);
    suite->passCnt += g_testCurPass;
    suite->failCnt += g_testCurFail;
    g_testTotalPass += g_testCurPass;
    g_testTotalFail += g_testCurFail;

    /* 串口输出用例结果 */
    if (g_testCurFail == 0) {
        OsUartPrintf("[PASS] %s:%s\n", tc->suite, tc->name);
    } else {
        OsUartPrintf("[FAIL] %s:%s fails=%d\n", tc->suite, tc->name, g_testCurFail);
    }
}

/* ====== 运行所有用例 ====== */

OS_SEC_KERNEL_TEXT void OsTestRunAll(void)
{
    U32 i;
    for (i = 0; i < g_osTestCaseCnt; i++) {
        OsTestRunCase(&g_osTestCases[i]);
    }
}

/* ====== 输出汇总 ====== */

OS_SEC_KERNEL_TEXT void OsTestPrintSummary(void)
{
    U32 total = g_testTotalPass + g_testTotalFail;
    U32 i;

    /* 串口汇总 */
    OsUartPrintf("\n=== LEOS TEST END ===  TOTAL: pass=%d fail=%d\n",
                g_testTotalPass, g_testTotalFail);

    /* VGA Row 0: 全局汇总 */
    OsPrintSetCursor(0);
    if (g_testTotalFail == 0) {
        kprintf("LEOS TEST: %d/%d ALL PASSED", g_testTotalPass, total);
    } else {
        kprintf("LEOS TEST: %d/%d passed  FAIL:", g_testTotalPass, total);
        for (i = 0; i < g_testSuiteCnt; i++) {
            if (g_testSuites[i].failCnt > 0) {
                kprintf(" %s:%d", g_testSuites[i].name, g_testSuites[i].failCnt);
            }
        }
    }

    /* VGA Row 1: 各 suite 比率 */
    OsPrintSetCursor((U16)(1 * 80));
    for (i = 0; i < g_testSuiteCnt; i++) {
        U32 sTotal = g_testSuites[i].passCnt + g_testSuites[i].failCnt;
        if (i > 0) {
            kprintf(" ");
        }
        kprintf("%s:%d/%d", g_testSuites[i].name, g_testSuites[i].passCnt, sTotal);
    }
}
