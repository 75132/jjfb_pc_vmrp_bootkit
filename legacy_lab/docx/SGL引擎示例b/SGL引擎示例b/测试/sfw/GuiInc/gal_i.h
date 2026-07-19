// SGL绘图模块 [4/30/2012 JianbinZhu]
// 封装各种常用绘图函数

#ifndef _SGL_GAL_H
#define _SGL_GAL_H


#include "types_i.h"
#include "mrc_base.h"
#include "font.h"
#include "ts_graphics.h"
#include "screenbuf.h"


#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */


typedef struct _screendevice {
	mr_screeninfo  scrInfo;
}*GAL_GC;


extern struct _screendevice scrdev;


/**
 * \brief Normal colors.
 */
#define COLOR_transparent	0x00000000
#define COLOR_darkred		0x00800000
#define COLOR_darkgreen		0x00008000
#define COLOR_darkyellow	0x00808000
#define COLOR_darkblue		0x00000080
#define COLOR_darkmagenta	0x00800080
#define COLOR_darkcyan		0x00008080
#define COLOR_lightgray		0x00c0c0c0
#define COLOR_darkgray		0x00808080
#define COLOR_red			0x00ff0000
#define COLOR_green			0x0000ff00
#define COLOR_yellow		0x00ffff00
#define COLOR_blue			0x000000f0
#define COLOR_magenta		0x008080ff
#define COLOR_cyan			0x0000ffff
#define COLOR_lightwhite	0x00ffffff
#define COLOR_black			0x00000000


#define SCREEN_WIDTH	(int)(scrdev.scrInfo.width)
#define SCREEN_HEIGHT	(int)(scrdev.scrInfo.height)


/**
 * \brief The phisical graphic context handle.
 */
#define PHYSICALGC	(&scrdev)

/**
 * \brief Intialize the graphic abstract layer.
 */
void GAL_Initialize (VOID);

/**
 * \brief Terminate the graphic abstract layer.
 */
void GAL_Terminate (VOID);

/**
 * \brief Get the bytes per pixel.
 */
#define GAL_BytesPerPixel(gc) \
	DIV((gc)->scrInfo.bit + 7 , 8)

/**
 * \brief Get the bits per pixel.
 */
#define GAL_BitsPerPixel(gc) \
	((gc)->scrInfo.bit)

/**
 * \brief Get the screen width.
 * \获取屏幕宽
 */
#define GAL_Width(gc) \
	((gc)->scrInfo.width)

/**
 * \brief Get the screen height.
 * \获取屏幕高
 */
#define GAL_Height(gc) \
	((gc)->scrInfo.height)

/**
 * \brief Get supported colors.
 * \得到支持的颜色
 */
#define GAL_Colors(gc) \
	(((gc)->scrInfo.bit >= 24)? (1 << 24): (1 << (gc)->scrInfo.bit))

/**
 * \brief Fill the box with a specific color.
 * \用指定颜色填充矩形
 */
#define GAL_FillBox(gc, x, y, w, h, pixel) \
	mrc_drawRect((int16)(x), (int16)(y), (int16)(w), (int16)(h), PIXEL888RED((pixel)), PIXEL888GREEN((pixel)), PIXEL888BLUE((pixel)))
//tsg_drawRect((int16)(x), (int16)(y), (int16)(w), (int16)(h), pixel)

//传 RECT参数 来填充矩形区域
#define GAL_FillRect(gc, pr, pixel) \
	GAL_FillBox(gc, (pr)->left, (pr)->top, (pr)->width, (pr)->height, pixel)

/**
 * \画水平线
 */
#define GAL_DrawHLine(gc, x, y, w, pixel) \
	tsg_drawHLine((int16)(x), (int16)(x+w-1), (int16)(y), pixel)
	//mrc_drawLine((int16)(x), (int16)(y), (int16)((x)+(w)-1), (int16)(y), PIXEL888RED(pixel), PIXEL888GREEN(pixel), PIXEL888BLUE(pixel))

/**
 * \画水平虚线
 */
VOID GAL_DrawDotHLine(GAL_GC gc, int x, int y, int w, Uint32 pixel);

/**
 * \画竖直线
 */
#define GAL_DrawVLine(gc, x, y, h, pixel) \
	tsg_drawVLine((int16)(x), (int16)(y), (int16)((y)+(h)-1), pixel)
//mrc_drawLine((int16)(x), (int16)(y), (int16)(x), (int16)((y)+(h)-1), PIXEL888RED(pixel), PIXEL888GREEN(pixel), PIXEL888BLUE(pixel))

/**
 * \画普通直线
 */
#define GAL_Line(gc, x1, y1, x2, y2, pixel) \
	tsg_drawLine((int16)(x1), (int16)(y1), (int16)(x2), (int16)(y2), pixel)
//mrc_drawLine((int16)(x1), (int16)(y1), (int16)(x2), (int16)(y2), PIXEL888RED(pixel), PIXEL888GREEN(pixel), PIXEL888BLUE(pixel))

/**
 * \画一个像素点
 */
#define GAL_DrawPixel(gc, x, y, pixel) \
	tsg_drawPoint((int16)(x), (int16)(y), pixel)
//mrc_drawPointEx((int16)(x), (int16)(y), PIXEL888RED(pixel), PIXEL888GREEN(pixel), PIXEL888BLUE(pixel))


