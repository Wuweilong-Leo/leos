#include "os_vga_internal.h"
#include "os_vga_external.h"
#include "os_def.h"
#include "os_hwi.h"
#include "os_print_external.h"
#include "os_uart_external.h"
#include "string.h"

OS_SEC_KERNEL_TEXT void OsVgaSetCursor(U16 pos)
{
    U8 high = (pos >> 8) & 0xff;
    U8 low = pos & 0xff;

    OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_VGA_CRT_ADDR_REG), "a"(OS_VGA_CUR_POS_HIGH_INDEX));
    OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_VGA_CRT_DATA_REG), "a"(high));
    OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_VGA_CRT_ADDR_REG), "a"(OS_VGA_CUR_POS_LOW_INDEX));
    OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_VGA_CRT_DATA_REG), "a"(low));
}

OS_SEC_KERNEL_TEXT U16 OsVgaGetCursor(void)
{
    U16 low;
    U16 high;

    OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_VGA_CRT_ADDR_REG), "a"(OS_VGA_CUR_POS_HIGH_INDEX));
    OS_EMBED_ASM("inb %%dx, %%al" : "=a"(high) : "d"(OS_VGA_CRT_DATA_REG));
    OS_EMBED_ASM("outb %%al, %%dx" ::"d"(OS_VGA_CRT_ADDR_REG), "a"(OS_VGA_CUR_POS_LOW_INDEX));
    OS_EMBED_ASM("inb %%dx, %%al" : "=a"(low) : "d"(OS_VGA_CRT_DATA_REG));

    return ((high << 8) & 0xff00) | (low & 0x00ff);
}

OS_SEC_KERNEL_TEXT void OsVgaWriteChar(U32 pos, char c, U8 attr)
{
    U8 *buf = (U8 *)OS_VGA_BUF_ADDR;
    U32 offset = pos * 2;

    buf[offset] = c;
    buf[offset + 1] = attr;
}

OS_SEC_KERNEL_TEXT void OsVgaScrollUp(void)
{
    U8 *dst = (U8 *)OS_VGA_BUF_ADDR;
    U8 *src = dst + OS_VGA_COL_NUM * 2;
    size_t size = (OS_VGA_POS_NUM - OS_VGA_COL_NUM) * 2;

    memcpy(dst, src, size);
}

OS_SEC_KERNEL_TEXT void OsVgaClearLine(U32 row)
{
    U8 *buf = (U8 *)OS_VGA_BUF_ADDR;
    U32 offset = row * OS_VGA_COL_NUM * 2;
    U32 i;

    for (i = 0; i < OS_VGA_COL_NUM; i++) {
        buf[offset++] = ' ';
        buf[offset++] = OS_VGA_ATTR_DEFAULT;
    }
}

/* 串口镜像包装函数，作为 mirrorChar 钩子注册给 print 模块 */
static OS_SEC_KERNEL_TEXT void OsVgaMirrorChar(char c)
{
    OsUartPutc(c);
}

OS_SEC_KERNEL_TEXT U32 OsVgaRegisterToPrint(void)
{
    struct OsPrintOps ops = {
        .setCursor = OsVgaSetCursor,
        .getCursor = OsVgaGetCursor,
        .writeChar = OsVgaWriteChar,
        .scrollUp = OsVgaScrollUp,
        .clearLine = OsVgaClearLine,
        .mirrorChar = OsVgaMirrorChar,
        .colNum = OS_VGA_COL_NUM,
        .posNum = OS_VGA_POS_NUM,
        .attrDefault = OS_VGA_ATTR_DEFAULT,
    };
    OsPrintRegisterOps(&ops);
    return OS_OK;
}