#ifndef OS_PGT_H
#define OS_PGT_H
#include "os_def.h"
struct OS_STRUCT_PACKED OsPgtEntry {
    U8 attrP: 1;
    U8 attrRw: 1;
    U8 attrUs: 1;
    U8 rsvd1: 2;
    U8 attrA: 1;
    U8 attrD: 1;
    U8 rsvd2: 2;
    U8 attrAvl: 3;
    U32 addr: 20;
};

#define OS_PG_P (1 << 0)
#define OS_PG_RW_R (0 << 1)
#define OS_PG_RW_W (1 << 1)
#define OS_PG_US_S (0 << 2)
#define OS_PG_US_U (1 << 2)

#define OS_PGD_ENTRY_NUM 1024
#define OS_PGT_ENTRY_NUM 1024

/* 虚拟地址在对应的页目录的索引 */
#define OS_PDE_IDX(addr) (((addr) & 0xFFC00000U) >> 22)
/* 虚拟地址在对应的页表的索引 */
#define OS_PTE_IDX(addr) (((addr) & 0x003FF000U) >> 12)

#define OS_PTE_EXIST(pteVaddr) (((*(pteVaddr)) & OS_PG_P) != 0)
#define OS_PDE_EXIST(pdeVaddr) (((*(pdeVaddr)) & OS_PG_P) != 0)

extern void OsMapVir2Phy(uintptr_t virAddr, uintptr_t phyAddr);
#endif 