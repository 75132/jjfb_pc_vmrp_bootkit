#ifndef _SMP_LABEL_H
#define _SMP_LABEL_H

#include "window.h"


/**
  \defgroup smp_label Simple Label

  To work with simple label:
  - Create the label window
  - Set the label content
  - Add to the parent window
  smp_label简单的标签defgroup’
　　工作与简单的标签。
　　1.创造标签窗口
　　2.设置标签内容
　　3.增添到父窗口

  \code

  hControl = SGL_CreateWindow(SMP_Label_WndProc, ...);
  SMP_Label_SetContent(hControl, RESID_INVALID, SGL_LoadString(STR_HELLO), 0);
  SGL_AddChildWindow(hParent, hControl);

  \endcode

  @ingroup controls
  @{
*/

	/**
	* \name Window Styles
	* @{
	*/

/**
 * \有下划线
 */
#define SMP_LABELS_LINK			0x0001L

/**
 * \label 只显示文字作用，不响应事件
 */
#define SMP_LABELS_STATIC		0x0002L

/**
 * \自动滚动标志
 */
#define SMP_LABELS_AUTOSCROLL	0x0004L

/**
 * \内容水平居中显示标志
 */
#define SMP_LABELS_HCENTER		0x0008L

/**
 * \内容竖直居中显示标志
 */
#define SMP_LABELS_VCENTER		0x0010L

/**
 * \多行文本样式
 */
#define SMP_LABELS_MUTILLINE	0x0040L

	/** @} */

	/**
	 * \name Window Notify Messages
	 * @{
	 */
	 
/**
 * \brief Sent when label clicked.
 *
 * \code
 *	case WM_COMMAND:
 *		WID id = LOWORD(wParam);
 *		WORD code = HIWORD(wParam);
 *
 *		if(id == "the label id" && code == SMP_LABELN_CLICKED)
 *		{
 *			HWND hLabel = (HWND)lParam;
 * 			//handle the button click notify message
 *		}
 * \endcode
 *
 * \param hLabel the Label handle which send this message
 */
#define SMP_LABELN_CLICKED		0x0001

//定义一个供label调用本地编辑框的句柄

	/** @} */

	/**
	 * \name Window Member Functions
	 * @{
	 */

/**
 * \brief Set the label content.
 *
 * \param hWnd the label window handle
 * \param bmpID the bitmap id show on the left
 * \param content the new string for the content
 * \param interval the auto scroll speed in ms
 *	- > 0 when label has SMP_LABELS_AUTOSCROLL style
 *	- 0 when label DOES NOT has SMP_LABELS_AUTOSCROLL style
 * type 文件类型
 */
VOID SMP_Label_SetContent(HWND hWnd, DWORD bmpID, PCWSTR content);

#define SMP_Label_SetText(hLabel, text) \
	SMP_Label_SetContent(hLabel, RESID_INVALID, (PCWSTR)text)

/*
*获取label上的标题文字
*用于处理文件
*/
PSTR SMP_Label_GetContent(HWND hWnd);


/**
* \brief Start auto scroll which stop by SMP_Label_StopAutoScroll.
*
* \note the label must has  SMP_LABELS_AUTOSCROLL style
*
* \param hWnd the label window handle
* \param reset TRUE play from start, FALSE continue palying
* \sa SMP_Label_StopAutoScroll
*/
VOID SMP_Label_StartAutoScroll(HWND hWnd, BOOL reset);

/**
 * \brief Stop playing.
 *
 * \note the label must has  SMP_LABELS_AUTOSCROLL style
 *
 * The label stop palying on hiden, and auto palying when shown again.
 * So when u stop a label, the SMP_Label_StartAutoScroll is not needed when the label shown again.
 *
 * \param hWnd the label window handle
 * \sa SMP_Label_StartAutoScroll
 */
VOID SMP_Label_StopAutoScroll(HWND hWnd);

	/** @} */

	/**
	 * \name Window Procedure
	 * @{
	 */

/**
 * \brief The simple Label window procedure.	
 *
 * \param hWnd the button window handle
 * \param Msg the window message
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of message process 
 */
LRESULT SMP_Label_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
	
	/** @} */

/** @} end of smp_label */

#endif
