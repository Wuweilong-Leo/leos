#ifndef OS_VGA_EXTERNAL_H
#define OS_VGA_EXTERNAL_H
#include "os_def.h"

extern void OsVgaSetCursor(U16 pos);
extern U16 OsVgaGetCursor(void);
extern void OsVgaWriteChar(U32 pos, char c, U8 attr);
extern void OsVgaScrollUp(void);
extern void OsVgaClearLine(U32 row);

#endif /* OS_VGA_EXTERNAL_H */