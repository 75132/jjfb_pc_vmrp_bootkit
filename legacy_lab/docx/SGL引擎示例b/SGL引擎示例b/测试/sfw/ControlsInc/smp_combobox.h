#ifndef _SMP_COMBOBOX_H
#define _SMP_COMBOBOX_H

#include "window.h"

/**
  \defgroup smp_combobox Simple Combo Box

  To work with the simple combo box:
  - Create the combo box window
  - Set the combo box items and the selected item
  - Add to the parent window
  - Response to the notify message

  \code
  	//create the combo box
  	hCombo = SGL_CreateWindow(SMP_ComboBox_WndProc, ...);
  	SMP_ComboBox_SetItems(hCombo, ...);
  	SMP_ComboBox_SetSelectedItem(hCombo, ...);
  	SGL_AddChildWindow(hWnd, hCombo);

  	//response to the notify message
	case WM_COMMAND:
		WID id = LOWORD(wParam);
		WORD code = HIWORD(wParam);

		if(id == "the combo id" && code == SMP_COMBON_VALUECHANGED)
		{
			int index = (int)lParam; //current selected index
			...
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
 * \brief Combo box value changed notify message.
 *
 * \code
 *	case WM_COMMAND:
 *		WID id = LOWORD(wParam);
 *		WORD code = HIWORD(wParam);
 *
 *		if(id == "the combo id" && code == SMP_COMBON_VALUECHANGED)
 *		{
 *			int index = (int)lParam; // current selected index
 *			...
 *		}
 * \endcode
 *
 * \param index the current selected index
 */

#define SMP_COMBON_VALUECHANGED		0x0001

	/** @} */

	/**
	 * \name Window Member Functions
	 * @{
	 */

/**
 * \brief Set the combo box list size.
 *
 * \param hWnd the combo box window handle
 * \param size the list size
 * \return TRUE on success, FALSE otherwise
 */
BOOL SMP_ComboBox_SetSize(HWND hWnd, Sint32 size);

/**
 * \brief Get the combo box list size.
 *
 * \param hWnd the combo box window handle
 * \return the combo list size
 */
Sint32 SMP_ComboBox_GetSize(HWND hWnd);

/**
 * \brief Set the combo box list item information.
 *
 * \param hWnd the combo box window handle
 * \param index the list index
 * \param str the displayed string 
 * \param userdata the user specific data
 */
VOID SMP_ComboBox_SetItem(HWND hWnd, Sint32 index, PCWSTR str, DWORD userdata);

/**
 * \brief Get the combo box list item information.
 *
 * \param hWnd the combo box window handle
 * \param index the item index
 * \param[out] str the displayed string
 * \param[out] userdata the user specific data
 */
VOID SMP_ComboBox_GetItem(HWND hWnd, Sint32 index, PCWSTR * str, DWORD* userdata);

/**
 * \brief Set the combo box list items.
 *
 * This is a wraper function for SMP_ComboBox_SetSize/SMP_ComboBox_SetItem,
 * So if SMP_ComboBox_SetItems used please do not call SMP_ComboBox_SetSize/SMP_ComboBox_SetItem again.
 *
 * \param hWnd the combo box window handle
 * \param items the combo box list items array
 * \param size the items array size
 * \return TRUE on success, FALSE otherwise
 * \sa SMP_ComboBox_SetSize, SMP_ComboBox_SetItem
 */
BOOL SMP_ComboBox_SetItems(HWND hWnd, const DWORD* items, Sint32 size);

/**
 * \brief Select a item by it's index.
 *
 * \param hWnd the combo box window handle
 * \param index the list item index
 * \param redraw if redraw the combo box
 * \param notify if send notify message
 */
VOID SMP_ComboBox_SetSelectedItem(HWND hWnd, Sint32 index, BOOL redraw, BOOL notify);

/**
 * \brief Get the selected item index.
 *
 * \param hWnd the combo box window handle
 * \return the selected item index
 */
Sint32 SMP_ComboBox_GetSelectedItem(HWND hWnd);

	/** @} */

	/**
	 * \name Window Procedure
	 * @{
	 */

/**
 * \brief Combo box procedure
 *
 * \param hWnd the window handle
 * \param Msg the window message
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of message process 
 */
LRESULT SMP_ComboBox_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

	/** @} */
	
/** @} end of smp_progbar */

#endif
