#include "os_print_external.h"
#include "os_hwi.h"
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
    if (g_printOps.clearLine != NULL && g_printOps.colNum != 0) {
        g_printOps.clearLine(g_printOps.posNum / g_printOps.colNum - 1);
    }
    if (g_printOps.colNum != 0) {
        OsPrintSetCursor(g_printOps.posNum - g_printOps.colNum);
    }
}

OS_SEC_KERNEL_TEXT void OsPrintChar(char c)
{
    U16 curPos;
    U16 nextCurPos;

    /* 镜像到辅助输出(如串口) */
    if (g_printOps.mirrorChar != NULL) {
        if (c == '\n') {
            g_printOps.mirrorChar('\r');
            g_printOps.mirrorChar('\n');
        } else if (c != '\r') {
            g_printOps.mirrorChar(c);
        }
    }

    if (g_printOps.colNum == 0 || g_printOps.posNum == 0) {
        return;
    }

    curPos = OsPrintGetCursor();

    if (c == '\r' || c == '\n') {
        nextCurPos = curPos - (curPos % g_printOps.colNum) + g_printOps.colNum;
        if (nextCurPos >= g_printOps.posNum) {
            OsPrintRollScreen();
        } else {
            OsPrintSetCursor(nextCurPos);
        }
    } else if (c == '\b') {
        if (curPos == 0) {
            return;
        }
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

#define KPRINTF_BUF_SIZE 512

OS_SEC_KERNEL_TEXT size_t vsprintf(char *str, size_t bufSize, const char *fmt, void *ap)
{
    char *bufPtr = str;
    char *bufEnd = str + bufSize - 1; /* 留 1 字节给 '\0' */
    const char *idxPtr = fmt;
    char idxChar = *idxPtr;
    S32 argInt;
    char *argStr;

    while (idxChar && bufPtr < bufEnd)
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
            if (argStr == NULL) {
                argStr = "(null)";
            }
            {
                size_t slen = strlen(argStr);
                size_t avail = (size_t)(bufEnd - bufPtr);
                if (slen > avail) {
                    slen = avail;
                }
                memcpy(bufPtr, argStr, slen);
                bufPtr += slen;
            }
            idxChar = *(++idxPtr);
            break;
        case 'x':
            argInt = OS_VA_ARG(ap, int);
            {
                /* 最多 8 位十六进制 + 可能的 0x 前缀 */
                char numBuf[9];
                char *numPtr = numBuf;
                itoa(argInt, &numPtr, 16);
                *numPtr = '\0';
                size_t slen = strlen(numBuf);
                size_t avail = (size_t)(bufEnd - bufPtr);
                if (slen > avail) {
                    slen = avail;
                }
                memcpy(bufPtr, numBuf, slen);
                bufPtr += slen;
            }
            idxChar = *(++idxPtr);
            break;
        case 'u':
            argInt = OS_VA_ARG(ap, int);
            {
                char numBuf[11]; /* 4294967295 = 10 digits + NUL */
                char *numPtr = numBuf;
                itoa((U32)argInt, &numPtr, 10);
                *numPtr = '\0';
                size_t slen = strlen(numBuf);
                size_t avail = (size_t)(bufEnd - bufPtr);
                if (slen > avail) {
                    slen = avail;
                }
                memcpy(bufPtr, numBuf, slen);
                bufPtr += slen;
            }
            idxChar = *(++idxPtr);
            break;
        case 'd':
            argInt = OS_VA_ARG(ap, int);
            if (argInt < 0) {
                if (bufPtr < bufEnd) {
                    *(bufPtr++) = '-';
                }
                argInt = 0 - argInt;
            }
            {
                char numBuf[11];
                char *numPtr = numBuf;
                itoa(argInt, &numPtr, 10);
                *numPtr = '\0';
                size_t slen = strlen(numBuf);
                size_t avail = (size_t)(bufEnd - bufPtr);
                if (slen > avail) {
                    slen = avail;
                }
                memcpy(bufPtr, numBuf, slen);
                bufPtr += slen;
            }
            idxChar = *(++idxPtr);
            break;
        case 'c':
            if (bufPtr < bufEnd) {
                *(bufPtr++) = OS_VA_ARG(ap, char);
            }
            idxChar = *(++idxPtr);
            break;
        case '%':
            if (bufPtr < bufEnd) {
                *(bufPtr++) = '%';
            }
            idxChar = *(++idxPtr);
            break;
        default:
            /* 未知格式符，原样输出 % 和字符 */
            if (bufPtr < bufEnd) {
                *(bufPtr++) = '%';
            }
            if (bufPtr < bufEnd) {
                *(bufPtr++) = idxChar;
            }
            idxChar = *(++idxPtr);
            break;
        }
    }
    *bufPtr = '\0';
    return (size_t)(bufPtr - str);
}

OS_SEC_KERNEL_TEXT size_t kprintf(const char *fmt, ...)
{
    char buf[KPRINTF_BUF_SIZE];
    void *args;
    size_t len;
    enum OsIntStatus intSave;

    intSave = OsIntLock();
    OS_VA_START(args, fmt);
    len = vsprintf(buf, sizeof(buf), fmt, args);
    OS_VA_END(args);
    OsPrintStr(buf);
    OsIntRestore(intSave);

    return len;
}
