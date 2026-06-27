#include "os_kbd_internal.h"
#include "os_hwi.h"
#include "os_hwi_external.h"
#include "os_io_i386.h"
#include "os_print_external.h"
#include "os_shell_external.h"
#include "string.h"

/*
 * PS/2 键盘驱动 (i8042)
 *
 * IRQ1 → OsKbdIsr → 读 scancode → 转 ASCII → echo + 存行缓冲区
 *                                  → 回车 → OsShellInput 提交给 shell
 */

/* ====== Scancode → ASCII 查表 ====== */

static OS_SEC_KERNEL_DATA const U8 g_kbdNormMap[128] = {
    0,    0x1B, '1',  '2',  '3',  '4',  '5',  '6',  /* 00-07 */
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t', /* 08-0F */
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  /* 10-17 */
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',  /* 18-1F */
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  /* 20-27 */
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',  /* 28-2F */
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  /* 30-37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,    /* 38-3F */
    0,    0,    0,    0,    0,    0,    0,    0,    /* 40-47 */
    0,    0,    0,    0,    0,    0,    '7',  '8',  /* 48-4F */
    '9',  '-',  '4',  '5',  '6',  '+',  '1',  '2',  /* 50-57 */
    '3',  '0',  '.',  0,    0,    0,    0,    0     /* 58-5F */
};

static OS_SEC_KERNEL_DATA const U8 g_kbdShiftMap[128] = {
    0,    0x1B, '!',  '@',  '#',  '$',  '%',  '^',  /* 00-07 */
    '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t', /* 08-0F */
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  /* 10-17 */
    'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',  /* 18-1F */
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  /* 20-27 */
    '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',  /* 28-2F */
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  /* 30-37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,    /* 38-3F */
    0,    0,    0,    0,    0,    0,    0,    0,    /* 40-47 */
    0,    0,    0,    0,    0,    0,    '7',  '8',  /* 48-4F */
    '9',  '-',  '4',  '5',  '6',  '+',  '1',  '2',  /* 50-57 */
    '3',  '0',  '.',  0,    0,    0,    0,    0     /* 58-5F */
};

/* ====== 模块全局变量 ====== */

static OS_SEC_KERNEL_BSS volatile U32 g_kbdShiftState;
static OS_SEC_KERNEL_BSS volatile U32 g_kbdLinePos;
static OS_SEC_KERNEL_BSS U8 g_kbdLineBuf[OS_KBD_LINE_BUF_SIZE];

/* ====== ISR ====== */

OS_SEC_KERNEL_TEXT void OsKbdIsr(U32 hwiNum)
{
    U8 scancode;
    U8 released;
    U8 ch;
    (void)hwiNum;

    scancode = OsInb(OS_KBD_DATA_PORT);

    /* 0xE0 = 扩展键前缀，暂不处理 */
    if (scancode == 0xE0) {
        return;
    }

    released = scancode & 0x80;
    scancode &= 0x7F;

    /* Shift 键 */
    if (scancode == 0x2A) {
        if (released) {
            g_kbdShiftState &= ~OS_KBD_LSHIFT_MSK;
        } else {
            g_kbdShiftState |= OS_KBD_LSHIFT_MSK;
        }
        return;
    }
    if (scancode == 0x36) {
        if (released) {
            g_kbdShiftState &= ~OS_KBD_RSHIFT_MSK;
        } else {
            g_kbdShiftState |= OS_KBD_RSHIFT_MSK;
        }
        return;
    }

    /* 释放码忽略 */
    if (released) {
        return;
    }

    /* 查表转换 */
    if (g_kbdShiftState & (OS_KBD_LSHIFT_MSK | OS_KBD_RSHIFT_MSK)) {
        ch = g_kbdShiftMap[scancode];
    } else {
        ch = g_kbdNormMap[scancode];
    }

    if (ch == 0) {
        return;
    }

    /* 回车：提交给 shell */
    if (ch == '\n') {
        OsPrintChar('\n');
        if (g_kbdLinePos > 0) {
            OsShellInput((const char *)g_kbdLineBuf, g_kbdLinePos);
        }
        g_kbdLinePos = 0;
        return;
    }

    /* 退格 */
    if (ch == '\b') {
        if (g_kbdLinePos > 0) {
            g_kbdLinePos--;
            OsPrintChar('\b');
        }
        return;
    }

    /* 普通字符：存入行缓冲区 + echo */
    if (g_kbdLinePos < OS_KBD_LINE_BUF_SIZE - 1) {
        g_kbdLineBuf[g_kbdLinePos++] = ch;
        OsPrintChar(ch);
    }
}

/* ====== 初始化 ====== */

OS_SEC_KERNEL_TEXT void OsKbdHwInit(void)
{
    U8 imr;

    g_kbdShiftState = 0;
    g_kbdLinePos = 0;

    /* 注册 IRQ1 中断处理 */
    OsHwiCreate(0x21, OsKbdIsr);

    /* 解屏蔽 IRQ1 */
    imr = OsInb(OS_PIC_M_DATA);
    imr &= ~0x02;
    OsOutb(OS_PIC_M_DATA, imr);
}
