#include "os_def.h"
#include "os_print_internal.h"
#include "string.h"
#include "os_hwi_i386.h"

OS_INLINE void OsPrintRollScreen(void) 
{
  memcpy(OS_ROLL_VIDEO_DST_ADDR, OS_ROLL_VIDEO_SRC_ADDR, 960 * 4);
}

OS_INLINE void OsPrintCleanLastLine(void) 
{
  U32 offset = 3840;
  U32 i;
  U8 *videoBaseAddr = (U8 *)OS_VIDEO_BASE_ADDR;

  for (i = 0; i < OS_SCREEN_COL_MAX; i++) {
    videoBaseAddr[offset++] = ' ';
    videoBaseAddr[offset++] = OS_BLK_BACK_WHT_WORD;
  }
}

OS_INLINE void OsPrintSetCursor(U16 target) 
{
  U8 high = (target >> 8) & 0xff;
  U8 low = target & 0xff;
  OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_CRT_ADDR_REG),
               "a"(OS_CUR_POS_HIGH_INDEX));
  OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_CRT_DATA_REG), "a"(high));
  OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_CRT_ADDR_REG),
               "a"(OS_CUR_POS_LOW_INDEX));
  OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_CRT_DATA_REG), "a"(low));
}

OS_INLINE U16 OsPrintGetCursor(void) {
  U16 curPosLow;
  U16 curPosHigh;
  OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_CRT_ADDR_REG),
               "a"(OS_CUR_POS_HIGH_INDEX));
  OS_EMBED_ASM("inb %%dx, %%al" : "=a"(curPosHigh) : "d"(OS_CRT_DATA_REG));
  OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_CRT_ADDR_REG),
               "a"(OS_CUR_POS_LOW_INDEX));
  OS_EMBED_ASM("inb %%dx, %%al" : "=a"(curPosLow) : "d"(OS_CRT_DATA_REG));
  return ((curPosHigh << 8) & 0xff00) | (curPosLow & 0x00ff);
}

OS_SEC_KERNEL_TEXT void OsPrintCheckOutOfScreen(U16 *nextPos) 
{
  U16 nextCurPos = *nextPos;
  if (nextCurPos >= OS_SCREEN_MAX) {
    OsPrintRollScreen();
    OsPrintCleanLastLine();
    *nextPos = OS_SCREEN_MAX - OS_SCREEN_COL_MAX;
  }
}

OS_SEC_KERNEL_TEXT void OsPrintChar(char c) 
{
  U16 curPos;
  U16 nextCurPos;
  U32 offset;
  U8 *videoBaseAddr = (U8 *)OS_VIDEO_BASE_ADDR;

  curPos = OsPrintGetCursor();

  if (c == '\r' || c == '\n') {
    nextCurPos = curPos - curPos % OS_SCREEN_COL_MAX + OS_SCREEN_COL_MAX;
    OsPrintCheckOutOfScreen(&nextCurPos);
  } else if (c == '\b') {
    offset = (curPos - 1) * 2;
    videoBaseAddr[offset] = ' ';
    videoBaseAddr[offset + 1] = OS_BLK_BACK_WHT_WORD;
    nextCurPos = curPos - 1;
  } else {
    offset = curPos * 2;
    videoBaseAddr[offset] = c;
    videoBaseAddr[offset + 1] = OS_BLK_BACK_WHT_WORD;
    nextCurPos = curPos + 1;
    OsPrintCheckOutOfScreen(&nextCurPos);
  }
  OsPrintSetCursor(nextCurPos);
}

OS_SEC_KERNEL_TEXT void OsPrintStr(char *str) 
{
  U32 i = 0;

  while (str[i] != 0) {
    OsPrintChar(str[i]);
    i++;
  }
}

OS_SEC_KERNEL_TEXT void OsPrintHex(U32 num) 
{
  U8 off = 7;
  U8 low;
  U32 numTmp = num;
  char p;
  char buf[9] = {0};

  if (numTmp == 0) {
    buf[off--] = '0';
  }

  while (numTmp != 0) {
    low = numTmp & 0xf;
    if (low >= 0 && low <= 9) {
      p = low + '0';
    } else {
      p = low - 10 + 'A';
    }
    buf[off--] = p;
    numTmp >>= 4;
  }

  OsPrintStr(buf + (++off));
}

OS_SEC_KERNEL_TEXT void itoa(U32 val, char ** bufPtrAddr, U8 base)
{
    U32 m = val % base;
    U32 i = val / base;

    if (i != 0) {
        itoa(i, bufPtrAddr, base);
    }

    if (m < 10) {
        *((*bufPtrAddr)++) = m + '0';
    } else {
        *((*bufPtrAddr)++) = m + 'A' - 10;
    }
}

OS_SEC_KERNEL_TEXT U32 vsprintf(char *str, const char *fmt, void *ap)
{
    char* bufPtr = str;
    const char* idxPtr = fmt;
    char idxChar = *idxPtr;
    S32 argInt;
    char* argStr;

    while(idxChar)		//挨个挨个字符来弄
    {
    	if(idxChar != '%')
    	{
    	    *(bufPtr++) = idxChar;
    	    idxChar = *(++idxPtr);
    	    continue;
    	}
    	idxChar = *(++idxPtr);
    	switch(idxChar)
    	{
    	    case 's':
    	    	argStr = OS_VA_ARG(ap, char *);
    	    	strcpy(bufPtr,argStr);
    	    	bufPtr += strlen(argStr);
    	    	idxChar = *(++idxPtr);
    	    	break;
    	    case 'x':
    	    	argInt = OS_VA_ARG(ap, int);
    	    	itoa(argInt,&bufPtr,16);
    	    	idxChar = *(++idxPtr);
    	    	break;
    	    case 'd':
    	    	argInt = OS_VA_ARG(ap, int);
    	    	if(argInt < 0)
    	    	{
    	    	    argInt = 0 - argInt;
    	    	    *(bufPtr++) = '-';
    	    	}
    	    	itoa(argInt, &bufPtr, 10);
    	    	idxChar = *(++idxPtr);
    	    	break;
    	    case 'c':
    	    	*(bufPtr++) = OS_VA_ARG(ap, char);
    	    	idxChar = *(++idxPtr);
    	}
    }
    return strlen(str);
}

OS_SEC_KERNEL_TEXT void kprintf(const char *fmt, ...)
{
    char buf[1024] = {0};
    void *args;
    U32 len;
    enum OsIntStatus intSave;

    intSave = OsIntLock();
    OS_VA_START(args, fmt);
    len = vsprintf(buf, fmt, args);
    OS_VA_END(args);
    OsPrintStr(buf);
    OsIntRestore(intSave);

    return len;
}