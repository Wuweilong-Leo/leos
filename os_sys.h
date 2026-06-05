#ifndef OS_SYS_H
#define OS_SYS_H
enum OsMid {
    OS_MID_BSS,
    OS_MID_SYS,
    OS_MID_HWI,
    OS_MID_TASK,
    OS_MID_MEM,
    OS_MID_SEM,
    OS_MID_USR,
    OS_MID_SCHED,
    OS_MID_TIMER,
    OS_MID_APP,
};

#define OS_HWI_ACTIVE_MSK 0x00000001U
#define OS_TICK_ACTIVE_MSK 0x00000002U

#define OS_HWI_ACTIVE(uniFlag) (((uniFlag) & OS_HWI_ACTIVE_MSK) != 0)
#define OS_TICK_ACTIVE(uniFlag) (((uniFlag) & OS_TICK_ACTIVE_MSK) != 0)

#define OS_SYS_ACTIVE_MSK (OS_HWI_ACTIVE_MSK | OS_TICK_ACTIVE_MSK)
#define OS_SYS_ACTIVE(uniFlag) (((uniFlag) & OS_SYS_ACTIVE_MSK) != 0)

#endif