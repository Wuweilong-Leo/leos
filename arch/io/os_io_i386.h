#ifndef OS_IO_I386_H
#define OS_IO_I386_H
#include "os_def.h"

OS_INLINE void OsOutb(U32 port, U8 data) {
  OS_EMBED_ASM("outb %b0, %w1" ::"a"(data), "Nd"(port));
}

OS_INLINE void OsOutw(U32 port, U16 data) {
  OS_EMBED_ASM("outw %w0, %w1" ::"a"(data), "Nd"(port));
}

OS_INLINE void OsOutsw(U32 port, const void *addr, U32 wordCnt) {
  OS_EMBED_ASM("cld; rep outsw" : "+S"(addr), "+c"(wordCnt) : "d"(port));
}

OS_INLINE U8 OsInb(U32 port) {
  U8 data;
  OS_EMBED_ASM("inb %w1, %b0" : "=a"(data) : "Nd"(port));
  return data;
}

OS_INLINE U16 OsInw(U32 port) {
  U16 data;
  OS_EMBED_ASM("inw %w1, %w0" : "=a"(data) : "Nd"(port));
  return data;
}

OS_INLINE void OsInsw(U32 port, void *addr, U32 wordCnt) {
  OS_EMBED_ASM("cld; rep insw"
               : "+D"(addr), "+c"(wordCnt)
               : "d"(port)
               : "memory");
}
#endif