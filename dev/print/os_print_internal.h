#ifndef OS_PRINT_INTERNAL_H
#define OS_PRINT_INTERNAL_H

#define OS_CRT_ADDR_REG ((volatile U16 *const)0x03D4)
#define OS_CRT_DATA_REG ((volatile U16 *const)0x03D5)
#define OS_CUR_POS_HIGH_INDEX 0x0E
#define OS_CUR_POS_LOW_INDEX 0x0F
/* 黑底白字 */
#define OS_BLK_BACK_WHT_WORD 0x07

#define OS_SCREEN_COL_MAX 80
#define OS_SCREEN_ROW_MAX 25
#define OS_SCREEN_MAX (OS_SCREEN_ROW_MAX * OS_SCREEN_COL_MAX)
#define OS_VIDEO_BASE_ADDR ((volatile U8 * const)0xC00B8000)
#define OS_ROLL_VIDEO_SRC_ADDR ((volatile U8 * const)0xC00B80A0)
#define OS_ROLL_VIDEO_DST_ADDR OS_VIDEO_BASE_ADDR

#define OS_VA_START(ap, v) ((ap) = (void *)&(v))
#define OS_VA_ARG(ap, t) (*(t*)((ap) += 4))
#define OS_VA_END(ap) ((ap) = NULL)
#endif