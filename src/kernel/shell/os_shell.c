#include "os_shell_external.h"
#include "os_task_external.h"
#include "os_msg_external.h"
#include "os_print_external.h"
#include "os_debug_external.h"
#include "os_tick_external.h"
#include "os_mem_external.h"
#include "os_test_framework.h"
#include "os_target.h"
#include "os_symtab_external.h"
#include "string.h"

/*
 * Shell 模块 — 命令注册表 + 解析执行
 *
 * OsShellInput: kbd ISR 收到回车后调用，把一行命令以消息发给 shell 任务
 * Shell 任务循环: OsMsgRecv → argv 解析 → 查命令表 → exec(argc, argv)
 */

#define OS_SHELL_PRIO     30   /* 低于测试任务，高于 idle */
#define OS_SHELL_PROMPT   "leos> "
#define OS_SHELL_MAX_ARGV  8

static OS_SEC_KERNEL_BSS U32 g_shellPid;

/* ====== 供 kbd ISR 调用 ====== */

OS_SEC_KERNEL_TEXT void OsShellInput(const char *cmd, size_t len)
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

/* ====== 命令实现（前向声明） ====== */

static OS_SEC_KERNEL_TEXT U32 OsShellCmdHelp(U32 argc, char *argv[]);
static OS_SEC_KERNEL_TEXT U32 OsShellCmdClear(U32 argc, char *argv[]);
static OS_SEC_KERNEL_TEXT U32 OsShellCmdVer(U32 argc, char *argv[]);
static OS_SEC_KERNEL_TEXT U32 OsShellCmdTaskList(U32 argc, char *argv[]);
static OS_SEC_KERNEL_TEXT U32 OsShellCmdMemInfo(U32 argc, char *argv[]);
static OS_SEC_KERNEL_TEXT U32 OsShellCmdUptime(U32 argc, char *argv[]);
static OS_SEC_KERNEL_TEXT U32 OsShellCmdLogLevel(U32 argc, char *argv[]);
static OS_SEC_KERNEL_TEXT U32 OsShellCmdTest(U32 argc, char *argv[]);
#ifdef OS_OPTION_SYMTAB
static OS_SEC_KERNEL_TEXT U32 OsShellCmdSyms(U32 argc, char *argv[]);
static OS_SEC_KERNEL_TEXT U32 OsShellCmdAddr2Name(U32 argc, char *argv[]);
#endif

/* ====== 命令注册表 ====== */

OS_SEC_KERNEL_DATA const struct OsShellCmd g_shellCmds[] = {
    OS_SHELL_CMD("help",     "show commands",       OsShellCmdHelp),
    OS_SHELL_CMD("clear",    "clear screen",        OsShellCmdClear),
    OS_SHELL_CMD("ver",      "show version",        OsShellCmdVer),
    OS_SHELL_CMD("tasklist", "show all tasks",      OsShellCmdTaskList),
    OS_SHELL_CMD("meminfo",  "show memory pools",   OsShellCmdMemInfo),
    OS_SHELL_CMD("uptime",   "show system ticks",   OsShellCmdUptime),
    OS_SHELL_CMD("loglevel", "set log level 0-4",   OsShellCmdLogLevel),
    OS_SHELL_CMD("test",     "run all tests",       OsShellCmdTest),
#ifdef OS_OPTION_SYMTAB
    OS_SHELL_CMD("syms",     "list/search symbols", OsShellCmdSyms),
    OS_SHELL_CMD("addr2name","addr to symbol name", OsShellCmdAddr2Name),
#endif
};

OS_SEC_KERNEL_DATA const U32 g_shellCmdCnt = sizeof(g_shellCmds) / sizeof(struct OsShellCmd);

/* ====== 命令实现 ====== */

static OS_SEC_KERNEL_TEXT U32 OsShellCmdHelp(U32 argc, char *argv[])
{
    U32 i;
    (void)argc; (void)argv;

    for (i = 0; i < g_shellCmdCnt; i++) {
        kprintf("%-10s %s\n", g_shellCmds[i].name, g_shellCmds[i].help);
    }
    return 0;
}

static OS_SEC_KERNEL_TEXT U32 OsShellCmdClear(U32 argc, char *argv[])
{
    U32 i;
    (void)argc; (void)argv;

    for (i = 0; i < 25; i++) {
        OsPrintChar('\n');
    }
    return 0;
}

static OS_SEC_KERNEL_TEXT U32 OsShellCmdVer(U32 argc, char *argv[])
{
    (void)argc; (void)argv;
    kprintf("leos v0.1\n");
    return 0;
}

static OS_SEC_KERNEL_TEXT U32 OsShellCmdTaskList(U32 argc, char *argv[])
{
    (void)argc; (void)argv;
    OsDebugPrintAllTasks();
    return 0;
}

