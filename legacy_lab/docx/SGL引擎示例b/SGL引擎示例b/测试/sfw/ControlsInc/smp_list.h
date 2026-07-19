#ifndef _SMP_LIST_H
#define _SMP_LIST_H

#include "window.h"

/**
  \defgroup smp_list Simple List

  To work with simple list:
  - Create your data provider callbacks
  - Create the simple list
  - Set the list data provider with your callbacks
  - Add to the parent window
  - Response to the notify events

  \code
  int YourListGetTotal(VOID)
  {
  	...
  }

  VOID YourListGetRowData(int index, PSMPROWDATA pRowData)
  {
  	...
  }

  //create the list
  hList = SGL_CreateWindow(SMP_List_WndProc, ...);
  SMP_List_SetDataProvider(hList, YourListGetTotal, YourListGetRowData);
  SGL_AddChildWindow(hWnd, hList);

  //response to the notify messages
	case WM_COMMAND:
		WID id = LOWORD(wParam);
		WORD code = HIWORD(wParam);

		if(id == "the list id")
		{
			int index = (int)lParam; //the selected row index
			switch(code)
			{
			case SMP_LISTN_HILICHANGED:
				break;
			case SMP_LISTN_SELECTED:
				break;
			case SMP_LISTN_CLICKED:
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
 * \brief Indicates the list has no scrollbar.
 */
#define SMP_LISTS_NOSCRBAR			0x0001L

/**
 * \高亮行变高的样式
 */
#define SMP_LISTS_EXPAND_FOCUSITEM	0x0002L

/**
 * \列表循环滚动样式（最后一行滚动到第一行）
 */
#define SMP_LISTS_CYCLESCROLL		0x0004L

	/** @} */

	/**
	 * \name Window Notify Messages
	 * @{
	 */

/**
 * \brief Sent when high light index changed.
 *
 * \code
 *	case WM_COMMAND:
 *		WID id = LOWORD(wParam);
 *		WORD code = HIWORD(wParam);
 *
 *		if(id == "the list id" && code == SMP_LISTN_HILICHANGED)
 *		{
 *			int index = (int)lParam;
 * 			//handle the notify message
 *		}
 * \endcode
 *
 * \param index current high light index
 */
#define SMP_LISTN_HILICHANGED			0x0001

/**
 * \brief Sent when KEY SELECT up.
 *
 * \code
 *	case WM_COMMAND:
 *		WID id = LOWORD(wParam);
 *		WORD code = HIWORD(wParam);
 *
 *		if(id == "the list id" && code == SMP_LISTN_SELECTED)
 *		{
 *			int index = (int)lParam;
 * 			//handle the notify message
 *		}
 * \endcode
 *
 * \param index current high light index
 */
#define SMP_LISTN_SELECTED			0x0002

/**
 * \brief Sent when MOUSE up.
 *
 * \code
 *	case WM_COMMAND:
 *		WID id = LOWORD(wParam);
 *		WORD code = HIWORD(wParam);
 *
 *		if(id == "the list id" && code == SMP_LISTN_CLICKED)
 *		{
 *			int index = (int)lParam;
 * 			//handle the notify message
 *		}
 * \endcode
 *
 * \param index current high light index
 */
#define SMP_LISTN_CLICKED				0x0003

/**
 * \List收到双击事件
 */
#define SMP_LISTN_DOUBCLICKED		    0x0004

/**
 * \选中列事件
 */
#define SMP_LISTN_CHECKED				0x0005

/**
 * \空白区域鼠标弹起
 */
#define SMP_LISTN_MOUSEUP				0x0006

	/** @} */

/**
 * \brief Max supported collumns.  
 */
#define SMP_LIST_MAX_STR_COL		2

/**
 * \brief Row information  行信息
 */
typedef struct SMP_RowData
{
	/** left margin     左间距*/
	Uint16 margin;
	/** icon col width  行上图标宽度*/
	Uint16 colWidth0;
	/**                 行上图标高度*/
	uint16 colHeight0;
	/**                 是否选中 */
	BOOL checked;
	/** icon handle     图标句柄*/
	HBITMAP hBmp;
	/** col information 行信息*/
	struct{
		/** col string  行字符*/
		PSTR str;
		/** col width   行字符显示宽度*/
		Uint16 width;
	}cols[SMP_LIST_MAX_STR_COL];	
}SMPROWDATA, *PSMPROWDATA;

/**
 * \brief The function provide to list to get total row count.
 *
 * \return the total rows of the list 返回列表总行数
 */
typedef int (*SMP_List_GetTotal)(VOID);

/**
 * \brief The function provide to list to get row data.
 *
 * \param index the row index
 * \param[out] pRowData the row data
 */
typedef VOID (*SMP_List_GetRowData)(int index, PSMPROWDATA pRowData);

