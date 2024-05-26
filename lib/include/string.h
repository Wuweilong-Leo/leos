#ifndef STRING_H
#define STRING_H
#include "os_def.h"
extern void memset(void *const dst, uint8_t value, uint32_t size);
extern void memcpy(void *dst, const void *src, uint32_t size);
extern uint32_t memcmp(void *s1, void *s2, uint32_t size);
extern char *strcpy(char *dst, const char *src);
extern uint32_t strlen(const char *str);
extern uint32_t strcmp(const char *s1, const char *s2);
extern char *strchr(const char *str, const uint8_t ch);
extern char *strrchr(const char *str, const uint8_t ch);
extern char *strcat(char *dst, const char *src);
extern uint32_t strchrs(const char *str, uint8_t ch);
#endif