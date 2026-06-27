#include "os_def.h"
#include "os_print_external.h"
#include "os_print_internal.h"
#include "os_vga_external.h"
#include "os_uart_external.h"
#include "os_kbd_external.h"

OS_SEC_KERNEL_TEXT U32 OsDevConfigInit(void)
{
    OsUartInit(); /* 串口先就绪,之后的内核日志即可镜像到串口 */
    OsVgaRegisterToPrint();
    OsKbdHwInit(); /* 注册 IRQ1 + 解屏蔽，按键后 ISR 直接 echo */
    return OS_OK;
}