#ifndef OS_IRQ_EXTERNAL_H
#define OS_IRQ_EXTERNAL_H
#include "os_def.h"

/* IRQ 处理函数类型 */
typedef void (*OsIrqHandlerFunc)(U32 irqNum);

/* 注册 IRQ 处理函数 */
extern U32 OsIrqCreate(U32 irqNum, OsIrqHandlerFunc isr);

/* IRQ 分发（由架构层中断入口调用） */
extern void OsIrqDispatcher(U32 irqNum);

/* 中断尾部处理（由架构层中断退出前调用） */
extern void OsIrqTail(void);

#endif /* OS_IRQ_EXTERNAL_H */