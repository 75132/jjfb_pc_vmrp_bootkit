#ifndef _SMP_TABWINDOW_H
#define _SMP_TABWINDOW_H

#include "window.h"
#include "smp.h"


/**
  \defgroup smp_tabwindow Simple tab window

  To work with the simple tab window:
	- Create the tab window
	- Create the tabs and add to the tab window
	- Add the tab window to the parent window
	- Response the notify messages
  
  \code
	//create a button
	HWND hTabWnd = SGL_CreateWindow(SMP_TabWindow_WndProc, ...);
	SGL_AddChildWindow(hWnd, hTabWnd);

	hControl = SGL_CreateWindow(TAB1_WndProc, ...);
	SMP_TabWindow_AddTab(hTabWnd, BMP_BUSY, SGL_LoadString(STR_OK), hControl);		

	//response to tab window notify messages
	case WM_COMMAND:
		WID id = LOWORD(wParam);
		WORD code = HIWORD(wParam);

		if(id == "the tab window id")
		{
			Sint32 index = (Sint32)lParam; // the tab index
			switch(code)
			{
			case SMP_TABWNDN_TABSWITCHED:
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
 * \brief Indicates that right arrow pressed
 */
#define SMP_TABWNDS_RIGHTARROW			0x0001L

/**
 * \brief Indicates that left arrow pressed
 */
#define SMP_TABWNDS_LEFTARROW			0x0002L

	/** @} */

	/**
	 * \name Window Notify Messages
	 * @{
	 */

/**
 * \brief Sent when tab window switched
 *
 * \code
 *	case WM_COMMAND:
 *		WID id = LOWORD(wParam);
 *		WORD code = HIWORD(wParam);
 * 
 *		if(id == "the tab window id")
 *		{
 *			Sint32 index = (Sint32)lParam; // the tab index
 *			switch(code)
 *			{
 *			case SMP_TABWNDN_TABSWITCHED:
 *				//handle the notify event.
 *				break;
 *			}
 *		}	
 * \endcode
 *
 * \param index the high light tab index 
 */
#define SMP_TABWNDN_TABSWITCHED		0x0001

	/** @} */

/**
 * \brief The max tab the tab window support
 */
#define SMP_TABWND_TABCOUNT			4

	/**
	 * \name Window Member Functions
	 * @{
	 */

/**
 * \brief Add a tab window
 *
 * \param hWnd the tab window handle
 * \param bmp the icon id
 * \param title the child tab window title
 * \param hTab the  child window handle
 */
VOID SMP_TabWindow_AddTab(HWND hWnd, DWORD bmp, PCWSTR title, HWND hTab);

/**
 * \brief Remove a tab
 *
 * \param hWnd the tab window handle
 * \param id the child tab window id
 */
VOID SMP_TabWindow_RemoveTab(HWND hWnd, WID id);

/**
 * \brief Remove all the tabs
 *
 * \param hWnd the tab window handle
 */
VOID SMP_TabWindow_RemoveAllTabs(HWND hWnd);

/**
 * \brief Get the tab child window by index
 *
 * \param hWnd the tab window handle
 * \param index the child index
 * \return the child tab window handle
 */
HWND SMP_TabWindow_GetTabByIndex(HWND hWnd, Sint32 index);

/**
 * \brief Get the tab child window by id
 *
 * \param hWnd the tab window handle
 * \param id the child tab window id
 * \return the child tab window handle
 */
HWND SMP_TabWindow_GetTabByID(HWND hWnd, WID id);

/**
 * \brief Get the active tab index
 *
 * \param hWnd the tab window handle
 * \return the active tab index
 */
Sint32 SMP_TabWindow_GetActiveTabIndex(HWND hWnd);

/**
 * \brief Set the tab with id as the active tab.
 *
 * \param hWnd the tab window handle
 * \param id the child window handle
 */
VOID SMP_TabWindow_SetActiveTabByID(HWND hWnd, WID id);

/**
 * \brief Set the tab of index as the active tab
 *
 * \param hWnd the tab window handle
 * \param index the child window index
 */
VOID SMP_TabWindow_SetActiveTabByIndex(HWND hWnd, Sint32 index);

//≤‚ ‘tab¥∞ø⁄ «∑Òº§ªÓ
BOOL SMP_TabWindow_IsTabActive(HWND hWnd, HWND hTabWnd);

	/** @} */

	/**
	 * \name Window Procedure
	 * @{
	 */

/**
 * \brief The simple tab window procedure.	
 *
 * \param hWnd the tab window handle
 * \param Msg the window message
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of message process 
 */
LRESULT SMP_TabWindow_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

	/** @} */

/** @} end of smp_tabwindow */


#endif