/**
 * \画矩形
 */
void GAL_Rectangle(GAL_GC gc, int x, int y, int w, int h, Uint32 pixel);

/**
 * \画一个特定厚度的矩形
 */
void GAL_Rectangle2(GAL_GC gc, int x, int y, int w, int h, int line, Uint32 pixel);

/**
 * \画一圆角矩形，4个顶点不绘制
 */
void GAL_Rectangle3(GAL_GC gc, int x, int y, int w, int h, Uint32 pixel);

//内外圆角矩形框
void GAL_Rectangle4(GAL_GC gc, int x, int y, int w, int h, Uint32 pixel);

typedef enum {
	SHADE_UPDOWN,		//从上到下
	SHADE_LEFTRIGHT,	//从左到右
	SHADE_DOWNUP,		//从下到上
	SHADE_RIGHTLEFT		//从右到左
}ORIENTATION;
/**
 * 渐变填充矩形区域
 * topclr：渐变起始色
 * d：渐变值
 * o：方向
 */
void GAL_ShadeFillRectEx(GAL_GC gc, int x, int y, int w, int h, uint8 AR, uint8 AG, uint8 AB, uint8 BR, uint8 BG, uint8 BB, ORIENTATION mode);
void GAL_ShadeFillRect(GAL_GC gc, int16 x, int16 y, int16 w, int16 h, uint32 pixelA, uint32 pixelB, ORIENTATION mode);


/**
 * \或一个钩
 * \参数 x,y：钩子的交点坐标
 * \参数 d：钩子厚度
 */
void GAL_DrawHook(int ax, int ay, int lw, int rw, int d, uint32 pixel);

//渐变填充矩形 上->下
void ShadeFillRect(int x, int y, int w, int h, uint32 color1, uint32 color2);

// 渐变填充矩形算法A [4/6/2012 JianbinZhu]
// 由一个起始颜色 和 3个颜色渐变值确定
VOID GAL_ShadeFillRectA(int x, int y, int w, int h, Uint32 colorStart, int dr, int dg, int db);

//画渐变水平线
VOID GAL_ShadeHLine(GAL_GC gc, int x, int y, int w, uint32 pixelA, uint32 pixelB);

//画办透明矩形
#define GAL_EffFillBox(gc, x, y, w, h, pixel) \
	mrc_EffSetCon((int16)x, (int16)y, (int16)w, (int16)h, (int16)PIXEL888RED(pixel), (int16)PIXEL888GREEN(pixel), (int16)PIXEL888BLUE(pixel))

/**
 * \brief Flush all the screen.
 * \刷新整个屏幕
 */
#define GAL_Flush(gc) \
	mrc_refreshScreen(0, 0, (uint16)(gc->scrInfo.width), (uint16)(gc->scrInfo.height))

/**
 * \brief Flush a region of the screen.
 * \刷新指定范围屏幕
 */
#define GAL_FlushRegion(gc,  x,  y,  w, h) \
	mrc_refreshScreen((int16)(x), (int16)(y), (uint16)(w), (uint16)(h))

/**
 * \brief clear all the screen.
 * \用指定颜色清楚整个屏幕
 */
#define GAL_ClearScreen(pixel) \
	mrc_clearScreen(PIXEL888RED(pixel), PIXEL888GREEN(pixel), PIXEL888BLUE(pixel))


//------------------ 其他常用宏 ---------------------------
/**
 * \设置一个点的值
 */
#define SET_POINT(po, ax, ay) \
	po.x = (int)ax; po.y = (int)ay

/**
 * 检查一个坐标是否在指定矩形内
 */
#define POINT_IS_INRECT(pr, x, y) \
	(x >= pr.left && x < pr.left + pr.width && y >= pr.top && y < pr.top + pr.height)

/**
 * 设置区域值1 
 * mr_screenRectSt r;
 */
#define SET_RECT1(r, ax, ay, aw, ah) \
	r.x = (uint16)(ax); r.y = (uint16)(ay); r.w = (uint16)(aw); r.h = (uint16)(ah)

/**
 * 设置区域值2 
 * RECT r;
 */
#define SET_RECT2(r, ax, ay, aw, ah)	\
	r.left = (int)(ax); r.top = (int)(ay); r.width = (int)(aw); r.height = (int)(ah)

#define SET_RECT21(r, al, at, ar, ab)	\
	do{	\
	r.left = (int)(al);	\
	r.top = (int)(at); 	\
	r.width = (int)(ar-al); 	\
	r.height = (int)(ab-at);	\
	}while(0)

/**
 * 设置区域值3 
 * skyfont_screenRect_t r;
 */
#define SET_RECT3(r, ax, ay, aw, ah)	\
	r.x = (int16)(ax); r.y = (int16)(ay); r.w = (int16)(aw); r.h = (int16)(ah)

/** 
 * 设置颜色值 
 * mr_colourSt color;
 */
#define SET_COLOR(color, pixel)	\
	color.r = PIXEL888RED(pixel); color.g = PIXEL888GREEN(pixel); color.b = PIXEL888BLUE(pixel)

/** 
 * \反转颜色(255-原始) 
 */
#define REVE_COLOR(pixel)\
	RGB2PIXEL888((255-PIXEL888RED(pixel)), (255-PIXEL888GREEN(pixel)), (255-PIXEL888BLUE(pixel)))


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* _SGL_GAL_H */

