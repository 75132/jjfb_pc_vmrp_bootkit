#ifndef _TEXTVIEW_H_
#define _TEXTVIEW_H_

#include "window.h"

//textview 属性：字符竖直间距
#define TVP_STRV_SPACE	2
//textview 属性：字符水平间距
#define TVP_STRH_SPACE	2
//textview 属性：字符与窗口左右间距
#define TVP_LEFT_MARGIN	3
//textview 属性：字符与窗口上下间距
#define TVP_TOP_MARGIN	3

//textview 样式：单行样式
#define TVS_SINGLELINE		0x0001	
//textview 样式：显示滚动条
#define TVS_SHOW_SCROBAR	0x0002


//设置文本(文本地址)
VOID TV_setText(HWND hWnd, PCWSTR text);

//设置文本(文本资源ID)
#define TV_setTextID(hWnd, resid) \
	TV_setText(hWnd, SGL_LoadString(resid))

//获取文本在指定宽度内显示的总高度
int32 TV_getTextHeight(HWND hWnd);

//设置最大行数
VOID TV_setMaxLineCount(HWND hWnd, int32 count);

LRESULT TextView_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL TV_lineDown(HWND hWnd);
BOOL TV_lineUp(HWND hWnd);

#endif