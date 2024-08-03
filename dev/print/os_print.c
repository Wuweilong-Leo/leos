#include "os_def.h"
#include "os_print_internal.h"
#include "string.h"
#include "os_hwi_i386.h"

OS_INLINE void OsPrintCleanLastLine(void)
{
    U32 lastLineOff = ((OS_SCREEN_ROW_NUM - 1) * OS_SCREEN_COL_NUM * 2);
    U8 *videoBaseAddr = (U8 *)OS_VIDEO_BASE_ADDR;
    U32 i;

    for (i = 0; i < OS_SCREEN_COL_NUM; i++) {
        videoBaseAddr[lastLineOff++] = ' ';
        videoBaseAddr[lastLineOff++] = OS_BLK_BACK_WHT_WORD;
    }
}

/* 设置鼠标位置 */
OS_SEC_KERNEL_TEXT void OsPrintSetCursor(U16 target) 
{
    U8 high = (target >> 8) & 0xff;
    U8 low = target & 0xff;

    OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_CRT_ADDR_REG), "a"(OS_CUR_POS_HIGH_INDEX));
    OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_CRT_DATA_REG), "a"(high));
    OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_CRT_ADDR_REG), "a"(OS_CUR_POS_LOW_INDEX));
    OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_CRT_DATA_REG), "a"(low));
}

OS_SEC_KERNEL_TEXT U16 OsPrintGetCursor(void) 
{
    U16 curPosLow;
    U16 curPosHigh;

    OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_CRT_ADDR_REG), "a"(OS_CUR_POS_HIGH_INDEX));
    OS_EMBED_ASM("inb %%dx, %%al" : "=a"(curPosHigh) : "d"(OS_CRT_DATA_REG));
    OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_CRT_ADDR_REG), "a"(OS_CUR_POS_LOW_INDEX));
    OS_EMBED_ASM("inb %%dx, %%al" : "=a"(curPosLow) : "d"(OS_CRT_DATA_REG));

    return ((curPosHigh << 8) & 0xff00) | (curPosLow & 0x00ff);
}

/* 满了，上卷屏幕 */
OS_SEC_KERNEL_TEXT void OsPrintRollScreen(void) 
{
    memcpy(OS_VIDEO_ROLL_DST_ADDR, OS_VIDEO_ROLL_SRC_ADDR, OS_VIDEO_ROLL_SIZE);

    /* 上卷了屏幕，要清空最后一行，并把鼠标放到行首 */
    OsPrintCleanLastLine();

    OsPrintSetCursor(OS_SCREEN_POS_NUM - OS_SCREEN_COL_NUM);
}

OS_INLINE bool OsPrintIsOutOfScreen(U32 pos)
{
    return pos >= OS_SCREEN_POS_NUM;
}

OS_SEC_KERNEL_TEXT void OsPrintChar(char c) 
{
    U16 curPos;
    U16 nextCurPos;
    U32 offset;
    U8 *videoBaseAddr = (U8 *)OS_VIDEO_BASE_ADDR;

    curPos = OsPrintGetCursor();

    if (c == '\r' || c == '\n') {
        /* 另起一行 */
        nextCurPos = curPos - (curPos % OS_SCREEN_COL_NUM) + OS_SCREEN_COL_NUM;
        if (OsPrintIsOutOfScreen(nextCurPos)) {
            OsPrintRollScreen();
        } else {
            OsPrintSetCursor(nextCurPos);
        }
    } else if (c == '\b') {
        offset = (curPos - 1) * 2;
        videoBaseAddr[offset] = ' ';
        videoBaseAddr[offset + 1] = OS_BLK_BACK_WHT_WORD;
        nextCurPos = curPos - 1;
        /* 因为是返回上一个cursor, 不会超出范围 */
        OsPrintSetCursor(nextCurPos);
    } else {
        offset = curPos * 2;
        videoBaseAddr[offset] = c;
        videoBaseAddr[offset + 1] = OS_BLK_BACK_WHT_WORD;
        nextCurPos = curPos + 1;
        if (OsPrintIsOutOfScreen(nextCurPos)) {
            OsPrintRollScreen();
        } else {
            OsPrintSetCursor(nextCurPos);
        }
    } 
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
    	    	if(argInt < 0) {
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
    char buf[256] = {0};
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