#include "os_btmp_internal.h"
#include "os_def.h"
#include "string.h"

OS_SEC_KERNEL_TEXT void OsBtmpInit(struct OsBtmp *btmp, U8 *base, U32 bitNum)
{
    btmp->base = base;
    btmp->byteLen = OS_BTMP_BITNUM_2_BYTELEN(bitNum);
    btmp->bitNum = bitNum;
    memset(base, 0, btmp->byteLen);
}

OS_SEC_KERNEL_TEXT void OsBtmpSet(struct OsBtmp *btmp, U32 idx)
{
    U32 bitOff = idx % 8;
    U32 byteOff = idx / 8;

    btmp->base[byteOff] |= (1 << bitOff);
}

OS_SEC_KERNEL_TEXT U8 OsBtmpGet(struct OsBtmp *btmp, U32 idx)
{
    U32 bitOff = idx % 8;
    U32 byteOff = idx / 8;

    return (btmp->base[byteOff] & (1 << bitOff)) != 0 ;
}

OS_SEC_KERNEL_TEXT void OsBtmpClear(struct OsBtmp *btmp, U32 idx)
{
    U32 bitOff = idx % 8;
    U32 byteOff = idx / 8;

    btmp->base[byteOff] &= ~(1 << bitOff);
}

/* 连续申请cnt个为val的位, val只能为1或者0 */
OS_SEC_KERNEL_TEXT bool OsBtmpScan(struct OsBtmp *btmp, U32 cnt, U8 val, U32 *idx) {
  U32 left = 0;
  U32 right = 0;
  while (right < btmp->bitNum) {
    if (OsBtmpGet(btmp, right) != val) {
      left = right + 1;
    }
    if (right - left + 1 == cnt) {
      *idx = left;
      return TRUE;
    }
    right++;
  }
  return FALSE;
}



