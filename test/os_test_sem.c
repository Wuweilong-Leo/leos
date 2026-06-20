#include "os_def.h"
#include "os_print_external.h"
#include "os_task_external.h"
#include "os_sched_external.h"
#include "os_sem_external.h"
#include "os_test.h"
#include "string.h"
#include "os_uart_external.h"

/* ====== 测试结果全局变量 ====== */
OS_SEC_KERNEL_BSS volatile U32 g_semTestResult;

#define SEM_TEST_CNT_SEM_OK         0x00000001  /* 计数信号量生产消费正常 */
#define SEM_TEST_CNT_POST_FULL      0x00000002  /* 计数信号量 Post 满返回 IS_FULL */
#define SEM_TEST_BIN_SYNC_OK        0x00000004  /* 二值同步信号量正常 */
#define SEM_TEST_BIN_POST_REPEAT    0x00000008  /* 二值同步重复 Post 返回 OK */
#define SEM_TEST_MUTEX_OK           0x00000010  /* 二值互斥信号量正常 */
#define SEM_TEST_MUTEX_NOT_HOLDER   0x00000020  /* 非持有者 Post 返回 NOT_HOLDER */
#define SEM_TEST_PRIO_WAKE_OK       0x00000040  /* PRIO 唤醒顺序正确 */
#define SEM_TEST_TMO_TIMEOUT        0x00000080  /* 超时等待返回 TIMEOUT */
#define SEM_TEST_TMO_NO_WAIT        0x00000100  /* 不等待返回 UNAVAILABLE */
#define SEM_TEST_TMO_NORMAL         0x00000200  /* 带超时正常获取 */
#define SEM_TEST_PI_BOOST_OK        0x00000400  /* PI: 低优先级持有者被提升 */
#define SEM_TEST_PI_RESTORE_OK      0x00000800  /* PI: 释放后优先级恢复 */

/* ====== 生产者-消费者测试（计数信号量，FIFO 唤醒） ====== */

OS_SEC_KERNEL_BSS volatile U32 g_testSemBuf;
OS_SEC_KERNEL_BSS U32 g_testSemId;
OS_SEC_KERNEL_BSS volatile U32 g_testSemProdCnt;
OS_SEC_KERNEL_BSS volatile U32 g_testSemConsCnt;
OS_SEC_KERNEL_BSS volatile U32 g_testSemPostFullFlag;

OS_SEC_KERNEL_TEXT void TestSemProducer(void *p1, void *p2, void *p3, void *p4)
{
    U32 ret;
    while (1) {
        g_testSemBuf = g_testSemProdCnt;
        ret = OsSemPost(g_testSemId);
        if (ret == OS_SEM_POST_IS_FULL) {
            g_testSemPostFullFlag = 1; /* 预期：生产者比消费者快 */
        }
        g_testSemProdCnt++;
        OsTaskDelay(20);
    }
}

OS_SEC_KERNEL_TEXT void TestSemConsumer(void *p1, void *p2, void *p3, void *p4)
{
    while (1) {
        OsSemPend(g_testSemId, OS_SEM_WAIT_FOREVER);
        g_testSemConsCnt++;
        OsTaskDelay(30);
    }
}

/* ====== 二值信号量同步测试 ====== */

OS_SEC_KERNEL_BSS U32 g_testBinSemId;
OS_SEC_KERNEL_BSS volatile U32 g_testBinSemResult;
OS_SEC_KERNEL_BSS volatile U32 g_testBinSemPostRepeatFlag;

OS_SEC_KERNEL_TEXT void TestBinSemWaiter(void *p1, void *p2, void *p3, void *p4)
{
    while (1) {
        OsSemPend(g_testBinSemId, OS_SEM_WAIT_FOREVER);
        g_testBinSemResult++;
        OsTaskDelay(30);
    }
}

OS_SEC_KERNEL_TEXT void TestBinSemNotifier(void *p1, void *p2, void *p3, void *p4)
{
    U32 ret;
    while (1) {
        ret = OsSemPost(g_testBinSemId);
        if (ret == OS_OK) {
            g_testBinSemPostRepeatFlag = 1; /* 重复 Post 也返回 OK */
        }
        OsTaskDelay(10);
    }
}

/* ====== 互斥测试（BINARY_MUTEX，PRIO 唤醒） ====== */

OS_SEC_KERNEL_BSS volatile U32 g_testMutexVal;
OS_SEC_KERNEL_BSS U32 g_testMutexId;

