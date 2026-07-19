#ifndef _SGL_TOAST_H
#define _SGL_TOAST_H

#include "window.H"

enum _toast_position
{
	TOAST_TOPSCN,
	TOAST_MIDSCN,
	TOAST_BUTTOMSCN
};

/**
 * \Toast 首次绘制
 * \此时参数 r 作为输出，用于确定toast位置尺寸
 * \重绘的时候就不需要重新计算位置尺寸了
 */
VOID SGL_ToastDraw(PRECT r, PCWSTR text);

/**
 * \Toast 重新绘制
 * \当Toast已经显示
 * \而需要重绘窗口又盖住了Toast的时候调用
 */
VOID SGL_ToastReDraw(PRECT r, PCWSTR text);

#endif