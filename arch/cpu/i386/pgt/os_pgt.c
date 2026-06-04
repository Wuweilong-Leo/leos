#include "os_def.h"
#include "os_pgt.h"
#include "string.h"
#include "os_cpu.h"
#include "os_io_i386.h"
#include "os_debug_external.h"
#include "os_mem_external.h"

OS_SEC_PGT_DATA struct OsPgtEntry g_pgd[OS_PGD_ENTRY_NUM];
OS_SEC_PGT_DATA struct OsPgtEntry g_pgt[256][OS_PGD_ENTRY_NUM];

OS_INLINE void OsCleanPgd(void)
{
    U8 *pgd = (U8 *)g_pgd;
    U32 i;

    for (i = 0; i < sizeof(g_pgd); i++) {
        pgd[i] = 0;
    }
}

OS_SEC_LOADER_TEXT void OsSetupPgt(void) 
{
    struct OsPgtEntry *pgd = &g_pgd[0];
    U32 addr = 0;
    U32 i;
    U32 firstPgtBase = &g_pgt[0][0];

    /* 清空页目录项 */
    OsCleanPgd();

    /* 把虚拟地址1M和3G+1M都映射到物理地址的1M内，都指向第一张页表 */
    *(U32 *)((uintptr_t)pgd + 0)= firstPgtBase | OS_PG_P | OS_PG_RW_W | OS_PG_US_U;
    *(U32 *)((uintptr_t)pgd + 0xc00) = firstPgtBase | OS_PG_P | OS_PG_RW_W | OS_PG_US_U;

    /* 最后一个页目录项指向页目录本身 */
    *(U32 *)((uintptr_t)pgd + 4092) = (U32)pgd | OS_PG_P | OS_PG_RW_W | OS_PG_US_U;

    /* 给第一张页表每个页表项赋值，完成2M映射 */
    for (i = 0; i < 512; i++) {
        *(U32 *)(&g_pgt[0][i]) = addr | OS_PG_P | OS_PG_RW_W | OS_PG_US_U;
        addr += OS_PG_SIZE;   
    }

    addr = (U32)&g_pgt[1][0];
    for (i = 769; i < 1023; i++) {
        *(U32 *)(&pgd[i]) = addr | OS_PG_P | OS_PG_RW_W | OS_PG_US_U;
        addr += OS_PG_SIZE;
    }
}

/* 保护模式下加载磁盘数据 (LBA28 端口 I/O，仅 IDE 硬盘) */
OS_SEC_LOADER_TEXT void OsReadDiskLba28(U32 secId, U32 secNum, uintptr_t dst)
{
    volatile U8 status;
    U32 readTimes;
    U32 dstAddr = (U32)dst;
    U16 data;

    /* 1. 等待 BSY=0 */
    do {
        status = OsInb(OS_DISK_CMD_STA_PORT);
    } while ((status & 0x80) != 0);

    /* 2. 写扇区数 */
    OsOutb(OS_DISK_SEC_CNT_PORT, (U8)secNum);

    /* 3. 写 LBA28 地址 */
    OsOutb(OS_DISK_LBA_LOW_PORT, (U8)secId);
    OsOutb(OS_DISK_LBA_MID_PORT, (U8)(secId >> 8));
    OsOutb(OS_DISK_LBA_HIGH_PORT, (U8)(secId >> 16));

    /* 4. 写设备/模式: LBA 模式 (bit6=1), drive=0 (bit4=0) */
    OsOutb(OS_DISK_DEV_PORT, (U8)(0xe0 | ((secId >> 24) & 0x0f)));

    /* 4.5 等待 DRDY=1 且 BSY=0 (设备就绪) */
    do {
        status = OsInb(OS_DISK_CMD_STA_PORT);
    } while ((status & 0xc0) != 0x40);  /* BSY=0, DRDY=1 */

    /* 5. 发送读命令 */
    OsOutb(OS_DISK_CMD_STA_PORT, OS_DISK_CMD_RD);

    /* 6. 等待 DRQ=1 (bit3) 且 BSY=0 (bit7) */
    /* 标准 ATA 等待: 先读状态 4 次作为 400ns 延迟 */
    OsInb(OS_DISK_CMD_STA_PORT);
    OsInb(OS_DISK_CMD_STA_PORT);
    OsInb(OS_DISK_CMD_STA_PORT);
    OsInb(OS_DISK_CMD_STA_PORT);
    do {
        status = OsInb(OS_DISK_CMD_STA_PORT);
    } while ((status & 0x88) != 0x08);

    /* 7. 读取数据 */
    readTimes = (secNum * 512) / 2;
    do {
        data = OsInw(OS_DISK_RD_PORT);
        *(U16 *)dstAddr = data;
        dstAddr += 2;
    } while ((--readTimes) > 0);
}

