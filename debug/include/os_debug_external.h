#ifndef OS_DEBUG_H
#define OS_DEBUG_H
#include "os_def.h"
#include "os_print_external.h"

#define DEBUG_ENABLE TRUE

extern void OsDebugPanicSpin(char *filename, U32 line, const char *func,
                             const char *condition);
extern void OsDebugPrintRdyList(void);

#define OS_PANIC(...)                                                          \
  OsDebugPanicSpin(__FILE__, __LINE__, __func__, __VA_ARGS__)

#if (DEBUG_ENABLE == TRUE)
#define OS_ASSERT(condition)                                                   \
    do {                                                                         \
        if (condition) {                                                           \
        ;                                                                        \
        } else {                                                                   \
            OS_PANIC(#condition);                                                    \
        }                                                                          \
    } while (0);

#define OS_DEBUG_PRINT_CHAR(c)  OsPrintChar(c)
#define OS_DEBUG_PRINT_HEX(hex) OsPrintHex(hex)
#define OS_DEBUG_PRINT_STR(str) OsPrintStr(str)
#define OS_DEBUG_KPRINT(fmt, ...) kprintf(fmt, __VA_ARGS__)
#else
#define ASSERT(condition)
#define OS_DEBUG_PRINT_CHAR(c)
#define OS_DEBUG_PRINT_HEX(hex)
#define OS_DEBUG_PRINT_STR(str)
#endif

#endif