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

    /* 进入调度器 */
    call OsSchedMain

.code32
.section .os.kernel.text, "ax"
.global OsLoadTsk
.global OsSwitch2Process

OsLoadTsk:
    movl 4(%esp), %eax
    movl 0(%eax), %esp

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

.macro OS_SAVE_CONTEXT
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


.macro OS_HWI_VECTOR hwiNum, balanceErrCode
.code32
.section .os.kernel.text, "ax"
.global OsHwiVector\hwiNum

OsHwiVector\hwiNum:
    \balanceErrCode
    OS_SAVE_CONTEXT

    /* runQue第一个变量就是runningTsk */
    /* runningTsk 第一个变量就是stkPtr */
    movl $g_runQue, %eax
    movl 0(%eax), %ebx // runningTsk
    movl %esp, 0(%ebx)
    
    /* 切到系统栈 */
   // movl $_os_kernel_sp_end, %eax
   // movl %eax, %esp

    /* 清中断 */
    movb $0x20, %al
    outb %al, $0xa0
    outb %al, $0x20

    movl 0(%ebx), %eax
    pushl %eax
    pushl $\hwiNum 
    call OsHwiDispatcher
    addl $8, %esp

    call OsHwiTail
.endm

.macro ZERO
pushl $0
.endm

.macro ERROR_CODE
nop
.endm

OS_HWI_VECTOR 0x00, ZERO
OS_HWI_VECTOR 0x01, ZERO
OS_HWI_VECTOR 0x02, ZERO
OS_HWI_VECTOR 0x03, ZERO
OS_HWI_VECTOR 0x04, ZERO
OS_HWI_VECTOR 0x05, ZERO
OS_HWI_VECTOR 0x06, ZERO
OS_HWI_VECTOR 0x07, ZERO
OS_HWI_VECTOR 0x08, ERROR_CODE
OS_HWI_VECTOR 0x09, ZERO
OS_HWI_VECTOR 0x0a, ERROR_CODE
OS_HWI_VECTOR 0x0b, ERROR_CODE
OS_HWI_VECTOR 0x0c, ERROR_CODE
OS_HWI_VECTOR 0x0d, ERROR_CODE
OS_HWI_VECTOR 0x0e, ERROR_CODE
OS_HWI_VECTOR 0x0f, ZERO
OS_HWI_VECTOR 0x10, ZERO
OS_HWI_VECTOR 0x11, ERROR_CODE
OS_HWI_VECTOR 0x12, ZERO
OS_HWI_VECTOR 0x13, ZERO 
OS_HWI_VECTOR 0x14, ZERO
OS_HWI_VECTOR 0x15, ZERO
OS_HWI_VECTOR 0x16, ZERO
OS_HWI_VECTOR 0x17, ZERO
OS_HWI_VECTOR 0x18, ZERO
OS_HWI_VECTOR 0x19, ZERO
OS_HWI_VECTOR 0x1a, ZERO
OS_HWI_VECTOR 0x1b, ZERO
OS_HWI_VECTOR 0x1c, ZERO
OS_HWI_VECTOR 0x1d, ZERO
OS_HWI_VECTOR 0x1e, ERROR_CODE
OS_HWI_VECTOR 0x1f, ZERO
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
