#ifndef OS_HWI_EXTERNAL_H
#define OS_HWI_EXTERNAL_H
#include "os_def.h"

/* 中断处理函数类型 */
typedef void (*OsHwiHandlerFunc)(U32 hwiNum);

/* 注册中断处理函数 */
extern U32 OsHwiCreate(U32 hwiNum, OsHwiHandlerFunc isr);

/* 中断分发（由架构层中断入口调用） */
extern void OsHwiDispatcher(U32 hwiNum);

/* 中断尾部处理（由架构层中断退出前调用） */
extern void OsHwiTail(void);

#endif /* OS_HWI_EXTERNAL_H */