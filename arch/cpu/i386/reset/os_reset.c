#include "os_reset.h"
#include "os_io_i386.h"
#include "os_hwi.h"

/*
 * i386 复位实现
 *
 * 方案1（首选）: 8042 键盘控制器复位
 *   向端口 0x64 写 0xFE，触发 CPU reset pulse
 *   这是 PC 兼容机最经典的复位方式
 *
 * 方案2（后备）: triple fault
 *   关中断 + 加载空 IDT + 触发异常 → triple fault → CPU reset
 *   任何 x86 CPU 都支持，作为后备方案
 */

OS_SEC_KERNEL_TEXT void OsReboot(void)
{
    /* 关中断，复位过程不允许被打断 */
    OsIntLock();

    /* 等待 8042 键盘控制器输入缓冲区空闲 */
    while ((OsInb(0x64) & 0x02) != 0) {
    }

    /* 向 8042 发送复位命令 0xFE = Pulse Output Port, low pulse on reset line */
    OsOutb(0x64, 0xFE);

    /*
     * 如果 8042 复位没有生效（某些硬件/模拟器不支持），
     * 走 triple fault 后备路径：
     * 加载空 IDT，然后触发 int 0 → triple fault → reset
     */
    {
        static struct {
            U16 limit;
            U32 base;
        } OS_STRUCT_PACKED nullIdtInfo = {0, 0};

        OS_EMBED_ASM("lidt %0" ::"m"(nullIdtInfo));
        OS_EMBED_ASM("int $0x00");
    }

    /* 不应到达这里 */
    while (1) {
    }
}