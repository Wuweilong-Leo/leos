#include "os_def.h"
#include "os_print_external.h"
#include "os_mem_external.h"
#include "os_mem_fsc_internal.h"
#include "os_test.h"
#include "string.h"
#include "os_hwi.h"
#include "os_debug_external.h"
#include "os_uart_external.h"

/* ====== 测试结果 ====== */
OS_SEC_KERNEL_BSS volatile U32 g_memTestResult;
OS_SEC_KERNEL_BSS volatile U32 g_memTestRunCnt;
OS_SEC_KERNEL_BSS volatile U32 g_memTestFailCnt;

#define MEM_TEST_BASIC_OK     0x00000001  /* 基本 alloc/free + freeSize 恢复 */
#define MEM_TEST_ALIGN_OK     0x00000002  /* 各对齐档位返回地址对齐 */
#define MEM_TEST_SPLIT_OK     0x00000004  /* 切分：右侧剩余变空闲块 */
#define MEM_TEST_COALESCE_OK  0x00000008  /* 合并：相邻块按任意顺序释放归一 */
#define MEM_TEST_REUSE_OK     0x00000010  /* 碎片后复用空洞 */
#define MEM_TEST_OOM_OK       0x00000020  /* 耗尽返回 NULL，全释放后恢复 */
#define MEM_TEST_STRESS_OK    0x00000040  /* 随机压力：不变量始终成立 */
#define MEM_TEST_MAGIC_OK     0x00000080  /* 尾魔数 + offset 字段正确 */
#define MEM_TEST_COLLISION    0x00000100  /* 诊断：页分配器内核池与 FSC 堆地址重合（Bug 2 坐实） */

/* ====== 独立测试池（BSS，不碰真实内核堆） ====== */
#define MEM_TEST_POOL_SIZE (64 * 1024)
OS_SEC_KERNEL_BSS U8 g_fscTestPool[MEM_TEST_POOL_SIZE];
OS_SEC_KERNEL_BSS struct OsMemFscCtrl *g_testCtrl;

/* 真实内核 FSC 控制器（os_mem.c 中定义，头文件未 extern，这里补声明） */
extern struct OsMemFscCtrl *g_kernelMemPtCtrl;

/* ====== 不变量校验工具 ====== */

/* 遍历 32 条空闲链，求空闲块总大小与块数（哨兵 btmp bit31 不挂真实块，不计入） */
static OS_SEC_KERNEL_TEXT U32 TestFscSumFree(struct OsMemFscCtrl *ctrl, U32 *cntOut)
{
    U32 sum = 0;
    U32 cnt = 0;
    U32 i;
    for (i = 0; i < OS_MEM_FSC_SIZE_NUM; i++) {
        struct OsMemFscHead *fl = &ctrl->freeList[i];
        struct OsMemFscHead *blk = fl->next;
        while (blk != fl) {
            sum += (U32)blk->size;
            cnt++;
            blk = blk->next;
        }
    }
    if (cntOut != NULL) {
        *cntOut = cnt;
    }
    return sum;
}

/* 两条不变量：空闲链总和==freeSize；本测试持有量==totalSize-freeSize */
static OS_SEC_KERNEL_TEXT bool TestFscCheckInv(struct OsMemFscCtrl *ctrl, U32 regSum, U32 total0)
{
    U32 cnt;
    U32 sumFree = TestFscSumFree(ctrl, &cnt);
    if (sumFree != (U32)ctrl->freeSize) {
        OsUartPrintf("[INV] sumFree=0x%x != freeSize=0x%x\n", sumFree, (U32)ctrl->freeSize);
        return FALSE;
    }
    if (regSum != (U32)(total0 - ctrl->freeSize)) {
        OsUartPrintf("[INV] regSum=0x%x != total-free=0x%x\n", regSum, (U32)(total0 - ctrl->freeSize));
        return FALSE;
    }
    return TRUE;
}

static OS_SEC_KERNEL_TEXT U32 TestFscCountFree(struct OsMemFscCtrl *ctrl)
{
    U32 cnt;
    (void)TestFscSumFree(ctrl, &cnt);
    return cnt;
}

/* ====== 确定性随机数（LCG，可复现） ====== */
OS_SEC_KERNEL_BSS U32 s_testRng;
static OS_SEC_KERNEL_TEXT U32 TestRand(void)
{
    s_testRng = s_testRng * 1103515245u + 12345u;
    return s_testRng;
}