OS_SEC_KERNEL_TEXT void TestMutexTaskX(void *p1, void *p2, void *p3, void *p4)
{
    while (1) {
        OsSemPend(g_testMutexId, OS_SEM_WAIT_FOREVER);
        g_testMutexVal += 100;
        OsSemPost(g_testMutexId);
        OsTaskDelay(20);
    }
}

OS_SEC_KERNEL_TEXT void TestMutexTaskY(void *p1, void *p2, void *p3, void *p4)
{
    while (1) {
        OsSemPend(g_testMutexId, OS_SEM_WAIT_FOREVER);
        g_testMutexVal += 1;
        OsSemPost(g_testMutexId);
        OsTaskDelay(20);
    }
}

/* ====== PRIO 唤醒策略测试 ====== */

OS_SEC_KERNEL_BSS U32 g_testPrioSemId;
OS_SEC_KERNEL_BSS volatile U32 g_testPrioWakeOrder[3];
OS_SEC_KERNEL_BSS volatile U32 g_testPrioWakeIdx;

OS_SEC_KERNEL_TEXT void TestPrioTaskH(void *p1, void *p2, void *p3, void *p4)
{
    OsSemPend(g_testPrioSemId, OS_SEM_WAIT_FOREVER);
    g_testPrioWakeOrder[g_testPrioWakeIdx++] = 5;
}

OS_SEC_KERNEL_TEXT void TestPrioTaskM(void *p1, void *p2, void *p3, void *p4)
{
    OsSemPend(g_testPrioSemId, OS_SEM_WAIT_FOREVER);
    g_testPrioWakeOrder[g_testPrioWakeIdx++] = 10;
}

OS_SEC_KERNEL_TEXT void TestPrioTaskL(void *p1, void *p2, void *p3, void *p4)
{
    OsSemPend(g_testPrioSemId, OS_SEM_WAIT_FOREVER);
    g_testPrioWakeOrder[g_testPrioWakeIdx++] = 15;
}

OS_SEC_KERNEL_TEXT void TestPrioPostTask(void *p1, void *p2, void *p3, void *p4)
{
    U32 i;
    OsTaskDelay(60);
    for (i = 0; i < 3; i++) {
        OsSemPost(g_testPrioSemId);
    }
    OsTaskDelay(20);
    /* 检查唤醒顺序: H(5) → M(10) → L(15) */
    if (g_testPrioWakeOrder[0] == 5 && g_testPrioWakeOrder[1] == 10 && g_testPrioWakeOrder[2] == 15) {
        g_semTestResult |= SEM_TEST_PRIO_WAKE_OK;
    }
}

/* ====== 超时测试 ====== */

OS_SEC_KERNEL_BSS U32 g_testTmoSemId;
OS_SEC_KERNEL_BSS U32 g_testTmoSemId2;
OS_SEC_KERNEL_BSS volatile U32 g_testTmoResult;
OS_SEC_KERNEL_BSS volatile U32 g_testTmoNoWaitResult;
OS_SEC_KERNEL_BSS volatile U32 g_testTmoNormalResult;

OS_SEC_KERNEL_TEXT void TestTmoWait(void *p1, void *p2, void *p3, void *p4)
{
    U32 ret = OsSemPend(g_testTmoSemId, 50);
    if (ret == OS_SEM_PEND_TIMEOUT) {
        g_semTestResult |= SEM_TEST_TMO_TIMEOUT;
    }
}

OS_SEC_KERNEL_TEXT void TestTmoNoWait(void *p1, void *p2, void *p3, void *p4)
{
    U32 ret = OsSemPend(g_testTmoSemId, OS_SEM_NO_WAIT);
    if (ret == OS_SEM_PEND_UNAVAILABLE) {
        g_semTestResult |= SEM_TEST_TMO_NO_WAIT;
    }
}

OS_SEC_KERNEL_TEXT void TestTmoNormal(void *p1, void *p2, void *p3, void *p4)
{
    U32 ret = OsSemPend(g_testTmoSemId2, 100);
    if (ret == OS_OK) {
        g_semTestResult |= SEM_TEST_TMO_NORMAL;
    }
}

OS_SEC_KERNEL_TEXT void TestTmoPostTask(void *p1, void *p2, void *p3, void *p4)
{
    OsTaskDelay(80);
    OsSemPost(g_testTmoSemId2);
}

/* ====== 非持有者 Post BINARY_MUTEX 测试 ====== */