static OS_SEC_KERNEL_TEXT U32 OsShellCmdMemInfo(U32 argc, char *argv[])
{
    (void)argc; (void)argv;
    OsDebugPrintMemPool(&g_kernelPhyMemPool, "kernel-phy");
    OsDebugPrintMemPool(&g_kernelVirMemPool, "kernel-vir");
    OsDebugPrintMemPool(&g_usrPhyMemPool, "usr-phy");
    return 0;
}

static OS_SEC_KERNEL_TEXT U32 OsShellCmdUptime(U32 argc, char *argv[])
{
    (void)argc; (void)argv;
    kprintf("uptime: %u ticks\n", (U32)g_uniTicks);
    return 0;
}

static OS_SEC_KERNEL_TEXT U32 OsShellCmdLogLevel(U32 argc, char *argv[])
{
    U32 level;
    (void)argv;

    if (argc < 2) {
        kprintf("loglevel: %u\n", OsDebugGetLogLevel());
        return 0;
    }

    level = 0;
    while (argv[1][0] >= '0' && argv[1][0] <= '9') {
        level = level * 10 + (argv[1][0] - '0');
        argv[1]++;
    }

    if (level > OS_LOG_DEBUG) {
        kprintf("invalid level (0-4)\n");
        return 1;
    }

    OsDebugSetLogLevel((enum OsLogLevel)level);
    kprintf("loglevel set to %u\n", level);
    return 0;
}

static OS_SEC_KERNEL_TEXT U32 OsShellCmdTest(U32 argc, char *argv[])
{
    (void)argc; (void)argv;
    OsTestRunAll();
    OsTestPrintSummary();
    return 0;
}

#ifdef OS_OPTION_SYMTAB
static OS_SEC_KERNEL_TEXT void OsShellSymPrintCb(const struct OsSymtabEntry *ent)
{
    kprintf("0x%08x %s\n", (uintptr_t)ent->addr, ent->name);
}

static OS_SEC_KERNEL_TEXT U32 OsShellCmdSyms(U32 argc, char *argv[])
{
    if (argc < 2) {
        kprintf("%u symbols\n", g_symtabCnt);
        return 0;
    }

    /* 前缀匹配 */
    U32 match = OsSymtabPrefixMatch(argv[1], OsShellSymPrintCb);
    kprintf("(%u matched)\n", match);
    return 0;
}

static OS_SEC_KERNEL_TEXT U32 OsShellCmdAddr2Name(U32 argc, char *argv[])
{
    uintptr_t addr;
    const struct OsSymtabEntry *ent;
    const char *name;

    if (argc < 2) {
        kprintf("usage: addr2name 0xXXXXXXXX\n");
        return 1;
    }

    /* 解析十六进制地址 */
    addr = 0;
    name = argv[1];

    if (name[0] == '0' && (name[1] == 'x' || name[1] == 'X')) {
        name += 2;
    }

    while (*name) {
        U32 digit;
        if (*name >= '0' && *name <= '9') {
            digit = *name - '0';
        } else if (*name >= 'a' && *name <= 'f') {
            digit = *name - 'a' + 10;
        } else if (*name >= 'A' && *name <= 'F') {
            digit = *name - 'A' + 10;
        } else {
            break;
        }
        addr = addr * 16 + digit;
        name++;
    }

    ent = OsSymtabLookup(addr);
    if (ent == (void *)0) {
        kprintf("0x%08x <unknown>\n", (uintptr_t)addr);
        return 1;
    }

    if (addr == (uintptr_t)ent->addr) {
        kprintf("0x%08x %s\n", (uintptr_t)addr, ent->name);
    } else {
        kprintf("0x%08x %s+0x%x\n", (uintptr_t)addr, ent->name, (uintptr_t)(addr - (uintptr_t)ent->addr));
    }

    return 0;
}
#endif /* OS_OPTION_SYMTAB */

/* ====== 命令解析与执行 ====== */

static OS_SEC_KERNEL_TEXT U32 OsShellParseLine(char *line, char *argv[], U32 maxArgv)
{
    U32 argc = 0;

    while (*line && argc < maxArgv) {
        while (*line == ' ') {
            *line++ = '\0';
        }
        if (*line == '\0') {
            break;
        }
        argv[argc++] = line;
        while (*line && *line != ' ') {
            line++;
        }
    }

    return argc;
}

static OS_SEC_KERNEL_TEXT void OsShellExecCmd(char *line)
{
    char *argv[OS_SHELL_MAX_ARGV];
    U32 argc;
    U32 i;

    argc = OsShellParseLine(line, argv, OS_SHELL_MAX_ARGV);
    if (argc == 0) {
        return;
    }

    for (i = 0; i < g_shellCmdCnt; i++) {
        if (strcmp(argv[0], g_shellCmds[i].name) == 0) {
            g_shellCmds[i].exec(argc, argv);
            return;
        }
    }

    kprintf("unknown: %s\n", argv[0]);
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

        OsShellExecCmd((char *)msg);
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
