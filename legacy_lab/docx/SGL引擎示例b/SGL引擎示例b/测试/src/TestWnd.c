#include "window.H"
#include "ListView.h"
#include "sgldef.h"
#include "arraylist.h"
#include "gal.h"
#include "ts_gui.h"
#include "gb2unicode.h"
#include "font.h"
#include "smp_titlebar.H"
#include "smp_toolbar.h"
#include "smp.h"
#include "smp_menu.h"
#include "Dialog.h"


enum{
	WID_TITLEBAR,
	WID_LISTVIEW,
	WID_TOOLBAR,
	WID_MENU,
	WID_DLG_EXIT,
	WID_DLG_SEARCH,
	WID_DLG_ENCODE,
	WID_DLG_COLOR,

	WID_MAX
};

static
int getCount(VOID){
	return 100;
}

static
VOID setItemInfo(int index, PLV_ITEMDATA pItem){
}

static
VOID drawItem(int index, PRECT r, PLV_ITEMDATA pItem){
	int32 fw, fh;
	uint32 fg;
	HFONT font = SGL_GetSystemFont();
	char name[128] = {0};
	BYTE buf[256] = {0};
	int x, y;

	//填充背景
	GAL_FillBox(PHYSICALGC, r->left, r->top, r->width, r->height, COLOR_MAIN_BG);

	if(pItem->pressed)// || pItem->pressed) 
	{
		ShadeFillRect(r->left, r->top, r->width, r->height-1, 0x80D0F0, 0x388CA8);
		fg = 0xffffff;
	}else{
		fg = 0x87ceeb;
	}
	GAL_DrawHLine(PHYSICALGC, r->left, r->top+r->height-1, r->width, 0x00074c87);

	x = r->left + pItem->padLeft;
	y = r->top + pItem->padTop+4;

	//文件名
	sprintf(name, "%d.%s", index+1, "测试列...");
	gb2uniBE((PWSTR)name, (PWSTR)buf, sizeof(buf));
	GAL_textWidthHeight((PWSTR)buf, &fw, &fh, font);
	GAL_drawText(buf, x, y, 0xffffff, 1, font);
}


static HWND hListView;
static LV_ADAPTER adapter1;

PCWSTR encodeList[4];	//编码列表


static const DWORD miOptions[] = 
{
	STR_HELP,
	STR_ABOUT,
	STR_EXIT
};

static const DWORD miConnections[] =
{
	STR_OK,
	STR_CANCEL
};

static VOID ShowOptMenu(HWND hWnd)
{
	SMP_Menu_ClearMenuItems();
	SMP_Menu_SetMenuItem2(0, miOptions, TABLESIZE(miOptions));
	SMP_Menu_SetMenuItem2(TABLESIZE(miOptions), miConnections, TABLESIZE(miConnections));
	SMP_Menu_SetSubMenu(0, TABLESIZE(miOptions));
	SMP_Menu_Popup(WID_MENU, SMP_MENUS_BOTTOMLEFT, hWnd, 0, _HEIGHT(hWnd) - SMP_TOOLBAR_HEIGHT, NULL);
}

LRESULT TestWnd_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch(Msg)
	{
	case WM_CREATE:
		{
			HWND hControl;

			_BGCOLOR(hWnd) = 0x0;
			_FGCOLOR(hWnd) = 0x00ff00;

			hControl = SGL_CreateWindow(SMP_Titlebar_WndProc, 
				0, 0, SCREEN_WIDTH, SMP_HEADER_HEIGHT, 
				WID_TITLEBAR, SMP_TITLEBARS_SHOWTIME, 0);
			SMP_Titlebar_SetContent(hControl, BMP_MRP, SGL_LoadString(STR_TITLE_MAIN));
			_FGCOLOR(hControl) = COLOR_lightwhite;
			SGL_AddChildWindow(hWnd, hControl);

			hListView = SGL_CreateWindow(ListView_WndProc,
				0, SMP_HEADER_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT-SMP_HEADER_HEIGHT-SMP_TOOLBAR_HEIGHT,
				WID_LISTVIEW, LVS_SHOW_SCROBAR, 0);
			adapter1.drawItem = drawItem;
			adapter1.getCount = getCount;
			adapter1.getItemData = setItemInfo;
			LV_setAdapter(hListView, &adapter1);
			//算行高
			{
				int32 fw, fh;
				GAL_textWidthHeight((uint8*)CHR_GB, &fw, &fh, SGL_GetSystemFont());
				LV_setRowHeight(hListView, fh+2*8);
			}
			SGL_AddChildWindow(hWnd, hListView);
			_BGCOLOR(hListView) = COLOR_MAIN_BG;		

			hControl = SGL_CreateWindow(SMP_Toolbar_WndProc, 
				0, SCREEN_HEIGHT-SMP_TOOLBAR_HEIGHT, SCREEN_WIDTH, SMP_TOOLBAR_HEIGHT, 
				WID_TOOLBAR, 0, 0);
			SMP_Toolbar_SetStrings(hControl, STR_OPTION, RESID_INVALID, STR_EXIT, FALSE);
			SGL_AddChildWindow(hWnd, hControl);
			_FGCOLOR(hControl) = COLOR_lightwhite;

			//其他数据
			encodeList[0] = SGL_LoadString(STR_ENC_GB);
			encodeList[1] = SGL_LoadString(STR_ENC_UCS2BE);
			encodeList[2] = SGL_LoadString(STR_ENC_UCS2LE);
			encodeList[3] = SGL_LoadString(STR_ENC_UTF8);
		}
		break;

	case WM_INITFOCUS:
		{
			SGL_SetFocusWindow(hWnd, SGL_FindChildWindow(hWnd, WID_LISTVIEW));
		}
		break;

	case WM_KEYUP:
		{
			switch(wParam)
			{
			case MR_KEY_SOFTLEFT:
				ShowOptMenu(hWnd);
				return 1;
			case MR_KEY_SOFTRIGHT:
				MessageDlg(WID_DLG_EXIT, SGL_LoadString(STR_HINT), SGL_LoadString(STR_HINTEXIT), ID_YESNO|DIALOGS_NOTITLE, hWnd);
				//HideTopWindow(_WID(hWnd), FALSE, TRUE);
				return 1;
			case MR_KEY_1:
				MessageDlg(WID_DLG_EXIT, SGL_LoadString(STR_HINT), SGL_LoadString(STR_HELP_READER), ID_OK, hWnd);
				//HideTopWindow(_WID(hWnd), FALSE, TRUE);
				return 1;
			case MR_KEY_2:
				SGL_ToastShow(SGL_LoadString(STR_ABOUT_STR), 3000);
				return 1;
			case MR_KEY_3:
				{
					INPUT_DLGDATA input;

					input.content = (PCWSTR)"\x51\x73\x95\x2E\x5B\x57\x0\x0";
					input.editBuf = NULL;
					input.inputType = ES_ALPHA;
					input.maxEditLen = 12;
					input.title = SGL_LoadString(STR_SEARCH);
					InputDlg(WID_DLG_SEARCH, &input, 0, hWnd);
				}
				return 1;
			case MR_KEY_4:
				{
					SingleChoiceDlg(WID_DLG_ENCODE, SGL_LoadString(STR_FORCEENCODE), hWnd, encodeList, DIV(sizeof(encodeList), sizeof(PCWSTR)), 2);
				}
				return 1;

			case MR_KEY_5:
				ColorDlg(WID_DLG_COLOR, SGL_LoadString(STR_COLOR_BG), hWnd);
				return 1;
			}
			break;
		}
	}

	return 0;
}
