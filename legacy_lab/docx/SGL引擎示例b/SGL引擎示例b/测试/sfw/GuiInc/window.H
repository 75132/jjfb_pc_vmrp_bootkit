/*
** window.h: the window header file, contains most simple gui library apis.
**
** Copyright (C) 2007-2008 SKY-MOBI AS.  All rights reserved.
**
** This file is part of the simple gui library.
** It may not be redistributed under any circumstances.
*/

#ifndef _SGL_WINDOWS_H
#define _SGL_WINDOWS_H

#include "types_i.h"
#include "gal_i.h"
#include "string.h"
#include "i18n.h"
#include "bmp.h"
#include "res_bmp.h"
#include "res_str.h"


/**
  @mainpage Simple GUI Libray (SGL)
  
  @section intro Introduction
  
  Simple GUI is not really a GUI framwork, it is just a simple window management module.
  For the GUI is what SGL intended to be, so the name kept here to remind us the
  things to do next. SGL interface is something like WIN32 APIs, so you will feel acquainted 
  when you have  the WIN32 SDK program experience.

  @section overview Overview

  @subsection typical_session A typical session
	- Initialize the Simple GUI Library // this is called by the framwork
	- Create the top-level window
	- Add top-level window to the DESKTOP

  \code
  	int InitApplication(VOID)
  	{
		HWND hWnd;

  		...

  		hWnd = SGL_CreateWindow(WND1_WndProc, ...);
  		SGL_AddChildWindow(HWND_DESKTOP, hWnd);

  		...
  	}
  \endcode

  @subsection create_tpwnd	Create your top-level window

	A SGL window is just a window procedure to response some window messages.

  \code
	LRESULT WND1_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		switch(Msg)
		{
		case WM_CREATE:
			//allocate the necessary memory for window private use and create some child windows.
			break;
			
		case WM_DESTROY:
			//free the private data if have any
			break;

		case WM_SHOW:
			//on this event you can update the window information of adjust the child window position.
			break;

		case WM_HIDE:
			//to do
			break;

		case WM_MOUSEDOWN:
			//handle mouse down event
			break;

		case WM_MOUSEUP:
			//handle mouse up event
			break;

		case WM_KEYDOWN:
			//handle the key down event
			break;
			
		case WM_KEYUP:
			//handle the key up event
			break;	
		
		case WM_COMMAND:
			//handle the child window notify message
			break;

		//other window messages
		... 
		}

		return 0;
	}
	
  \endcode
  
*/


#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */


/**
 * \brief Max windows count in the system.
 * 平台最大支持的窗口数
 */
#ifndef SGL_MAX_WINDOW_COUNT
#define SGL_MAX_WINDOW_COUNT		100  //限制系统窗口数
#endif

/**
 * \brief The key repeat start timer interval value
 */
#ifndef SGL_KEYREPEAT_START_INTERVAL
#define SGL_KEYREPEAT_START_INTERVAL	400	//长按多久开始重复
#endif

/**
 * \brief The key repeat timer interval value
 */
#ifndef SGL_KEYREPEAT_INTERVAL
#define SGL_KEYREPEAT_INTERVAL			80 //重复间隔
#endif 

/**
 * \响应双击的时间间隔
 */
#ifndef SGL_RECEIVE_DOUBCLICK_INTERVAL
#define SGL_RECEIVE_DOUBCLICK_INTERVAL	500 //500ms内再次点击则响应双击事件
#endif


/**
 * \defgroup windows Windows Manangement
 * @{
 */

	/**
	 * \defgroup wintypes Windows Data Types
	 * @ingroup Windows
	 * @{
	 */

/**
 * \brief A type definition for window procedure.
 * 窗口过程SGL_WndProc类型定义
 */
typedef LRESULT (*SGL_WndProc)(HWND, UINT, WPARAM, LPARAM);

/**
 * \brief A type definition for application events filter.
 */
typedef LRESULT (*SGL_AppEventFilter)(UINT, WPARAM, LPARAM);

/**
 * \brief A type definition for SGL window in the system.
 */
typedef struct SGL_Window
{
	/** the window identifier */
	WID id;
	 /** space align use */
	Uint16 reserve; 
	/** window sytle to keep some status of window such as WS_TRANSPARENT etc. */
	DWORD style;
	/** window rect in parent window coords */
	int left, top, width, height;
	/** background & foreground color */
	Uint32 bgcolor, fgcolor;
	/** window procedure */
	SGL_WndProc wndproc;
	/** handle to keep paretn window. the window is top level window when parent is HWND_DESKTOP */
	HWND parent; 
	/** the first child window hanlde */
	HWND child;
	/** the next sibling window handle */
	HWND next;
	/** the previous sibling window handle */
	HWND prev;
	/** the focus child window */
	HWND focus;
	/** a listener, when set SGL_NotifyParent will send message to it */
	HWND listener;
	/** userdata passed by create window */
	DWORD userdata;
	/** internal use for a window to hold window private data */
	DWORD windata;
}SGL_Window;

/**
 * 下面的宏用来操作窗口的属性
 * \brief A macro to return the window identifer of \a hWnd.
 * 窗口ID
 */
#define _WID(hWnd) \
	(hWnd)->id

/**
 * \brief A macro to return the window style of  \a hWnd.
 * 窗口样式
 */
#define _STYLE(hWnd) \
	(hWnd)->style

/**
 * \brief A macro to add the specific \a style to the window style of \a hWnd.
 * 设置窗口样式
 */
#define _SET_STYLE(hWnd, style) \
	(_STYLE(hWnd) |= (style))

/**
 * \brief A macro to clear the specific \a style from the window style of \a hWnd.
 * 清除窗口样式
 */
#define _CLR_STYLE(hWnd, style) \
	(_STYLE(hWnd) &= ~(style))

/**
 * \brief A macro to test if the window \a hWnd set any of the specific \a style.
 */
#define _IS_SET_ANY(hWnd, style) \
	(_STYLE(hWnd) & (style))

/**
 * \brief A macro to test if the window \a hWnd set all the specific \a style.
 */
#define _IS_SET_ALL(hWnd, style) \
	((_STYLE(hWnd) & (style)) == (style))

/**
 * \brief A macro to reverse the specific window style of \a hWnd.
 */
#define _REV_STYLE(hWnd, style) \
do{ \
if(_IS_SET_ANY(hWnd, style)) _CLR_STYLE(hWnd, style);\
else _SET_STYLE(hWnd, style);\
}while(0)

/**
 * \brief A macro to return the left positon of the window \a hWnd.
 */
