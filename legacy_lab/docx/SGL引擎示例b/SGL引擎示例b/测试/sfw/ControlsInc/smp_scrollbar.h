#ifndef _SMP_SCROLLBAR_H
#define _SMP_SCROLLBAR_H

#include "window.h"

/**
  \defgroup smp_scrollbar Simple Scrollbar

  To work with the simple scrollbar:
  - Create the scrollbar window
  - Set the scrollbar window information
  - Add to the parent window
  - Response to the notify message

  \code
  	//create the scrollbar
  	hScrbar = SGL_CreateWindow(SMP_Scrollbar_WndProc, ...);
  	SMP_Scrollbar_SetInfo(hScrbar);
  	SGL_AddChildWindow(hWnd, hScrbar);

  	//response to the notify message
	case WM_COMMAND:
		WID id = LOWORD(wParam);
		WORD code = HIWORD(wParam);

		if(id == "the scrollbar id" && code == SMP_SCRBARN_VALUECHANGED)
		{
			int index = (int)lParam; //current scrollbar value
		}	
  \endcode
  
  @ingroup controls
  @{
*/

	/**
	 * \name Window Notify Messages
	 * @{
	 */

/**
 * \brief Scrollbar window value changed notify message.
 * \scrollbar 位置改变
 * \code
 *	case WM_COMMAND:
 *		WID id = LOWORD(wParam);
 *		WORD code = HIWORD(wParam);
 *
		if(id == "the scrollbar id" && code == SMP_SCRBARN_VALUECHANGED)
		{
			int index = (int)lParam; //current scrollbar value
		}	
 * \endcode
 *
 * \param index the scrollbar current value
 * \当滚动条位置变化时通知父窗口这个消息 
 */
#define SMP_SCRBARN_VALUECHANGED		0x0001  


//一直显示样式（否则过一段时间消失）
#define SMP_SBS_ALWAYSSHOW		0x0001	

	/** @} */

	/**
	 * \name Window Member Functions
	 * @{
	 */

/**
 * \brief Set information of scrollbar.
 *
 * \param hWnd the scrollbar handle
 * \param min the minimum value
 * \param max the maximum value
 * \param pagesize the page size when when response to mouse event
 * \return TRUE on success, otherwise FALSE
 * 设置滚动条信息
 * 参数:
 * hWnd:		滚动条的窗口句柄
 * min:			滚动条的最小值(一般设置为0)
 * max:			滚动条的最大值(一般设置为要滚动的内容的总行数)
 * pagesize:	显示窗口每页显示的行数
 */
BOOL SMP_Scrollbar_SetInfo(HWND hWnd, int min, int max, int pagesize);

/**
 * \brief Get information of scrollbar.
 *
 * \param hWnd the scrollbar handle
 * \param min the minimum value
 * \param max the maximum value
 * \param pagesize the page size
 * \return TRUE on success, otherwise FALSE
 * 获取滚动条信息
 * hWnd:		滚动条的窗口句柄
 * min:			滚动条的最小值(一般设置为0)
 * max:			滚动条的最大值(一般设置为要滚动的内容的总行数)
 * pagesize:	显示窗口每页显示的行数
 */
BOOL SMP_Scrollbar_GetInfo(HWND hWnd, int* min, int* max, int* pagesize);

/**
 * \brief Set the scrollbar step
 *
 * \param hWnd the scrollbar handle
 * \param step the line step value
 */
VOID SMP_Scrollbar_SetStep(HWND hWnd, int step);

/**
 * \brief Set a new position of a scrollbar.
 *
 * \param hWnd the scrollbar
 * \param value the new value
 * \param redraw if redraw the window
 * \return TRUE on success, otherwise FALSE
 * 设置滚动条的位置
 * hWnd:		滚动条的窗口句柄
 * value:		设置滚动条的当前值, 范围[min, max]
 */
BOOL SMP_Scrollbar_SetValue(HWND hWnd, int value, BOOL redraw);

/**
 * \brief Get current position of a scrollbar.
 *
 * \param hWnd the scrollbar
 * \return current position value
 * 获取滚动条的当前值
 * hWnd:		滚动条的窗口句柄
 * 返回值:
 * 滚动条的当前值, 范围[min, max]
 */
int SMP_Scrollbar_GetValue(HWND hWnd);

/**
 * \brief Move cursor of scrollbar page size up.
 *
 * \param hWnd the scrollbar handle
 * \return TRUE on success, otherwise FALSE
 * 滚动条向前翻页
 */
BOOL SMP_Scrollbar_PageUp(HWND hWnd);

/**
 * \brief Move cursor of scrollbar page size down.
 *
 * \param hWnd the scrollbar handle
 * \return TRUE on success, otherwise FALSE
 * 滚动条向后翻页
 */
BOOL SMP_Scrollbar_PageDown(HWND hWnd);

/**
 * \brief Move cursor of scrollbar step size up.
 *
 * \param hWnd the scrollbar handle
 * \return TRUE on success, otherwise FALSE
 * 滚动条向前滚一行
 */
BOOL SMP_Scrollbar_LineUp(HWND hWnd);

/**
 * \brief Move cursor of scrollbar step size up.
 *
 * \param hWnd the scrollbar handle
 * \return TRUE on success, otherwise FALSE
 * 滚动条向后滚一行
 */
BOOL SMP_Scrollbar_LineDown(HWND hWnd);

	/** @} */

	/**
	 * \name Window Procedure
	 * @{
	 */
/**
 * \brief scrollbar window procedure
 *
 * \param hWnd the window handle
 * \param Msg the window message
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of message process 
 */
LRESULT SMP_Scrollbar_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

		/** @} */

/** @} end of smp_scrollbar */

#endif /* _SMP_SCROLLBAR_H */

