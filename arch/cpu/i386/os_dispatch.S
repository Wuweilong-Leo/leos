#include "os_cpu_i386.h"
.code32
.section .os.kernel.text, "ax"
.global OsTrapTsk

OsTrapTsk:
    pushl %esi
    pushl %edi
    pushl %ebx
    pushl %ebp
    /* 快速保存上下文 */
    pushl $OS_FAST_SAVE_FLAG

    /* 把栈指针存入tcb */
    movl 24(%esp), %eax
    movl %esp, 0(%eax)

    // 切到系统栈
    movl $g_kernelStackHigh, %eax
    movl 0(%eax), %esp

    /* 进入调度器 */
    call OsSchedMain

.code32
.section .os.kernel.text, "ax"
.global OsLoadTsk
.global OsSwitch2Process

OsLoadTsk:
    movl 4(%esp), %eax
    movl 0(%eax), %esp

// 这里不止切到进程会跑，只是这里切进程恰好要从这里走
OsSwitch2Process:
    popl %eax
    cmp $OS_ALL_SAVE_FLAG, %eax
    je OsAllLoad

OsFastLoad:
    popl %ebp
    popl %ebx
    popl %edi
    popl %esi
    ret

.macro OS_HWI_SAVE_CONTEXT
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    pushl %eax
    pushl %ecx
    pushl %edx
    pushl %ebx
    pushl %esp
    pushl %ebp
    pushl %esi
    pushl %edi
    pushl $OS_ALL_SAVE_FLAG
.endm

.macro OS_EXC_SAVE_CONTEXT
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    pushl %eax
    pushl %ecx
    pushl %edx
    pushl %ebx
    pushl %esp
    pushl %ebp
    pushl %esi
    pushl %edi
    movl %cr2, %eax // 保存错误地址
    pushl %eax
.endm

.macro OS_HWI_VECTOR hwiNum, balanceErrCode
.code32
.section .os.kernel.text, "ax"
.global OsHwiVector\hwiNum

OsHwiVector\hwiNum:
    \balanceErrCode
    OS_HWI_SAVE_CONTEXT // 全量保存寄存器

    movl $\hwiNum, %ebx // ebx先存入中断号

    // 先判断是否在系统栈
    movl $g_kernelStackHigh, %eax
    movl 0(%eax), %eax
    cmp %eax, %esp 
    jae OsHwiSwitchKernelStack // 不在系统栈, 去切栈
    movl $g_kernelStackLow, %eax
    movl 0(%eax), %eax
    cmp %eax, %esp 
    jb OsHwiSwitchKernelStack // 不在系统栈, 去切栈

    jmp OsHwiCommonHandler
.endm

OsHwiCommonHandler:
    /* 清中断 */
    movb $0x20, %al
    outb %al, $0xa0
    outb %al, $0x20

    pushl %ebx // 压入中断号
    call OsHwiDispatcher
    addl $4, %esp

    call OsHwiTail
    jmp OsHwiRet

OsHwiSwitchKernelStack:
    /* runQue第一个变量就是runningTsk */
    /* runningTsk 第一个变量就是stkPtr */
    movl $g_runQue, %eax
    movl 0(%eax), %eax // runningTsk
    movl %esp, 0(%eax)
    
    // 切到系统栈
    movl $g_kernelStackHigh, %eax
    movl 0(%eax), %esp
    jmp OsHwiCommonHandler

.macro OS_EXC_VECTOR excNum, balanceErrCode
.code32
.section .os.kernel.text, "ax"
.global OsExcVector\excNum

OsExcVector\excNum:
    \balanceErrCode // 压入错误码
    OS_EXC_SAVE_CONTEXT

    /* 清中断 */
    movb $0x20, %al
    outb %al, $0xa0
    outb %al, $0x20

    movl %esp, %eax
    pushl %eax // 把栈指针压入作为参数
    pushl $\excNum 
    call OsExcDispatcher
    addl $8, %esp

    jmp OsExcRet
.endm

.macro ZERO
pushl $0
.endm

.macro ERROR_CODE
nop
.endm

