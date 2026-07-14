#ifndef OS_VGA_INTERNAL_H
#define OS_VGA_INTERNAL_H

/* CRT 控制寄存器端口 */
#define OS_VGA_CRT_ADDR_REG       0x03D4
#define OS_VGA_CRT_DATA_REG       0x03D5
#define OS_VGA_CUR_POS_HIGH_INDEX  0x0E
#define OS_VGA_CUR_POS_LOW_INDEX   0x0F

/* VGA 文本模式缓冲区 */
#define OS_VGA_BUF_ADDR    0xC00B8000
#define OS_VGA_COL_NUM     80U
#define OS_VGA_ROW_NUM     25U
#define OS_VGA_POS_NUM     (OS_VGA_COL_NUM * OS_VGA_ROW_NUM)

/* 黑底白字属性 */
#define OS_VGA_ATTR_DEFAULT 0x07

#endif /* OS_VGA_INTERNAL_H */