#define _LEFT(hWnd) \
	(hWnd)->left

/**
 * \brief A macro to return the top positon of the window \a hWnd.
 */
#define _TOP(hWnd) \
	(hWnd)->top

/**
 * \brief A macro to return the width of the window \a hWnd.
 */
#define _WIDTH(hWnd) \
	(hWnd)->width

/**
 * \brief A macro to return the height of the window \a hWnd.
 */
#define _HEIGHT(hWnd) \
	(hWnd)->height

/**
 * \brief A macro to return the rect of the window \a hWnd.
 * 窗口矩形大小
 */
#define _WINRECT(hWnd, r) \
	(r).left = _LEFT(hWnd), \
	(r).top=_TOP(hWnd), \
	(r).width=_WIDTH(hWnd), \
	(r).height=_HEIGHT(hWnd)

/**
 * \brief A macro to return the background color of the window \a hWnd.
 * 窗口的背景颜色，好比画画的纸的颜色
 */
#define _BGCOLOR(hWnd) \
	(hWnd)->bgcolor

/**
 * \brief A macro to return the foreground color of the window \a hWnd.
 * 窗口的前景颜色，好比画在一张纸上的画笔颜色
 */
#define _FGCOLOR(hWnd) \
	(hWnd)->fgcolor

/**
 * \brief A macro to return the window procedure of the window \a hWnd.
 * 用来返回创建一个窗口
 */
#define _WNDPROC(hWnd) \
	(hWnd)->wndproc

/**
 * \brief A macro to return the user data of the window \a hWnd.
 * 窗口用户数据
 */
#define _USERDATA(hWnd) \
	(hWnd)->userdata

/**
 * \brief A macro to set the user \a data to the window \a hWnd.
 * 设置窗口用户数据
 */
#define _SET_USERDATA(hWnd, data) \
	_USERDATA(hWnd) = (DWORD)(data)

/**
 * \brief A macro to return the user data of the window \a hWnd with a specific type.
 * 获取窗口用户数据
 */
#define _GET_USERDATA(hWnd, type) \
	((type)_USERDATA(hWnd))

/**
 * \brief A macro to return the window data of the window \a hWnd.
 *  窗口相关属性数据 
 */
#define _WINDATA(hWnd) \
	((HWND)hWnd)->windata

/**
 * \brief A macro to set the window \a data to the window \a hWnd.
 * 设置窗口相关属性数据
 */
#define _SET_WINDATA(hWnd, data) \
	_WINDATA(hWnd) = (DWORD)(data)

/**
 * \brief A macro to return the window data of the window \a hWnd with a specific type.
 * 获取窗口相关属性数据
 */
#define _GET_WINDATA(hWnd, type) \
	((type)_WINDATA(hWnd))

/**
 * \brief A macro to return the window parent of the window \a hWnd.
 * 该窗口的父窗口
 */
#define _PARENT(hWnd) \
	(hWnd)->parent

/**
 * \brief A macro to return the next window of the window \a hWnd.
 * 该窗口的下一个兄弟窗口
 */
#define _NEXT(hWnd) \
	(hWnd)->next

/**
 * \brief A macro to return the previous(上一个) window of the window \a hWnd.
 * 该窗口的上一个兄弟窗口
 */
#define _PREV(hWnd) \
	(hWnd)->prev

/**
 * \brief A macro to return the first child window of the window \a hWnd.
 * 该窗口的第一个子窗口
 */
#define _CHILD(hWnd) \
	(hWnd)->child

/**
 * \brief A macro to return the focus child window of the top-level/modal window \a hWnd.
 * 窗口的获得焦点的子窗口(该窗口必须具有 top-level/modal 属性)
 */
#define _FOCUS(hWnd) \
	(hWnd)->focus

/**
 * \brief A macro to return the listener window of the window \a hWnd.
 * 该窗口的监听窗口
 */
#define _LISTENER(hWnd) \
	(hWnd)->listener

/**
 * \brief The handle of desktop.
 * 桌面窗口
 */
#define HWND_DESKTOP	(HWND)(0xFFFFFFFF)

/**
 * \brief A macro to check if the \a hWnd is ordinary window.
 * 判断窗口是否是普通窗口(既：不是无效或桌面窗口)
 */
#define IS_NORMAL_WINDOW(hWnd) \
	(NULL != hWnd && HWND_DESKTOP != hWnd)

/**
 * \判断一个窗口是不是顶级窗口
 * 2011/11/24 添加
 */
#define IS_TOPWND(hWnd) \
	(NULL != hWnd && _PARENT(hWnd) == HWND_DESKTOP)

/**
 * \brief A type definition for edit control information.
 * 编辑框控件结构
 */
typedef struct SGL_EditInfo
{
	/** title displayed on ime window */
	PCWSTR title; 
	/** the edit content buffer */
	PWSTR buffer;
	/** the buffer size */
	Uint32 size;
}SGL_EditInfo;

	/** @} end of wintypes */

	/**
	 * \defgroup winmsg Windows Messages
	 * @ingroup Windows
	 * @{
	 */

/**
 * \brief The mouse button down message.
 *
 * This message is posted to the window when the user presses down
 * the button of the mouse in the client area of the window.
 *
 * \code
 * case WM_MOUSEDOWN:
 *	int x = (int)wParam;
 *	int y = (int)lParam;
 * \endcode
 *
 * \param x the x position of the mouse in the window
 * \param y the y position of the mouse in the window
 * \注意他的位置信息是相对于窗口内部的？
 */// 鼠标按下产生的事件 段参数int x = (int)wParam;  长参数int y = (int)lParam;
#define WM_MOUSEDOWN		0x3001

/**
 * \brief The mouse button up message.
 *
 * This message is posted to the window when the user releases up
 * the button of the mouse in the client area of the window.
 *
 * \code
 * case WM_MOUSEUP:
 *	int x = (int)wParam;
 *	int y = (int)lParam;
 * \endcode
 *
 * \param x the x position of the mouse in the window
 * \param y the y position of the mouse in the window
 */
#define WM_MOUSEUP			0x3002

/**
 * \brief The mouse moved message.
 *
 * This message is posted to the window when the user moves the mouse
 * in the client area of the window.
 *
 * \code
 * case WM_MOUSEMOVE:
 *	int x = (int)wParam;
 *	int y = (int)lParam;
 * \endcode
 *
 * \param x the x position of the mouse in the window
 * \param y the y position of the mouse in the window
 */
#define WM_MOUSEMOVE		0x3003

