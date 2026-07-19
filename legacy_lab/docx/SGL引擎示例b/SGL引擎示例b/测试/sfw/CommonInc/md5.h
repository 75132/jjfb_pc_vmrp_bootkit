#ifndef _MD5_H
#define _MD5_H

#include "types_i.h"

/**
 * \brief wraper of above functions
 *
 * \param szSour the source string
 * \param iLen the source string length
 * \param[out] szDest 16 bytes memory for output
 */
VOID MD5(BYTE *szSour, int iLen, BYTE *szDest);

/**
 * MD5 校验函数
 * 说明：如果是包内文件，filename即为该文件的名字，如果不是，则filename可置为NULL
 * offset：   开始计算的位置(适用于保内文件校验)
 * correct：  正确的MD5码
 * CheckMrphead：  是否为包内文件
 * 返回值：
 *		MR_SUCCESS：检验成功
 */
int32 MD5Checksum(PSTR filename, int32 offset, BOOL CheckMrphead, PSTR correct);

//从文件名计算MD5值
int32 MD5_MakeFromFile(char *key, char *filename, int32 offset, int32 len, uint8* out);

#endif /*_MD5_H*/