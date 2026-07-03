#include "os_def.h"
#include "os_uart_external.h"
#include "os_io_i386.h"
#include "os_hwi.h"
#include "os_print_external.h"

/* COM1 8250/16550 寄存器 */
#define OS_UART_COM1_BASE 0x3F8
#define OS_UART_THR       (OS_UART_COM1_BASE + 0) /* 发送保持寄存器 */
#define OS_UART_IER       (OS_UART_COM1_BASE + 1) /* 中断使能 */
#define OS_UART_FCR       (OS_UART_COM1_BASE + 2) /* FIFO 控制 */
#define OS_UART_LCR       (OS_UART_COM1_BASE + 3) /* 线路控制 */
#define OS_UART_MCR       (OS_UART_COM1_BASE + 4) /* modem 控制 */
#define OS_UART_LSR       (OS_UART_COM1_BASE + 5) /* 线路状态 */
#define OS_UART_LSR_THRE  0x20                    /* 发送保持寄存器空 */

OS_SEC_KERNEL_TEXT void OsUartInit(void)
{
    /* 关中断,8N1,波特率 115200(divisor=1),开 FIFO,DTR+RTS */
    OsOutb(OS_UART_IER, 0x00);
    OsOutb(OS_UART_LCR, 0x80); /* 置 DLAB */
    OsOutb(OS_UART_THR, 0x01); /* divisor 低字节 = 1 */
    OsOutb(OS_UART_IER, 0x00); /* divisor 高字节 = 0 */
    OsOutb(OS_UART_LCR, 0x03); /* 清 DLAB,8N1 */
    OsOutb(OS_UART_FCR, 0xC7); /* 使能 FIFO,清空,14 字节阈值 */
    OsOutb(OS_UART_MCR, 0x0B); /* DTR + RTS + OUT2 */
}

OS_SEC_KERNEL_TEXT void OsUartPutc(char c)
{
    while ((OsInb(OS_UART_LSR) & OS_UART_LSR_THRE) == 0) {
    }
    OsOutb(OS_UART_THR, (U8)c);
}

OS_SEC_KERNEL_TEXT void OsUartPuts(const char *s)
{
    while (*s) {
        if (*s == '\n') {
            OsUartPutc('\r'); /* 终端需要 \r\n */
        }
        OsUartPutc(*s++);
    }
}

OS_SEC_KERNEL_TEXT size_t OsUartPrintf(const char *fmt, ...)
{
    char buf[256] = {0};
    void *args;
    size_t len;
    enum OsIntStatus intSave;

    intSave = OsIntLock();
    OS_VA_START(args, fmt);
    len = vsprintf(buf, sizeof(buf), fmt, args);
    OS_VA_END(args);
    OsUartPuts(buf);
    OsIntRestore(intSave);

    return len;
}