/* ====== 用例 ====== */

/* T1: 基本 alloc/free + freeSize 恢复 */
static OS_SEC_KERNEL_TEXT void TestFscBasic(void)
{
    U32 total0 = (U32)g_testCtrl->totalSize;
    void *p;
    struct OsMemFscHead *head;
    bool ok = TRUE;

    p = OsMemFscAlloc(g_testCtrl, 64, 4);
    if (p == NULL || ((uintptr_t)p % 4) != 0) {
        ok = FALSE;
    } else {
        head = OsMemFscGetHead((uintptr_t)p);
        if (head->ctrl != g_testCtrl) {
            ok = FALSE;
        }
    }
    OsMemFscFree(p);
    if ((U32)g_testCtrl->freeSize != total0) {
        ok = FALSE;
    }

    if (ok) {
        g_memTestResult |= MEM_TEST_BASIC_OK;
    } else {
        g_memTestFailCnt++;
        OsUartPrintf("[T1 BASIC] FAIL freeSize=0x%x\n", (U32)g_testCtrl->freeSize);
    }
}

/* T2: 对齐正确性 */
static OS_SEC_KERNEL_TEXT void TestFscAlign(void)
{
    static const U32 aligns[] = {4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    U32 total0 = (U32)g_testCtrl->totalSize;
    U32 i;
    bool ok = TRUE;

    for (i = 0; i < sizeof(aligns) / sizeof(aligns[0]); i++) {
        void *p = OsMemFscAlloc(g_testCtrl, 7, aligns[i]); /* 7 字节，逼出补齐 */
        if (p == NULL) {
            ok = FALSE;
            break;
        }
        if (((uintptr_t)p % aligns[i]) != 0) {
            ok = FALSE;
            OsUartPrintf("[T2] align %d got 0x%x\n", aligns[i], (U32)(uintptr_t)p);
        }
        OsMemFscFree(p);
        if ((U32)g_testCtrl->freeSize != total0) {
            ok = FALSE;
            OsUartPrintf("[T2] not restored after align %d\n", aligns[i]);
            break;
        }
    }

    if (ok) {
        g_memTestResult |= MEM_TEST_ALIGN_OK;
    } else {
        g_memTestFailCnt++;
    }
}

/* T3: 切分——分配后右侧剩余成为空闲块 */
static OS_SEC_KERNEL_TEXT void TestFscSplit(void)
{
    U32 total0 = (U32)g_testCtrl->totalSize;
    void *p = OsMemFscAlloc(g_testCtrl, 4096, 4);
    struct OsMemFscHead *head;
    U32 cnt;
    U32 sumFree;
    bool ok = TRUE;

    if (p == NULL) {
        g_memTestFailCnt++;
        return;
    }
    head = OsMemFscGetHead((uintptr_t)p);

    /* 分配后：持有量==head->size；空闲链总和==freeSize */
    if ((U32)(total0 - g_testCtrl->freeSize) != (U32)head->size) {
        ok = FALSE;
    }
    sumFree = TestFscSumFree(g_testCtrl, &cnt);
    if (sumFree != (U32)g_testCtrl->freeSize) {
        ok = FALSE;
    }
    /* 4096 从 64KB 切，剩余 ~59KB 必然独立成块 */
    if (cnt != 1) {
        ok = FALSE;
    }

    OsMemFscFree(p);
    if ((U32)g_testCtrl->freeSize != total0 || TestFscCountFree(g_testCtrl) != 1) {
        ok = FALSE;
    }

    if (ok) {
        g_memTestResult |= MEM_TEST_SPLIT_OK;
    } else {
        g_memTestFailCnt++;
        OsUartPrintf("[T3 SPLIT] FAIL freeSize=0x%x cnt=%d\n", (U32)g_testCtrl->freeSize, cnt);
    }
}

/* T4: 合并——相邻三块按各种顺序释放都归并成一块 */
static OS_SEC_KERNEL_TEXT bool TestFscCoalesceOrder(int order)
{
    U32 total0 = (U32)g_testCtrl->totalSize;
    void *a = OsMemFscAlloc(g_testCtrl, 100, 4);
    void *b = OsMemFscAlloc(g_testCtrl, 200, 4);
    void *c = OsMemFscAlloc(g_testCtrl, 300, 4);
    struct OsMemFscHead *ha, *hb, *hc;
    U32 combined;
    bool ok = TRUE;

    if (a == NULL || b == NULL || c == NULL) {
        return FALSE;
    }
    ha = OsMemFscGetHead((uintptr_t)a);
    hb = OsMemFscGetHead((uintptr_t)b);
    hc = OsMemFscGetHead((uintptr_t)c);

    /* FSC 从块尾部切分，连续分配地址递减，三块在内存里相邻无隙。
       按头地址升序排序后校验：sorted[i+1] == sorted[i] + sorted[i].size */
    {
        uintptr_t heads[3];
        size_t sizes[3];
        U32 k;
        heads[0] = (uintptr_t)ha;
        heads[1] = (uintptr_t)hb;
        heads[2] = (uintptr_t)hc;
        sizes[0] = ha->size;
        sizes[1] = hb->size;
        sizes[2] = hc->size;
        /* 简单冒泡（3 元素） */
        for (k = 0; k < 2; k++) {
            U32 j;
            for (j = 0; j < 2 - k; j++) {
                if (heads[j] > heads[j + 1]) {
                    uintptr_t th = heads[j];
                    size_t ts = sizes[j];
                    heads[j] = heads[j + 1];
                    sizes[j] = sizes[j + 1];
                    heads[j + 1] = th;
                    sizes[j + 1] = ts;
                }
            }
        }
        if (heads[1] != heads[0] + sizes[0] || heads[2] != heads[1] + sizes[1]) {
            ok = FALSE;
        }
    }
    combined = (U32)(ha->size + hb->size + hc->size);

    /* order: 0=a,b,c 1=c,b,a 2=a,c,b(中间最后释放) */
    switch (order) {
        case 0:
            OsMemFscFree(a);
            OsMemFscFree(b);
            OsMemFscFree(c);
            break;
        case 1:
            OsMemFscFree(c);
            OsMemFscFree(b);
            OsMemFscFree(a);
            break;
        case 2:
            OsMemFscFree(a);
            OsMemFscFree(c);
            OsMemFscFree(b); /* b 同时与左 a、右 c 合并 */
            break;
        default:
            break;
    }

    /* 全释放后：恢复成单块，大小==total0 */
    if ((U32)g_testCtrl->freeSize != total0) {
        ok = FALSE;
    }
    if (TestFscCountFree(g_testCtrl) != 1) {
        ok = FALSE;
    }
    if (!ok) {
        OsUartPrintf("[T4] order %d FAIL freeSize=0x%x cnt=%d combined=0x%x\n",
                     (U32)order, (U32)g_testCtrl->freeSize, TestFscCountFree(g_testCtrl), combined);
    }
    return ok;
}

static OS_SEC_KERNEL_TEXT void TestFscCoalesce(void)
{
    bool ok = TestFscCoalesceOrder(0) && TestFscCoalesceOrder(1) && TestFscCoalesceOrder(2);
    if (ok) {
        g_memTestResult |= MEM_TEST_COALESCE_OK;
    } else {
        g_memTestFailCnt++;
    }
}

/* T5: 碎片后复用空洞 */
static OS_SEC_KERNEL_TEXT void TestFscReuse(void)
{
    U32 total0 = (U32)g_testCtrl->totalSize;
    void *blk[10];
    U32 i;
    bool ok = TRUE;
    U32 regSum = 0;

    /* 分配 10 块，大小递增 */
    for (i = 0; i < 10; i++) {
        blk[i] = OsMemFscAlloc(g_testCtrl, 32 * (i + 1), 4);
        if (blk[i] == NULL) {
            ok = FALSE;
            break;
        }
        regSum += (U32)OsMemFscGetHead((uintptr_t)blk[i])->size;
    }
    if (!TestFscCheckInv(g_testCtrl, regSum, total0)) {
        ok = FALSE;
    }

    /* 释放偶数下标，挖出空洞 */
    for (i = 0; i < 10; i += 2) {
        struct OsMemFscHead *h = OsMemFscGetHead((uintptr_t)blk[i]);
        U32 sz = (U32)h->size;
        OsMemFscFree(blk[i]);
        regSum -= sz;
        blk[i] = NULL;
    }
    if (!TestFscCheckInv(g_testCtrl, regSum, total0)) {
        ok = FALSE;
    }

    /* 再分配小块，应能复用空洞 */
    for (i = 0; i < 5; i++) {
        void *p = OsMemFscAlloc(g_testCtrl, 16, 4);
        if (p == NULL) {
            ok = FALSE;
            break;
        }
        regSum += (U32)OsMemFscGetHead((uintptr_t)p)->size;
        OsMemFscFree(p);
        regSum -= (U32)OsMemFscGetHead((uintptr_t)p)->size;
    }
    if (!TestFscCheckInv(g_testCtrl, regSum, total0)) {
        ok = FALSE;
    }

    /* 释放剩余，全部恢复 */
    for (i = 0; i < 10; i++) {
        if (blk[i] != NULL) {
            struct OsMemFscHead *h = OsMemFscGetHead((uintptr_t)blk[i]);
            U32 sz = (U32)h->size;
            OsMemFscFree(blk[i]);
            regSum -= sz;
        }
    }
    if ((U32)g_testCtrl->freeSize != total0 || TestFscCountFree(g_testCtrl) != 1 || regSum != 0) {
        ok = FALSE;
    }

    if (ok) {
        g_memTestResult |= MEM_TEST_REUSE_OK;
    } else {
        g_memTestFailCnt++;
        OsUartPrintf("[T5 REUSE] FAIL regSum=0x%x freeSize=0x%x\n", regSum, (U32)g_testCtrl->freeSize);
    }
}

/* T6: 耗尽返回 NULL，全释放后恢复 */
static OS_SEC_KERNEL_TEXT void TestFscOom(void)
{
    U32 total0 = (U32)g_testCtrl->totalSize;
    void *blk[64];
    U32 n = 0;
    U32 i;
    U32 regSum = 0;
    bool ok = TRUE;
    void *extra;

    /* 用 2KB 块耗尽 ~64KB 池 */
    while (n < sizeof(blk) / sizeof(blk[0])) {
        void *p = OsMemFscAlloc(g_testCtrl, 2048, 4);
        if (p == NULL) {
            break;
        }
        blk[n++] = p;
        regSum += (U32)OsMemFscGetHead((uintptr_t)p)->size;
    }
    if (n == 0) {
        ok = FALSE;
    }
    /* 耗尽时不变量仍须成立 */
    if (!TestFscCheckInv(g_testCtrl, regSum, total0)) {
        ok = FALSE;
    }
    /* 再申请必然失败 */
    extra = OsMemFscAlloc(g_testCtrl, 2048, 4);
    if (extra != NULL) {
        ok = FALSE;
        OsMemFscFree(extra);
    }

    /* 全释放恢复 */
    for (i = 0; i < n; i++) {
        struct OsMemFscHead *h = OsMemFscGetHead((uintptr_t)blk[i]);
        U32 sz = (U32)h->size;
        OsMemFscFree(blk[i]);
        regSum -= sz;
    }
    if (regSum != 0) {
        ok = FALSE;
    }
    if ((U32)g_testCtrl->freeSize != total0 || TestFscCountFree(g_testCtrl) != 1) {
        ok = FALSE;
    }

    if (ok) {
        g_memTestResult |= MEM_TEST_OOM_OK;
    } else {
        g_memTestFailCnt++;
        OsUartPrintf("[T6 OOM] FAIL n=%d freeSize=0x%x\n", n, (U32)g_testCtrl->freeSize);
    }
}

/* T7: 随机压力——alloc/free 交错，不变量始终成立 */
#define MEM_STRESS_SLOTS 1536
#define MEM_STRESS_ROUNDS 3000
OS_SEC_KERNEL_BSS void *g_stressSlots[MEM_STRESS_SLOTS];

static OS_SEC_KERNEL_TEXT void TestFscStress(void)
{
    U32 total0 = (U32)g_testCtrl->totalSize;
    U32 round;
    U32 live = 0;
    U32 regSum = 0;
    bool ok = TRUE;

    s_testRng = 0xDEADBEEFu;
    memset(g_stressSlots, 0, sizeof(g_stressSlots));

    for (round = 0; round < MEM_STRESS_ROUNDS; round++) {
        if (live == 0 || (TestRand() & 1)) {
            /* 分配：size 1..256，align ∈ {4,8,16} */
            U32 size = 1 + (TestRand() % 256);
            U32 align = 4u << (TestRand() % 3);
            void *p;
            U32 slot;
            if (live >= MEM_STRESS_SLOTS) {
                /* 槽满：先随机释放一个 */
                U32 s = TestRand() % MEM_STRESS_SLOTS;
                if (g_stressSlots[s] != NULL) {
                    struct OsMemFscHead *h = OsMemFscGetHead((uintptr_t)g_stressSlots[s]);
                    U32 sz = (U32)h->size;
                    OsMemFscFree(g_stressSlots[s]);
                    g_stressSlots[s] = NULL;
                    live--;
                    regSum -= sz;
                }
            }
            p = OsMemFscAlloc(g_testCtrl, size, align);
            if (p != NULL) {
                /* 找空槽 */
                for (slot = 0; slot < MEM_STRESS_SLOTS; slot++) {
                    if (g_stressSlots[slot] == NULL) {
                        break;
                    }
                }
                if (slot < MEM_STRESS_SLOTS) {
                    g_stressSlots[slot] = p;
                    live++;
                    regSum += (U32)OsMemFscGetHead((uintptr_t)p)->size;
                } else {
                    OsMemFscFree(p);
                }
            }
        } else {
            /* 随机释放一个存活块 */
            U32 s = TestRand() % MEM_STRESS_SLOTS;
            if (g_stressSlots[s] != NULL) {
                struct OsMemFscHead *h = OsMemFscGetHead((uintptr_t)g_stressSlots[s]);
                U32 sz = (U32)h->size;
                OsMemFscFree(g_stressSlots[s]);
                g_stressSlots[s] = NULL;
                live--;
                regSum -= sz;
            }
        }
        if (!TestFscCheckInv(g_testCtrl, regSum, total0)) {
            ok = FALSE;
            OsUartPrintf("[T7] INV broke at round %d\n", round);
            break;
        }
    }

    /* 释放全部存活块 */
    {
        U32 s;
        for (s = 0; s < MEM_STRESS_SLOTS; s++) {
            if (g_stressSlots[s] != NULL) {
                struct OsMemFscHead *h = OsMemFscGetHead((uintptr_t)g_stressSlots[s]);
                U32 sz = (U32)h->size;
                OsMemFscFree(g_stressSlots[s]);
                g_stressSlots[s] = NULL;
                regSum -= sz;
                live--;
            }
        }
    }
    if ((U32)g_testCtrl->freeSize != total0 || TestFscCountFree(g_testCtrl) != 1 || regSum != 0) {
        ok = FALSE;
    }

    if (ok) {
        g_memTestResult |= MEM_TEST_STRESS_OK;
    } else {
        g_memTestFailCnt++;
        OsUartPrintf("[T7 STRESS] FAIL live=%d regSum=0x%x freeSize=0x%x\n",
                     live, regSum, (U32)g_testCtrl->freeSize);
    }
}

/* T8: 尾魔数 + offset 字段 */
static OS_SEC_KERNEL_TEXT void TestFscMagic(void)
{
    static const U32 sizes[] = {1, 7, 16, 64, 255, 1024};
    static const U32 aligns[] = {4, 16, 64, 256};
    U32 total0 = (U32)g_testCtrl->totalSize;
    U32 si, ai;
    bool ok = TRUE;

    for (si = 0; si < sizeof(sizes) / sizeof(sizes[0]) && ok; si++) {
        for (ai = 0; ai < sizeof(aligns) / sizeof(aligns[0]) && ok; ai++) {
            void *p = OsMemFscAlloc(g_testCtrl, sizes[si], aligns[ai]);
            struct OsMemFscHead *head;
            U32 magic;
            U32 off;
            U32 k;
            if (p == NULL) {
                ok = FALSE;
                break;
            }
            if (((uintptr_t)p % aligns[ai]) != 0) {
                ok = FALSE;
            }
            head = OsMemFscGetHead((uintptr_t)p);
            magic = *(U32 *)((uintptr_t)head + (U32)head->size - 4);
            if (magic != OS_MEM_FSC_TAIL_MAGIC) {
                ok = FALSE;
                OsUartPrintf("[T8] magic bad 0x%x\n", magic);
            }
            /* offset 字段：usr-4 处存 usr-head */
            off = *(U32 *)((uintptr_t)p - 4);
            if (off != (U32)((uintptr_t)p - (uintptr_t)head)) {
                ok = FALSE;
            }
            /* 写满用户请求 size 字节，魔数应不被破坏 */
            for (k = 0; k < sizes[si]; k++) {
                ((U8 *)p)[k] = (U8)(k & 0xFF);
            }
            magic = *(U32 *)((uintptr_t)head + (U32)head->size - 4);
            if (magic != OS_MEM_FSC_TAIL_MAGIC) {
                ok = FALSE;
                OsUartPrintf("[T8] magic clobbered after write size=%d\n", sizes[si]);
            }
            OsMemFscFree(p);
            if ((U32)g_testCtrl->freeSize != total0) {
                ok = FALSE;
            }
        }
    }
    /* 最终恢复成单块 */
    if (TestFscCountFree(g_testCtrl) != 1) {
        ok = FALSE;
    }

    if (ok) {
        g_memTestResult |= MEM_TEST_MAGIC_OK;
    } else {
        g_memTestFailCnt++;
    }
}

OS_SEC_KERNEL_TEXT U32 OsTestMemInit(void)
{
    U32 total0;
    enum OsLogLevel savedLevel;

    g_memTestResult = 0;
    g_memTestRunCnt = 0;
    g_memTestFailCnt = 0;

    /* OOM/压力测试会触发 FSC 的 OS_LOG_ERROR（池满），静音避免刷屏冲掉汇总行 */
    savedLevel = OsDebugGetLogLevel();

    OsUartPuts("\n==== FSC ALLOCATOR TEST START ====\n");

    /* 在 BSS 池上建独立 FSC 控制器 */
    g_testCtrl = OsMemFscInitPt((uintptr_t)g_fscTestPool, MEM_TEST_POOL_SIZE);
    if (g_testCtrl == NULL) {
        OsUartPuts("[MEM] OsMemFscInitPt failed\n");
        return OS_OK;
    }
    total0 = (U32)g_testCtrl->totalSize;
    OsUartPrintf("[MEM] pool init OK total=0x%x\n", total0);

    /* 初始不变量：空池单块，freeSize==totalSize */
    if ((U32)g_testCtrl->freeSize != total0 || TestFscCountFree(g_testCtrl) != 1) {
        OsUartPuts("[MEM] init invariant FAIL\n");
        g_memTestFailCnt++;
    }

    TestFscBasic();
    TestFscAlign();
    TestFscSplit();
    TestFscCoalesce();
    TestFscReuse();

    /* 这两个会命中池满（FSC 内部打 OS_LOG_ERROR），静音 */
    OsDebugSetLogLevel(OS_LOG_NONE);
    TestFscOom();
    TestFscMagic();
    TestFscStress();
    OsDebugSetLogLevel(savedLevel);

    /* 收尾：池应回到单块、freeSize==totalSize */
    if ((U32)g_testCtrl->freeSize != total0 || TestFscCountFree(g_testCtrl) != 1) {
        OsUartPrintf("[MEM] FINAL LEAK freeSize=0x%x cnt=%d\n",
                     (U32)g_testCtrl->freeSize, TestFscCountFree(g_testCtrl));
        g_memTestFailCnt++;
    }

    OsUartPrintf("[MEM_RESULT] 0x%x  fails=%d\n", g_memTestResult, g_memTestFailCnt);
    OsUartPuts("==== FSC ALLOCATOR TEST END ====\n\n");

    /* VGA 汇总（第 10 行） */
    OsPrintSetCursor((U16)(10 * 80));
    kprintf("MEM: result=0x%x fails=%d basic=%d align=%d split=%d coa=%d reuse=%d oom=%d stress=%d magic=%d",
            g_memTestResult, g_memTestFailCnt,
            (g_memTestResult & MEM_TEST_BASIC_OK) != 0,
            (g_memTestResult & MEM_TEST_ALIGN_OK) != 0,
            (g_memTestResult & MEM_TEST_SPLIT_OK) != 0,
            (g_memTestResult & MEM_TEST_COALESCE_OK) != 0,
            (g_memTestResult & MEM_TEST_REUSE_OK) != 0,
            (g_memTestResult & MEM_TEST_OOM_OK) != 0,
            (g_memTestResult & MEM_TEST_STRESS_OK) != 0,
            (g_memTestResult & MEM_TEST_MAGIC_OK) != 0);

    return OS_OK;
}