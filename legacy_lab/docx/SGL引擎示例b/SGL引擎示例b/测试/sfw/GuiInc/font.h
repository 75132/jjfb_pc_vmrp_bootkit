// SGL字库管理模块 [4/30/2012 JianbinZhu]
// 包括各种字库加载 及文本绘制

#ifndef	_SGL_FONT_H
#define _SGL_FONT_H


//编译开关
/**
 * \SKY12 字体开关
 */
#ifndef FONT_SKY12_ON
#define FONT_SKY12_ON 0
#endif

/**
 * \真字体开关
 */
#ifndef FONT_TTF_ON
#define FONT_TTF_ON 0
#endif

/**
 * \真字体开关
 */
#ifndef FONT_TSF_ON
#define FONT_TSF_ON 0
#endif



//字体类型
typedef enum fontType {
	GAL_FT_SYSTEM,	//系统字体
	GAL_FT_TSF,		//tsf字体
	GAL_FT_TTF,		//真字体
	GAL_FT_SKY12,	//斯凯12号
	GAL_FT_SKY16,	//斯凯16号

	GAL_FT_MAX
}E_GAL_FONTTYPE;

typedef enum {
	GAL_DT_AUTONEWLINE
}GAL_DRAWTEXT_FLAG;


//GAL上下文
typedef struct _gal_context {
	//需要保存的信息
	E_GAL_FONTTYPE font;	//系统字体（界面）

	//不需要保存的信息
	uint8 bSkyFontEnable;	//斯凯字库可用
	uint8 bTsFontEnable;	//TS字库可用
	uint8 bTTFEnable;		//真字库可用
}T_SGL_FONT_INFO;


//SGL字库模块信息
extern T_SGL_FONT_INFO gSglFontInfo;


#define SGL_GetSystemFont() gSglFontInfo.font
#define SGL_SetSystemFont(afont) gSglFontInfo.font = (afont)


//绘制单行文本
VOID GAL_drawText(uint8* pText, 
				  int x, int y, uint32 color, 
				  int flag, 
				  E_GAL_FONTTYPE font);

//绘制多行文本
//目前flag 参数唯一标示就是 是否换行 1 则自动换行
VOID GAL_drawTextLeft(uint8* pText, 
					  int x, int y, mr_screenRectSt rect, uint32 color, 
					  int flag, 
					  E_GAL_FONTTYPE font);

//获取单行文本宽高
VOID GAL_textWidthHeight(uint8* pText, 
						 int32 *width, int32 *height, 
						 E_GAL_FONTTYPE font);

//多行文本宽高
VOID GAL_textWidthHeightLines(uint8* pText, int showWidth,
							  int32 *width, int32 *height, int32 *lines,
							  E_GAL_FONTTYPE font);

//获取单个字宽高 UCS2BE
//width height:输出宽高，可为null
VOID GAL_charWidthHeight(uint16 ch, int32 *width, int32 *height, E_GAL_FONTTYPE font);


#endif