#ifndef _TSGUI_H_
#define _TSGUI_H_


#include "mrc_base.h"
#include "mrc_exb.h"
#include "string.h"
#include "types_i.h"
#include "gal_i.h"
//#include "ts_main.h"
//#include "ts_graphics.h"
//#include "ts_xml.h"
#include "window.h"
//#include "functions.h"
//#include "appdef.h"
#include "smp.h"



//user defined colors
#define COLOR_focus				0x00fe9900
#define COLOR_border			0x000a3a73
#define COLOR_controlbg			0x00e7f1f3
#define COLOR_controlhili		0x0075a4c7
#define COLOR_controlborder		0x00cfe3fe
#define COLOR_wndheader		    0x00074c87
#define COLOR_gray		        0x00c0c0c0

typedef struct {
	uint32 main_bg, main_fg;
	uint32 title;	//标题文字（标题栏、工具栏）

	struct {
		uint32 shade[2];
		uint16* bmp;
	}tilbar;

	struct {
		uint32 line[2];
		uint32 shade[2];
		uint16* bmp;
	}tolbar;

	struct {
		uint32 line[2];
		uint32 font_n, font_f;
		uint32 shade[2];
	}list;

	struct {
		uint32 font_til, font_n, font_f, font_d;
		uint32 main_bg, til_bg, board;
		uint32 line[2], shade[2];
	}menu;

	uint32 scrbar;

	uint16* radBtn, chkBtn;	//单选/复选按钮图标
}SKINDATA;

typedef struct {
	uint8 tilbarH, tolbarH;
	uint8 itemH;
	uint8 listItemH;
	uint8 fontH;	//中文字体高度
	uint8 btnH;	//按钮高度
	uint8 editH; //编辑框高度
}SIZEDATA;

//TSGUI系统上下文
typedef struct {
	SKINDATA skin;	//皮肤
	SIZEDATA size;	//尺寸
}TSGUI_CONTEXT;


//全局皮肤结构体
extern SKINDATA skin;
//全局控件尺寸结构体
extern SIZEDATA size;


//////////////////////////////////////////////////////////////////////////
#define TITLEBAR_HEIGHT (int)size.tilbarH
#define TOOLBAR_HEIGHT	(int)size.tolbarH
#define BUTTON_HEIGHT	(int)size.btnH
#define EDIT_HEIGHT		(int)size.editH
#define ITEM_HEIGHT		(int)size.itemH
#define FONT_HEIGHT		(int)size.fontH

//////////////////////////////////////////////////////////////////////////
#define COLOR_shadeTop	0x80D0F0	//渐变起始色
#define COLOR_shadeBtm	0x388CA8	//渐变终止色
#define COLOR_shadeFg	0xffffff	//渐变中上前景色

#define COLOR_MAIN_BG		0x0B2337	//0x123555 //0x0B2337	//主窗口背景
#define COLOR_MAIN_FG		0XF0F0F0
#define COLOR_scrbar		0x7ABEEF
#define COLOR_scrbarBg		0x0B2337

//初始化GUI系统
void TSGUI_init(void);

//加载皮肤包
int32 TSGUI_loadSkinPack(PSTR name);

//从皮肤包加载BMP图片封装
HBITMAP TSGUI_skinLoadBmp(char * name);

#endif