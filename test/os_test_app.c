#include "os_def.h"
#include "os_test.h"
#include "os_vga_internal.h"

/* APP 模块初始化 */
OS_SEC_KERNEL_TEXT U32 OsAppConfigInit(void)
{
    OsTestMemInit();
    OsTestPgFaultInit();
    OsTestTaskInit();
    OsTestSemInit();

    return OS_OK;
}