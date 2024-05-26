#ifndef OS_IO_I386_H
#define OS_IO_I386_H
#include "os_def.h"

INLINE void os_outb(uint32_t port, uint8_t data) {
  OS_EMBED_ASM("outb %b0, %w1" ::"a"(data), "Nd"(port));
}

INLINE void os_outsw(uint32_t port, const void *addr, uint32_t wordCnt) {
  OS_EMBED_ASM("cld; rep outsw" : "+S"(addr), "+c"(wordCnt) : "d"(port));
}

INLINE uint8_t os_inb(uint32_t port) {
  uint8_t data;
  OS_EMBED_ASM("inb %w1, %b0" : "=a"(data) : "Nd"(port));
  return data;
}

INLINE uint32_t os_inw(uint32_t port) {
  uint32_t data;
  OS_EMBED_ASM("inb %w1, %w0" : "=a"(data) : "Nd"(port));
  return data;
}

INLINE void os_insw(uint32_t port, void *addr, uint32_t wordCnt) {
  OS_EMBED_ASM("cld; rep insw"
               : "+D"(addr), "+c"(wordCnt)
               : "d"(port)
               : "memory");
}
#endif