#ifndef STRING_H
#define STRING_H
#include "os_def.h"
extern void memset(void *const dst, U8 value, U32 size);
extern void memcpy(void *dst, const void *src, U32 size);
extern U32 memcmp(void *s1, void *s2, U32 size);
extern char *strcpy(char *dst, const char *src);
extern U32 strlen(const char *str);
extern U32 strcmp(const char *s1, const char *s2);
extern char *strchr(const char *str, const U8 ch);
extern char *strrchr(const char *str, const U8 ch);
extern char *strcat(char *dst, const char *src);
extern U32 strchrs(const char *str, U8 ch);
#endif