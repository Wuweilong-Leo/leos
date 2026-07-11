#include "os_def.h"
#include "os_print_external.h"
#include "os_task_external.h"
#include "os_sched_external.h"
#include "os_msg_external.h"
#include "os_mem_external.h"
#include "os_tick_external.h"
#include "os_hwi.h"
#include "os_test_framework.h"
#include "os_uart_external.h"
#include "string.h"

/*
 * MSG 测试 — 任务间点对点消息
 */

/* ====== 辅助 ====== */

static OS_SEC_KERNEL_TEXT U32 TestMsgCreateTask(const char *name, U32 prio, OsTaskEntryFunc entry)
{
    U32 tskId;
    struct OsTaskCreateParam param;
    memset(&param, 0, sizeof(param));
    strcpy(param.name, name);
    param.prio = prio;
    param.entryFunc = entry;
    OsTaskCreate(&param, &tskId);
    return tskId;
}

static OS_SEC_KERNEL_TEXT void TestMsgCleanupTask(U32 tskId)
{
    U32 ret;
    U32 retry = 0;
    struct OsTaskCb *tskCb = OS_TASK_GET_CB(tskId);
    if (!(tskCb->status & OS_TASK_STATUS_USED)) {
        return;  /* 任务已自行退出 */
    }
    OsTaskSuspend(tskId);
    while ((ret = OsTaskDelete(tskId)) != OS_OK && retry < 5) {
        tskCb = OS_TASK_GET_CB(tskId);
        if (!(tskCb->status & OS_TASK_STATUS_USED)) {
            return;  /* 重试过程中任务退出 */
        }
        OsTaskResume(tskId);
        OsTaskDelay(2);
        OsTaskSuspend(tskId);
        retry++;
    }
    (void)ret;
}

/* ====== basic-send-recv ====== */

OS_SEC_KERNEL_BSS U32 g_msgTskSender;
OS_SEC_KERNEL_BSS U32 g_msgTskReceiver;
OS_SEC_KERNEL_BSS volatile U32 g_msgBasicRecvOk;

OS_SEC_KERNEL_TEXT void TestMsgSenderBasic(void *p1, void *p2, void *p3, void *p4)
{
    void *buf;
    U32 *data;
    (void)p1; (void)p2; (void)p3; (void)p4;

    buf = OsMsgAlloc(sizeof(U32) * 2);
    if (buf == NULL) {
        return;
    }
    data = (U32 *)buf;
    data[0] = 0xDEADBEEFu;
    data[1] = 0xCAFEBABEu;
    OsMsgSend(g_msgTskReceiver, buf);
}

OS_SEC_KERNEL_TEXT void TestMsgReceiverBasic(void *p1, void *p2, void *p3, void *p4)
{
    void *buf;
    U32 *data;
    U32 ret;
    (void)p1; (void)p2; (void)p3; (void)p4;

    ret = OsMsgRecv(OS_MSG_WAIT_FOREVER, &buf);
    if (ret != OS_OK) {
        return;
    }
    data = (U32 *)buf;
    if (data[0] == 0xDEADBEEFu && data[1] == 0xCAFEBABEu) {
        g_msgBasicRecvOk = 1;
    }
    OsMsgFree(buf);
}

OS_SEC_KERNEL_TEXT void TestMsgBasicSetup(void)
{
    g_msgBasicRecvOk = 0;
    g_msgTskReceiver = TestMsgCreateTask("MsgRx", 8, TestMsgReceiverBasic);
    g_msgTskSender = TestMsgCreateTask("MsgTx", 9, TestMsgSenderBasic);
    OsTaskResume(g_msgTskReceiver);
    OsTaskResume(g_msgTskSender);
}

OS_SEC_KERNEL_TEXT void TestMsgBasicVerify(void)
{
    OS_TEST_ASSERT(g_msgBasicRecvOk == 1);
    TestMsgCleanupTask(g_msgTskSender);
    TestMsgCleanupTask(g_msgTskReceiver);
}

/* ====== multi-msg: 连续发3条 ====== */

OS_SEC_KERNEL_BSS volatile U32 g_msgMultiCnt;
OS_SEC_KERNEL_BSS volatile U32 g_msgMultiOk;

OS_SEC_KERNEL_TEXT void TestMsgSenderMulti(void *p1, void *p2, void *p3, void *p4)
{
    U32 i;
    (void)p1; (void)p2; (void)p3; (void)p4;
    for (i = 0; i < 3; i++) {
        void *buf = OsMsgAlloc(sizeof(U32));
        if (buf != NULL) {
            *(U32 *)buf = i + 100;
            OsMsgSend(g_msgTskReceiver, buf);
        }
    }
}

OS_SEC_KERNEL_TEXT void TestMsgReceiverMulti(void *p1, void *p2, void *p3, void *p4)
{
    void *buf;
    U32 ret;
    (void)p1; (void)p2; (void)p3; (void)p4;
    g_msgMultiCnt = 0;
    g_msgMultiOk = 0;
    while (g_msgMultiCnt < 3) {
        ret = OsMsgRecv(200, &buf);
        if (ret != OS_OK) break;
        if (*(U32 *)buf == g_msgMultiCnt + 100) {
            g_msgMultiOk++;
        }
        OsMsgFree(buf);
        g_msgMultiCnt++;
    }
}

OS_SEC_KERNEL_TEXT void TestMsgMultiSetup(void)
{
    g_msgMultiCnt = 0;
    g_msgMultiOk = 0;
    g_msgTskReceiver = TestMsgCreateTask("MulRx", 8, TestMsgReceiverMulti);
    g_msgTskSender = TestMsgCreateTask("MulTx", 9, TestMsgSenderMulti);
    OsTaskResume(g_msgTskReceiver);
    OsTaskResume(g_msgTskSender);
}

