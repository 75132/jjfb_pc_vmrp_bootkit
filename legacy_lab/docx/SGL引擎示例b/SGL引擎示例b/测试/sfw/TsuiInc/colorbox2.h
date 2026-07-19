#ifndef _COLORBOX2_H_
#define _COLORBOX2_H_


#define COLORBOXN_SLECHANGED	0x0001	//颜色改变通知


uint32 ColorBox_GetColor(HWND hDlg);

LRESULT ColorBox2_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);


#endif
