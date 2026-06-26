#include "os_def.h"
#include "os_print_external.h"
#include "os_mem_external.h"
#include "os_mem_fsc_internal.h"
#include "os_test_framework.h"
#include "string.h"
#include "os_hwi.h"
#include "os_debug_external.h"
#include "os_uart_external.h"

/* ====== 缺页分配机制白盒测试 ====== */

#define PGF_PRESENT      0x1U

#define PGF_HEAP_BASE    OS_KERNEL_VIR_HEAP_MEM_BASE
#define PGF_HEAP_PAGES   (OS_KERNEL_VIR_HEAP_MEM_SIZE / OS_PG_SIZE)
#define PGF_PDE_LO       (PGF_HEAP_BASE >> 22)
#define PGF_PDE_HI       ((PGF_HEAP_BASE + OS_KERNEL_VIR_HEAP_MEM_SIZE - 1) >> 22)

#define PGF_PDE_VADDR(v) (0xFFFFF000U + (((U32)(v) >> 22) * 4U))
#define PGF_PTE_VADDR(v) (0xFFC00000U + (((U32)(v) & 0xFFC00000U) >> 10) + (((U32)(v) >> 12 & 0x3FFU) * 4U))

/* ====== 共享状态 ====== */
OS_SEC_KERNEL_BSS void *g_pgfBlks[48];
OS_SEC_KERNEL_BSS U32 g_pgfBlkN;
OS_SEC_KERNEL_BSS U32 g_pgfV0, g_pgfP0, g_pgfH0;   /* 第一批快照 */
OS_SEC_KERNEL_BSS U32 g_pgfV1, g_pgfP1, g_pgfH1;   /* 第二批快照 */

/* ====== 工具函数 ====== */

static OS_SEC_KERNEL_TEXT U32 TestPgfBtmpPop(struct OsBtmp *b, U32 n)
{
    U32 i, c = 0;
    for (i = 0; i < n; i++) {
        if (OsBtmpGet(b, i) != 0) {
            c++;
        }
    }
    return c;
}

static OS_SEC_KERNEL_TEXT U32 TestPgfVirPop(void)
{
    return TestPgfBtmpPop(&g_kernelVirMemPool.btmp, PGF_HEAP_PAGES);
}

static OS_SEC_KERNEL_TEXT U32 TestPgfPhyUsed(void)
{
    return TestPgfBtmpPop(&g_kernelPhyMemPool.btmp, g_kernelPhyMemPool.btmp.bitNum);
}

static OS_SEC_KERNEL_TEXT bool TestPgfPdePresent(U32 pdeIdx)
{
    return (*(volatile U32 *)PGF_PDE_VADDR((uintptr_t)(pdeIdx << 22)) & PGF_PRESENT) != 0;
}

static OS_SEC_KERNEL_TEXT bool TestPgfPtePresent(uintptr_t v)
{
    return (*(volatile U32 *)PGF_PTE_VADDR(v) & PGF_PRESENT) != 0;
}

static OS_SEC_KERNEL_TEXT U32 TestPgfPresentHeapPde(void)
{
    U32 cnt = 0, idx;
    for (idx = PGF_PDE_LO; idx <= PGF_PDE_HI; idx++) {
        if (TestPgfPdePresent(idx)) {
            cnt++;
        }
    }
    return cnt;
}

OS_SEC_KERNEL_TEXT U32 TestPgfInvariant(void)
{
    U32 i, bad = 0;
    for (i = 0; i < PGF_HEAP_PAGES; i++) {
        uintptr_t v = (uintptr_t)(PGF_HEAP_BASE + i * OS_PG_SIZE);
        U32 pdeIdx = (U32)(v >> 22);
        bool btmpBit = OsBtmpGet(&g_kernelVirMemPool.btmp, i) != 0;
        if (!TestPgfPdePresent(pdeIdx)) {
            if (btmpBit) {
                bad++;
            }
            continue;
        }
        if (TestPgfPtePresent(v) != btmpBit) {
            bad++;
        }
    }
    return bad;
}

/* ====== setup: 批量分配 + 快照 ====== */

OS_SEC_KERNEL_TEXT void TestPgfSetup(void)
{
    U32 k;
    g_pgfBlkN = 24;
    for (k = 0; k < g_pgfBlkN; k++) {
        g_pgfBlks[k] = OsMemKernelAlloc(3000, 16);
    }
    g_pgfV0 = TestPgfVirPop();
    g_pgfP0 = TestPgfPhyUsed();
    g_pgfH0 = TestPgfPresentHeapPde();

    for (k = 0; k < g_pgfBlkN; k++) {
        g_pgfBlks[g_pgfBlkN + k] = OsMemKernelAlloc(3000, 16);
    }
    g_pgfBlkN *= 2;
    g_pgfV1 = TestPgfVirPop();
    g_pgfP1 = TestPgfPhyUsed();
    g_pgfH1 = TestPgfPresentHeapPde();
}

/* ====== 用例 ====== */

OS_SEC_KERNEL_TEXT void TestPgfFault(void)
{
    U32 k;
    OS_TEST_ASSERT(g_pgfV1 > g_pgfV0);
    for (k = 0; k < g_pgfBlkN; k++) {
        OS_TEST_ASSERT(g_pgfBlks[k] != NULL);
    }
}

