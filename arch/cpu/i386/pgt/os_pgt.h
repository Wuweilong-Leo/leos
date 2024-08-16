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

extern struct OsPgtEntry g_pgd[OS_PGD_ENTRY_NUM];
extern struct OsPgtEntry g_pgt[256][OS_PGD_ENTRY_NUM];

/* 这是页目录的物理地址 */
#define OS_KERNEL_PGD_BASE ((uintptr_t)(&g_pgd[0]))
#define OS_PGD_KERNEL_IDX_START 0x300
/* 由于页目录最后一项是页目录本身，可通过此地址获取页目录的虚拟地址 */
#define OS_CUR_PGD_VIR_ADDR 0xFFFFF000

extern void OsMapVir2Phy(uintptr_t virAddr, uintptr_t phyAddr);
extern uintptr_t OsGetPaddrByVaddr(uintptr_t vaddr);
extern void OsLoadPgd(uintptr_t pgdPhyAddr);
#endif 