OS_SEC_KERNEL_TEXT void TestMsgMultiVerify(void)
{
    OS_TEST_ASSERT_EQ(g_msgMultiCnt, 3);
    OS_TEST_ASSERT_EQ(g_msgMultiOk, 3);
    TestMsgCleanupTask(g_msgTskSender);
    TestMsgCleanupTask(g_msgTskReceiver);
}

/* ====== block-recv: B先等，A后发 ====== */

OS_SEC_KERNEL_BSS volatile U32 g_msgBlockOk;

OS_SEC_KERNEL_TEXT void TestMsgReceiverBlock(void *p1, void *p2, void *p3, void *p4)
{
    void *buf;
    U32 ret;
    (void)p1; (void)p2; (void)p3; (void)p4;
    /* 先阻塞等待 */
    ret = OsMsgRecv(OS_MSG_WAIT_FOREVER, &buf);
    if (ret == OS_OK && *(U32 *)buf == 0x1234) {
        g_msgBlockOk = 1;
    }
    if (ret == OS_OK) OsMsgFree(buf);
}

OS_SEC_KERNEL_TEXT void TestMsgSenderBlock(void *p1, void *p2, void *p3, void *p4)
{
    void *buf;
    (void)p1; (void)p2; (void)p3; (void)p4;
    OsTaskDelay(20); /* 确保接收者先阻塞 */
    buf = OsMsgAlloc(sizeof(U32));
    if (buf != NULL) {
        *(U32 *)buf = 0x1234;
        OsMsgSend(g_msgTskReceiver, buf);
    }
}

OS_SEC_KERNEL_TEXT void TestMsgBlockSetup(void)
{
    g_msgBlockOk = 0;
    g_msgTskReceiver = TestMsgCreateTask("BlkRx", 8, TestMsgReceiverBlock);
    g_msgTskSender = TestMsgCreateTask("BlkTx", 9, TestMsgSenderBlock);
    OsTaskResume(g_msgTskReceiver);
    OsTaskResume(g_msgTskSender);
}

OS_SEC_KERNEL_TEXT void TestMsgBlockVerify(void)
{
    OS_TEST_ASSERT(g_msgBlockOk == 1);
    TestMsgCleanupTask(g_msgTskSender);
    TestMsgCleanupTask(g_msgTskReceiver);
}

/* ====== timeout: 超时返回 ====== */

OS_SEC_KERNEL_BSS volatile U32 g_msgTmoResult;

OS_SEC_KERNEL_TEXT void TestMsgReceiverTmo(void *p1, void *p2, void *p3, void *p4)
{
    void *buf;
    U32 ret;
    (void)p1; (void)p2; (void)p3; (void)p4;
    ret = OsMsgRecv(50, &buf); /* 50 ticks 超时 */
    g_msgTmoResult = ret;
}

OS_SEC_KERNEL_TEXT void TestMsgTmoSetup(void)
{
    g_msgTmoResult = 0;
    g_msgTskReceiver = TestMsgCreateTask("TmoRx", 8, TestMsgReceiverTmo);
    OsTaskResume(g_msgTskReceiver);
}

OS_SEC_KERNEL_TEXT void TestMsgTmoVerify(void)
{
    OS_TEST_ASSERT_EQ(g_msgTmoResult, OS_MSG_RECV_TIMEOUT);
    TestMsgCleanupTask(g_msgTskReceiver);
}

/* ====== no-wait: NO_WAIT 立即返回 ====== */

OS_SEC_KERNEL_BSS volatile U32 g_msgNoWaitResult;

OS_SEC_KERNEL_TEXT void TestMsgReceiverNoWait(void *p1, void *p2, void *p3, void *p4)
{
    void *buf;
    U32 ret;
    (void)p1; (void)p2; (void)p3; (void)p4;
    ret = OsMsgRecv(OS_MSG_NO_WAIT, &buf);
    g_msgNoWaitResult = ret;
}

OS_SEC_KERNEL_TEXT void TestMsgNoWaitSetup(void)
{
    g_msgNoWaitResult = 0;
    g_msgTskReceiver = TestMsgCreateTask("NwRx", 8, TestMsgReceiverNoWait);
    OsTaskResume(g_msgTskReceiver);
}

OS_SEC_KERNEL_TEXT void TestMsgNoWaitVerify(void)
{
    OS_TEST_ASSERT_EQ(g_msgNoWaitResult, OS_MSG_RECV_UNAVAILABLE);
    TestMsgCleanupTask(g_msgTskReceiver);
}

/* ====== alloc-free: 不 send 直接 free ====== */

OS_SEC_KERNEL_TEXT void TestMsgAllocFreeSetup(void)
{
    /* 纯同步测试 */
}

OS_SEC_KERNEL_TEXT void TestMsgAllocFreeVerify(void)
{
    void *buf = OsMsgAlloc(64);
    OS_TEST_ASSERT(buf != NULL);
    OsMsgFree(buf);
    /* 二次 alloc 验证堆没坏 */
    buf = OsMsgAlloc(128);
    OS_TEST_ASSERT(buf != NULL);
    OsMsgFree(buf);
}

/* ====== send-invalid-pid ====== */

OS_SEC_KERNEL_TEXT void TestMsgInvPidSetup(void)
{
    /* 纯同步测试 */
}

OS_SEC_KERNEL_TEXT void TestMsgInvPidVerify(void)
{
    void *buf = OsMsgAlloc(sizeof(U32));
    U32 ret;
    OS_TEST_ASSERT(buf != NULL);
    /* 发给不存在的 pid */
    ret = OsMsgSend(99, buf);
    OS_TEST_ASSERT_EQ(ret, OS_MSG_SEND_PID_INVALID);
    OsMsgFree(buf);
}
