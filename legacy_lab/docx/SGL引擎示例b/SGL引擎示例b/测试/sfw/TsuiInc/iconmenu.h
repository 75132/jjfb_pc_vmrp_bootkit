#ifndef	_SMP_ICONMWNU_H_
#define _SMP_ICONMWNU_H_

#include "ts_gui.h"

typedef struct  
{
	uint16 *buf;
	int16 w, h;
}MUICON, *PMUICON;

#define ICONMENU_MAX_ITEM_COUNT	8	//最大菜单项数
#define ICON_IMGW	32	//菜单项图标默认宽高，将裁切
#define ICON_IMGH	32


LRESULT SMP_IconMenu_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
VOID SMP_IconMenu_Show(WID id, HWND hParent, HWND hListener);
VOID SMP_IconMenu_AddItem( int id, MUICON icon, PCWSTR title );
VOID SMP_IconMenu_ClearItems(VOID);


#endif