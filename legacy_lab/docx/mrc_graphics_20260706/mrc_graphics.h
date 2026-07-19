#ifndef _MRC_GRAPHICS_H_
#define _MRC_GRAPHICS_H_

//版本v20260706, 修复了drawBitmapFlip函数的BUG
//版本v20260705, 优化了_DrawBitmapEx，支持缩放,旋转,修复了drawBitmapRotate的旋转方向
//版本v20260702, 优化了drawBitmapRotate的参数属性
//版本v20260630，完善了drawBitmapFlip函数，堪比官方
//版本v20260323，修复了drawBitmapRotate()黑边问题
//版本v20260322，修复了drawBitmapRotate()正负角度转向问题
//版本v20260321，保留必要的声明，非必要的声明移到了c文件内

#include "xl_bmp.h"

typedef struct
{
    uint32 width;  // 宽度
    uint32 height; // 高度
    uint32 stride;
    int32 format; // 格式 位数
    uint32 flags; // 0 for now未用
    void *ptr;    // 缓存
} BITMAPINFO;

typedef struct
{
    int16 A; // A, B, C, and D are fixed point values with an 8-bit integer part
    int16 B; // and an 8-bit fractional part.
    int16 C;
    int16 D;
    uint16 rop;
} mr_transMatrixSt;

/*
enum
{
    SHADE_UPDOWN,    // 从上到下
    SHADE_LEFTRIGHT, // 从左到右
    SHADE_DOWNUP,    // 从下到上
    SHADE_RIGHTLEFT  // 从右到左
};
*/

#define GRAPHICS_LEFT 3
#define GRAPHICS_TOP 48
#define GRAPHICS_RIGHT 5
#define GRAPHICS_CENTER 17
#define GRAPHICS_BOTTOM 80



/* 强烈建议从mrp中读取bitmap，
左上角像素非0时，会默认 ->mode = BM_TRANS;
也可在调用函数之后手动再 ->mode = BM_COPY;  */
extern BITMAP_565 *readBitmap565FromAssets(const char *filename);

/*     绘制bitmap的指定区域
b:     源图像对象
x, y:   绘制到屏幕的目标坐标
w, h:  要获取的图源区域宽高
sx, sy: 要获取的图源起始坐标 */
extern int32 drawBitmapTransform(BITMAP_565 *b, int32 x, int32 y, int32 w, int32 h, int32 sx, int32 sy);


/*
 * 高效的位图区域旋转绘制函数
 * b:      源图像对象
 * x, y:   绘制到屏幕的目标坐标
 * w, h:   要获取的图源区域宽高
 * rop:    旋转翻转模式 (TRANS_NONE ~ TRANS_MIRROR_ROT90)
 * sx, sy: 要获取的图源起始坐标*/
int32 drawBitmapFlip(BITMAP_565 *p, int32 x, int32 y, int32 w, int32 h, int32 rop, int32 sx, int32 sy) ;

/*将位图绕指定中心旋转并绘制到屏幕
b        图像对象
centerX  屏幕上的旋转中心 X 坐标
centerY  屏幕上的旋转中心 Y 坐标
bx       位图上旋转中心的 X 偏移（相对于位图左上角）
by       位图上旋转中心的 Y 偏移（相对于位图左上角）
r        旋转角度（单位：角度）*/
void drawBitmapRotate(BITMAP_565 *b, int32 centerX, int32 centerY, int32 bx, int32 by, int32 r);

//适合绘制整张图
void drawBitmap(BITMAP_565 *bmp, int32 x, int32 y);

// 扩展绘制bitmap，将bitmap上(tx,ty,tw,th)区域缩放绘制到屏幕(x,y,w,h)区域上
void drawBitmapEx(BITMAP_565 *bmp, int32 x, int32 y, int32 w, int32 h, int32 tx, int32 ty, int32 tw, int32 th);

