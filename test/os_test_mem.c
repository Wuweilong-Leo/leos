#include "os_def.h"
#include "os_print_external.h"
#include "os_mem_external.h"
#include "os_mem_fsc_internal.h"
#include "os_test_framework.h"
#include "string.h"
#include "os_hwi.h"
#include "os_debug_external.h"
#include "os_uart_external.h"

/* ====== 独立测试池（BSS，不碰真实内核堆） ====== */
#define MEM_TEST_POOL_SIZE (64 * 1024)
OS_SEC_KERNEL_BSS U8 g_fscTestPool[MEM_TEST_POOL_SIZE];
OS_SEC_KERNEL_BSS struct OsMemFscCtrl *g_testCtrl;
OS_SEC_KERNEL_BSS U32 g_memTotal0;  /* 池初始大小，各用例共享 */

extern struct OsMemFscCtrl *g_kernelMemPtCtrl;

/* ====== 不变量校验工具 ====== */

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

/* ====== 确定性随机数 ====== */
OS_SEC_KERNEL_BSS U32 s_testRng;
static OS_SEC_KERNEL_TEXT U32 TestRand(void)
{
    s_testRng = s_testRng * 1103515245u + 12345u;
    return s_testRng;
}

/* ====== setup ====== */

OS_SEC_KERNEL_TEXT void TestMemSetup(void)
{
    g_testCtrl = OsMemFscInitPt((uintptr_t)g_fscTestPool, MEM_TEST_POOL_SIZE);
    OS_TEST_ASSERT(g_testCtrl != NULL);
    if (g_testCtrl == NULL) {
        return;
    }
    g_memTotal0 = (U32)g_testCtrl->totalSize;
    OS_TEST_ASSERT_EQ((U32)g_testCtrl->freeSize, g_memTotal0);
    OS_TEST_ASSERT_EQ(TestFscCountFree(g_testCtrl), 1);
}

/* ====== 用例 ====== */

OS_SEC_KERNEL_TEXT void TestFscBasic(void)
{
    void *p = OsMemFscAlloc(g_testCtrl, 64, 4);
    OS_TEST_ASSERT(p != NULL);
    OS_TEST_ASSERT(((uintptr_t)p % 4) == 0);
    if (p != NULL) {
        struct OsMemFscHead *head = OsMemFscGetHead((uintptr_t)p);
        OS_TEST_ASSERT(head->ctrl == g_testCtrl);
    }
    OsMemFscFree(p);
    OS_TEST_ASSERT_EQ((U32)g_testCtrl->freeSize, g_memTotal0);
}

OS_SEC_KERNEL_TEXT void TestFscAlign(void)
{
    static const U32 aligns[] = {4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048};
    U32 i;

    for (i = 0; i < sizeof(aligns) / sizeof(aligns[0]); i++) {
        void *p = OsMemFscAlloc(g_testCtrl, 7, aligns[i]);
        OS_TEST_ASSERT(p != NULL);
        if (p != NULL) {
            OS_TEST_ASSERT(((uintptr_t)p % aligns[i]) == 0);
        }
        OsMemFscFree(p);
        OS_TEST_ASSERT_EQ((U32)g_testCtrl->freeSize, g_memTotal0);
    }
}

OS_SEC_KERNEL_TEXT void TestFscSplit(void)
{
    void *p = OsMemFscAlloc(g_testCtrl, 4096, 4);
    struct OsMemFscHead *head;
    U32 cnt;
    U32 sumFree;

    OS_TEST_ASSERT(p != NULL);
    if (p == NULL) {
        return;
    }
    head = OsMemFscGetHead((uintptr_t)p);

    OS_TEST_ASSERT_EQ((U32)(g_memTotal0 - g_testCtrl->freeSize), (U32)head->size);

    sumFree = TestFscSumFree(g_testCtrl, &cnt);
    OS_TEST_ASSERT_EQ(sumFree, (U32)g_testCtrl->freeSize);
    OS_TEST_ASSERT_EQ(cnt, 1);

    OsMemFscFree(p);
    OS_TEST_ASSERT_EQ((U32)g_testCtrl->freeSize, g_memTotal0);
    OS_TEST_ASSERT_EQ(TestFscCountFree(g_testCtrl), 1);
}

