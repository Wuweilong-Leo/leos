#ifndef OS_MEM_EXTERNAL_H
#define OS_MEM_EXTERNAL_H
#include "os_btmp_external.h"
struct OsMemPool {
    struct OsBtmp btmp;
    uintptr_t base;
    U32 size;
};

#define OS_KERNEL_MEM_VIR_ADDR_START 0xc0000000
#define OS_USR_MEM_VIR_ADDR_START 0x8048000
/* 用户最多申请1M */
#define OS_USR_VIR_MEM_SIZE (OS_KERNEL_MEM_VIR_ADDR_START - OS_USR_MEM_VIR_ADDR_START)

extern void OsMemConfig(void);
extern uintptr_t OsMemPoolGetFreePgs(struct OsMemPool *pool, U32 cnt);
extern uintptr_t OsMemKernelAllocPgs(U32 cnt);
extern uintptr_t OsMemUsrAllocPgs(U32 cnt);
extern uintptr_t OsMemUsrAllocPgByAddr(uintptr_t virAddr);
extern uintptr_t OsMemKernelAllocPgByAddr(uintptr_t virAddr);
extern void OsMemPoolInit(struct OsMemPool *memPool, uintptr_t memBase, 
                          U32 memSize, U8 *btmpBase);

extern struct OsMemPool g_kernelPhyMemPool;
extern struct OsMemPool g_usrPhyMemPool;
extern struct OsMemPool g_kernelVirMemPool;

#endif