/**
 * \brief User presses a key down.
 *
 * This message is posted to the current active window when the user 
 * presses a key down.
 *
 * \code
 * case WM_KEYDOWN:
 *	DWORD key = wParam;
 * \endcode
 *
 * \param key the key code for the pressed key
 */// 按键按下事件 DWORD key = wParam;
#define WM_KEYDOWN		0x3004

/**
 * \brief User releases up a key.
 *
 * This message is posted to the current active window when the user 
 * release a key up.
 *
 * \code
 * case WM_KEYUP:
 *	DWORD key = wParam;
 * \endcode
 *
 * \param key the key code for the released key 
 */
#define WM_KEYUP		0x3005

/**
 * \brief Indicates that the window has gained the input focus.
 *
 * This message is sent to the window procedure 
 * after the window gains the input focus.
 */// 当窗口获得焦点时  通知 父窗口
#define WM_SETFOCUS		0x3006

/**
 * \brief Indicates that the window has lost the input focus.
 *
 * This message is sent to the window procedure 
 * after the window lost the input focus.
 */// 失去焦点
#define WM_KILLFOCUS		0x3007

/**
 * \brief Indicates the window has been created, and gives you a chance to initialize your private objects.
 *
 * This messages is sent to the window after the window has been created.
 * You can initialize your own objects when you receive this message, 
 * and return zero to the system in order to indicates the success of initialization.
 * If you return non-zero to the system after handled this message, the created window
 * will be destroied immediately.
 */
#define WM_CREATE		0x3008

/**
 * \brief Indicates the window will be destroied.
 *
 * This message is sent to the window when SGL_DestroyWindow is calling. 
 * You can destroy your private objects
 * when receiving this message.
 */
#define WM_DESTROY		0x3009

/**
 * \brief Sent to the window if the window contains an invalid region.
 *
 * This message is sent to the window if the window contains an invalid region.
 *
 * \code 
 * case WM_PAINT:
 *	PRECT pRgn = (PRECT) wParam;
 * \endcode
 *
 * \param pRgn the invalid region, when NULL means the all the window area.
 * \return when return non-zero from the window procedure means the window will
 *	handle the it's children paint, and the sgl will not send the WM_PAINT for it's children.
 */
#define WM_PAINT		0x300A

/**
 * \brief Sent to the window when the window shown.
 * 
 * This message send to the window when the window became visible.
 */// 当窗口可见时 发送该消息
#define WM_SHOW		0x300B

/**
 * \brief Sent to the window when the window hiden.
 * 
 * This message sent to the window when a child window remove from it's parent,
 * or when a top level window overlap by another.
 */// 当子窗口从夫窗口上移除时 或当一个窗口别另一个覆盖
#define WM_HIDE			0x300C

/**
 * \brief The command message, indicates a notification message from child window.
 *
 * This message sent to the window when 
 * a child window has sent a notification message to the parent.
 *
 * \code 
 * case WM_COMMAND:
 *	WID id = LOWORD(wParam);
 *	WORD code = HIWORD(wParam);
 * \endcode
 *
 * \param id the child window id
 * \param code the child window notify message code
 */// 子窗口给父窗口发送通知消息后 
	//id ：既改子窗口ID
	//code :子窗口通知消息
#define WM_COMMAND		0x300D

/**
 * \brief Sent to the parent window when the menu show.
 *
 * This message sent to the parent window when a menu popuped
 * that give the parent a chance to handle something.
 *
 * \code
 * case WM_MENUSHOW:
 	HMENU hMenu = (HMENU)lParam;
 * \endcode
 *
 * \param hMenu the popup menu handle
 * \sa WM_MENUHIDE
 */
#define WM_MENUSHOW		0x3100

/**
 * \brief Sent to the parent window when menu closed.
 *
 * This message sent to the parent window when a menu closed
 * that give the parent a chance to clear something.
 *
 * \code
 * case WM_MENUHIDE:
 *	BOOL redraw = (BOOL)wParam;
 *	HMENU hMenu = (hMenu)lParam;
 * \endcode
 *
 * \param redraw the top window will be redrawed
 * \param hMenu the menu window handle
 * \sa WM_MENUSHOW
 */
#define WM_MENUHIDE		0x3101

/**
 * \brief Sent to parent window when mouse down on area out of the menu.
 *
 * NOT IMPLEMENTED YET
 */
#define WM_MENUMOUSEDOWN	0x3102

/**
 * \brief Sent to parent window when mouse up on area out of the menu.
 *
 * This message sent to parent window when mouse up out of the menu,
 * when return 0 will close the menu as normal, otherwise keep the menu.
 *
 * \code 
 * case WM_MENUMOUSEUP:
 *	int x = (int)wParam;
 *	int y = (int)lParam;
 * \endcode
 *
 * \param x the x position of the mouse in parent window
 * \param y the y position of the mouse in parent winodw
 */
#define WM_MENUMOUSEUP		0x3103

/**
 * \brief GUI framework send this message to get the submenu handle
 *	of a popup menu.
 *
 * The menu window must implements this menu.
 *
 * \code
 * case WM_GETSUBMENU:
 *	return hSubMenu;  //return the sub menu handle
 * \endcode
 *
 * \param[out] pMenu the pointer to the sub menu handle return to framework
 */
#define WM_GETSUBMENU		0x3104

/**
 * \brief Sent  to parent window when modal window shown.
 *
 * \code
 * case WM_MODALSHOW:
 *	HWND hDlg = (HWND)lParam;
 * \endcode
 *
 * \param hDlg the modal window handle
 * 模态对话框
 */
#define WM_MODALSHOW		0x3200

/**
 * \brief Sent to parent window when modal window closed.
 *
 * \code
 * case WM_MODALHIDE:
 *	HWND hDlg = (HWND)lParam;
 * \endcode
 *
 * \param hDlg the modal window handle
 */
#define WM_MODALHIDE		0x3201

/**
 * \brief Sent to parent window when mouse down out of the modal window.
 *
 * \code
 * case WM_MODALMOUSEDOWN:
 * 	int x = (int)wParam;
 *	int y = (int)lParam;
 * \endcode
 *
 * \param x the x position of the mouse in top window
 * \param y the y position of the mouse int top window
 */
#define WM_MODALMOUSEDOWN	0x3202

/**
 * \brief Sent to parent window when mouse up out of the modal window.
 *
 * \code
 * case WM_MODALMOUSEUP:
 * 	int x = (int)wParam;
 *	int y = (int)lParam;
 * \endcode
 *
 * \param x the x position of the mouse in top window
 * \param y the y position of the mouse int top window
 */
#define WM_MODALMOUSEUP		0x3203

