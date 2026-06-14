#include "os_def.h"
#include "os_test.h"
#include "os_vga_internal.h"
#include "os_hwi.h"
#include "os_task_external.h"

/* APP 模块初始化 */
OS_SEC_KERNEL_TEXT U32 OsAppConfigInit(void)
{
    /* 注册软中断：用于任务删除自己时回收栈 */
    OsHwiCreate(0x30, OsTaskRecycleHandler);

    OsTestTaskInit();
    OsTestSemInit();
    OsTestProcessInit();

    return OS_OK;
}