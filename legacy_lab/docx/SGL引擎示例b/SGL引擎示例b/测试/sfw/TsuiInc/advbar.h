#ifndef _SMP_ADVBAR_H
#define _SMP_ADVBAR_H

#include "window.h"
#include "mrc_advbar.h"

typedef enum
{
    SMP_ADVBAR_DOWNLOAD,	//广告插件下载完一个应用后通知父窗口的消息
    SMP_ADVBAR_BROWSER		//广告插件调用浏览器返回后通知父窗口的消息
}SMP_ADVBAR_EVENT;

enum
{
    ADV_STATE_ERROR,	//状态：广告插件加载错误
    ADV_STATE_OK,		//状态：广告插件加载完毕
    ADV_STATE_LOADING	//状态：广告插件加载中
};

/**
 * 将广告插件添加到父窗口
 * \参数：x,y,w 广告插件在父窗口的位置
 * \参数：id 广告插件窗口ID
 * \参数：hListener 监听广告插件时间的窗口
 */
HWND SMP_Advbar_AddToParent(HWND hParent, int x, int y, int w, WID id, HWND hListener);

/**
 * 设置广告控件参数
 * \advChunkID:广告ID
 **/
VOID SMP_Advbar_SetInfo(uint32 advChunkID);

/**
 * 菜单弹出的时候调用此方法停止广告插件，
 * 否则菜单无法响应触屏点击
 */
/**
 \code
 case WM_MENUHIDE: case WM_MODALHIDE:
 {
 SMP_Advbar_Show();
 break;
 }
 \endcode
 */
VOID SMP_Advbar_Hide(VOID);

/**
 * 菜单关闭后重新开始广告插件绘图
 */
/**
 \code
 case WM_MENUSHOW: case WM_MODALSHOW:
 {
 SMP_Advbar_Hide();
 break;
 }
 \endcode
 */
VOID SMP_Advbar_Show(VOID);

/**
 * \brief The simple Advbar window procedure.	
 *
 * \param hWnd the button window handle
 * \param Msg the window message
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of message process 
 */
LRESULT SMP_Advbar_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

#endif
