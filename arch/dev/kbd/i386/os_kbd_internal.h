#ifndef OS_KBD_INTERNAL_H
#define OS_KBD_INTERNAL_H
#include "os_kbd_external.h"

/* i8042 键盘控制器端口 */
#define OS_KBD_DATA_PORT    0x60
#define OS_KBD_STATUS_PORT  0x64
#define OS_KBD_CMD_PORT     0x64

/* Shift 状态掩码 */
#define OS_KBD_LSHIFT_MSK  0x01
#define OS_KBD_RSHIFT_MSK  0x02

/* 行缓冲区 */
#define OS_KBD_LINE_BUF_SIZE  128

#endif /* OS_KBD_INTERNAL_H */
