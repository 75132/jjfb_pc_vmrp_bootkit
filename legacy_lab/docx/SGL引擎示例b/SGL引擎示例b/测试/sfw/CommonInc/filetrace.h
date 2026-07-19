// 打印Log到文件模块 [4/30/2012 JianbinZhu]

#ifndef _FILE_TRACE_H
#define _FILE_TRACE_H


//log类型
typedef enum {
	TRACETYPE_INFO,	//普通信息
	TRACETYPE_WARING,	//警告信息
	TRACETYPE_ERROR	//错误信息
}E_TRACE_TYPE;

//初始化trace模块
void filetraceInit(const char * filename);

//关闭trace模块，释放资源
void filetraceClose(void);

//真正的打log函数
void filetrace(void *data, int len, E_TRACE_TYPE type);


//打印 信息
#ifndef _EXT_RELEASE
#define FTRACE_I(...) \
	do \
	{ \
		char buf[256]; \
		int l = mrc_sprintf(buf, __VA_ARGS__); \
		filetrace(buf, l, TRACETYPE_INFO);\
	} while (0)
#else
#define FTRACE_I(...)
#endif

//打印 错误
#ifndef _EXT_RELEASE
#define FTRACE_E(...) \
	do \
	{ \
		char buf[256]; \
		int l = mrc_sprintf(buf, __VA_ARGS__); \
		filetrace(buf, l, TRACETYPE_ERROR);\
	} while (0)
#else
#define FTRACE_I(...)
#endif

//打印 警告
#ifndef _EXT_RELEASE
#define FTRACE_W(...) \
	do \
	{ \
		char buf[256]; \
		int l = mrc_sprintf(buf, __VA_ARGS__); \
		filetrace(buf, l, TRACETYPE_WARING);\
	} while (0)
#else
#define FTRACE_I(...)
#endif

#endif