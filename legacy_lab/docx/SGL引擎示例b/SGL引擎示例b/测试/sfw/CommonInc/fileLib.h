#ifndef _FILELIB_H
#define _FILELIB_H


//获取文件名
int32 ExtractFileName(PCSTR path, char *out);

//获取文件拓展名 不带.
int32 ExtractFileExt(PCSTR name, char *out);

//获取文件所在文件夹
//返回：-1失败 0当前路径
int32 ExtractFileDir(PCSTR path, char *out);

//获取文件所在路径 末尾带 /
//返回：-1失败 0当前路径
int32 ExtractFilePath(PCSTR path, char* out);

int32 CopyFile(char *srcFile, char *destFile);
int32 CopyFileEx(char *srcFile, char *destFile, uint32 srcPos, uint32 destPos, uint32 copySize);

//复制文件-句柄版
//参数：源文件句柄，目标文件句柄，复制大小
//自己控制源文件、目标文件 开始读写指针，内部不处理
int32 CopyFileByHandle(int32 ifd, int32 ofd, uint32 copySize);

/**
 * 格式化路径字符串
 * 例：/aa//bb/c/ 得到 aa/bb/c （最标准的路径字符串）
 *
 * 返回格式化后的字符串
 */
int32 formatPathString( PSTR str );

#endif