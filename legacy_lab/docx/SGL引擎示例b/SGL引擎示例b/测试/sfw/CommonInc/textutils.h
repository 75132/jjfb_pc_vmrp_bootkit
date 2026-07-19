#ifndef _TEXTUTILS_H
#define _TEXTUTILS_H


//复制count个字，不是字节
void textncpy_gb(PCSTR src, int count, PSTR dst);

//单行取指定宽度文本，末尾追加 ...
//还会消除换行符
void textSplitByWidth(PCWSTR src, PWSTR dst, int width, E_GAL_FONTTYPE font);

//分割浮点数 输出 integer: 整数部分 decimal: 小数部分
//只取小数点后2位
void floatSplit(float f, int* integer, int* decimal);

//浮点转 百分比字符串 xx.xx%
//只取小数点后2位
void float2percstr(char *outBuf, float f);

//整理unicode be 字符串 消除换行符 制表符等
VOID unicodeStringDump( PWSTR str );

#endif
