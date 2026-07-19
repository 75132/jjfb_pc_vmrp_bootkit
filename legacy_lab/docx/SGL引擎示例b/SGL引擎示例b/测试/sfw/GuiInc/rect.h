/*
** funs.h: the functions header file.
**
** Copyright (C) 2007-2008 SKY-MOBI AS.  All rights reserved.
**
** This file is part of the simple gui library.
** It may not be redistributed under any circumstances.
*/

#ifndef _SGL_RECT_H
#define _SGL_RECT_H

#include "gal_i.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * \brief check 2 rect is intersect
 *
 * \param psrc1 the first rect
 * \param psrc2 the second rect
 */
BOOL GUIAPI DoesIntersect(PRECT psrc1, PRECT psrc2);

/**
 * \brief union psrc to pdst 取2矩形并集
 *
 * \param pdst the dest rect
 * \param psrc the src rect
 */
VOID GUIAPI UnionRect(PRECT pdst, PRECT psrc);

/**
 * \brief intersect rects 取2矩形交集
 *
 * \param pdst the result rect
 * \param psrc1 the first rect
 * \param psrc2 the second rect
 */
VOID GUIAPI IntersectRect(PRECT pdst, PRECT psrc1, PRECT psrc2);

#define SGL_SET_RECT(rDst, al, at, aw, ah) \
	do \
	{ \
		rDst.left = (int)(al);	\
		rDst.top = (int)(at); 	\
		rDst.width = (int)(aw); 	\
		rDst.height = (int)(ah);	\
	} while (0)
	
#define SGL_SET_RECTA(rDst, rSrc) \
	do \
	{ \
		rDst.left = rSrc.left;	\
		rDst.top = rSrc.top; 	\
		rDst.width = rSrc.width; 	\
		rDst.height = rSrc.height;	\
	} while (0)
	
#define SGL_SET_RECTB(rDst, prSrc) \
	do \
	{ \
		rDst.left = prSrc->left;	\
		rDst.top = prSrc->top; 	\
		rDst.width = prSrc->width; 	\
		rDst.height = prSrc->height;	\
	} while (0)

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _SGL_RECT_H */

