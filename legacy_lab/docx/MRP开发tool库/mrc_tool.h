#ifndef MRC_TOOL_H
#define MRC_TOOL_H

#include "mrc_base.h"
#include "mrc_exb.h"
#include "mrc_types.h"

//tool库v20240907
//编译时用tool_mtk.lib，tool_mtkt含t的是thumb模式的，（vs2005调试时项目属性链接器用tool_WIN32.lib）

char* getregactivateid();//本函数功能是返回绑定设备与游戏ID的代号(gb编码字符串)，需要由开发者在界面里提示用户拿着此代号 向QQ1284559320索取激活码

int32 checkregactivate(char* key);//需要由开发者做个编辑框由用户输入激活码，本函数功能是校验key是否为有效的激活码(key是gb编码字符串)，成功返回MR_SUCCESS，失败MR_FAILD

void my_sand(void);//科学的设置随机数种子

void buildpath(char* filename);//构建路径，自动判断路径包含的文件夹是否存在，若文件夹不存在会自动创建

int Readscore(char* datapath,int32* maxScore);//读取分数存档
void Savescore(char* datapath,int32 maxScore);//保存分数存档

// 点(x,y)绕(px,py)旋转指定弧度r，得到旋转后的坐标
// 参数：旋转中心点(px,py)，旋转横向半径rx，旋转纵向半径ry， 旋转弧度r, 旋转后坐标指针(*x,*y)
extern void toSpin2(int px, int py, int rx, int ry, int r, int *x, int *y);

#define CHAR_H get_str_height("A")
#define CHAR_W get_str_width("A")

extern mr_colourSt text_color;
#define set_text_color(new_color)                     \
    do                                           \
    {                                            \
        text_color.r = PIXEL888RED(new_color);   \
        text_color.g = PIXEL888GREEN(new_color); \
        text_color.b = PIXEL888BLUE(new_color);  \
    } while (0)
#define draw_text(x, y, str) mrc_drawText(str, x, y, text_color.r, text_color.g, text_color.b, 0, MR_FONT_MEDIUM)


extern int32 collRect(int32 x, int32 y, int32 w, int32 h, int32 xx, int32 yy, int32 ww, int32 hh);

//fps工具
extern int32 initFpsTool(int32 delta);
extern int32 getMainFps(void);
//绘制结束时定时器调用
extern int32 runFpsTool(void);

#endif
