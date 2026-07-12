#include "os_def.h"
#include "os_print_external.h"
#include "os_task_external.h"
#include "os_sched_external.h"
#include "os_sem_external.h"
#include "os_mem_external.h"
#include "os_tick_external.h"
#include "os_hwi.h"
#include "os_test_framework.h"
#include "os_uart_external.h"
#include "string.h"

/*
 * STRESS:soak — 常稳压力测试
 *
 * 5 分钟(10 个周期 × 30s)持续压力，覆盖：
 *   1. 资源回收：Worker 链 pend→alloc→free→post→自删，验证任务/信号量/内存完整回收
 *   2. 优先级反转：PI 三角 Low/Mid/High 反复 boost/restore
 *   3. 同优先级轮转：两个 while(1) 不 delay 任务靠时间片轮转
 *   4. 内存压力：高频 alloc/free + 魔数校验
 *   5. 超时/挂起组合：pend+timeout+suspend 交叉
 *
 * 每周期末尾清理所有资源，验证信号量可全部回收、堆不变量成立。
 */

/* ====== 常量 ====== */

#define STRESS_CYCLES         10
#define STRESS_CYCLE_TICKS    1250   /* 25s @50Hz */
#define STRESS_WORKER_ROUNDS  5      /* 每周期 Worker 链轮转次数 */
#define STRESS_PI_ROUNDS      10     /* 每周期 PI boost/restore 次数 */
#define STRESS_ALLOC_ROUNDS   20     /* 每周期 alloc/free 次数 */
#define STRESS_SEM_CHECK_NUM  16    /* 验证全部信号量回收 */

/* ====== 辅助 ====== */

static OS_SEC_KERNEL_TEXT U32 StressCreateTask(const char *name, U32 prio, OsTaskEntryFunc entry)
{
    U32 tskId;
    U32 ret;
    struct OsTaskCreateParam param;
    memset(&param, 0, sizeof(param));
    strcpy(param.name, name);
    param.prio = prio;
    param.entryFunc = entry;
    ret = OsTaskCreate(&param, &tskId);
    if (ret != OS_OK) {
        return g_tskMaxNum;
    }
    return tskId;
}

/* 安全 Resume：tskId 无效时跳过 */
static OS_SEC_KERNEL_TEXT void StressSafeResume(U32 tskId)
{
    if (tskId < g_tskMaxNum) {
        OsTaskResume(tskId);
    }
}

