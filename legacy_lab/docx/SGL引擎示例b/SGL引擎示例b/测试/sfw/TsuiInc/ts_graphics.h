#ifndef _TS_GRAPHICS_H_
#define _TS_GRAPHICS_H_


// 由RGB值生成颜色
#define MAKERGB888(r, g, b)   (uint32)(((uint32)r << 16) | ((uint32)g << 8) | ((uint32)b))
#define MAKERGB565(r, g, b)   (uint16)(((uint32)(r >> 3) << 11) | ((uint32)(g >> 2) << 5) | ((uint32)(b >> 3)))

// 转换颜色存储形式
#define RGB888TO565(c)    (uint16)((((uint32)(c) & 0x00f80000) >> 8) | (((uint32)(c) & 0x0000fc00) >> 5) | (((uint32)(c) & 0x000000f8) >> 3))
#define RGB565TO888(c)    (uint32)((((uint32)(c) & 0x0000f800) << 8) | (((uint32)(c) & 0x000007e0) << 5) | (((uint32)(c) & 0x0000001f) << 3))


void tsg_init(void);

void tsg_drawPoint_a(int x, int y, uint32 color);
void tsg_drawLine_a(int x1, int y1, int x2, int y2, uint32 color);
void tsg_drawRect_a(int x, int y, int w, int h, uint32 color);

void tsg_drawHLine(int16 x1, int16 x2, int16 y, uint32 color);
void tsg_drawHLineEx(int16 x1, int16 x2, int16 y, uint8 r, uint8 g, uint8 b);
void tsg_drawVLine(int16 x, int16 y1, int16 y2, uint32 color);
void tsg_drawVLineEx(int16 x, int16 y1, int16 y2, uint8 r, uint8 g, uint8 b);
void tsg_drawLineEx(int16 x1, int16 y1, int16 x2, int16 y2, uint8 r, uint8 g, uint8 b);
void tsg_drawLine(int16 x1, int16 y1, int16 x2, int16 y2, uint32 color);
void tsg_drawPointEx(int16 x, int16 y, uint8 r, uint8 g, uint8 b);
void tsg_drawPoint(int16 x, int16 y, uint32 color);

//下面两个函数效率极低，不要用
void tsg_drawRect(int16 x, int16 y, int16 w, int16 h, uint32 color);
void tsg_drawRectEx(int16 x, int16 y, int16 w, int16 h, uint8 r, uint8 g, uint8 b);

/*
#define TSG_MAKECOLOR(alpha, r,g,b) (((r) << 16) | ((g) << 8) | (b) | ((alpha) << 24))
#if 0

typedef enum {
	TS_GRAPHICS_FUNID_DRAWPOINT,
	TS_GRAPHICS_FUNID_DRAWLINE,
	TS_GRAPHICS_FUNID_DRAWRECT
}TS_GRAPHICS_FUNID;

//插件入口
#ifdef SDK_MOD
int32 ts_graphics_init(void);
int32 ts_graphics_event(int32, int32, int32);
int32 ts_graphics_pause(void);
int32 ts_graphics_resume(void);
int32 ts_graphics_exit(void);
#else
#define ts_graphics_init mrc_init
#define ts_graphics_event mrc_event
#define ts_graphics_pause mrc_pause
#define ts_graphics_resume mrc_resume
#define ts_graphics_exit mrc_exitApp
#endif



#define tsg_drawPoint(x, y, c) \
	mrc_extSendAppEventEx(ZHU_EXTID_GRAPHICS, TS_GRAPHICS_FUNID_DRAWPOINT, (int32)x, (int32)y, (int32)c, 0,0)

#define tsg_drawRect(x, y, w, h, c) \
	mrc_extSendAppEventEx(ZHU_EXTID_GRAPHICS, TS_GRAPHICS_FUNID_DRAWRECT, (int32)x, (int32)y, (int32)w, (int32)h, (int32)c)


//LIB库接口
void ts_graphics_load(void);

#else

void _tsg_drawPoint(int x, int y, uint32 color);
void _tsg_drawLine(int x1, int y1, int x2, int y2, uint32 color);
void _tsg_drawRect(int x, int y, int w, int h, uint32 color);
#define tsg_drawPoint(x, y, c) \
	_tsg_drawPoint((int)(x), (int)(y), (uint32)(c))

#define tsg_drawLine(x1, y1, x2, y2, c) \
	_tsg_drawLine((int)(x1), (int)(y1), (int)(x2), (int)(y2), (uint32)(c))

#define tsg_drawRect(x, y, w, h, c) \
	_tsg_drawRect((int)(x), (int)(y), (int)(w), (int)(h), (uint32)(c))

#endif
*/

#endif
