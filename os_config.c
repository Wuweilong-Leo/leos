#include "os_def.h"
#include "os_sys.h"
#include "os_hwi.h"
#include "os_timer.h"
#include "os_print_external.h"
#include "os_test.h"

typedef U32 (*OsConfigInitFunc)(void);

struct OsConfigInitInfo
{
    U32 mid;
    OsConfigInitFunc func;
};

extern U32 OsBssConfigInit(void);
extern U32 OsMemConfigInit(void);
extern U32 OsSysConfigInit(void);
extern U32 OsUsrConfigInit(void);
extern U32 OsSchedConfigInit(void);
extern U32 OsTaskConfigInit(void);
extern U32 OsSemConfigInit(void);

OS_SEC_KERNEL_DATA struct OsConfigInitInfo g_configInitTab[] = {
    {OS_MID_BSS, OsBssConfigInit},   {OS_MID_SYS, OsSysConfigInit},
    {OS_MID_HWI, OsHwiConfigInit},   {OS_MID_MEM, OsMemConfigInit},
    {OS_MID_USR, OsUsrConfigInit},   {OS_MID_SCHED, OsSchedConfigInit},
    {OS_MID_TASK, OsTaskConfigInit}, {OS_MID_TIMER, OsTimerConfigInit},
    {OS_MID_SEM, OsSemConfigInit},   {OS_MID_APP, OsAppConfigInit}};

OS_SEC_KERNEL_TEXT U32 OsConfigInit(void)
{
    U32 i;
    U32 configNum = sizeof(g_configInitTab) / sizeof(struct OsConfigInitInfo);
    OsConfigInitFunc func;
    U32 ret;

    for (i = 0; i < configNum; i++)
    {
        func = g_configInitTab[i].func;
        if (func == NULL)
        {
            continue;
        }
        ret = func();
        if (ret != OS_OK)
        {
            return ret;
        }
    }
    return OS_OK;
}