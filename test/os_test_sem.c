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

/* 共享缓冲区（生产者-消费者） */
OS_SEC_KERNEL_BSS volatile U32 g_testSemBuf;
OS_SEC_KERNEL_BSS U32 g_testSemId;  /* 计数信号量ID */

/* 互斥保护的共享资源 */
OS_SEC_KERNEL_BSS volatile U32 g_testMutexVal;      /* 被互斥保护的共享变量 */
OS_SEC_KERNEL_BSS U32 g_testMutexId;                /* 互斥信号量ID */
OS_SEC_KERNEL_BSS volatile U32 g_testNoMutexVal;     /* 无保护的对比变量 */

/* Producer: 每20 ticks生产一个数，写入共享缓冲区，Post信号量 */
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

/* Consumer: Pend信号量，读取共享缓冲区，显示消费的值 */
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

/*
 * 互斥测试：两个同优先级线程 X/Y 竞争对共享变量做 +1
 * X: Pend(mutex) → val+=100 → Post(mutex) → delay
 * Y: Pend(mutex) → val+=1   → Post(mutex) → delay
 * 如果互斥正确，每次 X+Y 一轮后 val 增量应精确
 */
OS_SEC_KERNEL_TEXT void TestMutexTaskX(void *p1, void *p2, void *p3, void *p4)
{
    TestPutChar(9, 0, 'X');
    TestPutChar(9, 1, ':');

    while (1) {
        OsSemPend(g_testMutexId);
        /* 临界区：对共享变量加100 */
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
        /* 临界区：对共享变量加1 */
        g_testMutexVal += 1;
        OsSemPost(g_testMutexId);
        TestPutHex(10, 3, g_testMutexVal);
        OsTaskDelay(20);
    }
}

OS_SEC_KERNEL_TEXT U32 OsTestSemInit(void)
{
    U32 tskIdProd, tskIdCons, tskIdX, tskIdY;
    struct OsTaskCreateParam param;
    struct OsTaskCb *tskCb;

    kprintf("[SEM] test init\n");

    /* 创建计数信号量，初始值0（缓冲区空），最大值10 */
    OsSemCreate(0, 10, &g_testSemId);

    /* 创建互斥信号量，初始值1（资源可用），最大值1 */
    OsSemCreate(1, 1, &g_testMutexId);

    /* 创建 Producer 线程 */
    memset(&param, 0, sizeof(param));
    strcpy(param.name, "SemProd");
    param.prio = 8;
    param.entryFunc = TestSemProducer;
    OsTaskCreate(&param, &tskIdProd);

    /* 创建 Consumer 线程 */
    memset(&param, 0, sizeof(param));
    strcpy(param.name, "SemCons");
    param.prio = 8;
    param.entryFunc = TestSemConsumer;
    OsTaskCreate(&param, &tskIdCons);

    /* 创建互斥测试线程 X */
    memset(&param, 0, sizeof(param));
    strcpy(param.name, "MutexX");
    param.prio = 6;
    param.entryFunc = TestMutexTaskX;
    OsTaskCreate(&param, &tskIdX);

    /* 创建互斥测试线程 Y */
    memset(&param, 0, sizeof(param));
    strcpy(param.name, "MutexY");
    param.prio = 6;
    param.entryFunc = TestMutexTaskY;
    OsTaskCreate(&param, &tskIdY);

    /* 入就绪队列 */
    tskCb = OS_TASK_GET_CB(tskIdProd);
    OsSchedRdyListEnqueTsk(tskCb);
    tskCb = OS_TASK_GET_CB(tskIdCons);
    OsSchedRdyListEnqueTsk(tskCb);
    tskCb = OS_TASK_GET_CB(tskIdX);
    OsSchedRdyListEnqueTsk(tskCb);
    tskCb = OS_TASK_GET_CB(tskIdY);
    OsSchedRdyListEnqueTsk(tskCb);

    return OS_OK;
}
