#include "os_shell_external.h"
#include "os_task_external.h"
#include "os_msg_external.h"
#include "os_print_external.h"
#include "string.h"

/*
 * Shell 模块 — 接收键盘输入的命令行并执行
 *
 * OsShellInput: kbd ISR 收到回车后调用，把一行命令以消息发给 shell 任务
 * Shell 任务循环: OsMsgRecv → 解析命令 → 执行 → 打印结果
 */

#define OS_SHELL_PRIO     30   /* 低于测试任务，高于 idle */
#define OS_SHELL_PROMPT   "leos> "

static OS_SEC_KERNEL_BSS U32 g_shellPid;

/* ====== 供 kbd ISR 调用 ====== */

OS_SEC_KERNEL_TEXT void OsShellInput(const char *cmd, U32 len)
{
    void *msg;

    if (len == 0 || g_shellPid == 0) {
        return;
    }

    msg = OsMsgAlloc(len + 1);
    if (msg == NULL) {
        return;
    }

    memcpy(msg, cmd, len);
    ((char *)msg)[len] = '\0';
    OsMsgSend(g_shellPid, msg);
}

/* ====== 命令处理 ====== */

static OS_SEC_KERNEL_TEXT void OsShellHelp(void)
{
    kprintf("help    - show commands\n");
    kprintf("clear   - clear screen\n");
    kprintf("ver     - show version\n");
}

static OS_SEC_KERNEL_TEXT void OsShellClear(void)
{
    U32 i;
    for (i = 0; i < 25; i++) {
        OsPrintChar('\n');
    }
}

static OS_SEC_KERNEL_TEXT void OsShellVer(void)
{
    kprintf("leos v0.1\n");
}

static OS_SEC_KERNEL_TEXT void OsShellExecCmd(const char *cmd)
{
    if (strcmp(cmd, "help") == 0) {
        OsShellHelp();
    } else if (strcmp(cmd, "clear") == 0) {
        OsShellClear();
    } else if (strcmp(cmd, "ver") == 0) {
        OsShellVer();
    } else if (cmd[0] != '\0') {
        kprintf("unknown: %s\n", cmd);
    }
}

/* ====== Shell 任务 ====== */

OS_SEC_KERNEL_TEXT void OsShellEntry(void *p1, void *p2, void *p3, void *p4)
{
    void *msg;
    U32 ret;
    (void)p1; (void)p2; (void)p3; (void)p4;

    kprintf("\n=== leos shell ===\n");
    kprintf(OS_SHELL_PROMPT);

    while (1) {
        ret = OsMsgRecv(OS_MSG_WAIT_FOREVER, &msg);
        if (ret != OS_OK) {
            continue;
        }

        OsShellExecCmd((const char *)msg);
        OsMsgFree(msg);
        kprintf(OS_SHELL_PROMPT);
    }
}

/* ====== 初始化 ====== */

OS_SEC_KERNEL_TEXT U32 OsShellConfigInit(void)
{
    struct OsTaskCreateParam param;
    U32 tskId;

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "shell");
    param.prio = OS_SHELL_PRIO;
    param.entryFunc = OsShellEntry;

    if (OsTaskCreate(&param, &tskId) != OS_OK) {
        return 1;
    }

    g_shellPid = tskId;
    OsTaskResume(tskId);
    return OS_OK;
}
