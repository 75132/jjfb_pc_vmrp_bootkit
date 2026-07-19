#ifndef _FONT16_T_H_

#define _FONT16_T_H_

//字间距
#define FONT16_WORD_SPACE (0)

#define FONT16_LINE_SPACE (1)

//小字体宽, ASCII码的字符被定义为小字体
#define FONT16_SMALL_WIDTH (8)
//大字体宽, 非ASCII码的字符被定义为大字体
#define FONT16_LARGE_WIDTH (16)
//字体高
#define FONT16_HEIGHT (16)

//是否支持超出可视宽时自动换行
#define FONT16_SUPPORT_AUTO_NEWLINE 2

/*
 * 从左往右画字符串,只支持Unicode编码
 输入：
szText		必须是Unicode编码的字符串
x			绘制x
y			绘制y
font_rect	mr_screenRectSt结构体，注意font_rect.x+font_rect.w<屏幕宽，font_rect.y+font_rect.h<屏幕高；否则可能绘制出错
color		mr_colourSt结构体，绘制颜色
flag		=FONT16_SUPPORT_AUTO_NEWLINE 则绘制在font_rect内自动换行，=0则不换行

返回：
   a)  0 成功
 * b) -1 失败
 */
extern int32 font16_t_drawTextLeft(uint8* szText,int32 x,int32 y,mr_screenRectSt font_rect,mr_colourSt color,int32 flag);
/*
 * 获取字符串的宽，只支持当行 宽
 *
 * 输入:
 * szText:           必须是Unicode编码的字符串
 * off:              有效字符串偏移,必须>=0,以一个Unicode占用的字节为单位
 * len:              有效字符串长度,若长度<0,则自动计算长度(以两个'\0'作标志),以一个Unicode占用的字节为单位
 *
 * 返回:
 * a)  0 成功
 * b) -1 失败
 */
extern int32 font16_t_textWidth(char *szText, int off, int len, int32 *width);

/*
 * 加载字库
 * lookfor: 是否只是检验是否加载了字体，=0 则载入字库
 * 返回: MR_SUCCESS成功, MR_FAILED失败
 */
extern int32 font16_t_load(int lookfor);

/*
 * 卸载字库,释放后字库将停止工作,如果要重新工作,请使用 mrc_font16_load 再次加载
 * 返回: MR_SUCCESS成功, MR_FAILED失败
 */
extern int32 font16_t_unload(void);

#endif
