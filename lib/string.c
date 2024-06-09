#include "os_def.h"

OS_SEC_KERNEL_TEXT void memset(void *const dst, U8 value, U32 size) {
  U32 i;
  U8 *dstTmp = (U8 *)dst;

  for (i = 0; i < size; i++) {
    dstTmp[i] = value;
  }
}

OS_SEC_KERNEL_TEXT void memcpy(void *dst, const void *src, U32 size) {
  U8 *dstTmp = (U8 *)dst;
  U8 *srcTmp = (U8 *)src;
  for (U32 i = 0; i < size; i++) {
    dstTmp[i] = srcTmp[i];
  }
}

OS_SEC_KERNEL_TEXT S32 memcmp(void *s1, void *s2, U32 size) {
  const char *s1Tmp = (const char *)s1;
  const char *s2Tmp = (const char *)s2;
  for (U32 i = 0; i < size; i++) {
    if (s1Tmp[i] != s2Tmp[i]) {
      return s1Tmp[i] > s2Tmp[i] ? 1 : -1;
    }
  }
  return 0;
}

OS_SEC_KERNEL_TEXT char *strcpy(char *dst, const char *src) {
  while (*src != 0) {
    *dst = *src;
    dst++;
    src++;
  }
}

OS_SEC_KERNEL_TEXT U32 strlen(const char *str) {
  const char *p = str;
  while (*p++) {}
  return p - str - 1;
}

OS_SEC_KERNEL_TEXT S32 strcmp(const char *s1, const char *s2) {
  while (*s1 != 0 && *s2 == *s1) {
    s1++;
    s2++;
  }
  return (*s1 < *s2) ? -1 : (*s1 > *s2);
}

OS_SEC_KERNEL_TEXT char *strchr(const char *str, const U8 ch) {
  while (*str != 0) {
    if (*str == ch) {
      return str;
    }
    str++;
  }
  return NULL;
}

OS_SEC_KERNEL_TEXT char *strrchr(const char *str, const U8 ch) {
  const char *lastChar = NULL;
  while (*str != 0) {
    if (*str == ch) {
      lastChar = str;
    }
    str++;
  }
  return lastChar;
}

OS_SEC_KERNEL_TEXT char *strcat(char *dst, const char *src) {
  char *str = dst;
  while (*str++) {}
  --str;
  while (*str++ = *src++) {}
  return dst;
}

OS_SEC_KERNEL_TEXT U32 strchrs(const char *str, U8 ch) {
  U32 chCnt = 0;
  const char *p = str;
  while (*p != 0) {
    if (*p == ch) {
      chCnt++;
    }
    p++;
  }
  return chCnt;
}
