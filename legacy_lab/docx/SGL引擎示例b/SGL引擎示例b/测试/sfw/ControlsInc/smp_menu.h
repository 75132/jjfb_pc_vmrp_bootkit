#ifndef _SMP_MENU_H
#define _SMP_MENU_H

#include "window.h"

/**
 * \defgroup controls Common Controls
 * @{
 */

/**
  \defgroup smp_menu Simple Menu

  To work with the simple menu:
  - Setup the menu items
  - Call simple menu APIs to show the menu
  - Response the menu notify message

  Below are the different ways to setup menu items and show a popup menu:
  
  Sample Code 1:
  \code
  	SMP_Menu_ClearMenuItems();
  	SMP_Menu_SetMenuItem(0, ..., 1);
  	SMP_Menu_SetMenuItem(1, ..., 2);
  	SMP_Menu_SetMenuItem(2, ..., 3);
  	...

  	SMP_Menu_Popup(...);
  \endcode

  Sample Code 2:
  \code
  	//global menu items information
	static const DWORD miOptions [] =
	{
		STR_CONNECT,
		STR_EXIT,
		STR_REGISTER,
		...
	};

	//your code
  	SMP_Menu_ClearMenuItems();
	SMP_Menu_SetMenuItem2(0, miOptions, sizeof(miOptions)/sizeof(miOptions[0]));
	...
	
  	SMP_Menu_Popup(...);
  \endcode

  Sample code 3:
  \code
  	//when the popup is a simple menu without submenu, there is a wrapper for sample code 2

  	//global menu items information
	static const DWORD miOptions [] =
	{
		STR_CONNECT,
		STR_EXIT,
		STR_REGISTER,
		...
	};

	SMP_Menu_Popup2(..., miOptions, sizeof(miOptions)/sizeof(miOptions[0]));
  \endcode

  Response to the Menu notify message.

  \code
	case WM_COMMAND:
		WID id = LOWORD(wParam);
		WORD code = HIWORD(wParam); // is the menu item id

		if(id == "the menu id")
		{
			DWORD userdata = (DWORD)lParam; //if setted with the menu item
			switch(code)
			{
			case STR_CONNECT:
				//handle the notify event.
				break;
			case STR_EXIT:
				break;
			case STR_REGISTER:
				break;
			...
			}
		}	
  \endcode

  @ingroup controls
  @{
 */

/**
 * \brief Max Menu items in a menu. Reconfigure it as needed.
  * 全局菜单最大菜单项定义 
 */
#define SMP_MENU_MAX_MENUITEM	 40

	/**
	* \name Window Styles
	* @{
	*/

/**
 * \brief Indicates the (x, y) position in popup function is the menu (left, button)
 *///  显示位置定位 左下脚
#define SMP_MENUS_BOTTOMLEFT			0x0001L	      	/* 菜单位置弹在左下角 */

#if 1 //not implemented yet
/**
 * \brief Indicates the (x, y) position in popup function is the menu (right, button)
 */
#define SMP_MENUS_BOTTOMRIGHT			0x0002L
#endif

/**
 * \brief Indicates the (x, y) position in popup function is the menu (left, top)
 *////  左上角位置定位
#define SMP_MENUS_TOPLEFT				0x0004L

#if 0 //not implemented yet
/**
 * \brief Indicates the (x, y) position in popup function is the menu (right, top)
 */
#define SMP_MENUS_TOPRIGHT			0x0008L
#endif

/**
 * \brief Indicates that is a flat menu with fixed width.
 *
 * This style only used in SMP_Menu_PopupFlat to show a flat style menu.
 * The flat menu just one level, so do not use this style for a normal menu,
 * it is used by combo box and some other controls.
 * 平面采用固定宽度的菜单样式，适用于combo box 等，不要做普通菜单使用
 */
#define SMP_MENUS_FLAT				0x0010L

	/** @} */

/**
 * \brief Seperator menu item id.
 */
#define SMP_MENU_SEPERATOR		0xffff     // 菜单项分割条 

	/**
	 * \name Window Member Functions
	 * @{
	 */

/**
 *brief Clear the simple menu items.
 * 
 * 清除所有的菜单项 
 * items:		全局菜单指针 
*/
 VOID SMP_Menu_ClearMenuItems(VOID);

/**
 * \brief Set a menu item with a index.
 * 
 * \param index the menu item index
 * \param id the menu item id
 * \param str the string displayed for the menuitem
 * \param data the menuitem data
 * \param next the next menu item, -1 means have no next menuitem
 * 添加菜单项
 * items:		全局菜单指针 
 * index:		菜单项索引
 * itemID:		菜单项ID(一般用字符串ID作为菜单项ID)
 * str:			菜单项显示文本
 * data:		菜单项存放的数据
 * nextIndex:	下一个菜单项索引
 */
