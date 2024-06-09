#include "os_def.h"
#include "os_print_external.h"
OS_SEC_KERNEL_TEXT S32 main(void)
{
    OsPrintStr("hello kernel\n");
    while (1) {}
}