// SGL屏幕缓冲区管理模块 [4/13/2012 JianbinZhu]
#ifndef _SGL_SCREEN_H
#define _SGL_SCREEN_H


typedef unsigned short * screenBuffer;	//屏幕缓冲区数据类型

//数据结构定义
typedef struct {
	screenBuffer originalScnBuf;	//原始屏幕缓冲区
	Uint16 originalWidth, originalHeight;	//原始屏幕宽高

	screenBuffer exScnBuf;		//拓展的屏幕缓冲区
	screenBuffer scnBuf;		//当前使用的屏幕缓冲区

	Uint16 width;		//屏幕宽度
	Uint16 height;		//屏幕高度
}T_SGL_SCREEN, *PT_SGL_SCREEN;


extern T_SGL_SCREEN g_sgl_screen;



//SGL 屏幕管理模块初始化
VOID GUIAPI SGL_ScreenInitialize(VOID);

//SGL 屏幕管理模块终止
VOID GUIAPI SGL_ScreenTerminate(VOID);

//获取当前屏幕缓冲区
screenBuffer SGL_GetScreenBuffer(VOID);

//获取原始屏幕缓冲区
screenBuffer SGL_GetOriScnBuf(VOID);

//获取拓展屏幕缓冲区
screenBuffer SGL_GetExScnBuf(VOID);

//设置当前屏幕缓冲区
VOID SGL_SetScreenBuffer(screenBuffer newBuf);

//恢复屏幕默认参数（缓冲区、大小 等）
VOID SGL_ScreenReset(VOID);

//改变屏幕尺寸
VOID SGL_SetScreenSize(Uint16 newWidth, Uint16 newHeight);

//获取当前屏幕尺寸
VOID SGL_GetScreenSize(Uint16 *width, Uint16 *height);

//复制屏幕缓冲区矩形区域到令一缓冲区
//默认原缓冲区 和 目的缓冲区 宽高一致
VOID SGL_ScnBufCpyRect(screenBuffer dst, screenBuffer src, PRECT srcRect, int dstx, int dsty);


#endif