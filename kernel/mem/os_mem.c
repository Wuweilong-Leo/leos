#include "os_def.h"
#include "os_mem_internal.h"
#include "os_cpu.h"
#include "os_debug_external.h"
#include "os_sched_external.h"
#include "os_mem_fsc_internal.h"

OS_SEC_KERNEL_DATA struct OsMemPool g_kernelPhyMemPool;
OS_SEC_KERNEL_DATA struct OsMemPool g_usrPhyMemPool;
OS_SEC_KERNEL_DATA struct OsMemPool g_kernelVirMemPool;
/* 每个btmp默认先给一页大小 */
OS_SEC_KERNEL_BSS U8 g_memPoolBtmp[OS_MEM_BTMP_MAX_NUM][OS_PG_SIZE];
OS_SEC_KERNEL_BSS struct OsMemFscCtrl *g_kernelMemPtCtrl;

OS_SEC_KERNEL_TEXT void OsMemPoolInit(struct OsMemPool *memPool, uintptr_t memBase, size_t memSize,
                                      U8 *btmpBase)
{
    memPool->base = memBase;
    memPool->size = memSize;

    OsBtmpInit(&memPool->btmp, btmpBase, OS_ROUND_UP(memSize, OS_PG_SIZE) / OS_PG_SIZE);
    OsListInit(&memPool->memCtrlList);
}

static OS_SEC_KERNEL_TEXT void OsPrintMemPoolInfo(struct OsMemPool *memPool, char *poolName)
{
    OS_DEBUG_PRINT_STR("mem pool info start\n");
    OS_DEBUG_PRINT_STR("name :");
    OS_DEBUG_PRINT_STR(poolName);
    OS_DEBUG_PRINT_STR("\n");
    OS_DEBUG_PRINT_STR("pool mem base: ");
    OS_DEBUG_PRINT_HEX((uintptr_t)memPool->base);
    OS_DEBUG_PRINT_STR("\n");
    OS_DEBUG_PRINT_STR("pool mem size: ");
    OS_DEBUG_PRINT_HEX((size_t)memPool->size);
    OS_DEBUG_PRINT_STR("\n");
    OS_DEBUG_PRINT_STR("pool btmp base: ");
    OS_DEBUG_PRINT_HEX((uintptr_t)memPool->btmp.base);
    OS_DEBUG_PRINT_STR("\n");
    OS_DEBUG_PRINT_STR("pool btmp bit num: ");
    OS_DEBUG_PRINT_HEX((size_t)memPool->btmp.bitNum);
    OS_DEBUG_PRINT_STR("\n");
    OS_DEBUG_PRINT_STR("pool btmp byte len: ");
    OS_DEBUG_PRINT_HEX((size_t)memPool->btmp.byteLen);
    OS_DEBUG_PRINT_STR("\n");
    OS_DEBUG_PRINT_STR("mem pool info end\n");
}

