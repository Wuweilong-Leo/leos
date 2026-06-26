#include "os_test_framework.h"

/* ====== 用例注册表 ====== */

/* MEM */
extern void TestMemSetup(void);
extern void TestFscBasic(void);
extern void TestFscAlign(void);
extern void TestFscSplit(void);
extern void TestFscCoalesce(void);
extern void TestFscReuse(void);
extern void TestFscOom(void);
extern void TestFscStress(void);
extern void TestFscMagic(void);

/* PGF */
extern void TestPgfSetup(void);
extern void TestPgfFault(void);
extern void TestPgfPhys(void);
extern void TestPgfCrossPde(void);
extern void TestPgfGuard(void);
extern void TestPgfPrimitive(void);
extern void TestPgfStress(void);
extern void TestPgfInvariantCase(void);

/* SEM */
extern void TestSemCntSetup(void);      extern void TestSemCntVerify(void);
extern void TestSemBinSyncSetup(void);  extern void TestSemBinSyncVerify(void);
extern void TestSemMutexSetup(void);    extern void TestSemMutexVerify(void);
extern void TestSemPrioSetup(void);     extern void TestSemPrioVerify(void);
extern void TestSemTmoSetup(void);      extern void TestSemTmoVerify(void);
extern void TestSemNotHolderSetup(void);extern void TestSemNotHolderVerify(void);
extern void TestSemPISetup(void);       extern void TestSemPIVerify(void);
extern void TestSemSusSetup(void);      extern void TestSemSusVerify(void);
extern void TestSemDeleteSetup(void);  extern void TestSemDeleteVerify(void);

/* TASK */
extern void TestTaskSetup(void);        extern void TestTaskVerify(void);

/* RR */
extern void TestRrSetup(void);          extern void TestRrVerify(void);

/* STRESS */
extern void TestStressSetup(void);      extern void TestStressVerify(void);

/* ====== 注册数组 ====== */

#define TC(suite_, name_, func_, delay_, setup_)  \
    { (suite_), (name_), (func_), (delay_), (setup_) }

OS_SEC_KERNEL_DATA const struct OsTestCase g_osTestCases[] = {
    /* MEM (同步) */
    TC("MEM", "setup",    TestMemSetup,    0, NULL),
    TC("MEM", "basic",    TestFscBasic,    0, NULL),
    TC("MEM", "align",    TestFscAlign,    0, NULL),
    TC("MEM", "split",    TestFscSplit,    0, NULL),
    TC("MEM", "coalesce", TestFscCoalesce, 0, NULL),
    TC("MEM", "reuse",    TestFscReuse,    0, NULL),
    TC("MEM", "oom",      TestFscOom,      0, NULL),
    TC("MEM", "stress",   TestFscStress,   0, NULL),
    TC("MEM", "magic",    TestFscMagic,    0, NULL),
    /* PGF (同步) */
    TC("PGF", "setup",    TestPgfSetup,        0, NULL),
    TC("PGF", "fault",    TestPgfFault,        0, NULL),
    TC("PGF", "phys",     TestPgfPhys,         0, NULL),
    TC("PGF", "crosspde", TestPgfCrossPde,     0, NULL),
    TC("PGF", "guard",    TestPgfGuard,        0, NULL),
    TC("PGF", "primitive",TestPgfPrimitive,    0, NULL),
    TC("PGF", "stress",   TestPgfStress,       0, NULL),
    TC("PGF", "invariant",TestPgfInvariantCase,0, NULL),
    /* SEM (异步) */
    TC("SEM", "cnt-sem",    TestSemCntVerify,      200, TestSemCntSetup),
    TC("SEM", "bin-sync",   TestSemBinSyncVerify,  200, TestSemBinSyncSetup),
    TC("SEM", "mutex",      TestSemMutexVerify,    200, TestSemMutexSetup),
    TC("SEM", "prio-wake",  TestSemPrioVerify,     100, TestSemPrioSetup),
    TC("SEM", "timeout",    TestSemTmoVerify,      100, TestSemTmoSetup),
    TC("SEM", "not-holder", TestSemNotHolderVerify,100, TestSemNotHolderSetup),
    TC("SEM", "pi",         TestSemPIVerify,       200, TestSemPISetup),
    TC("SEM", "suspend",    TestSemSusVerify,       60, TestSemSusSetup),
    TC("SEM", "delete",     TestSemDeleteVerify,    0,  TestSemDeleteSetup),
    /* TASK (异步) */
    TC("TASK", "self-delete", TestTaskVerify, 100, TestTaskSetup),
    /* RR (异步) */
    TC("RR", "round-robin", TestRrVerify, 100, TestRrSetup),
    /* STRESS (长时间常稳，约 5min) */
    TC("STRESS", "soak", TestStressVerify, 0, TestStressSetup),
};

OS_SEC_KERNEL_DATA const U32 g_osTestCaseCnt = sizeof(g_osTestCases) / sizeof(struct OsTestCase);
