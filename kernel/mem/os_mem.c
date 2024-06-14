#include "os_def.h"
#include "os_mem_internal.h"
#include "os_cpu_i386.h"
#include "os_debug_external.h"
#include "os_pgt.h"

OS_SEC_KERNEL_DATA struct OsMemPool g_kernelPhyMemPool;
OS_SEC_KERNEL_DATA struct OsMemPool g_usrPhyMemPool;
OS_SEC_KERNEL_DATA struct OsMemPool g_kernelVirMemPool;

OS_SEC_KERNEL_TEXT void OsMemPoolInit(struct OsMemPool *memPool, uintptr_t memBase, 
                                      U32 memSize, U8 *btmpBase)
{
    memPool->base = memBase;
    memPool->size = memSize;

    OsBtmpInit(&memPool->btmp, btmpBase, 
               OS_ROUND_UP(memSize, OS_PG_SIZE) / OS_PG_SIZE);
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

OS_SEC_KERNEL_TEXT void OsMemConfig(void)
{
    U32 freePhyMemSize;
    U32 freeKernelPhyMemSize;
    U32 freeUsrPhyMemSize;
    U32 freeKernelPhyMemPgNum;
    U32 freeUsrPhyMemPgNum;
    U32 freeKernelPhyMemBase;
    U32 freeUsrPhyMemBase;

    OS_DEBUG_PRINT_STR("OsMemConfig start\n");
    freePhyMemSize = OS_GET_FREE_PHY_MEM_SIZE(OS_USED_PHY_MEM_SIZE);
    freeKernelPhyMemSize = OS_GET_FREE_KERNEL_PHY_MEM_SIZE(freePhyMemSize);
    freeUsrPhyMemSize = OS_GET_FREE_USR_PHY_MEM_SIZE(freePhyMemSize);
    freeKernelPhyMemBase = OS_USED_PHY_MEM_SIZE;
    freeUsrPhyMemBase = freeKernelPhyMemBase + freeKernelPhyMemSize;

    OsMemPoolInit(&g_kernelPhyMemPool, (uintptr_t)freeKernelPhyMemBase,
                  freeKernelPhyMemSize, (U8 *)OS_MEM_BTMP0_BASE);    
    OsMemPoolInit(&g_usrPhyMemPool, (uintptr_t)freeUsrPhyMemBase,
                  freeUsrPhyMemSize, (U8 *)OS_MEM_BTMP1_BASE);
    OsMemPoolInit(&g_kernelVirMemPool, (uintptr_t)OS_KERNEL_VIR_MEM_BASE,
                  1024 * 1024 * 32, (U8 *)OS_MEM_BTMP2_BASE);

    OsPrintMemPoolInfo(&g_kernelPhyMemPool, "kernelPhyMemPool");
    OsPrintMemPoolInfo(&g_usrPhyMemPool, "usrPhyMemPool");
    OsPrintMemPoolInfo(&g_kernelVirMemPool, "kernelVirMemPool");
    OS_DEBUG_PRINT_STR("OsMemConfig end\n");
}

OS_SEC_KERNEL_TEXT uintptr_t OsMemPoolGetFreePgs(struct OsMemPool *pool, U32 cnt)
{
    uintptr_t addr;
    U32 idx;
    struct OsBtmp *btmp = &pool->btmp;
    U32 i;

    if (!OsBtmpScan(btmp, cnt, 0, &idx)) {
        OS_DEBUG_PRINT_STR("OsMemPoolGetFreePg: free pages not enough\n");
        return NULL;
    }

    addr = (uintptr_t)((U32)pool->base + idx * OS_PG_SIZE);

    for (i = 0; i < cnt; i++) {
        OsBtmpSet(btmp, idx + i);
    }

    return addr;
}

/* 内核申请内存 */
OS_SEC_KERNEL_TEXT uintptr_t OsMemKernelAllocPgs(U32 cnt)
{
    U32 i;
    uintptr_t virAddrBase;
    uintptr_t phyAddr;
    U32 virAddr;

    virAddrBase = OsMemPoolGetFreePgs(&g_kernelVirMemPool, cnt);
    if (virAddrBase == NULL) {
        OS_DEBUG_PRINT_STR("OsMemKernelAllocPgs: OsMemPoolGetFreePgs failed\n");
        return NULL;
    }

    virAddr = (U32)virAddrBase;
    for (i = 0; i < cnt; i++) {
        phyAddr = OsMemPoolGetFreePgs(&g_kernelPhyMemPool, 1);
        if (phyAddr == NULL) {
            OS_DEBUG_PRINT_STR("OsMemKernelAllocPgs: OsMemPoolGetFreePgs failed\n");
            return NULL;
        }  

        OsMapVir2Phy((uintptr_t)virAddr, phyAddr);

        virAddr += OS_PG_SIZE;
    }

    return virAddrBase;
}