/**
 * \brief GUI framework send this message to the edit 
 *	window to get edit infomation.
 *
 * The edit window must implements this messsage.
 *
 * \code 
 * case WM_GETEDITINFO:
 *		SGL_EditInfo* info = (SGL_EditInfo*)wParam;
 *		info->title = GET_TITLE(hEdit);
 *		info->buffer = GET_BUFFER(hEdit);
 *		info->size = GET_MAXSIZE(hEdit);
 * \endcode
 *
 * \param[out] info the edit information return to framework
 */
#define WM_GETEDITINFO		0x3300

/**
 * \brief Sent to the edit window when input finished.
 *
 * The edit window must implements to receive the content from system IME.
 *
 * \code
 * case WM_SETEDITTEXT:
 *	PCWSTR pContent = (PCWSTR)wParam;
 * \endcode
 *
 * \param pContent the text content receive from system IME
 */
#define WM_SETEDITTEXT		0x3301

/**
 * \brief Sent to the top-level or modal window to initialize the default focus child.
 */
#define WM_INITFOCUS		0x3302

/**
 * \brief The key down repeat event
 *
 * To enable this event should define ENABLE_SGL_KEYREPEAT
 */
#define WM_KEYDOWNREPEAT		0x3303

//绘制窗口背景在此消息
#define WM_PAINT_BG				0x3304

/**
 * \窗口尺寸改变时将收到此消息
 *
 * \code
 * case WM_MODALMOUSEUP:
 * 	int w = (int)wParam;
 *	int h = (int)lParam;
 * \endcode
 *
 * \param w 窗口新宽度
 * \param h 窗口新高度
 */
//
#define WM_SIZE					0x3305

/**
 * \窗口位置改变将收到此消息
 *
 * \code
 * case WM_MODALMOUSEUP:
 * 	int x = (int)wParam;
 *	int y = (int)lParam;
 * \endcode
 *
 * \param x 窗口新位置
 * \param y 窗口新位置
 */
#define WM_MOVE				0x3306

/**
 * \brief The first user-defined message.
 * 从0x3800 到 0xEFFF 为用户自定义消息
 * SGL reserved the range from 0x3800 to 0xEFFF for user-defined messages.
 * WM_USER is the first user-defined message you can use it by your own.
 */
#define WM_USER			    0x3800

/**
 * \后台运行消息
 */
#define WM_BACKRUN		    0x3801  //后台消息

/**
 * \浏览文件消息
 */
#define WM_GETFILE			0x3802	//浏览文件

/**
 * \双击触屏消息
 */
#define WM_MOUSEDOUBCLICK	0x3803

/**
 * \用户在自带字库/斯凯字库之间进行了切换
 * \可发送此消息通知窗口进行尺寸调整
 */
#define WM_APPLY_FONT		0x3804

/**
 * \更换了主题，发送此消息至顶级窗口，通知改变样式
 */
#define WM_APPLY_THEME		0x3805

/**
 * \TextWnd 按钮确认事件
 */
#define WM_TEXTWND			0x3806

/**
 * \调用本地编辑框
 */
#define WM_PLATEDIT_SHOW	0x3807

/**
 * \隐藏本地编辑框
 */
#define WM_PLATEDIT_HIDE	0x3808

//通知窗口画背景
#define WM_DRAWBG			0x3809

//后台运行返回消息
#define WM_BACKRUN_BACK		0x380a

/**
 * \brief The last user message.
 *
 * User Message Must not exceed this value
 */
#define WM_LASTUSERMSG		0xEFFF

	/** @} end of winmsg */

	/**
	 * \defgroup winstyle Windows Styles
	 * @ingroup Windows
	 * @{
	 */

/**
 * \brief Indicates it's a top-level window. 
 *
 * \note This style is mintained by framework. Application Engineer should not 
 * set or clear this style for a window
 */
#define WS_OVERLAPPED		0x80000000L

/**
 * \brief Indicates it's a child winodw. 
 *
 * \note This style is mintained by framework. Application Engineer should not 
 * set or clear this style for a window.
 */
#define WS_CHILD			0x40000000L

/**
 * \brief Indicates a popup menu.
 *
 * This style not only used to indicates a popup menu. When a Menu popuped on a top/modal window,
 * The parent window will get the style (WS_INACTIVE | WS_POPUP). So a popup menu does not only 
 * decided by style WS_POPUP, the WS_INACTIVE style should be checked also.
 *
 * \code
 *	if(!_IS_SET_ANY(hWnd, WS_INACTIVE) && _IS_SET_ANY(hWnd, WS_POPUP))
 *	{
 *		//is a menu 
 *	}
 * \endcode
 *
 * \note This style is mintained by framework. Application Engineer should not 
 * set or clear this style for a window.
 */
#define WS_POPUP			0x20000000L

/**
 * \brief Indicates a modal window.
 *
 * This style not only used to indicates a modal window. When a modal shown on a top window,
 * The parent window will get the style (WS_INACTIVE | WS_MODAL). So a modal window does not only 
 * decided by style WS_MODAL, the WS_INACTIVE style should be checked also.
 *
 * \code
 *	if(!_IS_SET_ANY(hWnd, WS_INACTIVE) && _IS_SET_ANY(hWnd, WS_MODAL))
 *	{
 *		//is a modal window 
 *	}
 * \endcode
 *
 * \note This style is mintained by framework. Application Engineer should not 
 * set or clear this style for a window.
 */
#define WS_MODAL			0x10000000L

/**
 * \brief Indicates the window is visible.
 *
 * \note This style is mintained by framework. Application Engineer should not 
 * set or clear this style for a window.
 */
#define WS_VISIBLE			0x08000000L

/**
 * \brief Indicates a top-lelel/modal window is inactive.
 *
 * A inactive window means a popup menu shown on the top-level/modal window.
 * or the modal window shown on the top-level window.
 * When a popup menu shown the parent will gets (WS_INACTIVE | WS_POPUP).
 * When a modal window shown the parent will gets (WS_INACTIVE | WS_MODAL).
 *
 * \note This style is mintained by framework. Application Engineer should not 
 * set or clear this style for a window.
 */
#define WS_INACTIVE			0x04000000L

/**
 * \brief Indicates the window get the input focus.
 *
 * \note This style is mintained by framework. Application Engineer should not 
 * set or clear this style for a window.
 */
#define WS_FOCUSED			0x02000000L

/**
 * \这个样式 告诉SGL不需要绘制这个窗口的背景
 * 
 * GUI will not draw the background for this window on event WM_PAINT.
 */
