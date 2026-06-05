#include "os_def.h"
#include "os_mem_internal.h"
#include "os_cpu.h"
#include "os_debug_external.h"
#include "os_pgt.h"
#include "os_sched_external.h"
#include "os_mem_fsc_internal.h"

OS_SEC_KERNEL_DATA struct OsMemPool g_kernelPhyMemPool;
OS_SEC_KERNEL_DATA struct OsMemPool g_usrPhyMemPool;
OS_SEC_KERNEL_DATA struct OsMemPool g_kernelVirMemPool;
/* 每个btmp默认先给一页大小 */
OS_SEC_KERNEL_BSS U8 g_memPoolBtmp[OS_MEM_BTMP_MAX_NUM][OS_PG_SIZE];
OS_SEC_KERNEL_BSS struct OsMemFscCtrl *g_kernelMemPtCtrl;

OS_SEC_KERNEL_TEXT void OsMemPoolInit(struct OsMemPool *memPool, uintptr_t memBase, U32 memSize,
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
    OS_DEBUG_PRINT_HEX((U32)memPool->base);
    OS_DEBUG_PRINT_STR("\n");
    OS_DEBUG_PRINT_STR("pool mem size: ");
    OS_DEBUG_PRINT_HEX((U32)memPool->size);
    OS_DEBUG_PRINT_STR("\n");
    OS_DEBUG_PRINT_STR("pool btmp base: ");
    OS_DEBUG_PRINT_HEX((U32)memPool->btmp.base);
    OS_DEBUG_PRINT_STR("\n");
    OS_DEBUG_PRINT_STR("pool btmp bit num: ");
    OS_DEBUG_PRINT_HEX((U32)memPool->btmp.bitNum);
    OS_DEBUG_PRINT_STR("\n");
    OS_DEBUG_PRINT_STR("pool btmp byte len: ");
    OS_DEBUG_PRINT_HEX((U32)memPool->btmp.byteLen);
    OS_DEBUG_PRINT_STR("\n");
    OS_DEBUG_PRINT_STR("mem pool info end\n");
}

OS_SEC_KERNEL_TEXT U32 OsMemConfigInit(void)
{
    U32 freePhyMemSize;
    U32 freeKernelPhyMemSize;
    U32 freeUsrPhyMemSize;
    U32 freeKernelPhyMemPgNum;
    U32 freeUsrPhyMemPgNum;
    U32 freeKernelPhyMemBase;
    U32 freeUsrPhyMemBase;
    struct OsMemFscCtrl *kernelMemCtrl;

    OS_DEBUG_PRINT_STR("OsMemConfig start\n");
    freePhyMemSize = OS_GET_FREE_PHY_MEM_SIZE(OS_USED_PHY_MEM_SIZE);
    freeKernelPhyMemSize = OS_GET_FREE_KERNEL_PHY_MEM_SIZE(freePhyMemSize);
    freeUsrPhyMemSize = OS_GET_FREE_USR_PHY_MEM_SIZE(freePhyMemSize);
    freeKernelPhyMemBase = OS_USED_PHY_MEM_SIZE;
    freeUsrPhyMemBase = freeKernelPhyMemBase + freeKernelPhyMemSize;

    OsMemPoolInit(&g_kernelPhyMemPool, (uintptr_t)freeKernelPhyMemBase, freeKernelPhyMemSize,
                  (U8 *)g_memPoolBtmp[0]);
    OsMemPoolInit(&g_usrPhyMemPool, (uintptr_t)freeUsrPhyMemBase, freeUsrPhyMemSize,
                  (U8 *)g_memPoolBtmp[1]);
    OsMemPoolInit(&g_kernelVirMemPool, (uintptr_t)OS_KERNEL_VIR_HEAP_MEM_BASE,
                  OS_KERNEL_VIR_HEAP_MEM_SIZE, (U8 *)g_memPoolBtmp[2]);

    /* 为虚拟堆映射所有物理页，OsMemFscInitPt需要整个区域可写 */
    {
        U32 heapPages = OS_KERNEL_VIR_HEAP_MEM_SIZE / OS_PG_SIZE;
        U32 mapOffset;
        for (mapOffset = 0; mapOffset < heapPages * OS_PG_SIZE; mapOffset += OS_PG_SIZE)
        {
            OsMapVir2Phy((uintptr_t)OS_KERNEL_VIR_HEAP_MEM_BASE + mapOffset,
                         OsMemPoolGetFreePgs(&g_kernelPhyMemPool, 1));
        }
    }

    g_kernelMemPtCtrl = OsMemFscInitPt(OS_KERNEL_VIR_HEAP_MEM_BASE, OS_KERNEL_VIR_HEAP_MEM_SIZE);
    OS_DEBUG_KPRINT("g_kernelMemPtCtrl = 0x%x\n", (U32)g_kernelMemPtCtrl);

    OS_DEBUG_PRINT_STR("OsMemConfig end\n");
    return OS_OK;
}

