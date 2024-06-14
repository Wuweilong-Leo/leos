#ifndef OS_MEM_EXTERNAL_H
#define OS_MEM_EXTERNAL_H
#include "os_btmp_external.h"
struct OsMemPool {
    struct OsBtmp btmp;
    uintptr_t base;
    U32 size;
};

extern void OsMemConfig(void);
extern uintptr_t OsMemPoolGetFreePgs(struct OsMemPool *pool, U32 cnt);
extern uintptr_t OsMemKernelAllocPgs(U32 cnt);

extern struct OsMemPool g_kernelPhyMemPool;
extern struct OsMemPool g_usrPhyMemPool;
extern struct OsMemPool g_kernelVirMemPool;

#endif