OS_EXC_VECTOR 0x00, ZERO
OS_EXC_VECTOR 0x01, ZERO
OS_EXC_VECTOR 0x02, ZERO
OS_EXC_VECTOR 0x03, ZERO
OS_EXC_VECTOR 0x04, ZERO
OS_EXC_VECTOR 0x05, ZERO
OS_EXC_VECTOR 0x06, ZERO
OS_EXC_VECTOR 0x07, ZERO
OS_EXC_VECTOR 0x08, ERROR_CODE
OS_EXC_VECTOR 0x09, ZERO
OS_EXC_VECTOR 0x0a, ERROR_CODE
OS_EXC_VECTOR 0x0b, ERROR_CODE
OS_EXC_VECTOR 0x0c, ERROR_CODE
OS_EXC_VECTOR 0x0d, ERROR_CODE
OS_EXC_VECTOR 0x0e, ERROR_CODE
OS_EXC_VECTOR 0x0f, ZERO
OS_EXC_VECTOR 0x10, ZERO
OS_EXC_VECTOR 0x11, ERROR_CODE
OS_EXC_VECTOR 0x12, ZERO
OS_EXC_VECTOR 0x13, ZERO 
OS_EXC_VECTOR 0x14, ZERO
OS_EXC_VECTOR 0x15, ZERO
OS_EXC_VECTOR 0x16, ZERO
OS_EXC_VECTOR 0x17, ZERO
OS_EXC_VECTOR 0x18, ZERO
OS_EXC_VECTOR 0x19, ZERO
OS_EXC_VECTOR 0x1a, ZERO
OS_EXC_VECTOR 0x1b, ZERO
OS_EXC_VECTOR 0x1c, ZERO
OS_EXC_VECTOR 0x1d, ZERO
OS_EXC_VECTOR 0x1e, ERROR_CODE
OS_EXC_VECTOR 0x1f, ZERO

OS_HWI_VECTOR 0x20, ZERO
OS_HWI_VECTOR 0x21, ZERO
OS_HWI_VECTOR 0x22, ZERO
OS_HWI_VECTOR 0x23, ZERO
OS_HWI_VECTOR 0x24, ZERO
OS_HWI_VECTOR 0x25, ZERO
OS_HWI_VECTOR 0x26, ZERO
OS_HWI_VECTOR 0x27, ZERO
OS_HWI_VECTOR 0x28, ZERO
OS_HWI_VECTOR 0x29, ZERO
OS_HWI_VECTOR 0x2a, ZERO
OS_HWI_VECTOR 0x2b, ZERO
OS_HWI_VECTOR 0x2c, ZERO
OS_HWI_VECTOR 0x2d, ZERO
OS_HWI_VECTOR 0x2e, ZERO
OS_HWI_VECTOR 0x2f, ZERO
OS_HWI_VECTOR 0x30, ZERO

/* ====== Syscall 入口 (INT 0x80, DPL=3) ====== */
.code32
.section .os.kernel.text, "ax"
.global OsSyscallVector80

OsSyscallVector80:
    pushl $0                    /* 假 errCode，平衡栈帧 */
    OS_HWI_SAVE_CONTEXT         /* 全量保存寄存器到进程内核栈 */

    /* 保存上下文指针到 runningTsk->stkPtr */
    movl $g_runQue, %eax
    movl 0(%eax), %eax          /* runningTsk */
    movl %esp, 0(%eax)          /* stkPtr = All Save 上下文 */

    /*
     * 直接在进程内核栈上跑 handler，不切系统栈。
     * 这样 OsTrapTsk 的 Fast Save 在 All Save 之上（同一栈），
     * 恢复后自然回到 All Save，与 Linux 一致。
     */

    /* 从 stkPtr 取上下文指针，逐字段压栈传参 */
    movl 0(%eax), %eax          /* stkPtr = OsAllSaveContext* */

    pushl 0x08(%eax)            /* ctx->esi = arg4 */
    pushl 0x18(%eax)            /* ctx->edx = arg3 */
    pushl 0x1C(%eax)            /* ctx->ecx = arg2 */
    pushl 0x14(%eax)            /* ctx->ebx = arg1 */
    pushl 0x20(%eax)            /* ctx->eax = sysno */
    call OsSyscallHandler
    addl $20, %esp

    /*
     * handler 可能阻塞（如 OsSemPend），OsTrapTsk 的 Fast Save
     * 会覆盖 stkPtr。但恢复后 Fast Save 已被 pop，esp 自然
     * 回到 All Save 上下文的位置。直接用 esp 即可。
     */
    movl %eax, 0x20(%esp)       /* ctx->eax = 返回值（esp 已指向 All Save） */
    jmp OsHwiRet

OsExcRet:
    addl $4, %esp // 越过cr2
    popl %edi
    popl %esi
    popl %ebp
    addl $4, %esp
    popl %ebx
    popl %edx
    popl %ecx
    popl %eax
    popl %gs
    popl %fs
    popl %es
    popl %ds
    addl $4, %esp /* 跳过errorCode */

    iret // 中断返回

OsHwiRet:
    addl $4, %esp // 越过保存标志
    popl %edi
    popl %esi
    popl %ebp
    addl $4, %esp
    popl %ebx
    popl %edx
    popl %ecx
    popl %eax
    popl %gs
    popl %fs
    popl %es
    popl %ds
    addl $4, %esp /* 跳过errorCode */

    iret // 异常返回

OsAllLoad:
    popl %edi
    popl %esi
    popl %ebp
    addl $4, %esp
    popl %ebx
    popl %edx
    popl %ecx
    popl %eax
    popl %gs
    popl %fs
    popl %es
    popl %ds
    addl $4, %esp /* 跳过errorCode */

    iret
