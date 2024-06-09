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

OsLoadTsk:
    movl 4(%esp), %eax
    mov 0(%eax), %esp

    popl %eax
    cmp $OS_ALL_SAVE_FLAG, %eax
    je OsAllLoad

OsFastLoad:
    popl %ebp
    popl %ebx
    popl %edi
    popl %esi
    ret

OsAllLoad: