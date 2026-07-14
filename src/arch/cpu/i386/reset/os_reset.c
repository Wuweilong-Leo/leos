#include "os_reset.h"
#include "os_hwi.h"

/*
 * OsPanic — 触发 #UD 异常，走正常异常处理流程
 *
 * ud2 是 i386 明确定义的"未定义指令"，
 * CPU 自动压栈保存上下文，进入：
 *   OsExcVector0x06 → OsExcDispatcher → OsExcReport → while(1) 挂死
 *
 * 这样异常处理流程能看到完整的寄存器现场。
 */
OS_SEC_KERNEL_TEXT void OsPanic(void)
{
    OS_EMBED_ASM("ud2");
    __builtin_unreachable();
}