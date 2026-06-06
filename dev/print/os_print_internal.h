#ifndef OS_PRINT_INTERNAL_H
#define OS_PRINT_INTERNAL_H

#include "os_vga_external.h"
#include "os_vga_internal.h"

#define OS_VA_START(ap, v) ((ap) = (void *)&(v))
#define OS_VA_ARG(ap, t)   (*(t *)((ap) += 4))
#define OS_VA_END(ap)       ((ap) = NULL)

#endif /* OS_PRINT_INTERNAL_H */