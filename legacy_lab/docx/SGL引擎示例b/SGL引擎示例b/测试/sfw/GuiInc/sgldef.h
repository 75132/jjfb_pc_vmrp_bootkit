// SGL内部通用的一些全局变量等 [4/30/2012 JianbinZhu]

#ifndef _SGLDEF_H_
#define _SGLDEF_H_


#define CHR_GB		"\x9f\x0e\x00\x00" //鼎
#define CHR_EN		"\x00\x61\x00\x00" //a //"\x00\x45\x00\x00"
#define WORD_GB		0x9f0e
#define WORD_EN		0x0061

#define DIR_SYSTEM	"system"
#define DIR_FONT	"fonts"



//释放一个缓冲并设为NULL
#define FREE_SET_NULL(ap) \
	do \
	{ \
	if(ap){ \
	free(ap); ap = NULL; \
	} \
	} while (0)

//安全关闭文件
#define FCLOSE(afd) \
	do \
	{ \
	if(afd) { \
	mrc_close(afd); \
	afd = 0; \
	} \
	} while (0)

//检查文件是否存在
#define IS_FILE_EXIST(apath) \
	(MR_IS_FILE == mrc_fileState(apath))

//检查目录是否存在
#define IS_DIR_EXIST(apath) \
	(MR_IS_DIR == mrc_fileState(apath))

//检查并创建目录
#define CHECK_ADN_MKDIR(apath) \
	if(!IS_DIR_EXIST(apath)) mrc_mkDir(apath)

//检查文件存在则删除它
#define CHECK_AND_DELFILE(apath) \
	if (IS_FILE_EXIST(apath)) mrc_remove(apath)


#endif