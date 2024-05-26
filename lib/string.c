#include "os_def.h"

OS_SEC_L2_TEXT void memset(void *const dst, uint8_t value, uint32_t size) {
  uint8_t *dst_tmp = (uint8_t *)dst;
  for (uint32_t i = 0; i < size; i++) {
    dst_tmp[i] = value;
  }
}

OS_SEC_L2_TEXT void memcpy(void *dst, const void *src, uint32_t size) {
  uint8_t *dst_tmp = (uint8_t *)dst;
  uint8_t *src_tmp = (uint8_t *)src;
  for (uint32_t i = 0; i < size; i++) {
    dst_tmp[i] = src_tmp[i];
  }
}

OS_SEC_L2_TEXT int32_t memcmp(void *s1, void *s2, uint32_t size) {
  const char *s1_tmp = (const char *)s1;
  const char *s2_tmp = (const char *)s2;
  for (uint32_t i = 0; i < size; i++) {
    if (s1_tmp[i] != s2_tmp[i]) {
      return s1_tmp[i] > s2_tmp[i] ? 1 : -1;
    }
  }
  return 0;
}

OS_SEC_L2_TEXT char *strcpy(char *dst, const char *src) {
  while (*src != 0) {
    *dst = *src;
    dst++;
    src++;
  }
}

OS_SEC_L2_TEXT uint32_t strlen(const char *str) {
  const char *p = str;
  while (*p++) {}
  return p - str - 1;
}

OS_SEC_L2_TEXT int32_t strcmp(const char *s1, const char *s2) {
  while (*s1 != 0 && *s2 == *s1) {
    s1++;
    s2++;
  }
  return (*s1 < *s2) ? -1 : (*s1 > *s2);
}

OS_SEC_L2_TEXT char *strchr(const char *str, const uint8_t ch) {
  while (*str != 0) {
    if (*str == ch) {
      return str;
    }
    str++;
  }
  return NULL;
}

OS_SEC_L2_TEXT char *strrchr(const char *str, const uint8_t ch) {
  const char *last_char = NULL;
  while (*str != 0) {
    if (*str == ch) {
      last_char = str;
    }
    str++;
  }
  return last_char;
}

OS_SEC_L2_TEXT char *strcat(char *dst, const char *src) {
  char *str = dst;
  while (*str++) {}
  --str;
  while (*str++ = *src++) {}
  return dst;
}

OS_SEC_L2_TEXT uint32_t strchrs(const char *str, uint8_t ch) {
  uint32_t ch_cnt = 0;
  const char *p = str;
  while (*p != 0) {
    if (*p == ch) {
      ch_cnt++;
    }
    p++;
  }
  return ch_cnt;
}
