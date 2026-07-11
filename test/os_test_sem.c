#include "os_def.h"
#include "os_print_external.h"
#include "os_task_external.h"
#include "os_sched_external.h"
#include "os_sem_external.h"
#include "os_test_framework.h"
#include "string.h"
#include "os_uart_external.h"

/* ====== 辅助 ====== */

static OS_SEC_KERNEL_TEXT U32 TestCreateTask(const char *name, U32 prio, OsTaskEntryFunc entry)
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

/* 删除任务（不关心返回值，任务可能已自行退出或暂时持有 mutex） */
static OS_SEC_KERNEL_TEXT void TestCleanupTask(U32 tskId)
{
    U32 ret;
    U32 retry = 0;
    struct OsTaskCb *tskCb = OS_TASK_GET_CB(tskId);
    if (!(tskCb->status & OS_TASK_STATUS_USED)) {
        return;  /* 任务已自行退出 */
    }
    OsTaskSuspend(tskId);
    /* 如果任务持有 mutex，resume 让它释放后再试 */
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

/* ====== 计数信号量（FIFO 唤醒） ====== */

OS_SEC_KERNEL_BSS U32 g_testSemId;
OS_SEC_KERNEL_BSS volatile U32 g_testSemProdCnt;
OS_SEC_KERNEL_BSS volatile U32 g_testSemConsCnt;
OS_SEC_KERNEL_BSS volatile U32 g_testSemPostFullFlag;
OS_SEC_KERNEL_BSS U32 g_testSemCntTskProd;
OS_SEC_KERNEL_BSS U32 g_testSemCntTskCons;

OS_SEC_KERNEL_TEXT void TestSemProducer(void *p1, void *p2, void *p3, void *p4)
{
    U32 ret;
    while (1) {
        g_testSemProdCnt++;
        ret = OsSemPost(g_testSemId);
        if (ret == OS_SEM_POST_IS_FULL) {
            g_testSemPostFullFlag = 1;
        }
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

OS_SEC_KERNEL_TEXT void TestSemCntSetup(void)
{
    OsSemCreate(OS_SEM_COUNTING, 0, 10, OS_SEM_WAKE_FIFO, &g_testSemId);
    g_testSemProdCnt = 0;
    g_testSemConsCnt = 0;
    g_testSemPostFullFlag = 0;
    g_testSemCntTskProd = TestCreateTask("SemProd", 8, TestSemProducer);
    g_testSemCntTskCons = TestCreateTask("SemCons", 8, TestSemConsumer);
    OsTaskResume(g_testSemCntTskProd);
    OsTaskResume(g_testSemCntTskCons);
}

OS_SEC_KERNEL_TEXT void TestSemCntVerify(void)
{
    OS_TEST_ASSERT(g_testSemProdCnt > 0);
    OS_TEST_ASSERT(g_testSemConsCnt > 0);
    /* 清理：计数信号量不持有，可直接删任务 */
    TestCleanupTask(g_testSemCntTskProd);
    TestCleanupTask(g_testSemCntTskCons);
    OsSemDelete(g_testSemId);
}

/* ====== 二值同步信号量 ====== */

OS_SEC_KERNEL_BSS U32 g_testBinSemId;
OS_SEC_KERNEL_BSS volatile U32 g_testBinSemResult;
OS_SEC_KERNEL_BSS volatile U32 g_testBinSemPostRepeatFlag;
OS_SEC_KERNEL_BSS U32 g_testBinTskWaiter;
OS_SEC_KERNEL_BSS U32 g_testBinTskNotifier;

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
            g_testBinSemPostRepeatFlag = 1;
        }
        OsTaskDelay(10);
    }
}

OS_SEC_KERNEL_TEXT void TestSemBinSyncSetup(void)
{
    OsSemCreate(OS_SEM_BINARY_SYNC, 0, 1, OS_SEM_WAKE_FIFO, &g_testBinSemId);
    g_testBinSemResult = 0;
    g_testBinSemPostRepeatFlag = 0;
    g_testBinTskWaiter = TestCreateTask("BinWait", 9, TestBinSemWaiter);
    g_testBinTskNotifier = TestCreateTask("BinNoti", 9, TestBinSemNotifier);
    OsTaskResume(g_testBinTskWaiter);
    OsTaskResume(g_testBinTskNotifier);
}

