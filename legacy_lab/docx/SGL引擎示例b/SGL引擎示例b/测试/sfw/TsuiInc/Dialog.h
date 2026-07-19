#ifndef _DIALOG_H_
#define _DIALOG_H_

#include "window.h"

typedef HWND HDLG;

#define DIALOG_ID_COMMON	0x1001	/* 通用提示对话框ID */

//对话框按钮样式
#define MB_NON		0x0000	//没有按钮
#define MB_OK		0x0001	//有一个确定按钮
#define MB_CANCEL	0x0002	//有一个取消按钮
#define MB_YESNO	0x0004	//有 是/否 按钮
//兼容
#define ID_NON		0x0000
#define ID_OK		0x0001L 	/* 有"确定"按钮的对话框 */
#define ID_CANCEL	0x0002L     /* 有"取消"按钮的对话框 */
#define ID_YESNO	0x0004L    /* "是""否"类型对话框 */


//对话框样式
#define DIALOGS_AUTOCLOSE		0x0010		//自动关闭对话框
#define DIALOGS_INPUT			0x0020		//输入内容对话框
#define DIALOGS_PROGRESS		0x0040		//进度提示对话框
#define DIALOGS_MESSAGE			0x0080		//显示消息对话框
#define DIALOGS_COLOR			0x0100		//颜色选取对话框
#define DIALOGS_NOTITLE			0x0200		//没有标题栏
#define DIALOGS_NOTRANSBG		0x0400		//对话框背景不是透明的（默认透明）
#define DIALOGS_SINGLECHOICE    0x0800		//单选列表
#define DIALOGS_HASTEXT			0x1000		//有textview
#define DIALOGS_EFFSCREEN		0x2000		//半透明掩盖屏幕背景

//MsgBox自动关闭的时长（单位ms）
#define DIALOG_CLOSE_INTERVAL		5000


//对话框通知消息
//注意：SGL内部也会调用
#define DIALOGN_CANCEL				0x0002	//对话框取消
#define DIALOGN_CONFIRM				0x0001	//对话框确认
#define DIALOGN_CHECK_CHANGEED		0x0003	//单选列表选择改变通知
#define DIALOGN_COLORCHANGED		0x0004	//选中颜色变化
#define DIALOGN_PROGRESS_CHANGED	0x0005	//进度改变
//#define DIALOGN_


//--------------消息对话框----------------------------------------------------
/**
 * \显示一个消息，不需要响应任何事件
 */
#define ShowMessage2(content) \
	MessageDlg(TOPWIN_DIALOG_COMMON, SGL_LoadString(STR_HINT), (PCWSTR)content, MB_OK|DIALOGS_AUTOCLOSE, NULL)

//使用字符串
#define ShowMessage(title, content) \
	MessageDlg(TOPWIN_DIALOG_COMMON, (PCWSTR)title, (PCWSTR)content, MB_OK, NULL)

//使用字符串ID
#define ShowMessage_strId(titleStrId, contentStrId) \
	MessageDlg(TOPWIN_DIALOG_COMMON, SGL_LoadString(titleStrId), SGL_LoadString(contentStrId), MB_OK, NULL)

/**
 * \显示一个对话框，用户自定义标题和内容
 * \通过 ID_OK, ID_CANCEL, ID_YESNO 设置对话框按钮（默认样式DIALOGS_MESSAGE）
 * \在当前顶级窗口弹出对话框，如果 listener=NULL，则由该顶级窗口接受对话框消息
 * \通过 id 来区别对话框消息
 * \返回对话框句柄
 */
HDLG MessageDlg(WID id, PCWSTR title, PCWSTR content, DWORD style, HWND listener);

//同上，使用字符串ID
#define MessageDlg2(id, titleStrId, contentStrId, style, listener)\
	MessageDlg(id, SGL_LoadString(titleStrId), SGL_LoadString(contentStrId), style, listener)

/**
 * \刷新消息对话框的文本显示区域
 */
VOID MessageDlgRefresh(HDLG hDlg, PWSTR content);


//--------------输入对话框----------------------------------------------------
//带输入框的对话框的数据
typedef struct
{
	PCWSTR title;			// 对话框标题 
	PCWSTR content;			// 对话框内容 
	int  maxEditLen;		// 输入框的最大允许字数(非字节数，内部算法unicode个数=maxEditLen/2-1 ) 
	PWSTR  editBuf;			// 输入框预置内容, NULL则为空 
	int  inputType;			// 输入类型 ES_NUM|ES_FLOAT|ES_PWD 和EDIT控件一致 
}INPUT_DLGDATA, *PINPUT_DLGDATA;

/**
 * \显示一个简单输入对话框（一个编辑框），用户自定义标题和内容
 * \默认有左右按钮（确定/取消）（默认样式DIALOGS_INPUT）
 * \在当前顶级窗口弹出对话框，如果 listener=NULL，则由该顶级窗口接受对话框消息
 * \通过 id 来区别对话框消息
 * \返回对话框句柄
 */
HDLG InputDlg(WID id, PINPUT_DLGDATA pData, DWORD style, HWND listener);

//获取输入框输入的内容
PWSTR DlgGetInput(VOID);


//--------------进度对话框----------------------------------------------------
//带进度条的对话框的数据
typedef struct
{
	PCWSTR title;			// 对话框标题
	PCWSTR content;			// 对话框内容
	int  maxValue;			// 进度条最大值 
	int  curValue;			// 进度条当前值, 进度=(curValue*100/maxValue)% 
}PROG_DLGDATA, *PPROG_DLGDATA;

/**
 * \显示一个简单进度对话框（一个进度条），用户自定义标题和内容
 * \默认有一个取消按钮（估计没实现取消）（默认样式DIALOGS_PROGRESS）
 * \是否响应事件未确定（可以响应进度改变）
 * \通过 id 来区别对话框消息
 * \返回对话框句柄
 */
HDLG ProgressDlg(WID id, PPROG_DLGDATA pData, DWORD style, HWND listener);

//刷新对话框内容，进度条信息有应用获取滚动条句柄，去设置信息
VOID ProgressDlgRefresh(HDLG hDlg, PWSTR content);

//获取进度条句柄
HWND ProgDlgGetProgbar(HDLG hDlg);
//设置当前值
VOID ProgDlgSetValue(HDLG hDlg, int value);
//设置进度范围
VOID ProgDlgSetRange(HDLG hDlg, int min, int max);


//--------------颜色对话框----------------------------------------------------
HDLG ColorDlg(WID id, PCWSTR title, HWND listener);

//获取选择的颜色
Uint32 DlgGetSelectColor(VOID);


//单选列表对话框
HDLG SingleChoiceDlg(WID id, PCWSTR title, HWND listener, 
					 PCWSTR itemsTitle[], int itemCount, int selectIndex);

//关闭任何形式的对话框
VOID DialogClose(VOID);

/**
 * \brief The msgbox window procedure.
 *
 * \param hDlg the window handle
 * \param Msg the window message
 * \param wParam the first parameter
 * \param lParam the second parameter
 * \return the result of message process 
 */
LRESULT Dialog_WndProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);

#endif

