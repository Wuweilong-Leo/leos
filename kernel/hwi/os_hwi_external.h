#ifndef OS_HWI_EXTERNAL_H
#define OS_HWI_EXTERNAL_H
#include "os_def.h"

/* IRQ 处理函数类型 */
typedef void (*OsHwiHandlerFunc)(U32 irqNum);

/* 注册 IRQ 处理函数 */
extern U32 OsHwiCreate(U32 irqNum, OsHwiHandlerFunc isr);

/* IRQ 分发（由架构层中断入口调用） */
extern void OsHwiDispatcher(U32 irqNum);

/* 中断尾部处理（由架构层中断退出前调用） */
extern void OsHwiTail(void);

#endif /* OS_HWI_EXTERNAL_H */