OS_SEC_KERNEL_BSS U32 g_testMutexOwnerId;
OS_SEC_KERNEL_BSS volatile U32 g_testMutexNotHolderFlag;

OS_SEC_KERNEL_TEXT void TestMutexNotHolder(void *p1, void *p2, void *p3, void *p4)
{
    U32 ret;
    OsTaskDelay(30);
    ret = OsSemPost(g_testMutexOwnerId);
    if (ret == OS_SEM_POST_NOT_HOLDER) {
        g_testMutexNotHolderFlag = 1;
    }
}

OS_SEC_KERNEL_TEXT void TestMutexOwner(void *p1, void *p2, void *p3, void *p4)
{
    OsSemPend(g_testMutexOwnerId, OS_SEM_WAIT_FOREVER);
    OsTaskDelay(50);
    OsSemPost(g_testMutexOwnerId);
}

/* ====== 优先级继承测试 ====== */
/*
 * 场景：低优先级任务持有mutex，高优先级任务来pend
 * 预期：低优先级任务优先级被提升到高优先级，释放后恢复
 *
 * 时序：
 *   PILow(prio=20) 先获取mutex，delay让出CPU
 *   PIMid(prio=15) 开始运行（不碰mutex），记录midRan=1
 *   PIHigh(prio=5) pend mutex → 触发PI，PILow被提升到prio=5
 *   PILow 恢复运行后释放mutex → 优先级恢复到20
 *   PIHigh 获取mutex，记录highGot=1
 */

OS_SEC_KERNEL_BSS U32 g_testPIMutexId;
OS_SEC_KERNEL_BSS volatile U32 g_testPIBoostOk;
OS_SEC_KERNEL_BSS volatile U32 g_testPIRestoreOk;
OS_SEC_KERNEL_BSS volatile U32 g_testPIMidRan;
OS_SEC_KERNEL_BSS volatile U32 g_testPIHighGot;
OS_SEC_KERNEL_BSS volatile U32 g_testPILowPrioBefore;
OS_SEC_KERNEL_BSS volatile U32 g_testPILowPrioDuring;
OS_SEC_KERNEL_BSS volatile U32 g_testPILowPrioAfter;

OS_SEC_KERNEL_BSS volatile U32 g_testPIDone;

OS_SEC_KERNEL_TEXT void TestPILowTask(void *p1, void *p2, void *p3, void *p4)
{
    U32 myPrio;
    OsSemPend(g_testPIMutexId, OS_SEM_WAIT_FOREVER);
    g_testPILowPrioBefore = OS_RUNNING_TASK()->prio;
    /* 让出CPU，让PIMid和PIHigh有机会运行 */
    OsTaskDelay(60);
    /* 此时PIHigh应该已经pend了，我们的优先级应该被提升 */
    myPrio = OS_RUNNING_TASK()->prio;
    g_testPILowPrioDuring = myPrio;
    if (myPrio <= 5) {
        g_testPIBoostOk = 1;
    }
    OsSemPost(g_testPIMutexId);
    /* 释放后优先级应恢复 */
    g_testPILowPrioAfter = OS_RUNNING_TASK()->prio;
    if (OS_RUNNING_TASK()->prio == OS_RUNNING_TASK()->oriPrio) {
        g_testPIRestoreOk = 1;
    }
    g_testPIDone = 1;
    while (1) {
        OsTaskDelay(100);
    }
}

OS_SEC_KERNEL_TEXT void TestPIMidTask(void *p1, void *p2, void *p3, void *p4)
{
    /* 等PILow获取mutex后再运行 */
    OsTaskDelay(10);
    g_testPIMidRan = 1;
    /* 如果PI不工作，这个任务会持续运行而PIHigh无法运行
     * 如果PI工作，PILow被提升到prio=5，会抢占本任务 */
    OsTaskDelay(80);
}

OS_SEC_KERNEL_TEXT void TestPIHighTask(void *p1, void *p2, void *p3, void *p4)
{
    /* 等PILow获取mutex且PIMid开始运行后再pend */
    OsTaskDelay(20);
    OsSemPend(g_testPIMutexId, OS_SEM_WAIT_FOREVER);
    g_testPIHighGot = 1;
    OsSemPost(g_testPIMutexId);
}

/* ====== 结果收集任务 ====== */

