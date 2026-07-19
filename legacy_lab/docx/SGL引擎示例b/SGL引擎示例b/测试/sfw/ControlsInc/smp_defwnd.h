#ifndef _SMP_MENUWND_H
#define _SMP_MENUWND_H

#include "window.h"

/**
  \defgroup smp_menuwnd Simple Menu Based Window

  This is not a window or control shown in the parent window.
  It is just a window procedure to add function to a parent window in this case:
  When the menu show on a top-level window, the toolbar should change
  it's left/right softkey to "OK/Cancel". Just call this function as the default window procedure
  of your top-level window which has the popup menu.
  
  \code
	LRESULT  YOUR_WINDOW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		switch(Msg)
		{
			//handle your messages
		}

		//call this function as default
		return SMP_MenuWnd_WndProc(hWnd, Msg, wParam, lParam);
	}
  
  \endcode

  @ingroup controls
  @{
 */

	/**
	 * \name Window Procedure
	 * @{
	 */

/**
 * \brief The menu based window procedure.
 *
 * \param hWnd the window handle
 * \param Msg the window message
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of the process
 */
LRESULT SMP_Default_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam); 

	/** @} */

/** @} end of smp_menuwnd */


#endif