static OS_SEC_KERNEL_TEXT bool TestFscCoalesceOrder(int order)
{
    void *a = OsMemFscAlloc(g_testCtrl, 100, 4);
    void *b = OsMemFscAlloc(g_testCtrl, 200, 4);
    void *c = OsMemFscAlloc(g_testCtrl, 300, 4);
    struct OsMemFscHead *ha, *hb, *hc;
    uintptr_t heads[3];
    size_t sizes[3];
    U32 k;
    bool ok = TRUE;

    if (a == NULL || b == NULL || c == NULL) {
        return FALSE;
    }
    ha = OsMemFscGetHead((uintptr_t)a);
    hb = OsMemFscGetHead((uintptr_t)b);
    hc = OsMemFscGetHead((uintptr_t)c);

    heads[0] = (uintptr_t)ha; heads[1] = (uintptr_t)hb; heads[2] = (uintptr_t)hc;
    sizes[0] = ha->size; sizes[1] = hb->size; sizes[2] = hc->size;
    for (k = 0; k < 2; k++) {
        U32 j;
        for (j = 0; j < 2 - k; j++) {
            if (heads[j] > heads[j + 1]) {
                uintptr_t th = heads[j]; size_t ts = sizes[j];
                heads[j] = heads[j + 1]; sizes[j] = sizes[j + 1];
                heads[j + 1] = th; sizes[j + 1] = ts;
            }
        }
    }
    if (heads[1] != heads[0] + sizes[0] || heads[2] != heads[1] + sizes[1]) {
        ok = FALSE;
    }

    switch (order) {
        case 0: OsMemFscFree(a); OsMemFscFree(b); OsMemFscFree(c); break;
        case 1: OsMemFscFree(c); OsMemFscFree(b); OsMemFscFree(a); break;
        case 2: OsMemFscFree(a); OsMemFscFree(c); OsMemFscFree(b); break;
        default: break;
    }

    OS_TEST_ASSERT_EQ((U32)g_testCtrl->freeSize, g_memTotal0);
    OS_TEST_ASSERT_EQ(TestFscCountFree(g_testCtrl), 1);
    return ok;
}

OS_SEC_KERNEL_TEXT void TestFscCoalesce(void)
{
    OS_TEST_ASSERT(TestFscCoalesceOrder(0));
    OS_TEST_ASSERT(TestFscCoalesceOrder(1));
    OS_TEST_ASSERT(TestFscCoalesceOrder(2));
}

OS_SEC_KERNEL_TEXT void TestFscReuse(void)
{
    void *blk[10];
    U32 i;
    U32 regSum = 0;

    for (i = 0; i < 10; i++) {
        blk[i] = OsMemFscAlloc(g_testCtrl, 32 * (i + 1), 4);
        OS_TEST_ASSERT(blk[i] != NULL);
        if (blk[i] != NULL) {
            regSum += (U32)OsMemFscGetHead((uintptr_t)blk[i])->size;
        }
    }
    OS_TEST_ASSERT(TestFscCheckInv(g_testCtrl, regSum, g_memTotal0));

    for (i = 0; i < 10; i += 2) {
        if (blk[i] != NULL) {
            struct OsMemFscHead *h = OsMemFscGetHead((uintptr_t)blk[i]);
            U32 sz = (U32)h->size;
            OsMemFscFree(blk[i]);
            regSum -= sz;
            blk[i] = NULL;
        }
    }
    OS_TEST_ASSERT(TestFscCheckInv(g_testCtrl, regSum, g_memTotal0));

    for (i = 0; i < 5; i++) {
        void *p = OsMemFscAlloc(g_testCtrl, 16, 4);
        OS_TEST_ASSERT(p != NULL);
        if (p != NULL) {
            regSum += (U32)OsMemFscGetHead((uintptr_t)p)->size;
            OsMemFscFree(p);
            regSum -= (U32)OsMemFscGetHead((uintptr_t)p)->size;
        }
    }
    OS_TEST_ASSERT(TestFscCheckInv(g_testCtrl, regSum, g_memTotal0));

    for (i = 0; i < 10; i++) {
        if (blk[i] != NULL) {
            struct OsMemFscHead *h = OsMemFscGetHead((uintptr_t)blk[i]);
            U32 sz = (U32)h->size;
            OsMemFscFree(blk[i]);
            regSum -= sz;
        }
    }
    OS_TEST_ASSERT_EQ((U32)g_testCtrl->freeSize, g_memTotal0);
    OS_TEST_ASSERT_EQ(TestFscCountFree(g_testCtrl), 1);
    OS_TEST_ASSERT_EQ(regSum, 0);
}