OS_SEC_KERNEL_TEXT void TestSemBinSyncVerify(void)
{
    OS_TEST_ASSERT(g_testBinSemResult > 0);
    OS_TEST_ASSERT(g_testBinSemPostRepeatFlag == 1);
    TestCleanupTask(g_testBinTskWaiter);
    TestCleanupTask(g_testBinTskNotifier);
    OsSemDelete(g_testBinSemId);
}

/* ====== 互斥信号量（PRIO 唤醒） ====== */

OS_SEC_KERNEL_BSS volatile U32 g_testMutexVal;
OS_SEC_KERNEL_BSS U32 g_testMutexId;
OS_SEC_KERNEL_BSS U32 g_testMutexTskX;
OS_SEC_KERNEL_BSS U32 g_testMutexTskY;

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

OS_SEC_KERNEL_TEXT void TestSemMutexSetup(void)
{
    OsSemCreate(OS_SEM_BINARY_MUTEX, 1, 1, OS_SEM_WAKE_PRIO, &g_testMutexId);
    g_testMutexVal = 0;
    g_testMutexTskX = TestCreateTask("MutexX", 6, TestMutexTaskX);
    g_testMutexTskY = TestCreateTask("MutexY", 6, TestMutexTaskY);
    OsTaskResume(g_testMutexTskX);
    OsTaskResume(g_testMutexTskY);
}

OS_SEC_KERNEL_TEXT void TestSemMutexVerify(void)
{
    OS_TEST_ASSERT(g_testMutexVal > 0);
    OS_TEST_ASSERT((g_testMutexVal % 101 == 0) || (g_testMutexVal > 101));
    /* mutex 场景：任务在 post 后 delay 时不持有 mutex，suspend 会停在不持有窗口 */
    TestCleanupTask(g_testMutexTskX);
    TestCleanupTask(g_testMutexTskY);
    OsSemDelete(g_testMutexId);
}

/* ====== PRIO 唤醒策略 ====== */

OS_SEC_KERNEL_BSS U32 g_testPrioSemId;
OS_SEC_KERNEL_BSS volatile U32 g_testPrioWakeOrder[3];
OS_SEC_KERNEL_BSS volatile U32 g_testPrioWakeIdx;
OS_SEC_KERNEL_BSS U32 g_testPrioTskH;
OS_SEC_KERNEL_BSS U32 g_testPrioTskM;
OS_SEC_KERNEL_BSS U32 g_testPrioTskL;
OS_SEC_KERNEL_BSS U32 g_testPrioTskPost;

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
}

OS_SEC_KERNEL_TEXT void TestSemPrioSetup(void)
{
    OsSemCreate(OS_SEM_COUNTING, 0, 3, OS_SEM_WAKE_PRIO, &g_testPrioSemId);
    g_testPrioWakeIdx = 0;
    g_testPrioTskH = TestCreateTask("PrioH", 5, TestPrioTaskH);
    g_testPrioTskM = TestCreateTask("PrioM", 10, TestPrioTaskM);
    g_testPrioTskL = TestCreateTask("PrioL", 15, TestPrioTaskL);
    g_testPrioTskPost = TestCreateTask("PrioPost", 4, TestPrioPostTask);
    OsTaskResume(g_testPrioTskH);
    OsTaskResume(g_testPrioTskM);
    OsTaskResume(g_testPrioTskL);
    OsTaskResume(g_testPrioTskPost);
}

OS_SEC_KERNEL_TEXT void TestSemPrioVerify(void)
{
    OS_TEST_ASSERT_EQ(g_testPrioWakeOrder[0], 5);
    OS_TEST_ASSERT_EQ(g_testPrioWakeOrder[1], 10);
    OS_TEST_ASSERT_EQ(g_testPrioWakeOrder[2], 15);
    /* H/M/L 已执行完自行退出（函数返回），PostTask 也退出了 */
    TestCleanupTask(g_testPrioTskH);
    TestCleanupTask(g_testPrioTskM);
    TestCleanupTask(g_testPrioTskL);
    TestCleanupTask(g_testPrioTskPost);
    OsSemDelete(g_testPrioSemId);
}

/* ====== 超时测试 ====== */

OS_SEC_KERNEL_BSS U32 g_testTmoSemId;
OS_SEC_KERNEL_BSS U32 g_testTmoSemId2;
OS_SEC_KERNEL_BSS U32 g_testTmoTskWait;
OS_SEC_KERNEL_BSS U32 g_testTmoTskNoWait;
OS_SEC_KERNEL_BSS U32 g_testTmoTskNorm;
OS_SEC_KERNEL_BSS U32 g_testTmoTskPost;