#define WS_TRANSPARENT	0x01000000L

/**
 * \brief Indicates the window can stop the focus.
 *
 * When child window created with this style, SGL_FocusNext/SGL_FocusPrev
 * can stop the focus on the child window.
 */
#define WS_TABSTOP			0x00800000L

/**
 * \brief Indicates the sgl should focus the child window except the window itself
 *
 * This style must used with WS_TABSTOP, when WS_FOCUSCHILD setted the 
 * child window will get the focus.
 */
#define WS_FOCUSCHILD		0x00400000L

/**
 * \brief Restore the focus when shown again.
 *
 * You can create a top-level window with this style.
 * The window will restore the focus window that has the focus before hiden.
 */
#define WS_KEEPFOCUS		0x00200000L

/**
 * \brief Indicates the framework will not redraw the top level window
 * when this child window removed.
 *
 * \note This style just only used in Menu and Modal window.
 */
#define WS_NOREDRAW		0x00100000L

/**
 * \brief Indicats the mouse has pressed on this window.
 *
 * This style maybe used in some controls which handle mouse down/up events as a pairs.
 */
#define WS_PRESSED			0x00080000L

/**
 * \brief Indicates the key has pressed on this window.
 *
 * This style maybe used in some cotrols which handle key down/up events as a pairs.
 */
#define WS_KEYDOWN			0x00040000L

/**
 * \此样式告诉SGL这个窗口的背景是半透明的
 * 
 * SGL将会进行复杂的处理背景重绘，会影响效率最好别用
 */
#define WS_TRANSLUCENT		0x00020000L

/**
 * \brief Indicates the window has a border(边框).
 *
 * This style should implemented(实现) by the controls.
 */
#define WS_BORDER			0x00010000L


/**
 * \brief Indicates the bits of system style.
 */
#define WS_MASK_SYSTEM	0xffff0000L

/**
 * \brief Indicates the bits of user implemented window style.
 */
#define WS_MASK_USER		0x0000ffffL

	/**
	 * \defgroup editstyle Edit input Styles
	 *
	 * For edit window style used by the framework, so their declaration here.
	 * These style should be used exclusively. So one edit can just set one or non of these style. 
	 *
	 * @ingroup winstyle
	 * @{
	 */

/**
 * \brief Edit window style number input.
 */
#define ES_NUM		0x00008000L

/**
 * \brief Edit window style password input.
 */
#define ES_PWD		0x00004000L

/**
 * \brief Edit window style characters inputs.
 */
#define ES_ALPHA	0x00002000L

	/** @} end of editstyle */

	/** @} end of winstyle */

	/**
	 * \defgroup winapi Windows APIs
	 * @ingroup Windows
	 * @{
	 */

/**
 * \brief Initialize the SKY GUI Library, this function must called before run application code.
 *
 * \sa SGL_Terminate
 */
VOID GUIAPI SGL_Initialize(VOID);

/**
 * \brief Terminate the SKY GUI Library, only called when exit the application.
 *
 * \sa SGL_Initialize
 */
VOID GUIAPI	SGL_Terminate(VOID);

//////// 2011.12.03 添加 ///////////////////////////////
/*
 * 获取当前显示的顶级窗口
 * 处于窗口链表第一个
 */
HWND SGL_GetCurTopWnd(VOID);

/*
 * 获取全局的弹出式菜单句柄
 * 系统只有一个弹出式菜单, 也就是说不能同时弹出两个弹出式菜单
 * 多级菜单是一个弹出式菜单
 */
HMENU SGL_GetPopMenu(void);

/*
 * 设置指定菜单句柄为弹出式菜单
 * 参数:
 * hWnd:	要设置的菜单的句柄
 * 返回值:	无
 */
void SGL_SetPopMenu(HMENU hWnd);

/* 
 * 获取全局模态对话框句柄
 * 系统只有一个模态窗口, 也就是说不通同时弹出两个msgbox
 * 返回值:	返回模态窗口的句柄
 */
HWND SGL_GetModalDlg(void);

/*
 * 设置指定窗口为模块窗口
 */
void SGL_SetModalDlg(HWND hWnd);

/*
 * 获取当前系统活动的输入窗口
 * 这个函数可以获取当前有输入焦点的那个EDIT控件的句柄
 */
HWND SGL_GetActiveEdit(void);

/*
 * 设置当前的窗口为活动输入窗口
 * 这个函数可以是当前窗口接收到本地输入框返回的消息
 */
void SGL_SetActiveEdit(HWND hWnd);

/*
 * 获取当前本地输入框句柄
 */
int32 SGL_GetPlatEdit(void);

/*
 * 保存当前本地输入框句柄
 */
void SGL_SetPlatEdit(int32 handle);
//////// 2011.12.03 添加 ///////////////////////////////

/**
 * \brief Suspend all the gdi function.
 *
 * This function will suspend all the gdi function that cause graphics function 
 * not work and output to the screen. This function should called with 
 * SGL_UnsuspendDrawing as pairs.
 *
 * \code 
 * 	SGL_SuspendDrawing();
 *	SGL_UpdateWindow(hWnd); //after suspend, hWnd will not get the WM_PAINT event
 * \endcode
 * \调用这个函数后，hWnd将不会收到 WM_PAINT 消息
 * \sa SGL_UnsuspendingDrawing
 */
VOID GUIAPI SGL_SuspendDrawing(VOID);

/**
 * \brief Unsuspend the gdi function.
 *
 * This should be called after a SGL_SuspendDrawing.
 * 必须与 SGL_SuspendDrawing 成对使用
 * \sa SGL_SuspendDrawing
 */
VOID GUIAPI SGL_UnsuspendDrawing(VOID);

/**
 * \brief Get the drawing lock value.
 *
 * \return the value of drawing lock
 */
BOOL GUIAPI SGL_IsSuspendDrawing(VOID);

/**
 * \brief Get the screeen or desktop size.
 *
 * \param pSize output the size of desktop
 */
//VOID GUIAPI SGL_GetScreenSize(PSIZE pSize);

/**
 * \brief Get current system font.
 *
 * \return current system font
 */
//HFONT GUIAPI SGL_GetSystemFont(VOID);

/**
 * \brief Set the system font.
 *
 * \param font the new font
 */
//VOID GUIAPI SGL_SetSystemFont(HFONT font);

/**
 * \brief Find child window with a spec identifier in a parent window.
 *
 * \param hWnd the parent window handle
 * \param id the child window id
 * \return the handle of the first child window with the spec identifier
 */
HWND GUIAPI SGL_FindChildWindow(HWND hWnd, WID id);

