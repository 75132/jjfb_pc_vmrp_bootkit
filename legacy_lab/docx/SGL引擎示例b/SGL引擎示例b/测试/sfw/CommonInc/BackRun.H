// 后台运行模块 [4/30/2012 JianbinZhu]

#ifndef BACKRUN_H__
#define BACKRUN_H__

#include "types_i.h"
#include "mrc_base.h"


typedef struct
{
	int32 appid;
	int32 time;
	int32 img_type;
	int32 img_size;
	int32 width;
	int32 height;
	uint16 color;
	uint8 *buff;
	uint8 destId;
}mr_pic_req;


/* 后台运行返回后平台收到的事件
* mrc_even(int32 code, int32 param0, int32 param1)
* code = MR_LOCALUI_EVENT
* param0 = 2
*/
//唤醒后台运行的 APP
int32 AwakeApp(int32 appid);

//后台运行APP
int32 MinApp(int32 appid);

//设置后台运行图标
/* 请求移植层显示一张图片（图片显示的位置由移植来确定，可能在idle，或者icon条，或者两个地方都显示等等情况）
* mr_pic_req 数据结构：如下
*/
/*	参数说明：
*	appid：请求画图的应用的id
*	time： 图片闪动的时间单位是：ms（毫秒）如果为0 表示不用闪动；
*	img_type:图片类型
*      0：BMP 图片；1：jpg 图片；2：png 图片；3：gif 图片
*      如果是bmp图片的话，buffer 指向的是解压后的点阵数据信息
*	img_size:buff 这个指针指向的空间的大小
*	color： 透明色（bmp 图片时候起作用）
*	buff：  图片数据。
*	destId: 0：表示画idle下的一个小图标
*          1：画idle下的整个背景

*	根据不同的timer大小，底层将确定进行怎样的操作
*	timer：     0 ：显示icon但不闪动（如果是QQ显示在线图标）
*				1000：闪动的显示icon（如果是QQ显示在线图标）
*				1：显示离开状态的QQ头像（QQ专用）
*				2：显示隐身状态的QQ头像（QQ专用）
*			    5：MRSTAR平台专用，用于mrp通知移植层，当前QQ已经启动可以实现end键转后台（QQ专用）；
*			    1001：显示离开状态的QQ头像（QQ专用）
*			    1002：显示隐身状态的QQ头像（QQ专用）
*			    -1：为显示离线状态的QQ头像（QQ专用）
*
*/
int32 ShowStateIcon(mr_pic_req pic);


//后台运行并 延时返回
//需要先调用 MinApp 如果成功则调用此函数开始定时返回，失败则无意义
int32 BRByDelay(int32 delayTime);

//后台运行并 监视文件返回
//需要先调用 MinApp 如果成功则调用此函数开始定时返回，失败则无意义
int32 BRByFile(PCSTR filename);



#endif // BACKRUN_H__