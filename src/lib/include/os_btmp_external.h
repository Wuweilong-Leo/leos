#ifndef OS_BTMP_EXTERNAL_H
#define OS_BTMP_EXTERNAL_H
#include "os_def.h"
#include "os_cpu.h"

struct OsBtmp {
    size_t byteLen;
    size_t bitNum;
    U8 *base;
};

#define OS_BTMP_BITNUM_2_BYTELEN(bitNum) (((bitNum) + 7) >> 3)

extern U8 OsBtmpGet(struct OsBtmp *btmp, U32 idx);
extern void OsBtmpSet(struct OsBtmp *btmp, U32 idx);
extern void OsBtmpClear(struct OsBtmp *btmp, U32 idx);
extern bool OsBtmpScan(struct OsBtmp *btmp, size_t cnt, U8 val, U32 *idx);
extern void OsBtmpInit(struct OsBtmp *btmp, U8 *base, size_t bitNum);

/* 用位图需要管理的内存大小获取位图所需内存页数 */
#define OS_BTMP_GET_PG_NUM_BY_MEM_SIZE(memSize)                                                    \
    (OS_ROUND_UP(OS_ROUND_UP(OS_ROUND_UP(memSize, OS_PG_SIZE) / OS_PG_SIZE, 8) / 8, OS_PG_SIZE) /  \
     OS_PG_SIZE)
#endif