VOID SMP_Menu_SetMenuItem(int index, WID id, PCWSTR str, DWORD data, int next);

/**
 * \brief Setup menuitems with a string resource array.
 * 该函数将使用一个字符串数组来设置菜单标题
 * \param start the start index
 * 开始位置
 * \param items the string resource array
 * 字符串数组
 * \param size the str array size
 * 字符串数组大小
 */
VOID SMP_Menu_SetMenuItem2(int start, const DWORD* items, int size);

/**
 * \brief Set a sub menu.
 * 设定一个子菜单
 *
 * \param index the menu item index which will has the submenu
 * \param sub the sub menu items
 * 设置子菜单项
 * index:		拥有该子菜单的子菜单索引
 * sub:	        菜单项次级(上级菜单最后一个索引)
 */
VOID SMP_Menu_SetSubMenu(int index, int sub);

/**
 * \brief Check a menu item.
 * 
 * \param index the menu item index
 * \param check TRUE checked, FALSE unchecked
 * 设置指定菜单项勾选状态
 * index:		菜单项索引
 * checked      菜单选中状态
 */
VOID SMP_Menu_CheckMenuItem(int index, BOOL check);

/**
 * \brief Disable a menu item.
 *
 * \param index the menu item index
 * \param disable TRUE disalbe, FALSE enable
 * 设置某个菜单项是否能用
 * index:		 菜单项索引
 * disabled 菜单是否能用
 */
VOID SMP_Menu_DisableMenuItem(int index, BOOL disable); 

/**
 * \brief Popup the global menu with menuitems.
 *
 * \param id the Menu Window ID
 * \param style the Menu style
 * \param hParent the top-level/modal window handle
 * \param x the left position
 * \param y the top position
 * \param listener the window to handle the WM_COMMAND message
 * \return the global menu handle
 * 弹出菜单项
 * id 菜单窗口id
 * style 菜单样式
 * hparent 最上层窗口句柄
 * x 左位置 y 顶点位置
 * 监听该菜单的窗口，既响应该菜单WM_COMMEND消息的窗口
 */
HMENU SMP_Menu_Popup(WID id, DWORD style, HWND hParent, int x, int y, HWND listener);

/**
 * \brief Popup the global menu with menuitems.
 *
 * \param id the Menu Window ID
 * \param style the Menu style
 * \param hParent the top-level/modal window handle
 * \param x the left position
 * \param y the top position
 * \param items the Menu Item string array
 * \param size the menu item string array size
 * \param listener the window to handle the WM_COMMAND message
 * \return the global menu handle
 * \弹出全局
 * \id 菜单窗口id
 * \style 菜单样式
 * \hparent 最上层窗口句柄
 * \x 左位置 y 顶点位置
 * \菜单上的字符（通过这个找到该菜单，强大）
 * \监听该菜单的窗口，既响应该菜单WM_COMMEND消息的窗口
 * \该函数可以用来弹出一个快捷菜单
 */
HMENU SMP_Menu_Popup2(WID id, DWORD style, HWND hParent, int x, int y, const DWORD* items, int size, HWND listener);

/**
 * \brief Popup the global flat style menu with fixed width.
 *
 * \param id the Menu Window ID
 * \param style the Menu style
 * \param hParent the top-level/modal window handle
 * \param x the left position
 * \param y the top position
 * \param w the menu width
 * \param listener the window to handle the WM_COMMAND message
 * \return the global menu handle
 * 弹出菜单项
 * id 菜单窗口id
 * style 菜单样式
 * hparent 最上层窗口句柄
 * x 左位置 y 顶点位置
 * w 菜单宽度
 * 监听该菜单的窗口，既响应该菜单WM_COMMEND消息的窗口
 */
HMENU SMP_Menu_PopupFlat(WID id, DWORD style, HWND hParent, int x, int y, int w, HWND listener);

//菜单主题
typedef struct  {
	uint32 bg;		//背景
	uint32 border;	//边框
}T_SMP_MENU_THEME;

	/** @} */

	/**
	 * \name Window Procedure
	 * @{
	 */

/**
 * \brief The simple menu window procedure.
 *
 * \param hMenu the window handle
 * \param Msg the window message
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of message process 
 */
LRESULT SMP_Menu_WndProc(HMENU hMenu, UINT Msg, WPARAM wParam, LPARAM lParam);

	/** @} */

/** @} end of smp_menu */

/** @} end of controls */

#endif /* _SMP_MENU_H */