/**
 * \brief The draw row function.
 *
 * \param index the row index
 * \param r the r rectangle
 * \param pRowData the row data
 * \param hilight the high light row index
 * \param flush if flush to the screen
 */
typedef VOID (*SMP_List_DrawRow)(int index, PRECT r, PSMPROWDATA pRowData, int hilight, BOOL flush);

	/**
	 * \name Window Member Functions
	 * @{
	 */

/**
 * 设置list基本信息
 * 参数:
 * \hWnd:	list控件的句柄
 * \listRowHeight: 行高
 * \reSet: 是否重设list
 * 返回值: 无
 * \如使用者不调用此函数则参与默认高度 SMP_LIST_ITEM_HEIGHT
 */
VOID SMP_List_SetRowHeight(HWND hList, int listRowHeight, BOOL reSet);

/**
 * \brief Set user data provide callbacks.
 *
 * \param hList the list window handle
 * \param fnGetTotal the function to get total row count
 * \param fnGetRowData the function to get row data by index that from 0
 * 设置list控件的获取内容和获取行数回调函数
 * 这个list控件和一般的GUI实现的list控件有所不同
 * 它本身并不存储list的内容, 而是通过两个回调函数来获取总共多少行和每行的详细信息
 * 所以使用者需要告诉list控件两个回调函数
 * 参数:
 * hWnd:			list控件的句柄
 * fnGetTotal:		获取列表总共行数的回调函数
 * fnGetRowData:	获取列表每行的内容的回调函数
 * 返回值: 无
 */
VOID SMP_List_SetDataProvider(HWND hList, SMP_List_GetTotal fnGetTotal, SMP_List_GetRowData fnGetRowData);

/**
 * \brief Set a user specific drawing function.
 *
 * \param hList the List window handle
 * \param fnDrawRow the new draw row function
 */
VOID SMP_List_SetDrawRowCallback(HWND hList, SMP_List_DrawRow fnDrawRow);

/**
 * \brief The default draw row function.
 *
 * \param index the index of the list item 列表索引
 * \param r the list item draw rect 列表内行绘制矩形
 * \param pRowData the row data 行数据
 * \param hilight current hilight list item index 当前高亮行索引
 * \param flush if flush to screen 是否刷新
 */
VOID SMP_List_DefaultDrawRowCallback(int index, PRECT r, PSMPROWDATA pRowData, int hilight, BOOL flush);

/**
 * \brief High light a row by index.
 *
 * \param hList the List window
 * \param index the row index
 */
VOID SMP_List_HilightByIndex(HWND hList, int index);
VOID SMP_List_SetHilightIndex(HWND hList, int index);

/**
 * \brief Get current high light row index.
 *
 * \param hList the List window handle
 * \return the row index
 */
int SMP_List_GetHilightIndex(HWND hList);

/**
 * \brief Get the current page start item index and page size.
 *
 * \param hList the list window handle
 * \param[out] pagestart the current page start list item  当前页第一行索引
 * \param[out] pagesize the list page size  每页显示的行数
 */
VOID SMP_List_GetPageInfo(HWND hList, int* pagestart, int* pagesize);


int SMP_List_GetHilightRowY(HWND hList);

/**
 * \brief Page up the list.
 *
 * \param hList the list window handle
 * \不能向上翻页返回FALSE 否则返回 TRUE
 */
BOOL SMP_List_PageUp(HWND hList);

/**
 * \brief Page down the list. 
 *
 * \param hList the list window handle
 * \不能向下翻页返回FALSE 否则返回 TRUE
 */
BOOL SMP_List_PageDown(HWND hList);

/**
 * \brief Line up the list.
 *
 * \param hList the list window handle
 */
BOOL SMP_List_LineUp(HWND hList);

/**
 * \brief Line down the list.
 *
 * \param hList the list window handle
 */
BOOL SMP_List_LineDown(HWND hList);

/**
 * \brief Notify list that total list items changed.
 *
 * \param hList the list window handle
 *
 * //刷新列表，设置列表页数、滚动条位置大小、
 * 刷新list控件, 当list的行数发生变化以后调用
 */
VOID SMP_List_Refresh(HWND hList);


/**
 * \brief Reset the list.
 * 
 * \param hList the list window handle
 * 焦点回到第一行
 */
VOID SMP_List_Reset(HWND hList);

	/** @} */

	/**
	 * \name Window Procedure
	 * @{
	 */

/**
 * \brief The simple list window procedure.
 *
 * \param hList the window handle
 * \param Msg the window message
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of message process 
 */
LRESULT SMP_List_WndProc(HWND hList, UINT Msg, WPARAM wParam, LPARAM lParam);

	/** @} */

/** @} end of smp_list */

#endif /* _SMP_LIST_H */