OS_SEC_KERNEL_TEXT U32 OsMemConfigInit(void)
{
    size_t freePhyMemSize;
    size_t freeKernelPhyMemSize;
    size_t freeUsrPhyMemSize;
    U32 freeKernelPhyMemPgNum;
    U32 freeUsrPhyMemPgNum;
    uintptr_t freeKernelPhyMemBase;
    uintptr_t freeUsrPhyMemBase;
    struct OsMemFscCtrl *kernelMemCtrl;

    OS_DEBUG_PRINT_STR("OsMemConfig start\n");
    freePhyMemSize = OS_GET_FREE_PHY_MEM_SIZE(OS_USED_PHY_MEM_SIZE);
    freeKernelPhyMemSize = OS_GET_FREE_KERNEL_PHY_MEM_SIZE(freePhyMemSize);
    freeUsrPhyMemSize = OS_GET_FREE_USR_PHY_MEM_SIZE(freePhyMemSize);
    freeKernelPhyMemBase = OS_USED_PHY_MEM_SIZE;
    freeUsrPhyMemBase = freeKernelPhyMemBase + freeKernelPhyMemSize;

    OsMemPoolInit(&g_kernelPhyMemPool, freeKernelPhyMemBase, freeKernelPhyMemSize,
                  (U8 *)g_memPoolBtmp[0]);
    OsMemPoolInit(&g_usrPhyMemPool, freeUsrPhyMemBase, freeUsrPhyMemSize,
                  (U8 *)g_memPoolBtmp[1]);
    OsMemPoolInit(&g_kernelVirMemPool, (uintptr_t)OS_KERNEL_VIR_HEAP_MEM_BASE,
                  OS_KERNEL_VIR_HEAP_MEM_SIZE, (U8 *)g_memPoolBtmp[2]);

    /*
     * 虚拟堆不预先映射：OsMemFscInitPt 只写第 0 页(控块+freeList+首个空闲块头都在堆首)，
     * 该写入触发缺页，由 OsExcHandleKernelPgFault -> OsMemKernelAllocPgByAddr 按需补一页物理并建映射。
     * 后续 FSC 向堆尾切片时，每碰到一个未映射页再缺页补一页，堆按需长出，虚拟位图由缺页处理程序逐位置位。
     */
    g_kernelMemPtCtrl = OsMemFscInitPt(OS_KERNEL_VIR_HEAP_MEM_BASE, OS_KERNEL_VIR_HEAP_MEM_SIZE);
    OS_DEBUG_KPRINT("g_kernelMemPtCtrl = 0x%x\n", (uintptr_t)g_kernelMemPtCtrl);

    OS_DEBUG_PRINT_STR("OsMemConfig end\n");
    return OS_OK;
}

OS_SEC_KERNEL_TEXT uintptr_t OsMemPoolGetFreePgs(struct OsMemPool *pool, size_t cnt)
{
    uintptr_t addr;
    U32 idx;
    struct OsBtmp *btmp = &pool->btmp;
    size_t i;

    if (!OsBtmpScan(btmp, cnt, 0, &idx)) {
        OS_LOG_WARN("pool 0x%x needs %u pages, not enough\n", (uintptr_t)pool->base, (size_t)cnt);
        return NULL;
    }

    addr = pool->base + (uintptr_t)idx * OS_PG_SIZE;

    for (i = 0; i < cnt; i++) {
        OsBtmpSet(btmp, idx + (U32)i);  /* idx is U32 from OsBtmpScan */
    }

    return addr;
}

/* 回滚：取消已映射的页表项、释放已占用的物理页与虚拟页 */
static OS_SEC_KERNEL_TEXT void OsMemAllocPgsRollback(struct OsMemPool *virMemPool,
                                                     struct OsMemPool *phyMemPool,
                                                     uintptr_t virAddrBase, size_t cnt, size_t allocated)
{
    size_t i;
    uintptr_t virAddr = virAddrBase;
    uintptr_t phyAddr;

    for (i = 0; i < allocated; i++) {
        phyAddr = OsUnmapVir2Phy(virAddr);
        if (phyAddr != (uintptr_t)NULL) {
            size_t phyIdx = (phyAddr - phyMemPool->base) / OS_PG_SIZE;
            OsBtmpClear(&phyMemPool->btmp, phyIdx);
        }
        virAddr += OS_PG_SIZE;
    }
    {
        size_t virIdx = (virAddrBase - virMemPool->base) / OS_PG_SIZE;
        for (i = 0; i < cnt; i++) {
            OsBtmpClear(&virMemPool->btmp, virIdx + i);
        }
    }
}

