#include "os_def.h"
#include "os_print_external.h"
#include "os_mem_external.h"
#include "os_mem_fsc_internal.h"
#include "os_test.h"
#include "string.h"
#include "os_hwi.h"
#include "os_debug_external.h"
#include "os_uart_external.h"

/* ====== 缺页分配机制白盒测试 ======
 * 直接对真实内核堆(g_kernelMemPtCtrl / g_kernelVirMemPool)验证:
 *   1. 核心不变量: 每个堆页的 PTE-present == 虚拟位图位
 *   2. 缺页确实发生: FSC 分配触及新页 -> 虚拟位图 popcount 增加(位图只能由
 *      缺页处理程序 OsMemKernelAllocPgByAddr 置位,FSC 自身不碰它)
 *   3. 物理记账: 新映射的堆数据页 + 新建的堆页表 == 物理池消耗增量
 *   4. 双映射守卫: 对已映射页再调 OsMemKernelAllocPgByAddr 应被拒绝
 *   5. 原语端到端: 对未映射页映射/读写/解映射/回收,状态可还原
 *   6. 跨 PDE: 堆尾落在 PDE 769,首次映射会经 OsMapVir2Phy 建新页表
 *   7. 压力: 大量 alloc/free 后不变量与记账仍成立
 */

/* ====== 结果位 ====== */
#define PGF_FAULT_OK    0x01U /* 缺页确实映射了新页 */
#define PGF_INV_OK      0x02U /* PTE-present == 虚拟位图位(全程) */
#define PGF_PHYS_OK     0x04U /* 物理池消耗 == 新映射堆页 + 新建堆页表 */
#define PGF_GUARD_OK    0x08U /* 已映射页不被重复映射 */
#define PGF_PRIM_OK     0x10U /* 映射/读写/解映射/回收可还原 */
#define PGF_CROSSPDE_OK 0x20U /* 跨 PDE 建页表成功 */
#define PGF_STRESS_OK   0x40U /* 压力后不变量与记账仍成立 */
#define PGF_ALL_OK      0x7FU

OS_SEC_KERNEL_BSS volatile U32 g_pgfTestResult;
OS_SEC_KERNEL_BSS volatile U32 g_pgfTestFailCnt;

/* ====== 自映射地址计算(与 os_pgt.c 的 OsGetPte/PdeVirAddr 同公式) ====== */
#define PGF_PDE_VADDR(v) (0xFFFFF000U + (((U32)(v) >> 22) * 4U))
#define PGF_PTE_VADDR(v) (0xFFC00000U + (((U32)(v) & 0xFFC00000U) >> 10) + (((U32)(v) >> 12 & 0x3FFU) * 4U))
#define PGF_PRESENT      0x1U

#define PGF_HEAP_BASE    OS_KERNEL_VIR_HEAP_MEM_BASE
#define PGF_HEAP_PAGES   (OS_KERNEL_VIR_HEAP_MEM_SIZE / OS_PG_SIZE)
/* 堆跨越的 PDE: 0xC0200000-0xC05FFFFF -> PDE 768 与 769 */
#define PGF_PDE_LO       (PGF_HEAP_BASE >> 22)
#define PGF_PDE_HI       ((PGF_HEAP_BASE + OS_KERNEL_VIR_HEAP_MEM_SIZE - 1) >> 22)

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

/* 堆范围内现存的 PDE 数(每个现存 PDE = 一张从物理池分配的页表,但 PDE 768
   的页表由内核镜像预映射占用、非来自物理池;用 DELTA 时此差异自动抵消) */
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

/* 核心不变量: 遍历所有堆页,PTE-present == 虚拟位图位。返回不一致数(0=OK)。
   PDE 不存在时其下所有 PTE 视为 not present,且禁止读 PTE(读会触发自映射缺页),
   此时只要求位图位为 0。 */
static OS_SEC_KERNEL_TEXT U32 TestPgfInvariant(void)
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
        bool pteBit = TestPgfPtePresent(v);
        if (pteBit != btmpBit) {
            bad++;
        }
    }
    return bad;
}

static OS_SEC_KERNEL_TEXT void TestPgfReport(const char *name, bool ok)
{
    OsUartPrintf("[PGF] %s %s\n", name, ok ? "OK" : "FAIL");
}