OS_SEC_KERNEL_TEXT void TestTmoWait(void *p1, void *p2, void *p3, void *p4)
{
    U32 ret = OsSemPend(g_testTmoSemId, 50);
    OS_TEST_ASSERT(ret == OS_SEM_PEND_TIMEOUT);
}

OS_SEC_KERNEL_TEXT void TestTmoNoWait(void *p1, void *p2, void *p3, void *p4)
{
    U32 ret = OsSemPend(g_testTmoSemId, OS_SEM_NO_WAIT);
    OS_TEST_ASSERT(ret == OS_SEM_PEND_UNAVAILABLE);
}

OS_SEC_KERNEL_TEXT void TestTmoNormal(void *p1, void *p2, void *p3, void *p4)
{
    U32 ret = OsSemPend(g_testTmoSemId2, 100);
    OS_TEST_ASSERT(ret == OS_OK);
}

OS_SEC_KERNEL_TEXT void TestTmoPostTask(void *p1, void *p2, void *p3, void *p4)
{
    OsTaskDelay(80);
    OsSemPost(g_testTmoSemId2);
}

OS_SEC_KERNEL_TEXT void TestSemTmoSetup(void)
{
    OsSemCreate(OS_SEM_BINARY_SYNC, 0, 1, OS_SEM_WAKE_FIFO, &g_testTmoSemId);
    OsSemCreate(OS_SEM_BINARY_SYNC, 0, 1, OS_SEM_WAKE_FIFO, &g_testTmoSemId2);
    g_testTmoTskWait = TestCreateTask("TmoWait", 7, TestTmoWait);
    g_testTmoTskNoWait = TestCreateTask("TmoNoWt", 7, TestTmoNoWait);
    g_testTmoTskNorm = TestCreateTask("TmoNorm", 7, TestTmoNormal);
    g_testTmoTskPost = TestCreateTask("TmoPost", 6, TestTmoPostTask);
    OsTaskResume(g_testTmoTskWait);
    OsTaskResume(g_testTmoTskNoWait);
    OsTaskResume(g_testTmoTskNorm);
    OsTaskResume(g_testTmoTskPost);
}

OS_SEC_KERNEL_TEXT void TestSemTmoVerify(void)
{
    OS_TEST_ASSERT(1);
    TestCleanupTask(g_testTmoTskWait);
    TestCleanupTask(g_testTmoTskNoWait);
    TestCleanupTask(g_testTmoTskNorm);
    TestCleanupTask(g_testTmoTskPost);
    OsSemDelete(g_testTmoSemId);
    OsSemDelete(g_testTmoSemId2);
}

/* ====== 非持有者 Post BINARY_MUTEX ====== */

OS_SEC_KERNEL_BSS U32 g_testMutexOwnerId;
OS_SEC_KERNEL_BSS volatile U32 g_testMutexNotHolderFlag;
OS_SEC_KERNEL_BSS U32 g_testNhTskOwner;
OS_SEC_KERNEL_BSS U32 g_testNhTskNH;

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

OS_SEC_KERNEL_TEXT void TestSemNotHolderSetup(void)
{
    OsSemCreate(OS_SEM_BINARY_MUTEX, 1, 1, OS_SEM_WAKE_FIFO, &g_testMutexOwnerId);
    g_testMutexNotHolderFlag = 0;
    g_testNhTskOwner = TestCreateTask("MutexOW", 7, TestMutexOwner);
    g_testNhTskNH = TestCreateTask("MutexNH", 8, TestMutexNotHolder);
    OsTaskResume(g_testNhTskOwner);
    OsTaskResume(g_testNhTskNH);
}

OS_SEC_KERNEL_TEXT void TestSemNotHolderVerify(void)
{
    OS_TEST_ASSERT(g_testMutexNotHolderFlag == 1);
    TestCleanupTask(g_testNhTskOwner);
    TestCleanupTask(g_testNhTskNH);
    OsSemDelete(g_testMutexOwnerId);
}

/* ====== 优先级继承 ====== */

