#include "os_def.h"
#include "os_vga_external.h"
#include "os_print_internal.h"

uintptr_t _os_kernel_sp_start;
uintptr_t _os_kernel_sp_end;

OS_SEC_KERNEL_BSS uintptr_t g_kernelStackLow;
OS_SEC_KERNEL_BSS uintptr_t g_kernelStackHigh;

OS_SEC_KERNEL_TEXT void OsSysRegKernelStack(void)
{
    g_kernelStackLow = &_os_kernel_sp_start;
    g_kernelStackHigh = &_os_kernel_sp_end;
}

OS_SEC_KERNEL_TEXT U32 OsSysConfigInit(void)
{
    OsSysRegKernelStack();
    OsVgaRegisterToPrint();
    return OS_OK;
}