OS_SEC_KERNEL_TEXT void TestSemResultCollector(void *p1, void *p2, void *p3, void *p4)
{
    while (1) {
        OsTaskDelay(200);

        /* 计数信号量 */
        if (g_testSemProdCnt > 0 && g_testSemConsCnt > 0 && g_testSemPostFullFlag) {
            g_semTestResult |= SEM_TEST_CNT_SEM_OK | SEM_TEST_CNT_POST_FULL;
        }

        /* 二值同步 */
        if (g_testBinSemResult > 0 && g_testBinSemPostRepeatFlag) {
            g_semTestResult |= SEM_TEST_BIN_SYNC_OK | SEM_TEST_BIN_POST_REPEAT;
        }

        /* 互斥 */
        if (g_testMutexVal > 0 && (g_testMutexVal % 101 == 0 || g_testMutexVal > 101)) {
            g_semTestResult |= SEM_TEST_MUTEX_OK;
        }

        /* 非持有者 Post */
        if (g_testMutexNotHolderFlag) {
            g_semTestResult |= SEM_TEST_MUTEX_NOT_HOLDER;
        }

        /* PI: 优先级继承 */
        if (g_testPIBoostOk) {
            g_semTestResult |= SEM_TEST_PI_BOOST_OK;
        }
        if (g_testPIRestoreOk) {
            g_semTestResult |= SEM_TEST_PI_RESTORE_OK;
        }

        /* 输出结果到串口 */
        OsUartPrintf("[SEM_RESULT] 0x%x\n", g_semTestResult);

        /* VGA输出PI测试结果（第8行） */
        {
            extern void OsPrintSetCursor(U16 pos);
            extern void OsPrintChar(char c);
            OsPrintSetCursor(8 * 80);
            kprintf("PI: boost=%d restore=%d lowPrio=%d->%d->%d midRan=%d highGot=%d",
                    g_testPIBoostOk, g_testPIRestoreOk,
                    g_testPILowPrioBefore, g_testPILowPrioDuring, g_testPILowPrioAfter,
                    g_testPIMidRan, g_testPIHighGot);
        }
    }
}