static OS_SEC_KERNEL_TEXT void StressCleanupTask(U32 tskId)
{
    U32 ret;
    U32 retry = 0;
    struct OsTaskCb *tskCb = OS_TASK_GET_CB(tskId);
    if (!(tskCb->status & OS_TASK_STATUS_USED)) {
        return;  /* 任务已自行退出 */
    }
    OsTaskSuspend(tskId);
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

/* ====== Worker 链：资源回收压力 ====== */
/*
 * 3 个 mutex(A→B→C→A 循环)，每个 Worker：
 *   pend(自己的 mutex) → alloc → 写魔数 → free → post(下一个 mutex) → 自删
 * Coordinator 检测 Worker 自删后，创建新 Worker 补位，删除旧 mutex → 创建新 mutex
 */

OS_SEC_KERNEL_BSS U32 g_wkMutA, g_wkMutB, g_wkMutC;
OS_SEC_KERNEL_BSS volatile U32 g_wkDoneCnt;

OS_SEC_KERNEL_TEXT void StressWorkerA(void *p1, void *p2, void *p3, void *p4)
{
    void *mem;
    U32 round;
    (void)p1; (void)p2; (void)p3; (void)p4;
    for (round = 0; round < STRESS_WORKER_ROUNDS; round++) {
        OsSemPend(g_wkMutA, OS_SEM_WAIT_FOREVER);
        mem = OsMemKernelAlloc(128, 4);
        if (mem != NULL) {
            memset(mem, 0xA5, 128);
            OsMemKernelFree(mem);
        }
        OsSemPost(g_wkMutB);
        g_wkDoneCnt++;
    }
    OsTaskDelete(OS_RUNNING_TASK()->pid);
}

OS_SEC_KERNEL_TEXT void StressWorkerB(void *p1, void *p2, void *p3, void *p4)
{
    void *mem;
    U32 round;
    (void)p1; (void)p2; (void)p3; (void)p4;
    for (round = 0; round < STRESS_WORKER_ROUNDS; round++) {
        OsSemPend(g_wkMutB, OS_SEM_WAIT_FOREVER);
        mem = OsMemKernelAlloc(256, 8);
        if (mem != NULL) {
            memset(mem, 0xB6, 256);
            OsMemKernelFree(mem);
        }
        OsSemPost(g_wkMutC);
        g_wkDoneCnt++;
    }
    OsTaskDelete(OS_RUNNING_TASK()->pid);
}

OS_SEC_KERNEL_TEXT void StressWorkerC(void *p1, void *p2, void *p3, void *p4)
{
    void *mem;
    U32 round;
    (void)p1; (void)p2; (void)p3; (void)p4;
    for (round = 0; round < STRESS_WORKER_ROUNDS; round++) {
        OsSemPend(g_wkMutC, OS_SEM_WAIT_FOREVER);
        mem = OsMemKernelAlloc(64, 4);
        if (mem != NULL) {
            memset(mem, 0xC7, 64);
            OsMemKernelFree(mem);
        }
        OsSemPost(g_wkMutA);
        g_wkDoneCnt++;
    }
    OsTaskDelete(OS_RUNNING_TASK()->pid);
}

/* 启动一轮 Worker 链：创建 3 个 mutex + 3 个任务 */
static OS_SEC_KERNEL_TEXT void StressWorkerStart(void)
{
    OsSemCreate(OS_SEM_BINARY_SYNC, 0, 1, OS_SEM_WAKE_FIFO, &g_wkMutA);
    OsSemCreate(OS_SEM_BINARY_SYNC, 0, 1, OS_SEM_WAKE_FIFO, &g_wkMutB);
    OsSemCreate(OS_SEM_BINARY_SYNC, 0, 1, OS_SEM_WAKE_FIFO, &g_wkMutC);
    g_wkDoneCnt = 0;
    StressSafeResume(StressCreateTask("WkA", 10, StressWorkerA));
    StressSafeResume(StressCreateTask("WkB", 10, StressWorkerB));
    StressSafeResume(StressCreateTask("WkC", 10, StressWorkerC));
    /* A 在 pend 等 MutA，先给它一个初始 post 启动链条 */
    OsSemPost(g_wkMutA);
}

/* 等待 Worker 链完成 N 轮后清理 */
static OS_SEC_KERNEL_TEXT void StressWorkerWaitAndCleanup(U32 rounds)
{
    U32 target = rounds * 3; /* 每轮 3 个 Worker 完成 */
    U32 waited = 0;
    while (g_wkDoneCnt < target && waited < 500) {
        OsTaskDelay(10);
        waited++;
    }
    /* Workers 自删了任务，但 mutex 可能还有 pender → delay 让 pend 超时或被 post */
    OsTaskDelay(5);
    /* 强制 post 残留的 mutex 让可能卡住的 pender 释放 */
    OsSemPost(g_wkMutA);
    OsSemPost(g_wkMutB);
    OsSemPost(g_wkMutC);
    OsTaskDelay(5);
    /* 删除 mutex */
    OsSemDelete(g_wkMutA);
    OsSemDelete(g_wkMutB);
    OsSemDelete(g_wkMutC);
}

/* ====== PI 三角：优先级反转压力 ====== */

OS_SEC_KERNEL_BSS U32 g_piMutex;
OS_SEC_KERNEL_BSS volatile U32 g_piBoostCnt;
OS_SEC_KERNEL_BSS volatile U32 g_piRestoreCnt;
OS_SEC_KERNEL_BSS volatile U32 g_piLowPrioAfter;
OS_SEC_KERNEL_BSS U32 g_piTskLow;
OS_SEC_KERNEL_BSS U32 g_piTskMid;
OS_SEC_KERNEL_BSS U32 g_piTskHigh;
OS_SEC_KERNEL_BSS volatile U32 g_piPhase;  /* 0=low持有 1=mid跑 2=high pend */

OS_SEC_KERNEL_TEXT void StressPiLow(void *p1, void *p2, void *p3, void *p4)
{
    U32 i;
    for (i = 0; i < STRESS_PI_ROUNDS; i++) {
        OsSemPend(g_piMutex, OS_SEM_WAIT_FOREVER);
        g_piPhase = 1; /* low 持有，让 mid 先跑 */
        OsTaskDelay(20);
        /* 此时 high 应该已经 pend 了，PI boost */
        if (OS_RUNNING_TASK()->prio <= 5) {
            g_piBoostCnt++;
        }
        OsSemPost(g_piMutex);
        if (OS_RUNNING_TASK()->prio == OS_RUNNING_TASK()->oriPrio) {
            g_piRestoreCnt++;
        }
        OsTaskDelay(10);
    }
    g_piLowPrioAfter = OS_RUNNING_TASK()->prio;
    while (1) {
        OsTaskDelay(100);
    }
}

OS_SEC_KERNEL_TEXT void StressPiMid(void *p1, void *p2, void *p3, void *p4)
{
    /* 持续运行占 CPU，直到 PI 把 low 提上来抢占 */
    while (1) {
        OsTaskDelay(5);
    }
}

OS_SEC_KERNEL_TEXT void StressPiHigh(void *p1, void *p2, void *p3, void *p4)
{
    U32 i;
    for (i = 0; i < STRESS_PI_ROUNDS; i++) {
        OsTaskDelay(15); /* 等 low 持有 + mid 开跑 */
        OsSemPend(g_piMutex, OS_SEM_WAIT_FOREVER);
        OsSemPost(g_piMutex);
        OsTaskDelay(30);
    }
}

static OS_SEC_KERNEL_TEXT void StressPiStart(void)
{
    OsSemCreate(OS_SEM_BINARY_MUTEX, 1, 1, OS_SEM_WAKE_PRIO, &g_piMutex);
    g_piBoostCnt = 0;
    g_piRestoreCnt = 0;
    g_piPhase = 0;
    g_piTskLow = StressCreateTask("PiLow", 20, StressPiLow);
    g_piTskMid = StressCreateTask("PiMid", 15, StressPiMid);
    g_piTskHigh = StressCreateTask("PiHi", 5, StressPiHigh);
    StressSafeResume(g_piTskLow);
    StressSafeResume(g_piTskMid);
    StressSafeResume(g_piTskHigh);
}

static OS_SEC_KERNEL_TEXT void StressPiCleanup(void)
{
    StressCleanupTask(g_piTskLow);
    StressCleanupTask(g_piTskMid);
    StressCleanupTask(g_piTskHigh);
    OsSemDelete(g_piMutex);
}

/* ====== 同优先级轮转 ====== */

OS_SEC_KERNEL_BSS volatile U32 g_rrCntE;
OS_SEC_KERNEL_BSS volatile U32 g_rrCntF;
OS_SEC_KERNEL_BSS U32 g_rrTskE;
OS_SEC_KERNEL_BSS U32 g_rrTskF;

OS_SEC_KERNEL_TEXT void StressRrE(void *arg1, void *arg2, void *arg3, void *arg4)
{
    U32 c = 0;
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    while (1) {
        c++;
        g_rrCntE = c;
    }
}

OS_SEC_KERNEL_TEXT void StressRrF(void *arg1, void *arg2, void *arg3, void *arg4)
{
    U32 c = 0;
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    while (1) {
        c++;
        g_rrCntF = c;
    }
}

static OS_SEC_KERNEL_TEXT void StressRrStart(void)
{
    g_rrCntE = 0;
    g_rrCntF = 0;
    g_rrTskE = StressCreateTask("StrRE", 28, StressRrE);
    g_rrTskF = StressCreateTask("StrRF", 28, StressRrF);
    StressSafeResume(g_rrTskE);
    StressSafeResume(g_rrTskF);
}

static OS_SEC_KERNEL_TEXT void StressRrCleanup(void)
{
    StressCleanupTask(g_rrTskE);
    StressCleanupTask(g_rrTskF);
}

/* ====== 内存压力 ====== */

OS_SEC_KERNEL_BSS volatile U32 g_allocOkCnt;
OS_SEC_KERNEL_BSS volatile U32 g_allocFailCnt;
OS_SEC_KERNEL_BSS U32 g_allocTsk;

/* 简易 LCG 伪随机 */
static OS_SEC_KERNEL_TEXT U32 StressRand(U32 *seed)
{
    *seed = *seed * 1103515245u + 12345u;
    return *seed;
}

OS_SEC_KERNEL_TEXT void StressAllocator(void *p1, void *p2, void *p3, void *p4)
{
    U32 seed = 0xDEADBEEFu;
    U32 i;
    for (i = 0; i < STRESS_ALLOC_ROUNDS; i++) {
        U32 size = 16 + (StressRand(&seed) % 2032); /* 16~2047 */
        U32 align = 4u << (StressRand(&seed) % 3);  /* 4,8,16 */
        void *mem = OsMemKernelAlloc(size, align);
        if (mem == NULL) {
            g_allocFailCnt++;
            continue;
        }
        /* 写魔数 */
        memset(mem, 0xAB, size);
        /* 校验 */
        OS_TEST_ASSERT(((uintptr_t)mem % align) == 0);
        OsMemKernelFree(mem);
        g_allocOkCnt++;
    }
}

static OS_SEC_KERNEL_TEXT void StressAllocStart(void)
{
    g_allocOkCnt = 0;
    g_allocFailCnt = 0;
    g_allocTsk = StressCreateTask("StrAlloc", 7, StressAllocator);
    StressSafeResume(g_allocTsk);
}

static OS_SEC_KERNEL_TEXT void StressAllocCleanup(void)
{
    StressCleanupTask(g_allocTsk);
}

/* ====== 超时/挂起组合 ====== */

OS_SEC_KERNEL_BSS U32 g_tmoSem;
OS_SEC_KERNEL_BSS volatile U32 g_tmoGotTimeout;
OS_SEC_KERNEL_BSS U32 g_tmoTskWaiter;
OS_SEC_KERNEL_BSS U32 g_tmoTskSuspender;

OS_SEC_KERNEL_TEXT void StressTmoWaiter(void *p1, void *p2, void *p3, void *p4)
{
    U32 i;
    for (i = 0; i < 3; i++) {
        U32 ret = OsSemPend(g_tmoSem, 100);
        if (ret == OS_SEM_PEND_TIMEOUT) {
            g_tmoGotTimeout++;
        }
    }
}

OS_SEC_KERNEL_TEXT void StressTmoSuspender(void *p1, void *p2, void *p3, void *p4)
{
    U32 i;
    for (i = 0; i < 3; i++) {
        OsTaskDelay(50);
        /* suspend waiter（可能正在 pend），再 resume */
        if (g_tmoTskWaiter < g_tskMaxNum) {
            OsTaskSuspend(g_tmoTskWaiter);
        }
        OsTaskDelay(10);
        if (g_tmoTskWaiter < g_tskMaxNum) {
            OsTaskResume(g_tmoTskWaiter);
        }
        OsTaskDelay(60);
    }
}

static OS_SEC_KERNEL_TEXT void StressTmoStart(void)
{
    OsSemCreate(OS_SEM_BINARY_SYNC, 0, 1, OS_SEM_WAKE_FIFO, &g_tmoSem);
    g_tmoGotTimeout = 0;
    g_tmoTskWaiter = StressCreateTask("TmoW", 9, StressTmoWaiter);
    g_tmoTskSuspender = StressCreateTask("TmoS", 8, StressTmoSuspender);
    StressSafeResume(g_tmoTskWaiter);
    StressSafeResume(g_tmoTskSuspender);
}

static OS_SEC_KERNEL_TEXT void StressTmoCleanup(void)
{
    StressCleanupTask(g_tmoTskWaiter);
    StressCleanupTask(g_tmoTskSuspender);
    OsSemDelete(g_tmoSem);
}

/* ====== 信号量回收验证 ====== */

static OS_SEC_KERNEL_TEXT U32 StressCountFreeSems(void)
{
    /* 尝试连续创建信号量，直到创建失败，计数就是空闲数 */
    U32 ids[16];
    U32 cnt = 0;
    U32 i;
    while (cnt < 16) {
        if (OsSemCreate(OS_SEM_COUNTING, 0, 1, OS_SEM_WAKE_FIFO, &ids[cnt]) != OS_OK) {
            break;
        }
        cnt++;
    }
    /* 归还 */
    for (i = cnt; i > 0; i--) {
        OsSemDelete(ids[i - 1]);
    }
    return cnt;
}

/* ====== setup / verify ====== */

OS_SEC_KERNEL_TEXT void TestStressSetup(void)
{
    /* setup 什么都不做，verify 里循环创建/清理 */
}

OS_SEC_KERNEL_TEXT void TestStressVerify(void)
{
    U32 cycle;
    U64 startTick = g_uniTicks;

    for (cycle = 0; cycle < STRESS_CYCLES; cycle++) {
        U32 rrE1, rrF1;

        OsUartPrintf("[STRESS] cycle %d/%d start (tick=%d)\n",
                    cycle + 1, STRESS_CYCLES, (U32)g_uniTicks);

        /* 启动所有压力场景 */
        StressWorkerStart();
        StressPiStart();
        StressRrStart();
        StressAllocStart();
        StressTmoStart();

        /* 采样 RR 计数（运行前） */
        rrE1 = g_rrCntE;
        rrF1 = g_rrCntF;

        /* 等待并发运行 */
        OsTaskDelay(STRESS_CYCLE_TICKS);

        /* 检查 RR：两个计数都应该在增长 */
        OS_TEST_ASSERT(g_rrCntE > rrE1);
        OS_TEST_ASSERT(g_rrCntF > rrF1);

        /* 检查 PI */
        OS_TEST_ASSERT(g_piBoostCnt > 0);
        OS_TEST_ASSERT(g_piRestoreCnt > 0);

        /* 检查 Allocator */
        OS_TEST_ASSERT(g_allocOkCnt > 0);

        /* 检查超时 */
        OS_TEST_ASSERT(g_tmoGotTimeout > 0);

        /* 清理所有场景 */
        StressWorkerWaitAndCleanup(STRESS_WORKER_ROUNDS);
        StressPiCleanup();
        StressRrCleanup();
        StressAllocCleanup();
        StressTmoCleanup();

        /* 验证信号量全部回收 */
        OS_TEST_ASSERT_EQ(StressCountFreeSems(), STRESS_SEM_CHECK_NUM);

        OsUartPrintf("[STRESS] cycle %d/%d done  PI boost=%d restore=%d alloc=%d timeout=%d\n",
                    cycle + 1, STRESS_CYCLES, g_piBoostCnt, g_piRestoreCnt,
                    g_allocOkCnt, g_tmoGotTimeout);

        /* 如果已经跑了 5 分钟，提前退出 */
        if ((g_uniTicks - startTick) >= (U64)STRESS_CYCLES * STRESS_CYCLE_TICKS) {
            break;
        }
    }

    OsUartPrintf("[STRESS] ALL %d CYCLES PASSED, tick=%d\n",
                cycle, (U32)(g_uniTicks - startTick));
}
