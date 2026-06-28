#ifndef OS_MSG_EXTERNAL_H
#define OS_MSG_EXTERNAL_H
#include "os_def.h"
#include "os_sys.h"

/* 超时常量 */
#define OS_MSG_WAIT_FOREVER  0xFFFFFFFFU
#define OS_MSG_NO_WAIT       0

/* 错误码 */
#define OS_MSG_ALLOC_FAIL        OS_BUILD_ERR_CODE(OS_MID_MSG, 0x0)
#define OS_MSG_SEND_PID_INVALID  OS_BUILD_ERR_CODE(OS_MID_MSG, 0x1)
#define OS_MSG_SEND_BUF_INVALID  OS_BUILD_ERR_CODE(OS_MID_MSG, 0x2)
#define OS_MSG_RECV_TIMEOUT      OS_BUILD_ERR_CODE(OS_MID_MSG, 0x3)
#define OS_MSG_RECV_UNAVAILABLE  OS_BUILD_ERR_CODE(OS_MID_MSG, 0x4)
#define OS_MSG_FREE_BUF_INVALID  OS_BUILD_ERR_CODE(OS_MID_MSG, 0x5)
#define OS_MSG_PARAM_INVALID     OS_BUILD_ERR_CODE(OS_MID_MSG, 0x6)

/* 公共 API */
extern void *OsMsgAlloc(size_t size);
extern U32   OsMsgSend(U32 targetPid, void *msgBuf);
extern U32   OsMsgRecv(U32 timeout, void **msgBuf);
extern U32   OsMsgFree(void *msgBuf);
extern U32   OsMsgConfigInit(void);

#endif
