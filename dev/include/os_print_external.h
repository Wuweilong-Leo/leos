#ifndef OS_PRINT_EXTERNAL_H
#define OS_PRINT_EXTERNAL_H
#include "os_def.h"
extern void OsPrintChar(char c);
extern void OsPrintStr(char *str);
extern void OsPrintHex(U32 num);
extern void OsPrintSetCursor(U16 target);
extern U16 OsPrintGetCursor(void);
extern S32 kprintf(const char *fmt, ...);
#endif