/**
 * \brief Find child window with (x,y) in parent coodrs.
 *
 * \param hWnd the window handler
 * \param x the left position
 * \param y the right position
 * \return the found window handler, NULL otherwise
 */
HWND GUIAPI SGL_FindChildWindowByPosition(HWND hWnd, int x, int y);

/**
 * \brief Retrieves the next control in a window.
 *
 * This function retrives the next child of the specified 
 * window \a hWnd. If you pass NULL for the argument
 * of \a hChild, the function will return the first child of the window.
 *
 * \param hWnd The handle to the window.
 * \param hChild The handle to a child of the window.
 * \return The handle of the next child. If you pass NULL for
 *		the argument of \a hChild, it will return the first child 
 *		of the window. When the child you passed is the last one, 
 *		this function will return NULL. If hWnd is not the parent
 *		of hChild, this function will return NULL.
 *  该函数用来获取当前窗口的下一个子窗口
 *  用法:
 *  1.返回当前窗口的下一个子窗口
 *		SGL_GetNextChild(hWnd, hChildWnd);
 *		hChildWnd必须是hWnd的子窗口, 结果返回hChildWnd的下一个兄弟窗口
 *	2.返回当前窗口的第一个子窗口
 *		SGL_GetNextChild(hWnd, NULL);
 *	3.返回屏幕最上层活动窗口
 *		SGL_GetNextChild(HWND_DESKTOP, NULL);
 */
HWND GUIAPI SGL_GetNextChild (HWND hWnd, HWND hChild);

/** 
 * \brief Get the top level window of the specific child window.
 *
 * \param hWnd the child window handle
 * \return the top level window
 * 获取指定窗口的顶层窗口(所谓顶层窗口就是整个屏幕显示的大窗口, 是HWND_DESKTOP的子窗口, 不是控件窗口)
 * 参数:
 * hWnd:	子窗口句柄
 */
HWND GUIAPI SGL_GetTopWindow(HWND hWnd);

/**
 * \brief Converts the window coordinates of a point to screen coordinates.
 *
 * This function converts the window coordinates of the specified point \a (*x,*y)
 * in the specified window \a hWnd to the screen coordinates.
 *
 * \param hWnd The handle to the window.
 * \param[in,out] x The pointer to the x coordinate.
 * \param[in,out] y The pointer to the y coordinate.
 * 
 * 把窗口坐标系转换为屏幕坐标系
 * 窗口的位置信息是相对于父窗口的相对位置, 改函数把子窗口的位置坐标转换成屏幕的绝对坐标
 * 此函数将子窗口相对于父窗口的位置转换成子窗口相对于屏幕坐标系的位置，
 * 如果要对子窗口进行额外的绘制工作必须调用此函数获得子窗口相对于屏幕
 * 坐标系的位置
 * hWnd与x,y值的关系，如果hWnd为他本身，则x=y=0,即该窗口的坐标位置
 * \sa SGl_ScreenToWindow
 */
VOID GUIAPI SGL_WindowToScreen (HWND hWnd, int* x, int* y);

/**
 * \brief Converts the screen coordinates of a point to window coordinates.
 *
 * This function converts the screen coordinates of the specified point \a (*x,*y)
 * to the window coordinates of the specfied window \a hWnd.
 *
 * \param hWnd The handle to the window.
 * \param[in,out] x The pointer to the x coordinate.
 * \param[in,out] y The pointer to the y coordinate.
 * 
 * 把屏幕坐标系转换为窗口坐标系
 * 窗口的位置信息是相对于父窗口的相对位置, 改函数把屏幕的绝对坐标转换成窗口的位置坐标
 * \sa SGL_WindowToScreen
 */
VOID GUIAPI SGL_ScreenToWindow (HWND hWnd, int* x, int* y);

/**
 * \brief Create a SGL window.
 *
 * \param wndproc the window procedure
 * \param x the left position of the window in parent coords
 * \param y the top postion of the window in parent coords
 * \param w the width of the window
 * \param h the height of the window
 * \param id the window id
 * \param style the the window style
 * \param userdata the user specfic data
 * \return 
 *	- the handle of the window when success
 *	- NULL when failed
 */
HWND GUIAPI SGL_CreateWindow(SGL_WndProc wndproc, int x, int y, int w, int h, WID id, DWORD style, DWORD userdata);

/**
 * \brief Destroy the window create by SGL_CreateWindow.
 *
 * \param hWnd the handle of the window to be destroyed
 * \return TRUE when success, otherwise FALSE
 */
BOOL GUIAPI SGL_DestroyWindow(HWND hWnd);
/**
 * \brief Send a message to a normal window.
 *
 * \param hWnd the window handle
 * \param Msg the window message
 * \param wParam the first param
 * \param lParam the second param
 * \return the result of message process
 */
LRESULT GUIAPI SGL_SendMessage(HWND hWnd, UINT Msg, DWORD wParam, DWORD lParam);

/**
 * \brief Send a message to the window \a hWnd and all it's children.
 *
 * \param hWnd the window handle
 * \param Msg the window message
 * \param wParam the first param
 * \param lParam the second param
 * \param hExcept the except window will not get the message
 * \return the result of message process
 */
VOID GUIAPI SGL_BroadcastMessageEx(HWND hWnd, UINT Msg, DWORD wParam, DWORD lParam, HWND hExcept);

/**
 * \brief Send a message to the window \a hWnd and all it's children.
 *
 * \param hWnd the window handle
 * \param Msg the window message
 * \param wParam the first param
 * \param lParam the second param
 * \return the result of message process
 */
#define SGL_BroadcastMessage(hWnd, Msg, wParam, lParam) \
	SGL_BroadcastMessageEx(hWnd, Msg, wParam, lParam, NULL)

/**
 * \brief Send notify message to a window.
 *
 * \param hWnd the window handle
 * \param id the notify window id 
 * \param code the notify code
 * \param userdata the user data sent with the message
 * \return the result of the message process
 */
#define SGL_SendNotifyMessage(hWnd, id, code, userdata) \
	SGL_SendMessage(hWnd, WM_COMMAND, (WPARAM)MAKELONG(id, code), (LPARAM)userdata)

/**
 * \brief Send notify message to it's parent.
 *
 * \param hWnd the window handle
 * \param code the notify code
 * \param userdata the user data sent with the message
 * \return the result of the message process
 */
#define  SGL_NotifyParent(hWnd, code, userdata) \
	SGL_SendNotifyMessage((_LISTENER(hWnd)? _LISTENER(hWnd) : _PARENT(hWnd)), _WID(hWnd), code, userdata)

