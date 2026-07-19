#ifndef _SMP_PROGBAR_H
#define _SMP_PROGBAR_H

#include "window.h"


/**
  \defgroup smp_progbar Simple Progress Bar

  To work with the simple progress bar:
  - Create the progress bar
  - Set the progress bar range
  - Add to the parent window
  - Response to the notify message

  \code
  	//create the scrollbar
  	hPorgBar = SGL_CreateWindow(SMP_ProgBar_WndProc, ...);
  	SMP_ProgBar_SetRange(hPorgBar, 1, 30); //the default range is [0 - 99] when this function not called
  	SGL_AddChildWindow(hWnd, hPorgBar);

  	//response to the notify message
	case WM_COMMAND:
		WID id = LOWORD(wParam);
		WORD code = HIWORD(wParam);

		if(id == "the progbar id" && code == SMP_PROGBARN_VALUECHANGED)
		{
			int value = (int)lParam; //current progress bar value
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
 * \brief Progress Bar window value changed notify message.
 *
 * \code
 *	case WM_COMMAND:
 *		WID id = LOWORD(wParam);
 *		WORD code = HIWORD(wParam);
 *
		if(id == "the progbar id" && code == SMP_PROGBARN_VALUECHANGED)
		{
			int value = (int)lParam; //current progbar value
		}	
 * \endcode
 *
 * \param value the  current value
 */
#define SMP_PROGBARN_VALUECHANGED		0x0001L


#define SMP_PROGBARS_DRAG	0x0001		//进度可拖拽

/** @} */

	/**
	 * \name Window Member Functions
	 * @{
	 */

/**
 * \brief Set the progress bar range. After the creation of the window the default range is [0-99].
 *
 * \param hWnd the progress bar window handle
 * \param min the min value of the progress bar
 * \param max the max value of the progress bar
 */
VOID SMP_ProgBar_SetRange(HWND hWnd, Sint32 min, Sint32 max);

/**
 * \brief Set the progress bar value
 *
 * \param hWnd the progress bar handle
 * \param value the new value
 * \param redraw if redraw the progress bar
 * \param notify send the notify message if value changed
 */
VOID SMP_ProgBar_SetValue(HWND hWnd, Sint32 value, BOOL redraw, BOOL notify);

//获取进度条当前值
Sint32 SMP_ProgBar_GetValue(HWND hWnd);

	/** @} */

	/**
	 * \name Window Procedure
	 * @{
	 */

/**
 * \brief Progress bar procedure
 *
 * \param hWnd the window handle
 * \param Msg the window message
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of message process 
 */
LRESULT SMP_ProgBar_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
	
	/** @} */
	
/** @} end of smp_progbar */

#endif
