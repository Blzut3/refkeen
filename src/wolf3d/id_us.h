//
//	ID Engine
//	ID_US.h - Header file for the User Manager
//	v1.0d1
//	By Jason Blochowiak
//

#ifndef	__ID_US__
#define	__ID_US__

#ifdef	__DEBUG__
#define	__DEBUG_UserMgr__
#endif

//#define	HELPTEXTLINKED

#define	MaxX	320
#define	MaxY	200

#define	MaxHelpLines	500

#define	MaxHighName	57
#define	MaxScores	7
typedef	struct
		{
			id0_char_t	name[MaxHighName + 1];
			id0_long_t	score;
			// *** SHAREWARE V1.0 APOGEE RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL1AP10)
			id0_word_t	completed;
#else
			id0_word_t	completed,episode;
#endif
		} HighScore;

#define	MaxGameName		32
#define	MaxSaveGames	6
typedef	struct
		{
			id0_char_t	signature[4];
			id0_word_t	*oldtest;
			id0_boolean_t	present;
			id0_char_t	name[MaxGameName + 1];
		} SaveGame;

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

//	Hack import for TED launch support
extern	id0_boolean_t		tedlevel;
// *** PRE-V1.4 APOGEE RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL6AP11)
extern	id0_word_t			tedlevelnum;
#else
extern	id0_int_t			tedlevelnum;
#endif
extern	void		TEDDeath(void);

extern	id0_boolean_t		ingame,		// Set by game code if a game is in progress
					abortgame,	// Set if a game load failed
					loadedgame,	// Set if the current game was loaded
					NoWait,
					HighScoresDirty;
extern	id0_char_t		*abortprogram;	// Set to error msg if program is dying
extern	GameDiff	restartgame;	// Normally gd_Continue, else starts game
extern	id0_word_t		PrintX,PrintY;	// Current printing location in the window
extern	id0_word_t		WindowX,WindowY,// Current location of window
					WindowW,WindowH;// Current size of window

extern	id0_boolean_t		Button0,Button1,
					CursorBad;
extern	id0_int_t			CursorX,CursorY;

extern	void		(*USL_MeasureString)(id0_char_t id0_far *,id0_word_t *,id0_word_t *),
					(*USL_DrawString)(id0_char_t id0_far *);

extern	id0_boolean_t		(*USL_SaveGame)(id0_int_t),(*USL_LoadGame)(id0_int_t);
extern	void		(*USL_ResetGame)(void);
extern	SaveGame	Games[MaxSaveGames];
extern	HighScore	Scores[];

#define	US_HomeWindow()	{PrintX = WindowX; PrintY = WindowY;}

extern	void	US_Startup(void),
				US_Setup(void),
				US_Shutdown(void),
				US_InitRndT(id0_boolean_t randomize),
				US_SetLoadSaveHooks(id0_boolean_t (*load)(id0_int_t),
									id0_boolean_t (*save)(id0_int_t),
									void (*reset)(void)),
				US_TextScreen(void),
				US_UpdateTextScreen(void),
				US_FinishTextScreen(void),
				US_DrawWindow(id0_word_t x,id0_word_t y,id0_word_t w,id0_word_t h),
				US_CenterWindow(id0_word_t,id0_word_t),
				US_SaveWindow(WindowRec *win),
				US_RestoreWindow(WindowRec *win),
				US_ClearWindow(void),
				US_SetPrintRoutines(void (*measure)(id0_char_t id0_far *,id0_word_t *,id0_word_t *),
									void (*print)(id0_char_t id0_far *)),
				US_PrintCentered(id0_char_t GAMEVER_COND_FARPTR *s),
				US_CPrint(id0_char_t GAMEVER_COND_FARPTR *s),
				US_CPrintLine(id0_char_t GAMEVER_COND_FARPTR *s),
				US_Print(id0_char_t GAMEVER_COND_FARPTR *s),
				US_PrintUnsigned(id0_longword_t n),
				US_PrintSigned(id0_long_t n),
				US_StartCursor(void),
				US_ShutCursor(void),
				US_CheckHighScore(id0_long_t score,id0_word_t other),
				US_DisplayHighScores(id0_int_t which);
extern	id0_boolean_t	US_UpdateCursor(void),
				US_LineInput(id0_int_t x,id0_int_t y,id0_char_t *buf,id0_char_t *def,id0_boolean_t escok,
								id0_int_t maxchars,id0_int_t maxwidth);
extern	id0_int_t		US_CheckParm(id0_char_t *parm,id0_char_t **strings),
				US_RndT(void);

		void	USL_PrintInCenter(id0_char_t GAMEVER_COND_FARPTR *s,Rect r);
		id0_char_t 	*USL_GiveSaveName(id0_word_t game);
#endif
