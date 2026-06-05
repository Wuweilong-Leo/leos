#include "os_def.h"
#include "os_print_external.h"
#include "os_task_external.h"
#include "os_sched_external.h"
#include "os_sem_external.h"
#include "os_test.h"
#include "string.h"

/* 通过打印模块在指定行列写字符 */
static OS_SEC_KERNEL_TEXT void TestPutChar(int row, int col, char c)
{
    OsPrintSetCursor((U16)(row * 80 + col));
    OsPrintChar(c);
}

/* 在指定行列显示十六进制数 */
static OS_SEC_KERNEL_TEXT void TestPutHex(int row, int col, U32 val)
{
    int i;
    for (i = 7; i >= 0; i--) {
        U32 nibble = (val >> (i * 4)) & 0xF;
        TestPutChar(row, col + (7 - i), "0123456789ABCDEF"[nibble]);
    }
}

/* ====== 生产者-消费者测试（FIFO 唤醒） ====== */

OS_SEC_KERNEL_BSS volatile U32 g_testSemBuf;
OS_SEC_KERNEL_BSS U32 g_testSemId;

OS_SEC_KERNEL_TEXT void TestSemProducer(void *p1, void *p2, void *p3, void *p4)
{
    U32 count = 0;
    TestPutChar(6, 0, 'P');
    TestPutChar(6, 1, ':');

    while (1) {
        g_testSemBuf = count;
        TestPutHex(6, 3, count);
        OsSemPost(g_testSemId);
        count++;
        OsTaskDelay(20);
    }
}

OS_SEC_KERNEL_TEXT void TestSemConsumer(void *p1, void *p2, void *p3, void *p4)
{
    U32 val;
    U32 count = 0;
    TestPutChar(7, 0, 'C');
    TestPutChar(7, 1, ':');

    while (1) {
        OsSemPend(g_testSemId);
        val = g_testSemBuf;
        TestPutHex(7, 3, val);
        count++;
        OsTaskDelay(30);
    }
}

/* ====== 互斥测试（PRIO 唤醒） ====== */

OS_SEC_KERNEL_BSS volatile U32 g_testMutexVal;
OS_SEC_KERNEL_BSS U32 g_testMutexId;

OS_SEC_KERNEL_TEXT void TestMutexTaskX(void *p1, void *p2, void *p3, void *p4)
{
    TestPutChar(9, 0, 'X');
    TestPutChar(9, 1, ':');

    while (1) {
        OsSemPend(g_testMutexId);
        g_testMutexVal += 100;
        OsSemPost(g_testMutexId);
        TestPutHex(9, 3, g_testMutexVal);
        OsTaskDelay(20);
    }
}

OS_SEC_KERNEL_TEXT void TestMutexTaskY(void *p1, void *p2, void *p3, void *p4)
{
    TestPutChar(10, 0, 'Y');
    TestPutChar(10, 1, ':');

    while (1) {
        OsSemPend(g_testMutexId);
        g_testMutexVal += 1;
        OsSemPost(g_testMutexId);
        TestPutHex(10, 3, g_testMutexVal);
        OsTaskDelay(20);
    }
}

/* ====== PRIO 唤醒策略测试 ====== */
/*
 * 三个不同优先级任务 H/M/L 同时 Pend 同一个信号量
 * Post 3 次后，验证唤醒顺序：H(5) → M(10) → L(15)
 * 用 g_testPrioWakeOrder 记录唤醒顺序，每个任务写入自己的优先级
 */
OS_SEC_KERNEL_BSS U32 g_testPrioSemId;
OS_SEC_KERNEL_BSS volatile U32 g_testPrioWakeOrder[3];
OS_SEC_KERNEL_BSS volatile U32 g_testPrioWakeIdx;

OS_SEC_KERNEL_TEXT void TestPrioTaskH(void *p1, void *p2, void *p3, void *p4)
{
    OsSemPend(g_testPrioSemId);
    /* 被唤醒，记录顺序 */
    g_testPrioWakeOrder[g_testPrioWakeIdx++] = 5;
    TestPutChar(12, 0, 'H');
    TestPutChar(12, 1, 'O');  /* H=High, first Out */
}

