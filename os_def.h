#ifndef OS_DEF_H
#define OS_DEF_H
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef unsigned long long U64;
typedef char S8;
typedef short S16;
typedef int S32;
typedef U8 bool;
typedef void *uintptr_t;
#define TRUE 1
#define FALSE 0
#define NULL ((void *)0)
#define OS_OK 0

#define OS_GET_BYTE_BY_IDX(num, byteIdx) (((num) >> (byteIdx)) & 0xFF)

/* 强制内联 */
#define OS_INLINE static __attribute__((always_inline))
#define OS_STRUCT_PACKED __attribute__((packed))
#define OS_EMBED_ASM(...) __asm__ volatile(__VA_ARGS__)

/* 段 */
#define OS_SEC_BOOT_TEXT __attribute__((section(".os.boot.text")))
#define OS_SEC_MBR_TEXT __attribute__((section(".os.mbr.text")))
#define OS_SEC_MBR_DATA __attribute__((section(".os.mbr.data")))
#define OS_SEC_ENTRY_TEXT __attribute__((section(".os.entry.text")))
#define OS_SEC_LOADER_TEXT __attribute__((section(".os.loader.text")))
#define OS_SEC_LOADER_DATA __attribute__((section(".os.loader.data")))
#define OS_SEC_LOADER_BSS __attribute__((section(".os.loader.bss")))
#define OS_SEC_GDT_DATA __attribute__((section(".os.gdt.data")))
#define OS_SEC_PGT_DATA __attribute__((section(".os.pgt.data")))
#define OS_SEC_INIT_TEXT __attribute__((section(".os.init.text")))
#define OS_SEC_KERNEL_TEXT   __attribute__((section(".os.kernel.text")))
#define OS_SEC_KERNEL_DATA   __attribute__((section(".os.kernel.data")))
#define OS_SEC_KERNEL_BSS    __attribute__((section(".os.kernel.bss")))

#define OS_BUILD_ERR_CODE(mid, errCode) (((mid) << 16) | ((0xFFFF) & (errCode)))
#endif