/* LBA48 读取 (支持大磁盘) */
OS_SEC_LOADER_TEXT void OsReadDiskLba48(U32 secId, U32 secNum, uintptr_t dst)
{
    volatile U8 status;
    U32 readTimes;
    U32 dstAddr = (U32)dst;
    U16 data;

    /* 1. 等待 BSY=0 */
    do {
        status = OsInb(OS_DISK_CMD_STA_PORT);
    } while ((status & 0x80) != 0);

    /* 2. LBA48: 先写高16位，再写低16位 */
    /* 高位 (LBA48 扩展) */
    OsOutb(0x1F1, 0);                   /* Features = 0 */
    OsOutb(OS_DISK_SEC_CNT_PORT, (U8)0);  /* Sector count high = 0 */
    OsOutb(OS_DISK_LBA_LOW_PORT, (U8)(secId >> 24));
    OsOutb(OS_DISK_LBA_MID_PORT, (U8)(secId >> 32));
    OsOutb(OS_DISK_LBA_HIGH_PORT, (U8)(secId >> 40));

    /* 低位 */
    OsOutb(0x1F1, 0);                   /* Features = 0 */
    OsOutb(OS_DISK_SEC_CNT_PORT, (U8)secNum);
    OsOutb(OS_DISK_LBA_LOW_PORT, (U8)secId);
    OsOutb(OS_DISK_LBA_MID_PORT, (U8)(secId >> 8));
    OsOutb(OS_DISK_LBA_HIGH_PORT, (U8)(secId >> 16));

    /* 3. 设备/模式: LBA48 (bit6=1) */
    OsOutb(OS_DISK_DEV_PORT, (U8)(0x40 | 0xe0));

    /* 3.5 等待 DRDY=1 且 BSY=0 */
    do {
        status = OsInb(OS_DISK_CMD_STA_PORT);
    } while ((status & 0xc0) != 0x40);

    /* 4. 发送 LBA48 读命令 */
    OsOutb(OS_DISK_CMD_STA_PORT, 0x24);

    /* 5. 等待 DRQ=1 且 BSY=0 */
    /* 标准 ATA 等待: 先读状态 4 次作为 400ns 延迟 */
    OsInb(OS_DISK_CMD_STA_PORT);
    OsInb(OS_DISK_CMD_STA_PORT);
    OsInb(OS_DISK_CMD_STA_PORT);
    OsInb(OS_DISK_CMD_STA_PORT);
    do {
        status = OsInb(OS_DISK_CMD_STA_PORT);
    } while ((status & 0x88) != 0x08);

    /* 6. 读取数据 */
    readTimes = (secNum * 512) / 2;
    do {
        data = OsInw(OS_DISK_RD_PORT);
        *(U16 *)dstAddr = data;
        dstAddr += 2;
    } while ((--readTimes) > 0);
}

/* 保护模式磁盘读取: 先试 LBA48, 失败试 LBA28 */
OS_SEC_LOADER_TEXT void OsReadDiskM32(U32 secId, U32 secNum, uintptr_t dst)
{
    /* 先试 LBA28 (简单可靠) */
    OsReadDiskLba28(secId, secNum, dst);
}

/* 根据虚拟地址找此虚拟地址对应的页表的虚拟地址 */
OS_INLINE uintptr_t OsGetPteVirAddr(uintptr_t vaddr) 
{
    uintptr_t pte;
    pte = (uintptr_t)(0xFFC00000 + ((vaddr & 0xFFC00000) >> 10) +
                      OS_PTE_IDX(vaddr) * 4);
    return pte;
}

/* 根据虚拟地址找此虚拟地址对应的页目录项的虚拟地址 */
OS_INLINE uintptr_t OsGetPdeVirAddr(uintptr_t vaddr)
{
    uintptr_t pde;
    pde = (uintptr_t)(0xFFFFF000 + OS_PDE_IDX(vaddr) * 4);
    return pde;
}

