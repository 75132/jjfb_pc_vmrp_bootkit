// -- [4/15/2012 JianbinZhu] --------------------
// listview 列表数据展示
// 最后修改：[4/15/2012 JianbinZhu]
// 
// ----------------------------------------------
#ifndef _LISTVIEW_H
#define _LISTVIEW_H


#include "window.H"



//listview属性值
#define LV_DEFROWHEIGHT		30		//默认行高
#define LV_CLICKLONG		500		//响应长按事件时长
#define LVP_ITEM_STR_COUNT	2		//每行显示字符数

//listview样式
#define LVS_SHOW_SCROBAR		0x0001		//有滚动条样式
#define LVS_SIGNLECHOICE		0x0002		//单选列表样式
#define LVS_MUTILCHOICE			0x0004		//多选列表样式

//listview通知父窗口事件
#define LVN_ITEMCLICKED			0x0001		//列表点击事件
#define LVN_ITEMLONGCLICKED		0x0002		//列表长点击事件
#define LVN_FOCUSCHANGED		0x0003		//焦点改变

#define LVN_SELCHANGED			0x0004		//单选列表选中项改变了


typedef struct {
	/** 页边距 */
	Uint8 padLeft, padTop, padRight, padBtm;
	/** 是否选中 */
	BOOL checked;
	/** 是否获得焦点 */
	BOOL focused;
	/** 是否按下 */
	BOOL pressed;
	/** 是否禁止 */
	BOOL disable;
	/** 图标句柄 */
	HBITMAP hBmp;
	/** 图标尺寸 */
	int icW, icH;
	/** 字符 */
	struct{
		/** 字符 */
		PCWSTR str;
		/** 字符显示宽度 */
		Uint16 width;
	}cols[LVP_ITEM_STR_COUNT];
	/** 附加数据 */
	DWORD userData;
}LV_ITEMDATA, *PLV_ITEMDATA;


//获取行数回调
typedef int (*LV_getCount)(VOID);

//获取item数据回调
typedef VOID (*LV_getItemData)(int index, PLV_ITEMDATA pItem);

//获取每行VIEW回调
typedef VOID (*LV_drawItem)(int index, PRECT pr, PLV_ITEMDATA pItem);


typedef struct 
{
	LV_getCount getCount;
	LV_getItemData getItemData;
	LV_drawItem drawItem;
}LV_ADAPTER, *PLV_ADAPTER;


VOID LV_setAdapter(HWND hList, PLV_ADAPTER adapter);
VOID LV_setRowHeight(HWND hWnd, int height);
VOID LV_Refresh(HWND hWnd);
VOID LV_setChecked(HWND hList, int index);
VOID LV_updateItem(HWND hList, int index);

VOID LV_getItemPosition(HWND hList, int index, int *y, int *h);


LRESULT ListView_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

#endif