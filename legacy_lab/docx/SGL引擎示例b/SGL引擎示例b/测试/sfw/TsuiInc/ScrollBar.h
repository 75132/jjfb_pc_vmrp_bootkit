#ifndef _SCROLLBAR_H_
#define _SCROLLBAR_H_

#include "ts_gui.h"


typedef struct
{
	int total;	//总数
	int c_pgSize; //一页显示数
	int steps;	//总步数
	int cur;	//当前位置
	int cursor_h;	//滑块高度
	BOOL b_show;	//是否显示状态
	int32 t_fade;		//自动消失定时器
}SCRBAR_WINDATA, * PSCRBAR_WINDATA;


//滚动条属性值
#define SBP_WIDTH	4	//滚动条宽度
#define SMP_AUTOFADE_TIME 1000	//滚动条自动消失前停留时间

//滚动条样式
#define SBS_AUTOFADE 0x0001		//自动隐藏样式

//设置滚动条信息
VOID SB_setInfo(HWND hWnd, int total, int pgSize);

//设置滚动条当前值
VOID SB_setValue(HWND hWnd, int index, BOOL redraw);

//滚动条窗口回调
LRESULT Scrollbar_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);


#endif