/**
 * \brief Check window is visible.
 *
 * \param hWnd the window handle
 * \return TRUE when visible, FALSE otherwise
 */
#define SGL_IsWindowVisible(hWnd) \
	(IS_NORMAL_WINDOW(hWnd) && _IS_SET_ANY(hWnd, WS_VISIBLE))

/**
 * \brief Check window is active.
 *
 * \return TRUE when NO popup menu or modal window showed on the top-level window
 */
#define SGL_IsWindowActive(hWnd) \
	(!_IS_SET_ANY(hWnd, WS_INACTIVE))

//测试一个窗口句柄 是不是 指定窗口类型的实例（窗口过程函数匹配）
#define SGL_INSTANCEOF(hWnd, wndproc) \
	(IS_NORMAL_WINDOW(hWnd) && _WNDPROC(hWnd)==wndproc)


/**
 * \brief Update the specific window.
 *
 * \param hWnd the handle of the window to be updated
 * \param pClipRect the clip rect
 * 刷新指定窗口
 * 参数:
 * hWnd:	需要刷新的窗口句柄
 * pClipRect:	需要刷新的区域
 */
VOID GUIAPI SGL_UpdateWindowEx(HWND hWnd, PRECT pClipRect);

/**
 * \brief Update the window.
 *
 * \param hWnd the handle of the window to be updated
 * 刷新指定窗口(全屏刷新)
 */
#define SGL_UpdateWindow(hWnd)	\
	SGL_UpdateWindowEx(hWnd, NULL)


// 只刷新该窗口 [4/9/2012 JianbinZhu]
VOID GUIAPI SGL_UpdateWindowOnly(HWND hWnd, PRECT pClipRect);

/**
 * \brief Paint a window with a clip region.
 *
 * \param hWnd the window handle
 * \param pClipRect the the clip region
 * \note Normally you should just use SGL_UpdateWindowEx or SGL_UpdateWindow, 
 *	and SGL_PaintWindowEx just paint the window but not flush to the screen, also it will not
 *	check the drawing lock and the window visible status.
 *
 * \sa SGL_UpdateWindowEx, SGL_UpdateWindow
 */
VOID GUIAPI SGL_PaintWindowEx(HWND hWnd, PRECT pClipRect);

/**
 * \brief Paint whole of the window
 *
 * \param hWnd the window handle
 * \note Same as SGL_PaintWindowEx just without clip region. 
 *	
 * \sa SGL_PaintWindowEx, SGL_UpdateWindowEx, SGL_UpdateWindow
 */
VOID GUIAPI SGL_PaintWindow(HWND hWnd);

//  [4/9/2012 JianbinZhu]
//只绘制该窗口，不会递归绘制其子窗口，特殊需求
VOID GUIAPI SGL_PaintWindowOnly(HWND hWnd, PRECT pClipRect);

/**
 * \brief Add a window to another window as it's child.
 *
 * \param hParent the parent window handle, when HWND_DESKTOP the hChild will became top-level window
 * \param hChild the child window handle
 * \return TRUE when success, otherwise FALSE
 * 添加窗口作为指定父窗口的子窗口
 * 参数:
 * hParentWnd:	父窗口句柄
 * hChildWnd:	要添加的子窗口句柄
 */
BOOL GUIAPI SGL_AddChildWindow(HWND hParent, HWND hChild);

/**
 * \brief Remove a child window from it's parent window.
 *
 * \param hChild the child window handle
 * \return TRUE when success, otherwise FALSE
 * 从父窗口节点删除该子窗口节点
 * 参数:
 * hChildWnd:	子窗口句柄
 */
BOOL GUIAPI SGL_RemoveChildWindow(HWND hChild);

VOID GUIAPI SGL_RemoveAllChilds(HWND hWnd);

/**
 * \brief The platform will call this function to push events to the simple gui library
 *
 * \param Msg the window message
 * \param wParam the first message parameter
 * \param lParam the second message parameter
 */
VOID GUIAPI SGL_DispatchEvents(UINT Msg, WPARAM wParam, LPARAM lParam);

/**
 * \brief Set hWnd as the focus window.
 *
 * \param hParent the parent window, this window must be top-level/modal window
 * \param hChild the child window in \a hParent window
 *
 * \param hWnd the focus child window
 * 设置指定窗口为焦点窗口
 * 参数:
 * hWnd: 要设置焦点窗口的句柄
 */
VOID GUIAPI SGL_SetFocusWindow(HWND hParent, HWND hChild);

/**
 * \检测 hChild 的父系窗口存不存在 hParent
 * \存在返回 TRUE
 */
BOOL GUIAPI SGL_IsChildOf(HWND hParent, HWND hChild);

/**
 * \brief Get the focus window.
 *
 * \param hWnd the top-level/modal window
 * \return the focus child window handle
 *
 * \return current focus window handle
 * 获取当前焦点窗口句柄
 * 返回值: 当前聚焦的窗口句柄
 */
HWND GUIAPI SGL_GetFocusWindow(HWND hWnd);

/**
 * \brief Find a child window between hHead and hEnd that can get the focus.
 *
 * \param hHead the first window handle
 * \param hEnd the one item past last window handle
 * \param next TRUE to search for next, FALSE to previous
 * 切换指定窗口的子窗体焦点
 * 参数:
 * hWnd:	父窗口的句柄
 *hEnd： 最后一个子窗口
 * next:	TRUE 下一个焦点, FALSE 前一个焦点
 */
HWND GUIAPI SGL_FindFocusNext(HWND hHead, HWND hEnd, BOOL next);

/**
 * \brief Find the next focus child for a top-level window
 *
 * \param hWnd the window handle, should be top window or modal window
 * \param next 
 *	- TRUE to focus the next window
 *	- FALSE to focus the previous window
 * \return the focus child handle
 */
HWND GUIAPI SGL_FindFocus4TopWnd(HWND hWnd, BOOL next);

/**
 * \brief Focus the next child window in window hWnd.
 *
 * \param hWnd the window handle, should be top window or modal window
 * \param next 
 *	- TRUE to focus the next window
 *	- FALSE to focus the previous window
 * 切换指定窗口的子窗体焦点
 * 参数:
 * hWnd:	父窗口的句柄
 * next:	TRUE 下一个焦点, FALSE 前一个焦点
 */
VOID GUIAPI SGL_FocusNext(HWND hWnd, BOOL next);


/////鼠标点击的这个获得焦点
//VOID GUIAPI SGL_FocusCurrent(HWND hWnd);

