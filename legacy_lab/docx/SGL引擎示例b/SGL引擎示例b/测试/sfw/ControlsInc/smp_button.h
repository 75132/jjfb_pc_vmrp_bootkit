#ifndef _SMP_BUTTON_H
#define _SMP_BUTTON_H

#include "window.h"


/**
  \defgroup smp_button Simple Button

  To work with the simple button:
	- Create the button window
	- Set the button title or other necessary information
	- Add to the parent window
	- Response the button notify messages
  
  \code
	//create a button
	hBtn = SGL_CreateWindow(SMP_Button_WndProc, ...);
	SMP_Button_SetTitle(hBtn, SGL_LoadString(STR_OK));
	SGL_AddChildWindow(hWnd, hBtn);

	//response to button notify messages
	case WM_COMMAND:
		WID id = LOWORD(wParam);
		WORD code = HIWORD(wParam);

		if(id == "the button id")
		{
			HWND hBtn = (HWND)lParam;
			switch(code)
			{
			case SMP_BUTTONN_CLICKED:
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
 * \brief Indicate the button is a checkbox button.
 */
#define SMP_BUTTONS_CHECKBOX		0x0001L

/**
 * \brief Indicate the button is a radiobox button.
 */
#define SMP_BUTTONS_RADIOBOX		0x0002L

/**
 * \brief The title is right aligned in horizonal.
 */
#define SMP_BUTTONS_HRIGHT			0x0004L

/**
 * \brief The title is center aligned in horizonal.
 */
#define SMP_BUTTONS_HCENTER			0x0008L

/**
 * \brief The title is center aligned in vertical.
 */
#define SMP_BUTTONS_VCENTER			0x0010L

/**
 * \brief The title is bottom aligned in vertical.
 */
#define SMP_BUTTONS_VBOTTOM			0x0020L

/**
 * \brief Indicates the button is checked.
 */
#define SMP_BUTTONS_CHECKED			0x0040L	

	/** @} */

	/**
	 * \name Window Notify Messages
	 * @{
	 */

/**
 * \brief Sent when button clicked.
 *
 * \code
 *	case WM_COMMAND:
 *		WID id = LOWORD(wParam);
 *		WORD code = HIWORD(wParam);
 *
 *		if(id == "the button id" && code == SMP_BUTTONN_CLICKED)
 *		{
 *			HWND hBtn = (HWND)lParam;
 * 			//handle the button click notify message
 *		}
 * \endcode
 *
 * \param hBtn the button send this message
 */
#define SMP_BUTTONN_CLICKED			0x0001

	/** @} */

	/**
	 * \name Window Member Functions
	 * @{
	 */

/**
 * \brief Set the button window title.
 * 
 * \param hBtn the button window handle
 * \param title the new title
 */
VOID SMP_Button_SetTitle(HWND hBtn, PCWSTR title);

/**
 * \brief Get the button title.
 *
 * \param hBtn the button window handle
 * \return the button window title
 */
PCWSTR SMP_Button_GetTitle(HWND hBtn);

/**
 * \brief Set the button background bitmap.
 *
 * \param hBtn the button window handle
 * \param bmp the nomal state background bitmap id
 * \param bmp_p the pressed state background bitmap id
 */
VOID SMP_Button_SetBitmapGroup(HWND hBtn, DWORD bmp, DWORD bmp_p);

/**
 * \brief Check the button.
 * 
 * \param hBtn the button window handle
 * \param checked
 *	- TRUE to check the button
 *	- FALSE to uncheck the button
 */
VOID SMP_Button_SetChecked(HWND hBtn, BOOL checked);

	/** @} */

	/**
	 * \name Window Procedure
	 * @{
	 */

/**
 * \brief The simple button window procedure.	
 *
 * \param hBtn the button window handle
 * \param Msg the window message
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of message process 
 */
LRESULT SMP_Button_WndProc(HWND hBtn, UINT Msg, WPARAM wParam, LPARAM lParam);

	/** @} */

/** @} end of smp_button */

#endif /* _SMP_BUTTON_H */

