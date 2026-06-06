#ifndef OS_VGA_EXTERNAL_H
#define OS_VGA_EXTERNAL_H
#include "os_def.h"

/* VGA 硬件操作（由架构层实现） */
extern void OsVgaSetCursor(U16 pos);
extern U16 OsVgaGetCursor(void);
extern void OsVgaWriteChar(U32 pos, char c, U8 attr);
extern void OsVgaScrollUp(void);
extern void OsVgaClearLine(U32 row);

/* 将 VGA 操作注册到 Print 模块 */
extern U32 OsVgaRegisterToPrint(void);

#endif /* OS_VGA_EXTERNAL_H */