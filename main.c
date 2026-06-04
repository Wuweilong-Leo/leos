#include "os_def.h"
#include "os_print_external.h"
#include "os_timer.h"
#include "os_hwi.h"
#include "os_debug_external.h"
#include "os_mem_external.h"
#include "os_task_external.h"
#include "os_sched_external.h"
#include "os_gdt.h"

extern U32 OsConfigInit(void);

/* 直接写 VGA 文本缓冲区 (0xB8000) */
static OS_SEC_KERNEL_TEXT void VgaPutChar(int row, int col, char c, char attr)
{
    volatile char *vga = (volatile char *)0xB8000;
    vga[(row * 80 + col) * 2] = c;
    vga[(row * 80 + col) * 2 + 1] = attr;
}

/* 线程A: 在第2行显示 A 和计数 */
OS_SEC_KERNEL_TEXT void TestTaskA(void *para1, void *param2, void *param3, void *param4)
{
    U32 count = 0;
    VgaPutChar(2, 0, 'A', 0x0E);
    while (1) {
        /* 显示计数（简单16进制） */
        VgaPutChar(2, 2, "0123456789ABCDEF"[(count >> 4) & 0xF], 0x0E);
        VgaPutChar(2, 3, "0123456789ABCDEF"[count & 0xF], 0x0E);
        count++;
        OsTaskDelay(10);
    }
}

/* 线程B: 在第3行显示 B 和计数 */
OS_SEC_KERNEL_TEXT void TestTaskB(void *para1, void *param2, void *param3, void *param4)
{
    U32 count = 0;
    VgaPutChar(3, 0, 'B', 0x0B);
    while (1) {
        VgaPutChar(3, 2, "0123456789ABCDEF"[(count >> 4) & 0xF], 0x0B);
        VgaPutChar(3, 3, "0123456789ABCDEF"[count & 0xF], 0x0B);
        count++;
        OsTaskDelay(20);
    }
}

/* 线程C: 在第4行显示 C 和计数 */
OS_SEC_KERNEL_TEXT void TestTaskC(void *para1, void *param2, void *param3, void *param4)
{
    U32 count = 0;
    VgaPutChar(4, 0, 'C', 0x0A);
    while (1) {
        VgaPutChar(4, 2, "0123456789ABCDEF"[(count >> 4) & 0xF], 0x0A);
        VgaPutChar(4, 3, "0123456789ABCDEF"[count & 0xF], 0x0A);
        count++;
        OsTaskDelay(30);
    }
}

OS_SEC_KERNEL_TEXT S32 main(void)
{
    U32 tskIdA, tskIdB, tskIdC;
    struct OsTaskCreateParam param;
    U32 ret;
    struct OsTaskCb *tskCb;

    (void)OsIntLock();
    
    volatile char *vga = (volatile char *)0xB8000;
    const char *msg = "LEOS TASK TEST";
    int i;
    for (i = 0; msg[i]; i++) {
        vga[i*2] = msg[i];
        vga[i*2+1] = 0x0F;
    }
    
    OsPrintStr("hello kernel\n");
    OsConfigInit();

    /* 创建三个线程 */
    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TaskA");
    param.prio = 5;
    param.entryFunc = TestTaskA;
    OsTaskCreate(&param, &tskIdA);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TaskB");
    param.prio = 5;
    param.entryFunc = TestTaskB;
    OsTaskCreate(&param, &tskIdB);

    memset(&param, 0, sizeof(param));
    strcpy(param.name, "TaskC");
    param.prio = 5;
    param.entryFunc = TestTaskC;
    OsTaskCreate(&param, &tskIdC);

    /* 直接入就绪队列 */
    tskCb = OS_TASK_GET_CB(tskIdA);
    OsSchedRdyListEnqueTsk(tskCb);
    tskCb = OS_TASK_GET_CB(tskIdB);
    OsSchedRdyListEnqueTsk(tskCb);
    tskCb = OS_TASK_GET_CB(tskIdC);
    OsSchedRdyListEnqueTsk(tskCb);

    OsSchedSwitchFirstTsk();
    
    while (1) {}
    return 0;
}