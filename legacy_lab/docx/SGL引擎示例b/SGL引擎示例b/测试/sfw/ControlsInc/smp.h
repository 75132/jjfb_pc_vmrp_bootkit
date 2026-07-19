#ifndef _SMP_H
#define _SMP_H

#include "types_i.h"

//user defined colors
#define COLOR_focus				0x00fe9900
#define COLOR_border			0x000a3a73
#define COLOR_controlbg			0x00e7f1f3
#define COLOR_controlhili		0x0075a4c7
#define COLOR_controlborder		0x00cfe3fe
#define COLOR_wndheader		    0x00074c87
#define COLOR_gray		        0x00c0c0c0


#ifdef SCREEN_SIZE_176220

#define SMP_RADIOBOX_SIZE		16
#define SMP_HEADER_HEIGHT		25
#define SMP_TOOLBAR_HEIGHT	    22 
#define SMP_ITEM_HEIGHT		    20
#define SMP_LABEL_HEIGHT	    20 
#define SMP_ITEM_SPACE			2
#define SMP_ITEM_MARGIN		    8
#define SMP_LIST_TOP_MARGIN	    4
#define SMP_MSGBOX_MARGIN		4
#define SMP_MSGBOX_TBHEIHT	    22
#define MAX_FILE_NUMBER         6  //每页最大支持文件数
#define SMP_BUTTON_HEIGHT		24 //按钮高度
#define SMP_LIST_ITEM_HEIGHT	28 //List默认高度

#endif


#ifdef SCREEN_SIZE_240320

#define SMP_RADIOBOX_SIZE		16
#define SMP_HEADER_HEIGHT		29
#define SMP_TOOLBAR_HEIGHT	    26 
#define SMP_ITEM_HEIGHT		    26 
#define SMP_ITEM_SPACE			3
#define SMP_ITEM_MARGIN		    9
#define SMP_MSGBOX_MARGIN		8
#define SMP_MSGBOX_TBHEIHT	    26
#define SMP_BUTTON_HEIGHT		30 //按钮高度
#define SMP_LABEL_HEIGHT	    20 
#define SMP_LIST_TOP_MARGIN	    5
#define SMP_LIST_ITEM_HEIGHT	32//SMP_ITEM_HEIGHT

#endif


#ifdef SCREEN_SIZE_240400

#define SMP_RADIOBOX_SIZE	   16
#define SMP_HEADER_HEIGHT	   29
#define SMP_TOOLBAR_HEIGHT	   26 
#define SMP_ITEM_HEIGHT		   26
#define SMP_LABEL_HEIGHT	   20 
#define SMP_ITEM_SPACE		   3
#define SMP_ITEM_MARGIN		   9
#define SMP_LIST_TOP_MARGIN	   3
#define SMP_MSGBOX_MARGIN	   8
#define SMP_MSGBOX_TBHEIHT	   26
#define SMP_BUTTON_HEIGHT	   30 //按钮高度
#define SMP_LIST_ITEM_HEIGHT	30//SMP_ITEM_HEIGHT

#endif

#define SMP_ITEM_LENGTH			 (SCREEN_WIDTH - 2*SMP_ITEM_MARGIN)
#define SMP_ITEM_SPACE_HEIGHT	 (SMP_ITEM_HEIGHT + SMP_ITEM_SPACE)
#define SMP_CONTENT_VIEW_WIDTH	 SCREEN_WIDTH
#define SMP_CONTENT_VIEW_HEIGHT	 (SCREEN_HEIGHT - SMP_HEADER_HEIGHT - SMP_TOOLBAR_HEIGHT)
#define SMP_CHECKBOX_SIZE		 SMP_RADIOBOX_SIZE
#define SMP_SCRBAR_WIDTH		 4           //滚动条宽度
#define SMP_MENU_BOTTOM_HEIGHT	 SMP_TOOLBAR_HEIGHT
#define SMP_LIST_LEFT_MARGIN	 3	
#define SMP_LIST_ITEM_WIDTH		 (SCREEN_WIDTH - SMP_SCRBAR_WIDTH - 2*SMP_LIST_LEFT_MARGIN)
#define SMP_PROGBAR_HEIGHT		 20


/**
 * \brief button left/right space
 */
#define SMP_ITEM_CONTENT_MARGIN		4 //pixels（像素）

/**
 * \brief Draw radio box frame.
 *
 * \param x the left position in screen's coords
 * \param y the top position in screen's coords
 * \param checked check the radio box
 */
VOID SMP_DrawRadioBox(int x, int y, BOOL checked);

/**
 * \brief Draw check box frame.
 *
 * \param x the left position in screen's coords
 * \param y the top position in screen's coords
 * \param checked check the check box
 */
VOID SMP_DrawCheckBox(int x, int y, BOOL checked);



VOID SMP_DrawEditFrame(int x, int y, int w, int h, BOOL focused);

/**
 * \brief Draw a top-level window header.
 *
 * \param x the left position of the msgbox
 * \param y the top position of the msgbox
 * \param w the width of the msgbox
 * \param h the height of the msgbox
 * \param bgcolor the background color
 * \param fgcolor the foreground color
 * \param bmpID the bitmap ID
 * \param str the title of the header
 */
VOID SMP_DrawWndHeader(int x, int y, int w, int h, Uint32 bgcolor, Uint32 fgcolor, DWORD bmpID, PCWSTR str);


#endif


