#ifndef _SMP_EDIT_H
#define _SMP_EDIT_H

#include "window.h"

/**
  \defgroup smp_edit Simple Edit

  To work with simple edit:
  - Create a simple edit, and a simple edit can have one of these style
	- ES_NUM, support number input    只输入数字
	- ES_PWD, support password input  输入密码
	- ES_ALPHA, support alpha(number & characters) input 无限制
  - Set the edit information
  - Add the edit to the parent window
  - Response to the edit notify message

  \code
  	//create the edit
  	HWND hEdit = SGL_CreateWindow(SMP_Edit_WndProc, ...);
  	SMP_Edit_SetInfo(hEdit, title, text, maxsize);
  	SGL_AddChildWindow(hWnd, hEdit);

  	//response to the notify message
	case WM_COMMAND:
		WID id = LOWORD(wParam);
		WORD code = HIWORD(wParam);

		if(id == "the edit id")
		{
			HWND hEdit = (HWND)lParam;
			switch(code)
			{
			case SMP_EDITN_TEXTCHANGED:
				//handle the notify event.
				break;
			}
		}	
  \endcode

  @ingroup controls
  @{
 */
	/**
	* \name Window Styles
	* @{
	*/

/**
 * \brief Indicates the content is right aligned in horizonal.
 */
#define SMP_EDITS_HRIGHT		0x0001L
/**
 * \brief Indicates the content will be remove all entry and newline data.
 */
#define SMP_EDITS_TRIM_CR_LF    0x0002L

#define SMP_EDITS_DISABLED      0x0004L

#define SMP_EDITS_TRIM_SPACE    0x0008L
	/** @} */

	/**
	 * \name Window Notify Messages
	 * @{
	 */

/**
 * \brief Edit text changed notify message.
 *
 * \code
 *	case WM_COMMAND:
 *		WID id = LOWORD(wParam);
 *		WORD code = HIWORD(wParam);
 *
 *		if(id == "the edit id" && code == SMP_EDITN_TEXTCHANGED)
 *		{
 *			HWND hEdit = (HWND)lParam;
 * 			//handle the button click notify message
 *		}
 * \endcode
 *
 * \param hEdit the edit window send this notify message
 */
#define SMP_EDITN_TEXTCHANGED		0x0001  	/* 编辑框内容被修改后, 会发送这个消息给父窗口 */

	/** @} */

/**
 * \brief Cursor blink timer interval in ms
 */
#define SMP_EDIT_BLINK_INTERVAL	1000 //ms

	/**
	 * \name Window Member Functions
	 * @{
	 */

/**
 * \brief Set edit information.
 *
 * \note This function must be called before add to the parent window.
 *
 * \param hEdit the edit handle
 * \param title the title string will displayed in input window
 * \param[in,out] text the buffer to receive the input text
 * \param maxsize text buffer size
 * \注意：这里的title自从该编辑框创建以后就一直做为他的内容的缓冲区
 * \因此使用前必须给他申请一个空间来保存他的字符
 */
VOID SMP_Edit_SetInfo(HWND hEdit, PCWSTR title, PWSTR text, int maxsize);

/**
 * \brief Get the edit information.
 *
 * \param hEdit the edit window
 * \param[out] title the title string
 * \param[out] text the buffer
 * \param[out] maxsize the buffer size
 * \return TRUE on success, otherwise FALSE
 */
BOOL SMP_Edit_GetInfo(HWND hEdit, PCWSTR* title, PWSTR* text, int* maxsize);

/**
 * \brief Get the text.
 *
 * \param hEdit the edit window handle
 * \return the text data
 *  edit 通过父窗口发送来的消息，去获得用户输入的文字
 */
PWSTR SMP_Edit_GetText(HWND hEdit);

/**
 * \brief Set a new string to edit.
 *
 * \param hEdit the edit window handle
 * \param pstr the new string
 * \param redraw if redraw the window
 * \param notify if send SMP_EDITN_TEXTCHANGED message to parent
 */
VOID SMP_Edit_SetText(HWND hEdit, PCWSTR pstr, BOOL redraw, BOOL notify);

/**
 * \brief Start the cursor blink.
 *
 * \param hEdit the edit window handle
 * \param update if to update the edit window
 */
VOID SMP_Edit_StartBlink(HWND hEdit, BOOL update);

/**
 * \brief Stop the cursor blink.
 *
 * \param hEdit the edit window handle
 * \param update if to update the edit window
 */
VOID SMP_Edit_StopBlink(HWND hEdit, BOOL update);

/**
 * \brief Set the cursor position.
 *
 * \param hEdit the edit window handle
 * \param pos the cursor position
 * \param update if redraw the edit window
 */
BOOL SMP_Edit_SetCursorPosition(HWND hEdit, int pos, BOOL update);

	/** @} */

	/**
	 * \name Window Procedure
	 * @{
	 */
VOID SMP_Edit_AdjustPosEnd(HWND hEdit);

/**
 * \brief The simple edit window procedure.
 *
 * \param hEdit the window handle
 * \param Msg the window message
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of message process
  * 创建输入框控件并添加到父窗口
 * 参数:
 * parent:		button所属的父窗口句柄
 * rect:		button的大小和位置信息
 * id:			控件ID
 * style:		控件属性(ES_NUM|ES_PWD|ES_FLOAT)
 * userdata:	用户数据
 * listener:	监听事件窗口
 */
LRESULT SMP_Edit_WndProc(HWND hEdit, UINT Msg, WPARAM wParam, LPARAM lParam);

void SMP_Edit_SetIntroduceText(HWND hEdit, PCWSTR pIntroduce);

	/** @} */

/** @} end of smp_edit */

#endif /* _SMP_EDIT_H */

