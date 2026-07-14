#ifndef OS_SHELL_EXTERNAL_H
#define OS_SHELL_EXTERNAL_H
#include "os_def.h"

/* ====== 命令描述符 ====== */

struct OsShellCmd {
    const char *name;                           /* 命令名 */
    const char *help;                           /* 一行帮助文本 */
    U32 (*exec)(U32 argc, char *argv[]);        /* 执行函数，返回 0=成功 */
};

#define OS_SHELL_CMD(name_, help_, exec_)  \
    { (name_), (help_), (exec_) }

/* ====== Shell API ====== */

/* 将一行键盘输入提交给 shell 任务（由 kbd ISR 调用） */
extern void OsShellInput(const char *cmd, size_t len);

/* Shell 模块初始化（创建 shell 任务） */
extern U32 OsShellConfigInit(void);

#endif /* OS_SHELL_EXTERNAL_H */
