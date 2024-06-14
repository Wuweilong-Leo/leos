#ifndef OS_BTMP_EXTERNAL_H
#define OS_BTMP_EXTERNAL_H
#include "os_def.h"

struct OsBtmp {
    U32 byteLen;
    U32 bitNum;
    U8 *base;
};

#define OS_BTMP_BITNUM_2_BYTELEN(bitNum) \
    (((bitNum) + 7) >> 3)

extern U8 OsBtmpGet(struct OsBtmp *btmp, U32 idx);
extern void OsBtmpSet(struct OsBtmp *btmp, U32 idx);
extern void OsBtmpClear(struct OsBtmp *btmp, U32 idx);
extern bool OsBtmpScan(struct OsBtmp *btmp, U32 cnt, U8 val, U32 *idx);
extern void OsBtmpInit(struct OsBtmp *btmp, U8 *base, U32 bitNum);
#endif