#ifndef OS_TEST_FRAMEWORK_H
#define OS_TEST_FRAMEWORK_H

#include "os_def.h"

/* ====== 断言宏 ====== */

/* 条件不满足 → 串口输出 [FAIL] suite:name file:line cond="..."，计数+1，继续运行 */
#define OS_TEST_ASSERT(cond)                                                       \
    do {                                                                           \
        if (LIKELY(cond)) {                                                        \
            OsTestRecordPass();                                                    \
        } else {                                                                   \
            OsTestRecordFail(__FILE__, __LINE__, #cond);                           \
        }                                                                          \
    } while (0)

/* 断言两个 U32 值相等，不等时输出 got/expected */
#define OS_TEST_ASSERT_EQ(actual, expected)                                        \
    do {                                                                           \
        U32 _a = (U32)(actual);                                                    \
        U32 _e = (U32)(expected);                                                  \
        if (LIKELY(_a == _e)) {                                                    \
            OsTestRecordPass();                                                    \
        } else {                                                                   \
            OsTestRecordFailEq(__FILE__, __LINE__, _a, _e);                        \
        }                                                                          \
    } while (0)

/* ====== 用例描述 ====== */

typedef void (*OsTestCaseFunc)(void);

struct OsTestCase {
    const char *suite;        /* "MEM" / "PGF" / "SEM" / "TASK" / "RR" */
    const char *name;         /* "basic" / "prio-wake" / "round-robin" */
    OsTestCaseFunc func;      /* 同步=测试体；异步=verify 函数 */
    U32 asyncDelay;           /* 0=同步；>0=setup 后等多少 tick 再调 func */
    OsTestCaseFunc setup;     /* NULL=同步；异步=创建子任务 */
};

/* 注册表（定义在 os_test_registry.c） */
extern const struct OsTestCase g_osTestCases[];
extern const U32 g_osTestCaseCnt;

/* ====== 框架函数 ====== */

/* 初始化框架状态，输出开始标记 */
extern void OsTestFrameworkInit(void);

/* 记录一次通过 */
extern void OsTestRecordPass(void);

/* 记录一次失败（条件字符串） */
extern void OsTestRecordFail(const char *file, U32 line, const char *cond);

/* 记录一次失败（got != expected） */
extern void OsTestRecordFailEq(const char *file, U32 line, U32 actual, U32 expected);

/* 运行所有已注册的用例 */
extern void OsTestRunAll(void);

/* VGA + 串口输出最终汇总 */
extern void OsTestPrintSummary(void);

#endif /* OS_TEST_FRAMEWORK_H */
