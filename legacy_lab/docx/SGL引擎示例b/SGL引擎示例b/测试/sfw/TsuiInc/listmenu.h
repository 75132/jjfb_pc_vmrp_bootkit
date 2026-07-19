// 列表形式的菜单 [3/28/2012 JianbinZhu]
// 一般用作上下文菜单
#ifndef _LISTMENU_H_
#define _LISTMENU_H_

#include "ts_gui.h"

#define MAX_MENUITEM_COUNT	10	//最大菜单项数


//列表菜单样式：菜单项多选
#define LTMUS_CHECKBOX 0x0001

//列表菜单样式：菜单项单选
#define LTMUS_RADIOBOX 0x0002



//创建新菜单前请先清除所有子菜单
VOID MU_clearItems(VOID);

//添加主菜单项
int32 MU_addItems(int ids[], PCWSTR titles[], int count);

//添加二级子菜单项
//index 子菜单在一级菜单中的位置（父菜单位置）
int32 MU_addSubItems(int index, int ids[], PCWSTR titles[], int count);

//设置菜单其他数据
VOID MU_setTitle(PCWSTR title);

//显示菜单
VOID MU_show(WID id, HWND hListener);

//关闭菜单
VOID MU_close(void);

#endif
