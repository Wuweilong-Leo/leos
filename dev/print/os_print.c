#include "os_print_internal.h"
#include "os_hwi.h"
#include "os_print_external.h"
#include "string.h"

OS_SEC_KERNEL_BSS struct OsPrintOps g_printOps;

OS_SEC_KERNEL_TEXT void OsPrintRegisterOps(const struct OsPrintOps *ops)
{
    g_printOps = *ops;
}

OS_SEC_KERNEL_TEXT void OsPrintSetCursor(U16 target)
{
    if (g_printOps.setCursor != NULL) {
        g_printOps.setCursor(target);
    }
}

OS_SEC_KERNEL_TEXT U16 OsPrintGetCursor(void)
{
    if (g_printOps.getCursor != NULL) {
        return g_printOps.getCursor();
    }
    return 0;
}

OS_SEC_KERNEL_TEXT void OsPrintRollScreen(void)
{
    if (g_printOps.scrollUp != NULL) {
        g_printOps.scrollUp();
    }
    if (g_printOps.clearLine != NULL) {
        g_printOps.clearLine(g_printOps.posNum / g_printOps.colNum - 1);
    }
    OsPrintSetCursor(g_printOps.posNum - g_printOps.colNum);
}

OS_SEC_KERNEL_TEXT void OsPrintChar(char c)
{
    U16 curPos;
    U16 nextCurPos;

    curPos = OsPrintGetCursor();

    if (c == '\r' || c == '\n') {
        nextCurPos = curPos - (curPos % g_printOps.colNum) + g_printOps.colNum;
        if (nextCurPos >= g_printOps.posNum) {
            OsPrintRollScreen();
        } else {
            OsPrintSetCursor(nextCurPos);
        }
    } else if (c == '\b') {
        if (g_printOps.writeChar != NULL) {
            g_printOps.writeChar(curPos - 1, ' ', g_printOps.attrDefault);
        }
        OsPrintSetCursor(curPos - 1);
    } else {
        if (g_printOps.writeChar != NULL) {
            g_printOps.writeChar(curPos, c, g_printOps.attrDefault);
        }
        nextCurPos = curPos + 1;
        if (nextCurPos >= g_printOps.posNum) {
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

OS_SEC_KERNEL_TEXT void itoa(U32 val, char **bufPtrAddr, U8 base)
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
    char *bufPtr = str;
    const char *idxPtr = fmt;
    char idxChar = *idxPtr;
    S32 argInt;
    char *argStr;

    while (idxChar)
    {
        if (idxChar != '%') {
            *(bufPtr++) = idxChar;
            idxChar = *(++idxPtr);
            continue;
        }
        idxChar = *(++idxPtr);
        switch (idxChar) {
        case 's':
            argStr = OS_VA_ARG(ap, char *);
            strcpy(bufPtr, argStr);
            bufPtr += strlen(argStr);
            idxChar = *(++idxPtr);
            break;
        case 'x':
            argInt = OS_VA_ARG(ap, int);
            itoa(argInt, &bufPtr, 16);
            idxChar = *(++idxPtr);
            break;
        case 'd':
            argInt = OS_VA_ARG(ap, int);
            if (argInt < 0) {
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

OS_SEC_KERNEL_TEXT S32 kprintf(const char *fmt, ...)
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