#include "os_def.h"
#include "string.h"

extern uintptr_t _os_bss_table_start;
extern uintptr_t _os_bss_table_end;

OS_SEC_KERNEL_TEXT void OsBssInit(void)
{
    uintptr_t bssTabStart = &_os_bss_table_start;
    uintptr_t bssTabEnd = (U32)bssTabStart + 4;
    uintptr_t bssStart;
    uintptr_t bssEnd;

    while (bssTabEnd < &_os_bss_table_end) {
        bssStart = (uintptr_t)*bssTabStart;
        bssEnd = (uintptr_t)*bssTabEnd;

        memset(bssStart, 0, (U32)bssEnd - (U32)bssStart);

        bssTabStart += 1;
        bssTabEnd += 1;
    }
}