OS_SEC_KERNEL_TEXT void TestPrioTaskM(void *p1, void *p2, void *p3, void *p4)
{
    OsSemPend(g_testPrioSemId);
    g_testPrioWakeOrder[g_testPrioWakeIdx++] = 10;
    TestPutChar(12, 3, 'M');
    TestPutChar(12, 4, 'O');
}

OS_SEC_KERNEL_TEXT void TestPrioTaskL(void *p1, void *p2, void *p3, void *p4)
{
    OsSemPend(g_testPrioSemId);
    g_testPrioWakeOrder[g_testPrioWakeIdx++] = 15;
    TestPutChar(12, 6, 'L');
    TestPutChar(12, 7, 'O');
}

/* Post 任务：确保 H/M/L 都在 pend 后再 Post 3 次 */
OS_SEC_KERNEL_TEXT void TestPrioPostTask(void *p1, void *p2, void *p3, void *p4)
{
    U32 i;
    U32 order;
    TestPutChar(12, 9, ':');

    /* 等足够久让 H/M/L 都进入 pend 状态 */
    OsTaskDelay(60);

    /* 连续 Post 3 次 */
    for (i = 0; i < 3; i++) {
        OsSemPost(g_testPrioSemId);
    }

    /* 等唤醒完成，显示唤醒顺序 */
    OsTaskDelay(20);
    order = (g_testPrioWakeOrder[0] << 16) | (g_testPrioWakeOrder[1] << 8) | g_testPrioWakeOrder[2];
    TestPutHex(13, 11, order);
}

OS_SEC_KERNEL_TEXT U32 OsTestSemInit(void)
{
    U32 tskIdProd, tskIdCons, tskIdX, tskIdY;
    U32 tskIdH, tskIdM, tskIdL, tskIdPost;
    struct OsTaskCreateParam param;
    struct OsTaskCb *tskCb;

    kprintf("[SEM] test init\n");

    /* 计数信号量：FIFO 唤醒（生产者-消费者） */
    OsSemCreate(0, 10, OS_SEM_WAKE_FIFO, &g_testSemId);

    /* 互斥信号量：PRIO 唤醒 */
    OsSemCreate(1, 1, OS_SEM_WAKE_PRIO, &g_testMutexId);

    /* PRIO 唤醒策略测试信号量：初始0，PRIO 策略 */
    OsSemCreate(0, 3, OS_SEM_WAKE_PRIO, &g_testPrioSemId);
    g_testPrioWakeIdx = 0;

    /* --- 生产者-消费者线程 --- */
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

    /* --- 互斥测试线程 --- */
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

    /* --- PRIO 唤醒策略测试线程 --- */
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
    param.prio = 4;   /* 比 H 还高，确保 Post 任务先调度 */
    param.entryFunc = TestPrioPostTask;
    OsTaskCreate(&param, &tskIdPost);

    /* 入就绪队列 */
    tskCb = OS_TASK_GET_CB(tskIdProd);
    OsSchedRdyListEnqueTsk(tskCb);
    tskCb = OS_TASK_GET_CB(tskIdCons);
    OsSchedRdyListEnqueTsk(tskCb);
    tskCb = OS_TASK_GET_CB(tskIdX);
    OsSchedRdyListEnqueTsk(tskCb);
    tskCb = OS_TASK_GET_CB(tskIdY);
    OsSchedRdyListEnqueTsk(tskCb);
    tskCb = OS_TASK_GET_CB(tskIdH);
    OsSchedRdyListEnqueTsk(tskCb);
    tskCb = OS_TASK_GET_CB(tskIdM);
    OsSchedRdyListEnqueTsk(tskCb);
    tskCb = OS_TASK_GET_CB(tskIdL);
    OsSchedRdyListEnqueTsk(tskCb);
    tskCb = OS_TASK_GET_CB(tskIdPost);
    OsSchedRdyListEnqueTsk(tskCb);

    return OS_OK;
}