OS_SEC_KERNEL_TEXT U32 OsTestPgFaultInit(void)
{
    U32 invBad = 0;
    U32 v0, p0, h0, v1, p1, h1, dVir, dPhy, dPde;
    U32 k;
    void *blks[48];
    U32 blkN;
    enum OsLogLevel savedLevel;

    g_pgfTestResult = 0;
    g_pgfTestFailCnt = 0;

    OsUartPuts("\n==== PAGE FAULT DEMAND-PAGING TEST START ====\n");

    /* 初始不变量(此时 task/sem 初始化已映射了堆首页 + 若干尾页) */
    if (TestPgfInvariant() == 0) {
        invBad = 0;
    } else {
        invBad = 1;
        OsUartPuts("[PGF] invariant broken at START\n");
    }

    /* ---- 批量 FSC 分配,强制按需缺页 ---- */
    blkN = 24;
    for (k = 0; k < blkN; k++) {
        blks[k] = OsMemKernelAlloc(3000, 16);
    }
    v0 = TestPgfVirPop();
    p0 = TestPgfPhyUsed();
    h0 = TestPgfPresentHeapPde();
    /* 再分配一批,观察 delta(确保触及新页) */
    for (k = 0; k < blkN; k++) {
        blks[blkN + k] = OsMemKernelAlloc(3000, 16);
    }
    blkN *= 2;
    v1 = TestPgfVirPop();
    p1 = TestPgfPhyUsed();
    h1 = TestPgfPresentHeapPde();

    /* PGF_FAULT_OK: 虚拟位图 popcount 增加 -> 缺页处理程序确实映射了新页 */
    {
        bool ok = (v1 > v0);
        for (k = 0; k < blkN; k++) {
            if (blks[k] == NULL) {
                ok = FALSE;
            }
        }
        if (ok) {
            g_pgfTestResult |= PGF_FAULT_OK;
        } else {
            g_pgfTestFailCnt++;
        }
        TestPgfReport("fault-maps-new-page", ok);
        OsUartPrintf("[PGF] virPop %d -> %d\n", v0, v1);
    }

    /* PGF_PHYS_OK: 物理池消耗增量 == 新映射堆页 + 新建堆页表 */
    dVir = v1 - v0;
    dPhy = p1 - p0;
    dPde = h1 - h0;
    {
        bool ok = (dPhy == dVir + dPde);
        if (ok) {
            g_pgfTestResult |= PGF_PHYS_OK;
        } else {
            g_pgfTestFailCnt++;
        }
        TestPgfReport("phys-accounting", ok);
        OsUartPrintf("[PGF] dVir=%d dPhy=%d dPde=%d\n", dVir, dPhy, dPde);
    }

    /* PGF_CROSSPDE_OK: 堆尾在 PDE 769,FSC 从尾切必然先建 PDE 769 页表 */
    {
        bool ok = TestPgfPdePresent(PGF_PDE_HI);
        if (ok) {
            g_pgfTestResult |= PGF_CROSSPDE_OK;
        } else {
            g_pgfTestFailCnt++;
        }
        TestPgfReport("cross-pde-769", ok);
    }

    /* 分配后再查不变量 */
    if (TestPgfInvariant() != 0) {
        invBad++;
        OsUartPuts("[PGF] invariant broken after ALLOC\n");
    }

    /* ---- 双映射守卫:对已映射页(堆首页,ctrl 所在)再要求映射应被拒绝 ---- */
    {
        U32 before = TestPgfVirPop();
        U32 pbefore = TestPgfPhyUsed();
        uintptr_t r;
        savedLevel = OsDebugGetLogLevel();
        OsDebugSetLogLevel(OS_LOG_NONE); /* 屏蔽 "already allocated" WARN */
        r = OsMemKernelAllocPgByAddr((uintptr_t)PGF_HEAP_BASE);
        OsDebugSetLogLevel(savedLevel);
        {
            bool ok = (r == (uintptr_t)NULL) && (TestPgfVirPop() == before) &&
                      (TestPgfPhyUsed() == pbefore);
            if (ok) {
                g_pgfTestResult |= PGF_GUARD_OK;
            } else {
                g_pgfTestFailCnt++;
            }
            TestPgfReport("guard-no-remap", ok);
        }
    }

    /* ---- 原语端到端:对未映射的中段页 映射/读写/解映射/回收 ---- */
    {
        uintptr_t mid = (uintptr_t)(PGF_HEAP_BASE + 256 * OS_PG_SIZE); /* PDE 768 中段,FSC 不会触及 */
        U32 bv0 = TestPgfVirPop();
        U32 bp0 = TestPgfPhyUsed();
        bool preUnmapped = (OsBtmpGet(&g_kernelVirMemPool.btmp, 256) == 0) && !TestPgfPtePresent(mid);
        uintptr_t r = OsMemKernelAllocPgByAddr(mid);
        bool ok = FALSE;
        if (preUnmapped && r == mid) {
            *(volatile U8 *)mid = 0x5A;
            U8 rd = *(volatile U8 *)mid;
            U32 bv1 = TestPgfVirPop();
            U32 bp1 = TestPgfPhyUsed();
            if (rd == 0x5A && OsBtmpGet(&g_kernelVirMemPool.btmp, 256) != 0 && TestPgfPtePresent(mid) &&
                (bv1 - bv0) == 1 && (bp1 - bp0) == 1) {
                /* 回收:解映射 + 清虚拟位图 + 清物理位图 */
                uintptr_t phy = OsUnmapVir2Phy(mid);
                if (phy != (uintptr_t)NULL) {
                    OsBtmpClear(&g_kernelVirMemPool.btmp, 256);
                    OsBtmpClear(&g_kernelPhyMemPool.btmp,
                                (U32)((phy - g_kernelPhyMemPool.base) / OS_PG_SIZE));
                }
                if (OsBtmpGet(&g_kernelVirMemPool.btmp, 256) == 0 && !TestPgfPtePresent(mid) &&
                    TestPgfVirPop() == bv0 && TestPgfPhyUsed() == bp0) {
                    ok = TRUE;
                }
            }
        }
        if (ok) {
            g_pgfTestResult |= PGF_PRIM_OK;
        } else {
            g_pgfTestFailCnt++;
        }
        TestPgfReport("primitive-map-verify-unmap", ok);
    }

    /* ---- 压力:大量 alloc/free 后不变量与记账仍成立 ---- */
    {
        U32 vs = TestPgfVirPop();
        U32 ps = TestPgfPhyUsed();
        U32 hs = TestPgfPresentHeapPde();
        U32 j;
        bool ok;
        void *sblk[32]; /* 独立数组,避免与 blks[] 混用导致 double-free */
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
            ok = (TestPgfInvariant() == 0) && ((pe - ps) == ((ve - vs) + (he - hs)));
        }
        if (ok) {
            g_pgfTestResult |= PGF_STRESS_OK;
        } else {
            g_pgfTestFailCnt++;
        }
        TestPgfReport("stress-invariant+accounting", ok);
    }

    /* 释放批量分配的块(堆页一旦映射不再解映射,这是按需分页的预期行为) */
    for (k = 0; k < blkN; k++) {
        if (blks[k] != NULL) {
            OsMemKernelFree(blks[k]);
        }
    }

    /* 全程不变量汇总 */
    if (TestPgfInvariant() != 0) {
        invBad++;
        OsUartPuts("[PGF] invariant broken at END\n");
    }
    {
        bool ok = (invBad == 0);
        if (ok) {
            g_pgfTestResult |= PGF_INV_OK;
        } else {
            g_pgfTestFailCnt++;
        }
        TestPgfReport("invariant-PTE==btmp", ok);
    }

    OsUartPrintf("[PGF_RESULT] 0x%x  fails=%d\n", g_pgfTestResult, g_pgfTestFailCnt);
    OsUartPuts("==== PAGE FAULT DEMAND-PAGING TEST END ====\n\n");

    OsPrintSetCursor((U16)(11 * 80));
    kprintf("PGF: result=0x%x fails=%d fault=%d inv=%d phys=%d guard=%d prim=%d xpde=%d stress=%d",
            g_pgfTestResult, g_pgfTestFailCnt,
            (g_pgfTestResult & PGF_FAULT_OK) != 0, (g_pgfTestResult & PGF_INV_OK) != 0,
            (g_pgfTestResult & PGF_PHYS_OK) != 0, (g_pgfTestResult & PGF_GUARD_OK) != 0,
            (g_pgfTestResult & PGF_PRIM_OK) != 0, (g_pgfTestResult & PGF_CROSSPDE_OK) != 0,
            (g_pgfTestResult & PGF_STRESS_OK) != 0);

    return OS_OK;
}