OS_SEC_KERNEL_TEXT void OsMapVir2Phy(uintptr_t virAddr, uintptr_t phyAddr)
{
    uintptr_t pteVaddr;
    uintptr_t pdeVaddr;
    uintptr_t ptPhyAddr;

    /* 
     * 如果虚拟地址确定，虚拟地址的页目录和页表也能确定，
     * 先找到此虚拟内存的对应的页目录和页表虚拟地址
     */
    pteVaddr = OsGetPteVirAddr(virAddr);
    pdeVaddr = OsGetPdeVirAddr(virAddr);

    /* 如果页目录项已存在，则对应页表已经存在，只用更改页表项 */
    if (OsPdeIsExisted(pdeVaddr)) {
        /* 如果页表项还不存在，添加页表项 */
        if (!OsPteIsExisted(pteVaddr)) {
            *(U32 *)pteVaddr = (U32)phyAddr | OS_PG_US_U | OS_PG_RW_W | OS_PG_P;
        } else {
            OS_DEBUG_PRINT_STR("pte repeat\n");
        }
    } else {
        /* 如果页目录项不存在，说明没对应页表，先申请4K物理内存作为页表 */
        /* 页表的内存都由内核出 */
        ptPhyAddr = OsMemPoolGetFreePgs(&g_kernelPhyMemPool, 1);
        /* 
         * 因为页目录的最后一项是本身地址，一旦把页表物理地址写入页目录, 
         * 无论内核态还是用户态，都可以通过pteVaddr来访问页表项了
         */
        *(U32 *)pdeVaddr = (U32)ptPhyAddr | OS_PG_US_U | OS_PG_RW_W | OS_PG_P;
        /* 把整张页表初始化为0 */
        memset(pteVaddr & 0xFFFFF000, 0, OS_PG_SIZE);
        /* 写入页表项 */
        *(U32 *)pteVaddr = (U32)phyAddr | OS_PG_US_U | OS_PG_RW_W | OS_PG_P;
    }
}

/* 根据虚拟地址获取对应的物理地址 */
OS_SEC_KERNEL_TEXT uintptr_t OsGetPaddrByVaddr(uintptr_t vaddr)
{
    uintptr_t pte = OsGetPteVirAddr(vaddr);

    return (uintptr_t)(((*(U32 *)pte) & 0xfffff000) + ((U32)vaddr & 0xfff));
}

/* 每个进程要维护一张页目录 */
OS_SEC_KERNEL_TEXT uintptr_t OsCreateProcessPgd(void)
{
    struct OsPgtEntry *pgdBase;
    uintptr_t pgdPhyAddr;

    /* 进程页目录用内核的内存 */
    pgdBase = (struct OsPgtEntry *)OsMemKernelAllocPgs(1);
    if (pgdBase == NULL) {
        OS_DEBUG_KPRINT("%s", "OsCreateProcessPgd: OsMemKernelAllocPgs failed\n");
        return NULL;
    }

    OS_DEBUG_KPRINT("OsCreateProcessPgd: pgdBase = 0x%x\n", (U32)pgdBase);

    /* 对页目录项进行复制，要把内核1G全部复制过来 */
    memcpy((uintptr_t)((U32)pgdBase + OS_PGD_KERNEL_IDX_START * sizeof(struct OsPgtEntry)), 
           (uintptr_t)((U32)OS_CUR_PGD_VIR_ADDR + OS_PGD_KERNEL_IDX_START * sizeof(struct OsPgtEntry)),
           OS_PG_SIZE / 4);

    /* 要把页目录的物理地址写入最后一项 */
    pgdPhyAddr = OsGetPaddrByVaddr((uintptr_t)pgdBase);
    OS_DEBUG_KPRINT("OsCreateProcessPgd: pgdPhyAddr = 0x%x\n", (U32)pgdPhyAddr);
    *(U32 *)(&pgdBase[OS_PGD_ENTRY_NUM - 1]) = (U32)pgdPhyAddr | OS_PG_RW_W | OS_PG_US_U | OS_PG_P;

    return (uintptr_t)pgdBase;
}

OS_SEC_KERNEL_TEXT void OsLoadPgd(uintptr_t pgdPhyAddr)
{
    OS_EMBED_ASM("movl %0, %%cr3"::"r"(pgdPhyAddr):"memory");
}