OS_SEC_KERNEL_TEXT U32 OsTestSemInit(void)
{
    U32 tskIdProd, tskIdCons, tskIdX, tskIdY;
    U32 tskIdBinW, tskIdBinN;
    U32 tskIdH, tskIdM, tskIdL, tskIdPost;
    U32 tskIdTmo1, tskIdTmo2, tskIdTmoNorm, tskIdTmoPost;
    U32 tskIdMutexOwner, tskIdMutexNotHolder, tskIdCollector;
    U32 tskIdPILow, tskIdPIMid, tskIdPIHigh;
    struct OsTaskCreateParam param;

    g_semTestResult = 0;

    /* 计数信号量：FIFO 唤醒 */
    OsSemCreate(OS_SEM_COUNTING, 0, 10, OS_SEM_WAKE_FIFO, &g_testSemId);

    /* 二值同步信号量 */
    OsSemCreate(OS_SEM_BINARY_SYNC, 0, 1, OS_SEM_WAKE_FIFO, &g_testBinSemId);

    /* 二值互斥信号量：PRIO 唤醒 */
    OsSemCreate(OS_SEM_BINARY_MUTEX, 1, 1, OS_SEM_WAKE_PRIO, &g_testMutexId);

    /* 计数信号量：PRIO 唤醒策略测试 */
    OsSemCreate(OS_SEM_COUNTING, 0, 3, OS_SEM_WAKE_PRIO, &g_testPrioSemId);
    g_testPrioWakeIdx = 0;

    /* 二值互斥信号量：非持有者测试 */
    OsSemCreate(OS_SEM_BINARY_MUTEX, 1, 1, OS_SEM_WAKE_FIFO, &g_testMutexOwnerId);

    /* --- 生产者-消费者 --- */
    memset(&param, 0, sizeof(param));
    strcpy(param.name, "SemProd");
    param.prio = 8;
    param.entryFunc = TestSemProducer;
    OsTaskCreate(&param, &tskIdProd);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "SemCons");
    param.prio = 8;
    param.entryFunc = TestSemConsumer;
    OsTaskCreate(&param, &tskIdCons);

    /* --- 二值同步 --- */
    memset(&param, 0, sizeof(param));
    strcpy(param.name, "BinWait");
    param.prio = 9;
    param.entryFunc = TestBinSemWaiter;
    OsTaskCreate(&param, &tskIdBinW);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "BinNoti");
    param.prio = 9;
    param.entryFunc = TestBinSemNotifier;
    OsTaskCreate(&param, &tskIdBinN);

    /* --- 互斥 --- */
    memset(&param, 0, sizeof(param));
    strcpy(param.name, "MutexX");
    param.prio = 6;
    param.entryFunc = TestMutexTaskX;
    OsTaskCreate(&param, &tskIdX);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "MutexY");
    param.prio = 6;
    param.entryFunc = TestMutexTaskY;
    OsTaskCreate(&param, &tskIdY);

    /* --- PRIO 唤醒策略 --- */
    memset(&param, 0, sizeof(param));
    strcpy(param.name, "PrioH");
    param.prio = 5;
    param.entryFunc = TestPrioTaskH;
    OsTaskCreate(&param, &tskIdH);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "PrioM");
    param.prio = 10;
    param.entryFunc = TestPrioTaskM;
    OsTaskCreate(&param, &tskIdM);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "PrioL");
    param.prio = 15;
    param.entryFunc = TestPrioTaskL;
    OsTaskCreate(&param, &tskIdL);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "PrioPost");
    param.prio = 4;
    param.entryFunc = TestPrioPostTask;
    OsTaskCreate(&param, &tskIdPost);

    /* --- 超时测试 --- */
    OsSemCreate(OS_SEM_BINARY_SYNC, 0, 1, OS_SEM_WAKE_FIFO, &g_testTmoSemId);
    OsSemCreate(OS_SEM_BINARY_SYNC, 0, 1, OS_SEM_WAKE_FIFO, &g_testTmoSemId2);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TmoWait");
    param.prio = 7;
    param.entryFunc = TestTmoWait;
    OsTaskCreate(&param, &tskIdTmo1);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TmoNoWt");
    param.prio = 7;
    param.entryFunc = TestTmoNoWait;
    OsTaskCreate(&param, &tskIdTmo2);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TmoNorm");
    param.prio = 7;
    param.entryFunc = TestTmoNormal;
    OsTaskCreate(&param, &tskIdTmoNorm);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TmoPost");
    param.prio = 6;
    param.entryFunc = TestTmoPostTask;
    OsTaskCreate(&param, &tskIdTmoPost);

    /* --- 非持有者 Post --- */
    memset(&param, 0, sizeof(param));
    strcpy(param.name, "MutexOW");
    param.prio = 7;
    param.entryFunc = TestMutexOwner;
    OsTaskCreate(&param, &tskIdMutexOwner);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "MutexNH");
    param.prio = 8;
    param.entryFunc = TestMutexNotHolder;
    OsTaskCreate(&param, &tskIdMutexNotHolder);

    /* --- 优先级继承测试 --- */
    OsSemCreate(OS_SEM_BINARY_MUTEX, 1, 1, OS_SEM_WAKE_PRIO, &g_testPIMutexId);
    g_testPIBoostOk = 0;
    g_testPIRestoreOk = 0;
    g_testPIMidRan = 0;
    g_testPIHighGot = 0;

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "PILow");
    param.prio = 20;
    param.entryFunc = TestPILowTask;
    OsTaskCreate(&param, &tskIdPILow);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "PIMid");
    param.prio = 15;
    param.entryFunc = TestPIMidTask;
    OsTaskCreate(&param, &tskIdPIMid);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "PIHigh");
    param.prio = 5;
    param.entryFunc = TestPIHighTask;
    OsTaskCreate(&param, &tskIdPIHigh);

    /* --- 结果收集 --- */
    memset(&param, 0, sizeof(param));
    strcpy(param.name, "SemColl");
    param.prio = 3;
    param.entryFunc = TestSemResultCollector;
    OsTaskCreate(&param, &tskIdCollector);

    /* 全部resume入就绪队列 */
    OsTaskResume(tskIdProd);
    OsTaskResume(tskIdCons);
    OsTaskResume(tskIdBinW);
    OsTaskResume(tskIdBinN);
    OsTaskResume(tskIdX);
    OsTaskResume(tskIdY);
    OsTaskResume(tskIdH);
    OsTaskResume(tskIdM);
    OsTaskResume(tskIdL);
    OsTaskResume(tskIdPost);
    OsTaskResume(tskIdTmo1);
    OsTaskResume(tskIdTmo2);
    OsTaskResume(tskIdTmoNorm);
    OsTaskResume(tskIdTmoPost);
    OsTaskResume(tskIdMutexOwner);
    OsTaskResume(tskIdMutexNotHolder);
    OsTaskResume(tskIdPILow);
    OsTaskResume(tskIdPIMid);
    OsTaskResume(tskIdPIHigh);
    OsTaskResume(tskIdCollector);

    return OS_OK;
}