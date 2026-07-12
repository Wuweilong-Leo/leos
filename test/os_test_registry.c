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

/* PROC */
extern void TestProcSetup(void);       extern void TestProcVerify(void);
extern void TestUsrSemSetup(void);     extern void TestUsrSemVerify(void);
extern void TestProcRecycleSetup(void);extern void TestProcRecycleVerify(void);
extern void TestProcCrossSemSetup(void);extern void TestProcCrossSemVerify(void);
extern void TestForkBasicSetup(void);   extern void TestForkBasicVerify(void);
extern void TestForkWaitpidSetup(void); extern void TestForkWaitpidVerify(void);

/* MSG */
extern void TestMsgBasicSetup(void);    extern void TestMsgBasicVerify(void);
extern void TestMsgMultiSetup(void);    extern void TestMsgMultiVerify(void);
extern void TestMsgBlockSetup(void);    extern void TestMsgBlockVerify(void);
extern void TestMsgTmoSetup(void);      extern void TestMsgTmoVerify(void);
extern void TestMsgNoWaitSetup(void);   extern void TestMsgNoWaitVerify(void);
extern void TestMsgAllocFreeSetup(void);extern void TestMsgAllocFreeVerify(void);
extern void TestMsgInvPidSetup(void);   extern void TestMsgInvPidVerify(void);

/* ====== 注册数组 ====== */

#define TC(suite_, name_, func_, delay_, setup_)  \
    { (suite_), (name_), (func_), (delay_), (setup_) }

OS_SEC_KERNEL_DATA const struct OsTestCase g_osTestCases[] = {
    /* PROC (异步) — 进程创建和用户态运行 */
    TC("PROC", "user-run", TestProcVerify, 100, TestProcSetup),
    TC("PROC", "usr-sem",  TestUsrSemVerify, 100, TestUsrSemSetup),
    TC("PROC", "recycle",  TestProcRecycleVerify, 0, TestProcRecycleSetup),
    TC("PROC", "cross-sem", TestProcCrossSemVerify, 200, TestProcCrossSemSetup),
    TC("PROC", "fork-basic", TestForkBasicVerify, 200, TestForkBasicSetup),
    TC("PROC", "fork-waitpid", TestForkWaitpidVerify, 200, TestForkWaitpidSetup),
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
    TC("MSG", "basic",      TestMsgBasicVerify,   100, TestMsgBasicSetup),
    TC("MSG", "multi",      TestMsgMultiVerify,   100, TestMsgMultiSetup),
    TC("MSG", "block-recv", TestMsgBlockVerify,   100, TestMsgBlockSetup),
    TC("MSG", "timeout",    TestMsgTmoVerify,      80, TestMsgTmoSetup),
    TC("MSG", "no-wait",    TestMsgNoWaitVerify,   30, TestMsgNoWaitSetup),
    /* MSG (同步) */
    TC("MSG", "alloc-free", TestMsgAllocFreeVerify,  0, TestMsgAllocFreeSetup),
    TC("MSG", "inv-pid",    TestMsgInvPidVerify,     0, TestMsgInvPidSetup),
    /* STRESS (长时间常稳，约 5min) */
    TC("STRESS", "soak", TestStressVerify, 0, TestStressSetup),
};

OS_SEC_KERNEL_DATA const U32 g_osTestCaseCnt = sizeof(g_osTestCases) / sizeof(struct OsTestCase);
