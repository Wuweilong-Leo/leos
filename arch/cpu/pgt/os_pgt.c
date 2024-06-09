#include "os_def.h"
#include "os_pgt.h"
#include "string.h"
#include "os_cpu_i386.h"
#include "os_io_i386.h"
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
    struct OsPgtEntry *pgd = g_pgd;
    U32 addr = 0;
    U32 i;
    U32 firstPgtBase = &g_pgt[0][0];

    /* 清空页目录项 */
    OsCleanPgd();

    /* 把虚拟地址1M和3G+1M都映射到物理地址的1M内，都指向第一张页表 */
    *(U32 *)((U32)pgd + 0)= firstPgtBase | OS_PG_P | OS_PG_RW_W | OS_PG_US_U;
    *(U32 *)((U32)pgd + 0xc00) = firstPgtBase | OS_PG_P | OS_PG_RW_W | OS_PG_US_U;

    /* 最后一个页目录项指向页目录本身 */
    *(U32 *)((U32)pgd + 4092) = (U32)pgd | OS_PG_P | OS_PG_RW_W | OS_PG_US_U;

    /* 给第一张页表每个页表项赋值，完成1M映射 */
    for (i = 0; i < 256; i++) {
        *(U32 *)(&g_pgt[0][i]) = addr | OS_PG_P | OS_PG_RW_W | OS_PG_US_U;
        addr += OS_PG_SIZE;   
    }

    addr = (U32)&g_pgt[1][0];
    for (i = 769; i < 1023; i++) {
        *(U32 *)(&pgd[i]) = addr | OS_PG_P | OS_PG_RW_W | OS_PG_US_U;
        addr += OS_PG_SIZE;
    }
}

/* 保护模式下加载磁盘数据 */
OS_SEC_LOADER_TEXT void OsReadDiskM32(U32 secId, U32 secNum, uintptr_t dst)
{
    volatile U8 diskRdy;
    U32 readTimes;
    U32 dstAddr = (U32)dst;
    U16 data;

    OsOutw(OS_DISK_SEC_CNT_PORT, (U16)secNum);

    OsOutb(OS_DISK_LBA_LOW_PORT, (U8)secId);
    OsOutb(OS_DISK_LBA_MID_PORT, (U8)(secId >> 8));
    OsOutb(OS_DISK_LBA_HIGH_PORT, (U8)(secId >> 16));

    OsOutb(OS_DISK_DEV_PORT, (U8)(0x0 | 0xe0));
    OsOutb(OS_DISK_CMD_STA_PORT, OS_DISK_CMD_RD);

    do {
        OS_EMBED_ASM("NOP");
        diskRdy = OsInb(OS_DISK_CMD_STA_PORT);
    } while ((diskRdy & 0x88) != 0x08);

    readTimes = (secNum * 512) / 2;
    do {
        data = OsInw(OS_DISK_RD_PORT);
        *(U16 *)dstAddr = data;
        dstAddr += 2;
    } while ((--readTimes) > 0);
}