OS_SEC_KERNEL_TEXT uintptr_t OsMemAllocPgs(enum OsMemFlag flag, size_t cnt)
{
    struct OsMemPool *virMemPool;
    struct OsMemPool *phyMemPool;
    uintptr_t virAddr;
    uintptr_t virAddrBase;
    size_t i;
    uintptr_t phyAddr;
    size_t allocated = 0;

    if (flag == OS_MEM_KERNEL) {
        virMemPool = &g_kernelVirMemPool;
        phyMemPool = &g_kernelPhyMemPool;
    } else {
        virMemPool = &OS_RUNNING_TASK()->usrVirMemPool;
        phyMemPool = &g_usrPhyMemPool;
    }

    virAddrBase = OsMemPoolGetFreePgs(virMemPool, cnt);
    if (virAddrBase == (uintptr_t)NULL) {
        OS_LOG_ERROR("virMemPool get free pgs failed, cnt=%u\n", (size_t)cnt);
        return (uintptr_t)NULL;
    }

    virAddr = virAddrBase;
    for (i = 0; i < cnt; i++) {
        phyAddr = OsMemPoolGetFreePgs(phyMemPool, 1);
        if (phyAddr == (uintptr_t)NULL) {
            OS_LOG_ERROR("phyMemPool alloc page %u/%u failed, rollback\n", (size_t)allocated, (size_t)cnt);
            OsMemAllocPgsRollback(virMemPool, phyMemPool, virAddrBase, cnt, allocated);
            return (uintptr_t)NULL;
        }
        if (!OsMapVir2Phy(virAddr, phyAddr)) {
            OS_LOG_ERROR("OsMemAllocPgs: map vaddr 0x%x failed, rollback\n", (uintptr_t)virAddr);
            /* 本页物理已分配但未映射成功，先归还 */
            OsBtmpClear(&phyMemPool->btmp, (phyAddr - phyMemPool->base) / OS_PG_SIZE);
            OsMemAllocPgsRollback(virMemPool, phyMemPool, virAddrBase, cnt, allocated);
            return (uintptr_t)NULL;
        }
        virAddr += OS_PG_SIZE;
        allocated++;
    }

    return virAddrBase;
}

/* 内核申请内存 */
OS_SEC_KERNEL_TEXT uintptr_t OsMemKernelAllocPgs(size_t cnt)
{
    return OsMemAllocPgs(OS_MEM_KERNEL, cnt);
}

/* 用户申请内存 */
OS_SEC_KERNEL_TEXT uintptr_t OsMemUsrAllocPgs(size_t cnt)
{
    return OsMemAllocPgs(OS_MEM_USR, cnt);
}

OS_SEC_KERNEL_TEXT uintptr_t OsMemAllocPgByAddr(enum OsMemFlag flag, uintptr_t virAddr)
{
    struct OsMemPool *virMemPool;
    struct OsMemPool *phyMemPool;
    uintptr_t phyAddr;
    size_t idx;

    if (flag == OS_MEM_KERNEL) {
        virMemPool = &g_kernelVirMemPool;
        phyMemPool = &g_kernelPhyMemPool;
    } else {
        virMemPool = &OS_RUNNING_TASK()->usrVirMemPool;
        phyMemPool = &g_usrPhyMemPool;
    }

    idx = (virAddr - virMemPool->base) / OS_PG_SIZE;
    /* 这个地址已经被分配出去了 */
    if (OsBtmpGet(&virMemPool->btmp, idx) != 0) {
        OS_LOG_WARN("vaddr 0x%x already allocated\n", (uintptr_t)virAddr);
        return NULL;
    }

    phyAddr = OsMemPoolGetFreePgs(phyMemPool, 1);
    if (phyAddr == (uintptr_t)NULL) {
        OS_LOG_ERROR("phyMemPool get free pgs failed, vaddr=0x%x\n",
                     (uintptr_t)virAddr);
        return NULL;
    }

    /* 进行虚实映射；失败则归还物理页，虚拟位图保持不变 */
    if (!OsMapVir2Phy(virAddr, phyAddr)) {
        OsBtmpClear(&phyMemPool->btmp, (phyAddr - phyMemPool->base) / OS_PG_SIZE);
        return NULL;
    }

    /* 虚拟地址位图置1 */
    OsBtmpSet(&virMemPool->btmp, idx);

    return virAddr;
}

/* 指定一个地址分配内存 */
OS_SEC_KERNEL_TEXT uintptr_t OsMemKernelAllocPgByAddr(uintptr_t virAddr)
{
    return OsMemAllocPgByAddr(OS_MEM_KERNEL, virAddr);
}

/* 指定一个地址分配内存 */
OS_SEC_KERNEL_TEXT uintptr_t OsMemUsrAllocPgByAddr(uintptr_t virAddr)
{
    return OsMemAllocPgByAddr(OS_MEM_USR, virAddr);
}

OS_SEC_KERNEL_TEXT void *OsMemKernelAlloc(size_t size, U32 align)
{
    return OsMemFscAlloc(g_kernelMemPtCtrl, size, align);
}

OS_SEC_KERNEL_TEXT void OsMemKernelFree(void *addr)
{
    return OsMemFscFree(addr);
}
