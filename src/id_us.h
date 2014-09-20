/* Keen Dreams Source Code
 * Copyright (C) 2014 Javier M. Chavez
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

//
//	ID Engine
//	ID_US.h - Header file for the User Manager
//	v1.0d1
//	By Jason Blochowiak
//

#ifndef	__TYPES__
#include "id_types.h"
#endif

#ifndef	__ID_US__
#define	__ID_US__

#ifdef	__DEBUG__
#define	__DEBUG_UserMgr__
#endif

#define	HELPTEXTLINKED

#define	MaxString	128	// Maximum input string size

typedef	struct
		{
			id0_int_t	x,y,
				w,h,
				px,py;
		} WindowRec;	// Record used to save & restore screen windows

typedef	enum
		{
			gd_Continue,
			gd_Easy,
			gd_Normal,
			gd_Hard
		} GameDiff;

extern	id0_boolean_t		ingame,	// Set by game code if a game is in progress
					abortgame,	// Set if a game load failed
					loadedgame;	// Set if the current game was loaded
extern	id0_char_t		*abortprogram;	// Set to error msg if program is dying
extern	GameDiff	restartgame;	// Normally gd_Continue, else starts game
extern	id0_word_t		PrintX,PrintY;	// Current printing location in the window
extern	id0_word_t		WindowX,WindowY,// Current location of window
					WindowW,WindowH;// Current size of window

#define	US_HomeWindow()	{PrintX = WindowX; PrintY = WindowY;}

extern	void	US_Startup(void),
				US_Setup(void),
				US_Shutdown(void),
				US_InitRndT(id0_boolean_t randomize),
				US_SetLoadSaveHooks(id0_boolean_t (*load)(int),
									id0_boolean_t (*save)(int),
									void (*reset)(void)),
				US_TextScreen(void),
				US_UpdateTextScreen(void),
				US_FinishTextScreen(void),
				US_ControlPanel(void),
				US_DrawWindow(id0_word_t x,id0_word_t y,id0_word_t w,id0_word_t h),
				US_CenterWindow(id0_word_t,id0_word_t),
				US_SaveWindow(WindowRec *win),
				US_RestoreWindow(WindowRec *win),
				US_ClearWindow(void),
				US_SetPrintRoutines(void (*measure)(id0_char_t id0_far *,id0_word_t *,id0_word_t *),
									void (*print)(id0_char_t id0_far *)),
				US_PrintCentered(id0_char_t *s),
				US_CPrint(id0_char_t *s),
				US_CPrintLine(id0_char_t *s),
				US_Print(id0_char_t *s),
				US_PrintUnsigned(id0_longword_t n),
				US_PrintSigned(id0_long_t n),
				US_StartCursor(void),
				US_ShutCursor(void),
				US_ControlPanel(void),
				US_CheckHighScore(id0_long_t score,id0_word_t other),
				US_DisplayHighScores(id0_int_t which);
extern	id0_boolean_t	US_UpdateCursor(void),
				US_LineInput(id0_int_t x,id0_int_t y,id0_char_t *buf,id0_char_t *def,id0_boolean_t escok,
								id0_int_t maxchars,id0_int_t maxwidth);
extern	id0_int_t		US_CheckParm(id0_char_t *parm,id0_char_t **strings),
				US_RndT(void);

#endif