OS_SEC_KERNEL_TEXT uintptr_t OsMemPoolGetFreePgs(struct OsMemPool *pool, U32 cnt)
{
    uintptr_t addr;
    U32 idx;
    struct OsBtmp *btmp = &pool->btmp;
    U32 i;

    if (!OsBtmpScan(btmp, cnt, 0, &idx))
    {
        OS_LOG_WARN("OsMemPoolGetFreePgs: pool 0x%x needs %u pages, not enough\n", (U32)pool->base,
                    cnt);
        return NULL;
    }

    addr = (uintptr_t)((U32)pool->base + idx * OS_PG_SIZE);

    for (i = 0; i < cnt; i++)
    {
        OsBtmpSet(btmp, idx + i);
    }

    return addr;
}

OS_SEC_KERNEL_TEXT uintptr_t OsMemAllocPgs(enum OsMemFlag flag, U32 cnt)
{
    struct OsMemPool *virMemPool;
    struct OsMemPool *phyMemPool;
    U32 virAddr;
    uintptr_t virAddrBase;
    U32 i;
    uintptr_t phyAddr;
    U32 allocated = 0;

    if (flag == OS_MEM_KERNEL)
    {
        virMemPool = &g_kernelVirMemPool;
        phyMemPool = &g_kernelPhyMemPool;
    }
    else
    {
        virMemPool = &OS_RUNNING_TASK()->usrVirMemPool;
        phyMemPool = &g_usrPhyMemPool;
    }

    virAddrBase = OsMemPoolGetFreePgs(virMemPool, cnt);
    if (virAddrBase == (uintptr_t)NULL)
    {
        OS_LOG_ERROR("OsMemAllocPgs: virMemPool get free pgs failed, cnt=%u\n", cnt);
        return (uintptr_t)NULL;
    }

    virAddr = (U32)virAddrBase;
    for (i = 0; i < cnt; i++)
    {
        phyAddr = OsMemPoolGetFreePgs(phyMemPool, 1);
        if (phyAddr == (uintptr_t)NULL)
        {
            OS_LOG_ERROR("OsMemAllocPgs: phyMemPool alloc page %u/%u failed, rollback\n", allocated,
                         cnt);
            /* 回滚：清除已映射的页表项并释放物理页 */
            virAddr = (U32)virAddrBase;
            for (i = 0; i < allocated; i++)
            {
                /* 页表项虚拟地址 = 0xFFC00000 + (vaddr>>10) + PTE索引*4 */
                U32 pdeIdx = virAddr >> 22;
                U32 pteIdx = (virAddr >> 12) & 0x3FF;
                U32 pteVaddr = 0xFFC00000 + (pdeIdx << 12) + pteIdx * 4;
                U32 pteVal = *(U32 *)pteVaddr;
                if ((pteVal & 1) != 0)
                {
                    phyAddr = pteVal & 0xFFFFF000;
                    *(U32 *)pteVaddr = 0;
                    U32 phyIdx = (phyAddr - (U32)phyMemPool->base) / OS_PG_SIZE;
                    OsBtmpClear(&phyMemPool->btmp, phyIdx);
                }
                virAddr += OS_PG_SIZE;
            }
            /* 回滚：释放虚拟页 */
            {
                U32 virIdx = ((U32)virAddrBase - (U32)virMemPool->base) / OS_PG_SIZE;
                for (i = 0; i < cnt; i++)
                {
                    OsBtmpClear(&virMemPool->btmp, virIdx + i);
                }
            }
            return (uintptr_t)NULL;
        }
        OsMapVir2Phy((uintptr_t)virAddr, phyAddr);
        virAddr += OS_PG_SIZE;
        allocated++;
    }

    return virAddrBase;
}

/* 内核申请内存 */
OS_SEC_KERNEL_TEXT uintptr_t OsMemKernelAllocPgs(U32 cnt)
{
    return OsMemAllocPgs(OS_MEM_KERNEL, cnt);
}

/* 用户申请内存 */
OS_SEC_KERNEL_TEXT uintptr_t OsMemUsrAllocPgs(U32 cnt)
{
    return OsMemAllocPgs(OS_MEM_USR, cnt);
}

OS_SEC_KERNEL_TEXT uintptr_t OsMemAllocPgByAddr(enum OsMemFlag flag, uintptr_t virAddr)
{
    struct OsMemPool *virMemPool;
    struct OsMemPool *phyMemPool;
    uintptr_t phyAddr;
    U32 idx;

    if (flag == OS_MEM_KERNEL)
    {
        virMemPool = &g_kernelVirMemPool;
        phyMemPool = &g_kernelPhyMemPool;
    }
    else
    {
        virMemPool = &OS_RUNNING_TASK()->usrVirMemPool;
        phyMemPool = &g_usrPhyMemPool;
    }

    idx = ((U32)virAddr - (U32)virMemPool->base) / OS_PG_SIZE;
    /* 这个地址已经被分配出去了 */
    if (OsBtmpGet(&virMemPool->btmp, idx) != 0)
    {
        OS_LOG_WARN("OsMemAllocPgByAddr: vaddr 0x%x already allocated\n", (U32)virAddr);
        return NULL;
    }

    phyAddr = OsMemPoolGetFreePgs(phyMemPool, 1);
    if (phyAddr == (uintptr_t)NULL)
    {
        OS_LOG_ERROR("OsMemAllocPgByAddr: phyMemPool get free pgs failed, vaddr=0x%x\n",
                     (U32)virAddr);
        return NULL;
    }

    /* 进行虚实映射 */
    OsMapVir2Phy(virAddr, phyAddr);

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