OS_SEC_KERNEL_BSS U32 g_testPIMutexId;
OS_SEC_KERNEL_BSS volatile U32 g_testPIBoostOk;
OS_SEC_KERNEL_BSS volatile U32 g_testPIRestoreOk;
OS_SEC_KERNEL_BSS volatile U32 g_testPIMidRan;
OS_SEC_KERNEL_BSS volatile U32 g_testPIHighGot;
OS_SEC_KERNEL_BSS volatile U32 g_testPILowPrioBefore;
OS_SEC_KERNEL_BSS volatile U32 g_testPILowPrioDuring;
OS_SEC_KERNEL_BSS volatile U32 g_testPILowPrioAfter;
OS_SEC_KERNEL_BSS U32 g_testPiTskLow;
OS_SEC_KERNEL_BSS U32 g_testPiTskMid;
OS_SEC_KERNEL_BSS U32 g_testPiTskHigh;

OS_SEC_KERNEL_TEXT void TestPILowTask(void *p1, void *p2, void *p3, void *p4)
{
    U32 myPrio;
    OsSemPend(g_testPIMutexId, OS_SEM_WAIT_FOREVER);
    g_testPILowPrioBefore = OS_RUNNING_TASK()->prio;
    OsTaskDelay(60);
    myPrio = OS_RUNNING_TASK()->prio;
    g_testPILowPrioDuring = myPrio;
    if (myPrio <= 5) {
        g_testPIBoostOk = 1;
    }
    OsSemPost(g_testPIMutexId);
    g_testPILowPrioAfter = OS_RUNNING_TASK()->prio;
    if (OS_RUNNING_TASK()->prio == OS_RUNNING_TASK()->oriPrio) {
        g_testPIRestoreOk = 1;
    }
    while (1) {
        OsTaskDelay(100);
    }
}

OS_SEC_KERNEL_TEXT void TestPIMidTask(void *p1, void *p2, void *p3, void *p4)
{
    OsTaskDelay(10);
    g_testPIMidRan = 1;
    OsTaskDelay(80);
}

OS_SEC_KERNEL_TEXT void TestPIHighTask(void *p1, void *p2, void *p3, void *p4)
{
    OsTaskDelay(20);
    OsSemPend(g_testPIMutexId, OS_SEM_WAIT_FOREVER);
    g_testPIHighGot = 1;
    OsSemPost(g_testPIMutexId);
}

OS_SEC_KERNEL_TEXT void TestSemPISetup(void)
{
    OsSemCreate(OS_SEM_BINARY_MUTEX, 1, 1, OS_SEM_WAKE_PRIO, &g_testPIMutexId);
    g_testPIBoostOk = 0;
    g_testPIRestoreOk = 0;
    g_testPIMidRan = 0;
    g_testPIHighGot = 0;
    g_testPiTskLow = TestCreateTask("PILow", 20, TestPILowTask);
    g_testPiTskMid = TestCreateTask("PIMid", 15, TestPIMidTask);
    g_testPiTskHigh = TestCreateTask("PIHigh", 5, TestPIHighTask);
    OsTaskResume(g_testPiTskLow);
    OsTaskResume(g_testPiTskMid);
    OsTaskResume(g_testPiTskHigh);
}

OS_SEC_KERNEL_TEXT void TestSemPIVerify(void)
{
    OS_TEST_ASSERT(g_testPIBoostOk == 1);
    OS_TEST_ASSERT(g_testPIRestoreOk == 1);
    OS_TEST_ASSERT(g_testPIMidRan == 1);
    OS_TEST_ASSERT(g_testPIHighGot == 1);

    /* VGA Row 8: PI 详细信息 */
    OsPrintSetCursor((U16)(8 * 80));
    kprintf("PI: boost=%d restore=%d lowPrio=%d->%d->%d midRan=%d highGot=%d",
            g_testPIBoostOk, g_testPIRestoreOk,
            g_testPILowPrioBefore, g_testPILowPrioDuring, g_testPILowPrioAfter,
            g_testPIMidRan, g_testPIHighGot);

    TestCleanupTask(g_testPiTskLow);
    TestCleanupTask(g_testPiTskMid);
    TestCleanupTask(g_testPiTskHigh);
    OsSemDelete(g_testPIMutexId);
}

/* ====== SUSPENDED 任务在 pendList 上的唤醒 ====== */

OS_SEC_KERNEL_BSS U32 g_testSusSemId;
OS_SEC_KERNEL_BSS volatile U32 g_testSusSuspendedGot;
OS_SEC_KERNEL_BSS volatile U32 g_testSusNormalGot;
OS_SEC_KERNEL_BSS volatile U32 g_testSusSuspendedTskId;
OS_SEC_KERNEL_BSS volatile U32 g_testSusPostDone;
OS_SEC_KERNEL_BSS U32 g_testSusTskW1;
OS_SEC_KERNEL_BSS U32 g_testSusTskW2;
OS_SEC_KERNEL_BSS U32 g_testSusTskPost;

OS_SEC_KERNEL_TEXT void TestSusWaiterSuspended(void *p1, void *p2, void *p3, void *p4)
{
    OsSemPend(g_testSusSemId, OS_SEM_WAIT_FOREVER);
    g_testSusSuspendedGot = 1;
}

OS_SEC_KERNEL_TEXT void TestSusWaiterNormal(void *p1, void *p2, void *p3, void *p4)
{
    OsSemPend(g_testSusSemId, OS_SEM_WAIT_FOREVER);
    g_testSusNormalGot = 1;
}

OS_SEC_KERNEL_TEXT void TestSusPostTask(void *p1, void *p2, void *p3, void *p4)
{
    OsTaskDelay(10);
    OsTaskSuspend(g_testSusSuspendedTskId);
    OsTaskDelay(10);
    OsSemPost(g_testSusSemId);
    OsTaskDelay(10);
    g_testSusPostDone = 1;
    while (1) {
        OsTaskDelay(100);
    }
}

OS_SEC_KERNEL_TEXT void TestSemSusSetup(void)
{
    OsSemCreate(OS_SEM_BINARY_SYNC, 0, 1, OS_SEM_WAKE_FIFO, &g_testSusSemId);
    g_testSusSuspendedGot = 0;
    g_testSusNormalGot = 0;
    g_testSusPostDone = 0;
    g_testSusTskW1 = TestCreateTask("SusW1", 12, TestSusWaiterSuspended);
    g_testSusTskW2 = TestCreateTask("SusW2", 12, TestSusWaiterNormal);
    g_testSusTskPost = TestCreateTask("SusPost", 11, TestSusPostTask);
    g_testSusSuspendedTskId = g_testSusTskW1;
    OsTaskResume(g_testSusTskW1);
    OsTaskResume(g_testSusTskW2);
    OsTaskResume(g_testSusTskPost);
}

OS_SEC_KERNEL_TEXT void TestSemSusVerify(void)
{
    OS_TEST_ASSERT(g_testSusPostDone == 1);
    OS_TEST_ASSERT(g_testSusNormalGot == 1);
    OS_TEST_ASSERT(g_testSusSuspendedGot == 0);
    TestCleanupTask(g_testSusTskW1);
    TestCleanupTask(g_testSusTskW2);
    TestCleanupTask(g_testSusTskPost);
    OsSemDelete(g_testSusSemId);
}

/* ====== OsSemDelete 测试 ====== */

OS_SEC_KERNEL_TEXT void TestSemDeleteSetup(void)
{
    /* 纯同步测试，无需 setup */
}

OS_SEC_KERNEL_TEXT void TestSemDeleteVerify(void)
{
    U32 semId1, semId2;
    U32 ret;

    /* 1. 创建计数信号量，直接删除（无 pender/holder） */
    OsSemCreate(OS_SEM_COUNTING, 0, 10, OS_SEM_WAKE_FIFO, &semId1);
    ret = OsSemDelete(semId1);
    OS_TEST_ASSERT_EQ(ret, OS_OK);

    /* 2. 删除后 ID 应可复用：再创建一个，应该拿到同一个 ID */
    OsSemCreate(OS_SEM_COUNTING, 0, 10, OS_SEM_WAKE_FIFO, &semId2);
    OS_TEST_ASSERT_EQ(semId2, semId1);

    /* 3. 清理 */
    OsSemDelete(semId2);

    /* 4. BINARY_MUTEX 被持有时不能删除 */
    U32 mtxId;
    OsSemCreate(OS_SEM_BINARY_MUTEX, 1, 1, OS_SEM_WAKE_PRIO, &mtxId);
    /* pend 立即获取（val=1） */
    OsSemPend(mtxId, OS_SEM_NO_WAIT);
    /* 此时 holder=当前任务，删除应被拒绝 */
    ret = OsSemDelete(mtxId);
    OS_TEST_ASSERT_EQ(ret, OS_SEM_DELETE_HAS_HOLDER);
    /* 释放后再删除应成功 */
    OsSemPost(mtxId);
    ret = OsSemDelete(mtxId);
    OS_TEST_ASSERT_EQ(ret, OS_OK);
}
