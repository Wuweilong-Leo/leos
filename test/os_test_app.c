#include "os_def.h"
#include "os_task_external.h"
#include "os_test.h"
#include "string.h"

/* APP 模块初始化：只创建测试任务 */
OS_SEC_KERNEL_TEXT U32 OsAppConfigInit(void)
{
    U32 tskId;
    struct OsTaskCreateParam param;

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TestMain");
    param.prio = 2;
    param.entryFunc = OsTestMainTask;
    OsTaskCreate(&param, &tskId);
    OsTaskResume(tskId);

    return OS_OK;
}
