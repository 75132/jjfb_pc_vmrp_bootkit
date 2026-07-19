/*
** bmp.h: the bitmap header file.
**
** Copyright (C) 2007-2008 SKY-MOBI AS.  All rights reserved.
**
** This file is part of the simple gui library.
** It may not be redistributed under any circumstances.
*/

#ifndef _SGL_BMP_H
#define _SGL_BMP_H

#include "types_i.h"

#ifdef __cplusplus
extern "C"{
#endif  /* __cplusplus */


/**
  \defgroup bitmap Bitmap Management

  Bitmap Management is something like Strings Management, but the difference is 
  bitmap management sub module just keep the bitmap files information but not the data.
 
  The steps to work with bitmap sub module.
	- Fill the bitmap files information in the excel sheet(see the image).
		Different screen size maybe have different bitmap file information.
	- Enable one screen size, just define the macro for that column. 
		When SCREEN_SIZE_176220 defined, that SCREEN_SIZE_176220 column information will be used.
	- Compile the correct bitmap files to the mrp file.
	- Load the bitmap with it's ID
	\code
		#include "bmp.h"
		...
		//a bitmap id "BMP_BUSY"
		int w, h; //width and height
		HBITMAP bmp = SGL_LoadBitmap(BMP_BUSY, &w, &h);
	\endcode

   <br><br>
   \image html bitmaps.gif "Excel Manangement Bitmap information"

  @ingroup resource
  @{
 */
	 
#include "res_bmp.h"


typedef struct  
{
	uint16 *buf;
	int w, h;
}T_BMP565, *PT_BMP565;

/**
 * \brief Load bitmap by it's ID, and user can release the bitmap by call SGL_ReleaseBitmap.
 *
 * \param bmp the bitmap id
 * \param[out] width the bitmap width, can be NULL when not requiered 
 * \param[out] height the bitmap height, can be NULL when not required
 * \return
 *	- the loaded bitmap handle on success
 *	- NULL otherwise
 * \sa SGL_ReleaseBitmap.
 */
HBITMAP GUIAPI SGL_LoadBitmap(DWORD resid, int* width, int* height);

/**
 * \brief Destroy a bitmap and free it's memory, nothing happen when the bitmap not loaded before.
 *
 * \param bmp the bitmap id
 * \sa SGL_LoadBitmap
 */
VOID GUIAPI SGL_ReleaseBitmap(DWORD resid);

//绘制指定ID的bitmap，全图
VOID GUIAPI SGL_DrawBitmap(DWORD resid, int x, int y, uint16 rop);

/** @} end of bitmap */
	
#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _SGL_BMP_H */

