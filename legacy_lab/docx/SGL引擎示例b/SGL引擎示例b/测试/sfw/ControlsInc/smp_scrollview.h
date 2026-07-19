#ifndef _SMP_SCROLLVIEW_H
#define _SMP_SCROLLVIEW_H

#include "window.h"

/**
  \defgroup smp_scrollview Simple ScrollView

  To work with the simple ScrollView:
	- Create the ScrollView
	- Get the Content view of the ScrollView
	- Add your window to the content view 
	- Set the LISTENER of your window to your top-level windos
  
  \code
	//create a ScrollView
	hScrollView = SGL_CreateWindow(SMP_ScrollView_WndProc, ...);
	hContent = SMP_ScrollView_GetContentView(hScrollView);

	hControl = SGL_CreateWindow("your window", ...);
	_LISTENER(hControl) = hWnd; // should set your listener and the notify message will send to hWnd
	SGL_AddChildWindow(hContent, hControl);

	......
	SGL_AddChildWindow(hWnd, hScrollView);
  \endcode
  
  @ingroup controls
  @{
*/

	/**
	 * \name Window Member Functions
	 * @{
	 */

/**
 * \brief Get the scrollview content view
 *
 * For we must add your window to a scrollview's content view not the scrolview itself.
 *
 * \param hWnd the scrollview window handle
 * \return the content view handle
 */
HWND SMP_ScrollView_GetContentView(HWND hWnd);



	/** @} */

	/**
	 * \name Window Procedure
	 * @{
	 */

/**
 * \brief Simple ScrollView window procedure
 *
 * \param hWnd the window handle
 * \param Msg the window message 
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of message process 
 */

LRESULT SMP_ScrollView_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

	/** @} */

/** @} end of smp_scrollview */

#endif
