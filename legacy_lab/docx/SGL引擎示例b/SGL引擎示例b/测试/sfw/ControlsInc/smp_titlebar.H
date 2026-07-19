//简单工具栏
//  [3/8/2012 JianbinZhu]

#ifndef _SMP_TITLEBAT_H_
#define _SMP_TITLEBAT_H_


#include "window.H"

#define SMP_TITLEBARS_SHOWTIME	0x0001		//显示本地时间样式
//将废弃
#define SMP_TITLEBAR_STATIC 0x0001

//停止刷新时间
VOID SMP_Titlebar_StopTime(HWND hWnd);

//开始刷新时间
VOID SMP_Titlebar_StartTime(HWND hWnd);

//设置标题栏内容
VOID SMP_Titlebar_SetContent(HWND hWnd, DWORD bmpID, PCWSTR content);

//标题栏窗口事件
LRESULT SMP_Titlebar_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

#endif