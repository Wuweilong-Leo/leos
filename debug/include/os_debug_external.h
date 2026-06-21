#ifndef OS_DEBUG_EXTERNAL_H
#define OS_DEBUG_EXTERNAL_H
#include "os_def.h"
#include "os_print_external.h"
#include "os_reset.h"

/* ---- 日志级别 ---- */

enum OsLogLevel {
    OS_LOG_NONE = 0,  /* 静默，不输出任何日志 */
    OS_LOG_ERROR = 1, /* 仅错误 */
    OS_LOG_WARN = 2,  /* 错误 + 警告 */
    OS_LOG_INFO = 3,  /* 一般信息 */
    OS_LOG_DEBUG = 4, /* 详细调试 */
};

extern enum OsLogLevel g_logLevel;

extern void OsDebugSetLogLevel(enum OsLogLevel level);
extern enum OsLogLevel OsDebugGetLogLevel(void);

/* ---- Panic / Assert ---- */

#define OS_PANIC(...)                                                                               \
    do {                                                                                           \
        kprintf("[PANIC][%s:%d] " __VA_ARGS__, __func__, __LINE__);                                 \
        OsPanic();                                                                                  \
    } while (0)
extern void OsDebugAssertFail(const char *filename, U32 line, const char *func, const char *cond);

#define OS_ASSERT(cond)                                                                            \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            OsDebugAssertFail(__FILE__, __LINE__, __func__, #cond);                                \
        }                                                                                          \
    } while (0)

/* ---- 日志宏 ---- */

#define OS_LOG_LEVEL_CHECK(level) ((level) <= g_logLevel)

#define OS_LOG(level, ...)                                                                         \
    do {                                                                                           \
        if (OS_LOG_LEVEL_CHECK(level)) {                                                           \
            kprintf(__VA_ARGS__);                                                                  \
        }                                                                                          \
    } while (0)

/* 便捷日志宏 */
#define OS_LOG_ERROR(...) OS_LOG(OS_LOG_ERROR, "[E][%s:%d] " __VA_ARGS__, __func__, __LINE__)
#define OS_LOG_WARN(...)  OS_LOG(OS_LOG_WARN, "[W][%s:%d] " __VA_ARGS__, __func__, __LINE__)
#define OS_LOG_INFO(...)  OS_LOG(OS_LOG_INFO, "[I][%s:%d] " __VA_ARGS__, __func__, __LINE__)
#define OS_LOG_DEBUG(...) OS_LOG(OS_LOG_DEBUG, "[D][%s:%d] " __VA_ARGS__, __func__, __LINE__)

/* ---- 兼容旧接口（映射到日志宏） ---- */

#if (DEBUG_ENABLE != FALSE)
#define OS_DEBUG_PRINT_CHAR(c)  OsPrintChar(c)
#define OS_DEBUG_PRINT_HEX(hex) OsPrintHex(hex)
#define OS_DEBUG_PRINT_STR(str) OsPrintStr(str)
#define OS_DEBUG_KPRINT(...)    kprintf(__VA_ARGS__)
#else
#define OS_DEBUG_PRINT_CHAR(c)
#define OS_DEBUG_PRINT_HEX(hex)
#define OS_DEBUG_PRINT_STR(str)
#define OS_DEBUG_KPRINT(...)
#endif

/* ---- 调试函数声明 ---- */

struct OsList;
struct OsTaskCb;
struct OsMemPool;

extern void OsDebugPrintList(struct OsList *list);
extern void OsDebugPrintRdyList(void);
extern void OsDebugPrintTaskInfo(struct OsTaskCb *tsk);
extern void OsDebugPrintAllTasks(void);
extern void OsDebugPrintMemPool(struct OsMemPool *pool, const char *name);
extern void OsDebugSystemStatus(void);

#endif