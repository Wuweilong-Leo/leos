#ifndef OS_PRINT_INTERNAL_H
#define OS_PRINT_INTERNAL_H

#define OS_CRT_ADDR_REG ((volatile U16 *const)0x03D4)
#define OS_CRT_DATA_REG ((volatile U16 *const)0x03D5)
#define OS_CUR_POS_HIGH_INDEX 0x0E
#define OS_CUR_POS_LOW_INDEX 0x0F
/* 黑底白字 */
#define OS_BLK_BACK_WHT_WORD 0x07

#define OS_SCREEN_COL_NUM 80U
#define OS_SCREEN_ROW_NUM 25U
#define OS_SCREEN_POS_NUM (OS_SCREEN_COL_NUM * OS_SCREEN_ROW_NUM)
#define OS_VIDEO_BASE_ADDR ((volatile U8 * const)0xC00B8000)

#define OS_VIDEO_ROLL_DST_ADDR OS_VIDEO_BASE_ADDR
/* 每个pos要2个字节 */
#define OS_VIDEO_ROLL_SRC_ADDR ((volatile U8 * const)((U32)OS_VIDEO_BASE_ADDR + (OS_SCREEN_COL_NUM * 2)))
/* 上卷 pos数 * 2字节 */
#define OS_VIDEO_ROLL_SIZE ((OS_SCREEN_POS_NUM - OS_SCREEN_COL_NUM) * 2)

#define OS_VA_START(ap, v) ((ap) = (void *)&(v))
#define OS_VA_ARG(ap, t) (*(t*)((ap) += 4))
#define OS_VA_END(ap) ((ap) = NULL)
#endif