#include "os_def.h"
#include "os_print_external.h"
#include "os_print_internal.h"
#include "os_vga_external.h"

OS_SEC_KERNEL_TEXT U32 OsDevConfigInit(void)
{
    OsVgaRegisterToPrint();
    return OS_OK;
}