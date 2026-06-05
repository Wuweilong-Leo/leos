#include "os_def.h"
#include "os_print_external.h"
#include "os_test.h"

/* APP 模块初始化 — 在 OsConfigInit 表末尾调用 */
OS_SEC_KERNEL_TEXT U32 OsAppConfigInit(void)
{
    kprintf("[APP] init start\n");

    /* 注册所有测试模块 */
    OsTestTaskInit();
    OsTestSemInit();
    OsTestProcessInit();

    kprintf("[APP] init done\n");
    return OS_OK;
}