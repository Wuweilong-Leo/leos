#ifndef STRING_H
#define STRING_H
#include "os_def.h"
extern void memset(void *const dst, U8 value, size_t size);
extern void memcpy(void *dst, const void *src, size_t size);
extern S32 memcmp(void *s1, void *s2, size_t size);
extern char *strcpy(char *dst, const char *src);
extern size_t strlen(const char *str);
extern S32 strcmp(const char *s1, const char *s2);
extern char *strchr(const char *str, const U8 ch);
extern char *strrchr(const char *str, const U8 ch);
extern char *strcat(char *dst, const char *src);
extern size_t strchrs(const char *str, U8 ch);
#endif