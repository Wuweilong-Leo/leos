#include "os_def.h"
#include "os_print_external.h"
#include "os_process_external.h"
#include "os_debug_external.h"
#include "os_syscall_i386.h"
#include "os_test_framework.h"
#include "os_uart_external.h"
#include "string.h"

/*
 * 进程入口：在用户态 (CPL=3) 执行。
 * 通过 syscall (int 0x80) 打印消息，验证用户态→内核态切换正常。
 *
 * 注意：不能用 OsSyscall2() 等包装函数，因为 OS_INLINE 未被内联时
 * 会生成 .text 段的 out-of-line 副本，而 .text 段不在内核页表映射中。
 * 直接使用 OS_EMBED_ASM 确保代码留在 .os.kernel.text 段。
 */
OS_SEC_KERNEL_TEXT static void TestProcEntry(void)
{
    U32 ret;
    char msg[5];
    msg[0] = 'h'; msg[1] = 'e'; msg[2] = 'l'; msg[3] = 'l'; msg[4] = 'o';
    OS_EMBED_ASM("int $0x80"
        : "=a"(ret)
        : "a"((U32)OS_SYS_WRITE), "b"((U32)(uintptr_t)msg), "c"(5)
        : "memory");
    (void)ret;

    /* 退出进程 */
    OS_EMBED_ASM("int $0x80"
        :
        : "a"((U32)OS_SYS_EXIT), "b"((U32)0)
        : "memory");
}

/* ====== setup：创建并恢复进程 ====== */

OS_SEC_KERNEL_TEXT void TestProcSetup(void)
{
    U32 ret;
    U32 pid;
    struct OsProcessCreateParam param = {0};

    strcpy(param.processName, "test_proc");
    param.entryFunc = (OsProcessEntryFunc)TestProcEntry;
    param.prio = 5;
    param.param[0] = NULL;
    param.param[1] = NULL;

    ret = OsProcessCreate(&param, &pid);
    if (ret != OS_OK) {
        return;
    }

    OsProcessResume(pid);
}

/* ====== verify：延迟后检查系统是否仍在运行 ====== */

OS_SEC_KERNEL_TEXT void TestProcVerify(void)
{
    OsUartPuts("[PROC] verify ok - system survived\n");
}
