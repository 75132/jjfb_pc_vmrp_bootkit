#ifndef _GB2UNICODE_H_
#define _GB2UNICODE_H_

/* 说明
 * 参数： gbCode：待转换的gb字符串
 *		   gbLen：gb字符串字节长度
 *		   unicode：保存转换后的unicode缓冲区（该内存自己申请）
 *
 * 返回： 成功：转换后unicode长度
 *		   失败：-1
 */
int gb2uniBE(unsigned char *gbCode, unsigned char *unicode, int bufSize);

#endif