/*
图片旋转缩放绘制
将srcbmp中的图片从(sx, sy)开始的宽高为w, h的区域，绘制到屏幕(dx,dy)。

模式rop：BM_COPY或BM_TRANSPARENT

根据变换公式，可以绘出不同效果的图像，
ABCD为放大256倍的定点数，比如：
旋转,缩放图像：
A = 256 * cos（角度）* 缩放倍数
B = 256 * -sin（角度）* 缩放倍数
C = 256 * sin（角度）* 缩放倍数
D = 256 * cos（角度）* 缩放倍数
*/
void _DrawBitmapEx(BITMAP_565 *srcbmp, uint16 dx, uint16 dy, uint16 sx, uint16 sy, uint16 w, uint16 h, mr_transMatrixSt *trans);

// 获取bitmap信息
int32 bitmap565getInfo(BITMAP_565 *bmp, BITMAPINFO *info);

// 释放bitmap
extern int32 bitmapFree(BITMAP_565 *b);

// 不建议从内存卡读取bitmap
extern BITMAP_565 *readBitmap(char *filename);

//清屏
void gl_drawColor(uint32 color);
void gl_clearScreen(int32 r, int32 g, int32 b);
// 画矩形
void gl_drawRect(int32 x, int32 y, int32 w, int32 h, uint32 color);
void gl_drawHollowRect(int x, int y, int width, int height, uint32 color);
// 画线
void gl_drawLine(int32 x1, int32 y1, int32 x2, int32 y2, uint32 color);
// 绘制三角形
void gl_drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32 color);
// 绘制空心三角形
void gl_drawHollowTriangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32 color);
// 画圆
void gl_drawCir(int32 x, int32 y, int32 r, uint32 color);
// 绘制空心圆
void gl_drawHollowCir(int x0, int y0, int r, uint32 color);
// 绘制旋转的矩形
void gl_drawRotatedRect(int16 centerX, int16 centerY, int16 width, int16 height, int32 bx, int32 by, float angle, uint32 color);
// 绘制旋转的空心矩形
void gl_drawRotatedHollowRect(int16 centerX, int16 centerY, int16 width, int16 height, int32 bx, int32 by, float angle, uint32 color);

// 绘制渐变矩形
void drawShadeRect(int32 x, int32 y, int32 w, int32 h, uint32 colorA, uint32 colorB, int mode);

// 透明度绘制bitmap
void drawBitmapAlpha(BITMAP_565 *b, int32 x, int32 y, int32 alpha);

// 将bitmap缩放,生成一个新的BITMAP
BITMAP_565 *createBitmapFromBitmap(BITMAP_565 *bmp, int32 width, int32 height);
// 将buf图片指定区域绘制到di中
void drawBitmap565Old(BITMAP_565 *di, BITMAP_565 *buf, int32 x, int32 y, int32 w, int32 h, int32 sx, int32 sy);
// 保存bitmap到文件
void saveBitmap(BITMAP_565 *bmp, const char *filename);
// 在bitmap内绘制空心三角形
void bmp_drawHollowTriangle(BITMAP_565 *bmp, int x1, int y1, int x2, int y2, int x3, int y3, uint32 color);
// 在bitmap内绘制三角形
void bmp_drawTriangle(BITMAP_565 *bmp, int x1, int y1, int x2, int y2, int x3, int y3, uint32 color);
// 在bitmap内绘制空心圆
void bmp_drawHollowCircle(BITMAP_565 *bmp, int32 centerX, int32 centerY, int32 radius, uint32 color);
// 在bitmap内绘制圆形
void bmp_drawCircle(BITMAP_565 *bmp, int32 cx, int32 cy, int32 radius, uint32 color);
// 在bitmap内绘制线段
void bmp_drawLine(BITMAP_565 *bmp, int32 x1, int32 y1, int32 x2, int32 y2, uint32 color);
// 在bitmap内绘制点
void bmp_drawPoint(BITMAP_565 *bmp, int32 x, int32 y, uint32 color);
// 在bitmap内绘制矩形
void bmp_drawRect(BITMAP_565 *bmp, int32 x, int32 y, int32 w, int32 h, uint32 color);
#endif