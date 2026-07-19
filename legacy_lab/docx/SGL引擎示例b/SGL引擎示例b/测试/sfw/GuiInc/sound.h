/*
** sound.h: the sound header file.
**
** Copyright (C) 2007-2008 SKY-MOBI AS.  All rights reserved.
**
** This file is part of the simple gui library.
** It may not be redistributed under any circumstances.
*/

#ifndef _SGL_SOUND_H
#define _SGL_SOUD_H

#include "types_i.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */


/**
  \defgroup sound Sound Managment.

  Sound Management same as Bitmap Management just keep the sound files information.

  The steps to work with sound sub module:
	- Fill the sound information excel(see the image).
	- Compile the correct sound files to the mrp.
	- Play the sound with that sound ID.
	\code
		#include "sound.h"
		...
		//a sound has the id "SOUND_ONLINE"
		SGL_PlaySound(SOUND_ONLINE, FALSE);
	\endcode
  
  <br><br>
  \image html sounds.gif "Excel Manangement Sound information"
  
  @ingroup resource
  @{
 */

#include "res_sound.h"

/**
 * \brief Play a sound by it's id.
 *
 * \param sound the sound id
 * \param loop the play mode
 *	- TRUE play the sound in loop
 *	- FALSE play the sound once
 * \return TRUE on success, FALSE otherwise
 */
BOOL GUIAPI SGL_PlaySound(DWORD sound, BOOL loop);

/**
 * \brief Stop a sound.
 *
 * \param sound the sound id
 * \return TRUE on success, FALSE otherwise
 */
BOOL GUIAPI SGL_StopSound(DWORD sound);

/**
 * \brief Release the sound resource.
 *
 * \param sound the sound id.
 * \note nothing happen when the sound not loaded.
 */
VOID GUIAPI SGL_ReleaseSound(DWORD sound);

/** @} end of sound */

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif
