#ifndef OS_SHELL_EXTERNAL_H
#define OS_SHELL_EXTERNAL_H
#include "os_def.h"

/* 将一行键盘输入提交给 shell 任务（由 kbd ISR 调用） */
extern void OsShellInput(const char *cmd, U32 len);

/* Shell 模块初始化（创建 shell 任务） */
extern U32 OsShellConfigInit(void);

#endif /* OS_SHELL_EXTERNAL_H */