OS_SEC_KERNEL_TEXT void TestPgfPhys(void)
{
    U32 dVir = g_pgfV1 - g_pgfV0;
    U32 dPhy = g_pgfP1 - g_pgfP0;
    U32 dPde = g_pgfH1 - g_pgfH0;
    OS_TEST_ASSERT_EQ(dPhy, dVir + dPde);
}

OS_SEC_KERNEL_TEXT void TestPgfCrossPde(void)
{
    OS_TEST_ASSERT(TestPgfPdePresent(PGF_PDE_HI));
}

OS_SEC_KERNEL_TEXT void TestPgfGuard(void)
{
    U32 before = TestPgfVirPop();
    U32 pbefore = TestPgfPhyUsed();
    uintptr_t r;
    enum OsLogLevel savedLevel = OsDebugGetLogLevel();
    OsDebugSetLogLevel(OS_LOG_NONE);
    r = OsMemKernelAllocPgByAddr((uintptr_t)PGF_HEAP_BASE);
    OsDebugSetLogLevel(savedLevel);
    OS_TEST_ASSERT(r == (uintptr_t)NULL);
    OS_TEST_ASSERT_EQ(TestPgfVirPop(), before);
    OS_TEST_ASSERT_EQ(TestPgfPhyUsed(), pbefore);
}

OS_SEC_KERNEL_TEXT void TestPgfPrimitive(void)
{
    uintptr_t mid = (uintptr_t)(PGF_HEAP_BASE + 256 * OS_PG_SIZE);
    U32 bv0 = TestPgfVirPop();
    U32 bp0 = TestPgfPhyUsed();
    bool preUnmapped = (OsBtmpGet(&g_kernelVirMemPool.btmp, 256) == 0) && !TestPgfPtePresent(mid);
    uintptr_t r = OsMemKernelAllocPgByAddr(mid);

    if (!(preUnmapped && r == mid)) {
        OS_TEST_ASSERT(0);
        return;
    }
    *(volatile U8 *)mid = 0x5A;
    {
        U8 rd = *(volatile U8 *)mid;
        U32 bv1 = TestPgfVirPop();
        U32 bp1 = TestPgfPhyUsed();
        OS_TEST_ASSERT(rd == 0x5A);
        OS_TEST_ASSERT(OsBtmpGet(&g_kernelVirMemPool.btmp, 256) != 0);
        OS_TEST_ASSERT(TestPgfPtePresent(mid));
        OS_TEST_ASSERT_EQ(bv1 - bv0, 1);
        OS_TEST_ASSERT_EQ(bp1 - bp0, 1);
    }
    {
        uintptr_t phy = OsUnmapVir2Phy(mid);
        if (phy != (uintptr_t)NULL) {
            OsBtmpClear(&g_kernelVirMemPool.btmp, 256);
            OsBtmpClear(&g_kernelPhyMemPool.btmp,
                        (U32)((phy - g_kernelPhyMemPool.base) / OS_PG_SIZE));
        }
        OS_TEST_ASSERT(OsBtmpGet(&g_kernelVirMemPool.btmp, 256) == 0);
        OS_TEST_ASSERT(!TestPgfPtePresent(mid));
        OS_TEST_ASSERT_EQ(TestPgfVirPop(), bv0);
        OS_TEST_ASSERT_EQ(TestPgfPhyUsed(), bp0);
    }
}

OS_SEC_KERNEL_TEXT void TestPgfStress(void)
{
    U32 vs = TestPgfVirPop();
    U32 ps = TestPgfPhyUsed();
    U32 hs = TestPgfPresentHeapPde();
    U32 j;
    void *sblk[32];

    for (j = 0; j < 32; j++) {
        sblk[j] = OsMemKernelAlloc(2000, 16);
    }
    for (j = 0; j < 32; j++) {
        if (sblk[j] != NULL) {
            OsMemKernelFree(sblk[j]);
        }
    }
    for (j = 0; j < 16; j++) {
        sblk[j] = OsMemKernelAlloc(4000, 16);
    }
    for (j = 0; j < 16; j++) {
        if (sblk[j] != NULL) {
            OsMemKernelFree(sblk[j]);
        }
    }
    {
        U32 ve = TestPgfVirPop();
        U32 pe = TestPgfPhyUsed();
        U32 he = TestPgfPresentHeapPde();
        OS_TEST_ASSERT_EQ(TestPgfInvariant(), 0);
        OS_TEST_ASSERT_EQ(pe - ps, (ve - vs) + (he - hs));
    }
}

OS_SEC_KERNEL_TEXT void TestPgfInvariantCase(void)
{
    OS_TEST_ASSERT_EQ(TestPgfInvariant(), 0);
    /* 释放批量块 */
    {
        U32 k;
        for (k = 0; k < g_pgfBlkN; k++) {
            if (g_pgfBlks[k] != NULL) {
                OsMemKernelFree(g_pgfBlks[k]);
            }
        }
    }
    OS_TEST_ASSERT_EQ(TestPgfInvariant(), 0);
}

/* PGF VGA 汇总（Row 11） */
OS_SEC_KERNEL_TEXT void TestPgfPrintVga(void)
{
    OsPrintSetCursor((U16)(11 * 80));
    kprintf("PGF: vir=%d phy=%d pde=%d inv=%d",
            TestPgfVirPop(), TestPgfPhyUsed(),
            TestPgfPresentHeapPde(), TestPgfInvariant());
}
