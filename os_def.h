#ifndef OS_DEF_H
#define OS_DEF_H
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef char int8_t;
typedef short int16_y;
typedef int int32_t;
typedef int8_t bool;
typedef void *uintptr_t;
#define TRUE 1
#define FALSE 0
#define NULL ((void *)0)
#define OS_OK 0

#define OS_GET_BYTE_BY_IDX(num, byteIdx) (((num) >> (byteIdx)) & 0xFF)

/* 强制内联 */
#define INLINE static __attribute__((always_inline))

#define OS_EMBED_ASM(...) __asm volatile(__VA_ARGS__)

/* 段 */
#define OS_SEC_BOOT_TEXT __attribute__((section(".os.boot.text")))
#define OS_SEC_MBR_TEXT __attribute__((section(".os.mbr.text")))
#define OS_SEC_MBR_DATA __attribut__((section(".os.mbr.data")))
#define OS_SEC_GDT_DATA __attribute__((section(".os.gdt.data")))
#define OS_SEC_INIT_TEXT __attribute__((section(".os.init.text")))
#define OS_SEC_L1_TEXT   __attribute__((section(".os.l1.text")))
#define OS_SEC_L1_DATA   __attribute__((section(".os.l1.data")))
#define OS_SEC_L1_BSS    __attribute__((section(".os.l1.bss")))
#define OS_SEC_L2_TEXT   __attribute__((section(".os.l2.text")))
#define OS_SEC_L2_DATA   __attribute__((section(".os.l2.data")))
#define OS_SEC_L2_BSS    __attribute__((section(".os.l2.bss")))

#define OS_BUILD_ERR_CODE(mid, err_code) (((mid) << 16) | ((0xFFFF) & (err_code)))
#endif