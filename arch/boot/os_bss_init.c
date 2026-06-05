#include "os_def.h"
#include "string.h"

extern uintptr_t _os_bss_table_start;
extern uintptr_t _os_bss_table_end;

struct OsBssInfo
{
    uintptr_t bssStart;
    uintptr_t bssEnd;
};

OS_SEC_KERNEL_TEXT void OsBssInit(void)
{
    struct OsBssInfo *bssTab = (struct OsBssInfo *)(uintptr_t)&_os_bss_table_start;
    size_t bssTabSize = (uintptr_t)&_os_bss_table_end - (uintptr_t)&_os_bss_table_start;
    U32 bssTabNum;
    U32 i;
    uintptr_t bssStart;
    uintptr_t bssEnd;

    if (bssTabSize % sizeof(struct OsBssInfo) != 0)
    {
        return;
    }

    bssTabNum = bssTabSize / sizeof(struct OsBssInfo);

    for (i = 0; i < bssTabNum; i++)
    {
        bssStart = bssTab[i].bssStart;
        bssEnd = bssTab[i].bssEnd;

        if (bssEnd <= bssStart)
        {
            continue;
        }

        memset(bssStart, 0, bssEnd - bssStart);
    }
}

OS_SEC_KERNEL_TEXT U32 OsBssConfigInit(void)
{
    OsBssInit();
    return OS_OK;
}