/**
 * \brief Set mouse tracker window.
 *
 * \param hWnd the new mouse tracker window
 */
VOID GUIAPI SGL_SetMouseTracker(HWND hWnd);

/**
 * \brief Get current mouse tracker window.
 *
 * \return the mouse tracker window
 */
HWND GUIAPI SGL_GetMouseTracker(VOID);

/**
 * \brief Bring a child window to the top in the z-order of it's parent.
 *
 * \param hWnd the child window handle
 * \return TRUE on success, otherwise FALSE
 * 把指定窗口移动到其父窗口的第一个子窗口
 * hWnd:	窗口的句柄
 */
BOOL GUIAPI SGL_BringWindowToTop(HWND hWnd);

/**
 * \brief Close the popup menu.
 *
 * \param hMenu the men handle
 * \param redraw force redraw
 */
VOID GUIAPI SGL_CloseMenu(HMENU hMenu, BOOL redraw);

/**
 * \brief Popup the menu window.
 *
 * \param hMenu the menu window handle
 * \param x the left position of the menu
 * \param y the top position of the menu
 * \param hParent the parent window handle which will receive message from menu
 * \return TRUE on success, otherwise FALSE
 */
BOOL GUIAPI SGL_PopupMenu(HMENU hMenu, int x, int y, HWND hParent);

/**
 * \brief Select a menu item.
 *
 * \param hMenu the menu handle
 * \param item the select menuitem id
 * \param userdata data
 */
VOID GUIAPI SGL_SelectMenuItem(HMENU hMenu, WID item, DWORD userdata);

/**
 * \brief Get current popup window handle.
 *
 * \return the popup menu handle, NULL when there is no popup window.
 */
HMENU GUIAPI SGL_GetPopupMenu(VOID);

/**
 * \brief Show a window in modal.
 *
 * \param hDlg the window handle
 * \param hParent the parent window handle, should not be NULL
 * \return TRUE on success, otherwise FALSE
 */
BOOL GUIAPI SGL_DoModal(HWND hDlg, HWND hParent);

/**
 * \brief Close the modal window with return code.
 *
 * \param hDlg the window handle
 * \param code the return code
 * \return TRUE on success, otherwise FALSE
 */
BOOL GUIAPI SGL_ExitModal(HWND hDlg);

/**
 * \brief Get current modal window.
 *
 * 获取当前的模态窗口
 * \return the modal window handle, NULL when there is no modal window.
 */
HWND GUIAPI SGL_GetModalWindow(VOID);

/**
 * \brief Call platform input for the edit.
 *
 * \param hEdit the target edit window handle
 */
VOID GUIAPI SGL_TextInput(HWND hEdit);

/**
 * \brief Set a application event filter.
 * 
 * \param filter the filter callback
 * \return the old handler
 */
LRESULT GUIAPI SGL_SetAppEventFilter(SGL_AppEventFilter filter);


//定义Toast显示时间长度
#define TOAST_SHOW_LONG		3000
#define TOAST_SHOW_SHORT	2000
#define TOAST_SHOW_EVER		-1		//不会自动消失，需要手动调用SGL_ToastHide
/**
 * \显示一个弹出信息
 * \text:内容
 * \time:时间
 */
VOID GUIAPI SGL_ToastShow(PCWSTR text, int time);

/**
 * \隐藏一个弹出信息
 */
VOID GUIAPI SGL_ToastHide(VOID);

/**
 * 半透明屏幕
 * \参数：alpha 透明度值，0~255
 */
VOID GUIAPI SGL_EffScreen(Uint32 color);

/**
 * 取消屏幕半透明效果
 */
VOID GUIAPI SGL_UnEffScreen(VOID);

/**
 * 2011.12.24 添加，刷新屏幕区域
 * List 刷新单行的时候要用到，以后将废弃
 */
VOID GUIAPI SGL_FlushRegion(int x, int y, int w, int h);

#ifdef ENABLE_SGL_KEYREPEAT		
/**
 * \brief Stop the key down repeat timer
 */
VOID GUIAPI SGL_StopKeyRepeat(VOID);

#endif


/**
 * \设置窗口尺寸，窗口将收到 WM_SIZE 消息
 * [3/29/2012 JianbinZhu]
 */
VOID GUIAPI SGL_SetWindowSize(HWND hWnd, int w, int h);

/**
 * \设置窗口位置，窗口将收到 WM_MOVE 消息
 */
VOID GUIAPI SGL_SetWindowPosition(HWND hWnd, int x, int y);

//检测系统菜单是否弹出
#define SGL_IS_MENUSHOW \
	(NULL != SGL_GetPopupMenu())

//检测模态窗口是否弹出
#define SGL_IS_MODALSHOW \
	(NULL != SGL_GetModalWindow())

//[2012/5/26] 添加，获取鼠标最后一次点下位置
PPOINT GUIAPI SGL_GetLastMouseDown(VOID);

	/** @} end of winapi */

/** @} end of windows */


/**
 * \brief The window procdure of SGL WINDOW, it is the most generic window of SGL
 *
 * \param hWnd the window handle
 * \param Msg the window message
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of message process
 */
LRESULT GUIAPI SGL_Window_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

/**
 * \brief Application entry called by the framework.
 *
 * After gui and some other module initialize, framework call this function
 * to give the application the chance to initialize the application. So this function
 * must be implemented by the user.
 *
 * \return MR_SUCCESS on success, MR_FAILED otherwise
 * \sa mrc_init
 */
int InitApplication(VOID); 

/** 
 * \brief Exit function called by the framework before Application exit.
 *
 * Before application exit, framework call this function to give user a chance
 * to do some exit cleaning, something like free the allocate resource, close 
 * all the opened file etc. This function must be implemented by the application.
 *
 * \return MR_SUCCESS on success, MR_FAILED otherwise
 * \sa mrc_exitApp
 */
int ExitApplication(VOID);

/**
 * \brief Pause application callback.
 *
 * This function will be called in system interruption by the framework to give
 * the application a change to do something like suspend the drawing or stop 
 * a timer or save the status etc. This function must be implemented by the application.
 *
 * \return MR_SUCCESS on success, MR_FAILED otherwise
 * \sa mrc_pause
 */
int PauseApplication(VOID);

/**
 * \brief Resume application callback.
 *
 * This function will be called when the application resumed by the system to
 * give the application the chance to restore the status.
 * 
 * \return MR_SUCCESS on success, MR_FAILED otherwise
 * \sa mrc_resume
 */
int ResumeApplication(VOID);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _SGL_WINDOWS_H */

