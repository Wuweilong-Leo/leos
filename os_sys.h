#ifndef OS_SYS_H
#define OS_SYS_H
enum OsMid {
    OS_MID_TASK = 0,
    OS_MID_MEM,
};

#define OS_HWI_ACTIVE_MSK 0x00000001U

#define OS_HWI_ACTIVE(uniFlag) (((uniFlag) & OS_HWI_ACTIVE_MSK) != 0)
#define OS_HWI_NOT_ACTIVE(uniFlag) (((uniFlag) & OS_HWI_ACTIVE_MSK) == 0)
#endif