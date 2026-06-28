#ifndef OS_UART_EXTERNAL_H
#define OS_UART_EXTERNAL_H
#include "os_def.h"

/* 8250/16550 串口(COM1)输出 API。
 * - OsUartPutc/Puts 为串口专用通道,不经 VGA,供需要"只进串口日志"的场景(如测试逐条输出)。
 * - 内核通用输出(kprintf/OS_LOG)由 print 层镜像到串口,见 OsPrintMirrorSerial。
 */
extern void OsUartInit(void);
extern void OsUartPutc(char c);
extern void OsUartPuts(const char *s);
/* 串口专用格式化打印(与 kprintf 同 %s/%x/%d/%c 语法,只输出到 COM1,不进 VGA) */
extern size_t OsUartPrintf(const char *fmt, ...);

#endif /* OS_UART_EXTERNAL_H */