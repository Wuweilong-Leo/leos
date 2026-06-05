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

/* 共享缓冲区 */
OS_SEC_KERNEL_BSS volatile U32 g_testSemBuf;
OS_SEC_KERNEL_BSS U32 g_testSemId;  /* 信号量ID */

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

OS_SEC_KERNEL_TEXT U32 OsTestSemInit(void)
{
    U32 tskIdProd, tskIdCons;
    struct OsTaskCreateParam param;
    struct OsTaskCb *tskCb;

    kprintf("[SEM] test init\n");

    /* 创建信号量，初始值0（缓冲区空） */
    OsSemCreate(0, 10, &g_testSemId);

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

    /* 入就绪队列 */
    tskCb = OS_TASK_GET_CB(tskIdProd);
    OsSchedRdyListEnqueTsk(tskCb);
    tskCb = OS_TASK_GET_CB(tskIdCons);
    OsSchedRdyListEnqueTsk(tskCb);

    return OS_OK;
}
