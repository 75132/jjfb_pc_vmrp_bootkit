//-------------------------------------------------
// gridview控件 [4/9/2012 JianbinZhu]
// 版本：1.0
// 摘要：暂不支持滚动
// 作者：JianbinZhu 
// 最后修改：2012/4/9
//-------------------------------------------------
#ifndef _GRIDVIEW_H
#define _GRIDVIEW_H


#include "window.H"
#include "bmp.h"

#define GRIDVIEWN_ITEMCLICKED	0x0001	//点击事件
#define GRIDVIEWN_FOCUSCHANGED	0x0002	//焦点改变事件

//焦点回滚样式，（当焦点到达最后一个时，继续按键回到第一个焦点
//即他会永远占有此按键事件，在一些需要的地方则无法响应）
#define GRIDVIEWS_ROLLFOCUS		0x0001	


//翻页模式定义
typedef enum {
	GV_PGMOD_HOR, //水平翻页（在水平方向分成N页，按页面切换）
	GV_PGMOD_VER  //竖直翻页（所有图标竖直方向像素滑动）
}E_GV_PAGEMODE;

//每个宫格数据
typedef struct
{
	T_BMP565 icon;	//图标
	PCWSTR title;	//标题
	int padLeft, padTop;	//左边和右边的间距
}GRIDDATA, *PGRIDDATA;


//适配器 获取数目
typedef int (*GV_getItemCount)(VOID);

//适配器 获取没格数据
typedef VOID (*GV_getItemData)(int index, PGRIDDATA pGrid);

//绘制每格
typedef VOID (*GV_drawItem)(int index, PGRIDDATA pGrid, PRECT rect, BOOL focused);

//适配器
typedef struct {
	GV_getItemCount getItemCount;  //获取数目回调
	GV_getItemData getItemData;		//获取每项数据回调
	GV_drawItem drawItem;	//绘制每项
}GV_ADAPTER, *PGV_ADAPTER;


//设置适配器（此函数将废弃，用GV_setProvider代替）
VOID GV_setAdapter(HWND hWnd, PGV_ADAPTER adapter);

//设置数据来源
VOID GV_setProvider(HWND hWnd, 
					GV_getItemCount getItemCount,  //获取数目回调
					GV_getItemData getItemData,		//获取每项数据回调
					GV_drawItem drawItem	//绘制每项
					);

//设置 宫格尺寸
VOID GV_setGridSize(HWND hWnd, int w, int h);

//设置 最大列数
VOID GV_setMaxColumnNum(HWND hWnd, int num);

//通知 gridview 数据有变化，gridview将重新获取数据并刷新
VOID GV_refresh(HWND hWnd, BOOL reDraw);

//系统系统的 绘制宫格算法
VOID GV_defaultDrawItem(int index, PGRIDDATA pGrid, PRECT pr, BOOL focused);

//设置获得焦点的宫格
BOOL GV_setFocusIndex(HWND hWnd, int index);

//设置选中的项，一般用于第一次显示的时候
VOID GV_setSelection(HWND hWnd, int index);

//上下方向切换焦点
BOOL GV_focusUpDown(HWND hWnd, BOOL up);

//左右方向切换焦点
BOOL GV_focusLeftRight(HWND hWnd, BOOL left);

//设置翻页模式
VOID GV_setPageMode(HWND hWnd, E_GV_PAGEMODE mode);

//窗口回调函数
LRESULT GridView_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

#endif