OS_SEC_KERNEL_TEXT void TestFscOom(void)
{
    void *blk[64];
    U32 n = 0;
    U32 i;
    U32 regSum = 0;
    void *extra;
    enum OsLogLevel savedLevel = OsDebugGetLogLevel();

    while (n < sizeof(blk) / sizeof(blk[0])) {
        void *p = OsMemFscAlloc(g_testCtrl, 2048, 4);
        if (p == NULL) {
            break;
        }
        blk[n++] = p;
        regSum += (U32)OsMemFscGetHead((uintptr_t)p)->size;
    }
    OS_TEST_ASSERT(n > 0);
    OS_TEST_ASSERT(TestFscCheckInv(g_testCtrl, regSum, g_memTotal0));

    OsDebugSetLogLevel(OS_LOG_NONE);
    extra = OsMemFscAlloc(g_testCtrl, 2048, 4);
    OsDebugSetLogLevel(savedLevel);
    OS_TEST_ASSERT(extra == NULL);
    if (extra != NULL) {
        OsMemFscFree(extra);
    }

    for (i = 0; i < n; i++) {
        struct OsMemFscHead *h = OsMemFscGetHead((uintptr_t)blk[i]);
        U32 sz = (U32)h->size;
        OsMemFscFree(blk[i]);
        regSum -= sz;
    }
    OS_TEST_ASSERT_EQ(regSum, 0);
    OS_TEST_ASSERT_EQ((U32)g_testCtrl->freeSize, g_memTotal0);
    OS_TEST_ASSERT_EQ(TestFscCountFree(g_testCtrl), 1);
}

#define MEM_STRESS_SLOTS 1536
#define MEM_STRESS_ROUNDS 3000
OS_SEC_KERNEL_BSS void *g_stressSlots[MEM_STRESS_SLOTS];

OS_SEC_KERNEL_TEXT void TestFscStress(void)
{
    U32 round;
    U32 live = 0;
    U32 regSum = 0;
    enum OsLogLevel savedLevel = OsDebugGetLogLevel();

    s_testRng = 0xDEADBEEFu;
    memset(g_stressSlots, 0, sizeof(g_stressSlots));

    OsDebugSetLogLevel(OS_LOG_NONE);
    for (round = 0; round < MEM_STRESS_ROUNDS; round++) {
        if (live == 0 || (TestRand() & 1)) {
            U32 size = 1 + (TestRand() % 256);
            U32 align = 4u << (TestRand() % 3);
            void *p;
            U32 slot;
            if (live >= MEM_STRESS_SLOTS) {
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
        if (!TestFscCheckInv(g_testCtrl, regSum, g_memTotal0)) {
            OsUartPrintf("[STRESS] INV broke at round %d\n", round);
            OS_TEST_ASSERT(0);
            break;
        }
    }

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
    OsDebugSetLogLevel(savedLevel);

    OS_TEST_ASSERT_EQ((U32)g_testCtrl->freeSize, g_memTotal0);
    OS_TEST_ASSERT_EQ(TestFscCountFree(g_testCtrl), 1);
    OS_TEST_ASSERT_EQ(regSum, 0);
}

OS_SEC_KERNEL_TEXT void TestFscMagic(void)
{
    static const U32 sizes[] = {1, 7, 16, 64, 255, 1024};
    static const U32 aligns[] = {4, 16, 64, 256};
    U32 si, ai;
    enum OsLogLevel savedLevel = OsDebugGetLogLevel();

    OsDebugSetLogLevel(OS_LOG_NONE);
    for (si = 0; si < sizeof(sizes) / sizeof(sizes[0]); si++) {
        for (ai = 0; ai < sizeof(aligns) / sizeof(aligns[0]); ai++) {
            void *p = OsMemFscAlloc(g_testCtrl, sizes[si], aligns[ai]);
            struct OsMemFscHead *head;
            U32 magic;
            U32 off;
            U32 k;
            OS_TEST_ASSERT(p != NULL);
            if (p == NULL) {
                continue;
            }
            OS_TEST_ASSERT(((uintptr_t)p % aligns[ai]) == 0);
            head = OsMemFscGetHead((uintptr_t)p);
            magic = *(U32 *)((uintptr_t)head + (U32)head->size - 4);
            OS_TEST_ASSERT_EQ(magic, OS_MEM_FSC_TAIL_MAGIC);
            off = *(U32 *)((uintptr_t)p - 4);
            OS_TEST_ASSERT_EQ(off, (U32)((uintptr_t)p - (uintptr_t)head));
            for (k = 0; k < sizes[si]; k++) {
                ((U8 *)p)[k] = (U8)(k & 0xFF);
            }
            magic = *(U32 *)((uintptr_t)head + (U32)head->size - 4);
            OS_TEST_ASSERT_EQ(magic, OS_MEM_FSC_TAIL_MAGIC);
            OsMemFscFree(p);
            OS_TEST_ASSERT_EQ((U32)g_testCtrl->freeSize, g_memTotal0);
        }
    }
    OsDebugSetLogLevel(savedLevel);
    OS_TEST_ASSERT_EQ(TestFscCountFree(g_testCtrl), 1);
}

/* MEM VGA 汇总（Row 10，与框架 Row 0-1 不冲突） */
OS_SEC_KERNEL_TEXT void TestMemPrintVga(void)
{
    OsPrintSetCursor((U16)(10 * 80));
    kprintf("MEM: pool=0x%x free=0x%x cnt=%d",
            g_memTotal0, (U32)g_testCtrl->freeSize, TestFscCountFree(g_testCtrl));
}
