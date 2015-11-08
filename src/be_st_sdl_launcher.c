/* Copyright (C) 2015 NY00123
 *
 * This file is part of Reflection Keen.
 *
 * Reflection Keen is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "SDL.h"

#include "refkeen.h"
#include "be_st_sdl_private.h"
#include "be_title_and_version.h"

// Borrowed from other files
extern const uint32_t g_sdlEGABGRAScreenColors[];
extern SDL_GameController *g_sdlControllers[BE_ST_MAXJOYSTICKS];
extern SDL_Window *g_sdlWindow;
extern SDL_Renderer *g_sdlRenderer;
extern SDL_Texture *g_sdlTexture, *g_sdlTargetTexture;
extern SDL_Rect g_sdlAspectCorrectionBorderedRect;
extern const int g_sdlJoystickAxisBinaryThreshold, g_sdlJoystickAxisDeadZone, g_sdlJoystickAxisMax, g_sdlJoystickAxisMaxMinusDeadZone;

static int g_sdlKeyboardLastKeyPressed;
static bool g_sdlKeyboardLastKeyPressedIsShifted;
static int g_sdlControllerLastButtonPressed;

static uint32_t g_sdlInputLastBinaryPressTime;
static uint32_t g_sdlInputLastBinaryPressTimeDelay;

// Need these for triggers while letting the user selection buttons for gameplay
static bool g_sdlLauncherTriggerBinaryStates[2];

static uint8_t g_sdlLauncherGfxCache[BE_LAUNCHER_PIX_WIDTH*BE_LAUNCHER_PIX_HEIGHT]; // Launcher gets pointer to this for drawing
static bool g_sdlLauncherGfxCacheMarked = false;

#define BE_LAUNCHER_MAX_CHOICE_STRBUFFLEN 10

/*** Convenience macros - Note that the label *must* be a C string literal ***/
#define BEMENUITEM_DEF_TARGETMENU(menuItemName, label, menuPtr) BEMENUITEM_DEF_GENERIC(menuItemName, NULL, NULL, menuPtr, label, BE_MENUITEM_TYPE_TARGETMENU)
#define BEMENUITEM_DEF_SELECTION(menuItemName, label, choices) BEMENUITEM_DEF_GENERIC(menuItemName, NULL, choices, NULL, label, BE_MENUITEM_TYPE_SELECTION)
#define BEMENUITEM_DEF_HANDLER(menuItemName, label, handlerPtr) BEMENUITEM_DEF_GENERIC(menuItemName, handlerPtr, NULL, NULL, label, BE_MENUITEM_TYPE_HANDLER)
#define BEMENUITEM_DEF_SELECTION_WITH_HANDLER(menuItemName, label, choices, handlerPtr) BEMENUITEM_DEF_GENERIC(menuItemName, handlerPtr, choices, NULL, label, BE_MENUITEM_TYPE_SELECTION_WITH_HANDLER)
#define BEMENUITEM_DEF_DYNAMIC_SELECTION(menuItemName, label, choices, handlerPtr) BEMENUITEM_DEF_GENERIC(menuItemName, handlerPtr, choices, NULL, label, BE_MENUITEM_TYPE_DYNAMIC_SELECTION)
#define BEMENUITEM_DEF_STATIC(menuItemName, label) BEMENUITEM_DEF_GENERIC(menuItemName, NULL, NULL, NULL, label, BE_MENUITEM_TYPE_STATIC)

// A little hack - Store a copy of the label string literal that can be modified
#define BEMENUITEM_DEF_GENERIC(menuItemName, handlerPtr, choices, menuPtr, label, type) \
	static char menuItemName ## _label[] = label; \
	static BEMenuItem menuItemName = {handlerPtr, choices, menuPtr, menuItemName ## _label, 0, 0, 0, 0, 0, 0, type};

static const char *g_be_settingsChoices_boolean[] = {"No","Yes",NULL};

/*** Main menu ***/

BEMENUITEM_DEF_TARGETMENU(g_beMainMenuItem_PlayGame, "Play game", &g_beSelectGameMenu)
BEMENUITEM_DEF_TARGETMENU(g_beMainMenuItem_Settings, "Settings", &g_beSettingsMenu)
BEMENUITEM_DEF_TARGETMENU(g_beMainMenuItem_ShowVersion, "Show version", &g_beShowVersionMenu)
BEMENUITEM_DEF_TARGETMENU(g_beMainMenuItem_Quit, "Quit", &g_beQuitConfirmMenu)

BEMenu g_beMainMenu = {
	REFKEEN_TITLE_STRING,
	&g_beQuitConfirmMenu,
	(BEMenuItem *[])
	{
		&g_beMainMenuItem_PlayGame,
		&g_beMainMenuItem_Settings,
		&g_beMainMenuItem_ShowVersion,
		&g_beMainMenuItem_Quit,
		NULL
	},
	// Ignore the rest
};

/*** Select game menu ***/

/*static*/ BEMenuItem g_beSelectGameMenuItems[BE_GAMEVER_LAST];
/*static*/ char g_beSelectGameMenuItemsStrs[BE_GAMEVER_LAST][78]; // Should be MUTABLE strings for layout preparation
/*static*/ BEMenuItem *g_beSelectGameMenuItemsPtrs[BE_GAMEVER_LAST+3];

BEMENUITEM_DEF_TARGETMENU(g_beSelectGameMenuItem_DisappearedGameHelp, "Help! An installed game disappeared from the list!", &g_beDisappearedGameHelpMenu)
BEMENUITEM_DEF_TARGETMENU(g_beSelectGameMenuItem_AddMissingGameVersion, "Add missing game version", &g_beSelectInitialPathMenu)

BEMenu g_beSelectGameMenu = {
	"Select game",
	&g_beMainMenu,
	g_beSelectGameMenuItemsPtrs, // Array of menu items
	// Ignore the rest
};

BEMENUITEM_DEF_STATIC(g_beDisappearedGameHelpMenuItem_Explanation,
"Reflection Keen can detect compatible DOS game versions from certain installations, including the Catacombs games from GOG.com. Once such a game installation is updated in any minor way, Reflection Keen may fail to locate it. These are the expected behaviors.\nAs an alternative, you can manually add a compatible game installation (if not yet listed)."
);

/*** Disappeared game menu ***/

BEMenu g_beDisappearedGameHelpMenu = {
	"Where may a game disappear",
	&g_beSelectGameMenu,
	(BEMenuItem *[])
	{
		&g_beDisappearedGameHelpMenuItem_Explanation,
		NULL
	},
	// Ignore the rest
};

/*** Select initial path menu ***/

static BEMenuItem *g_beSelectInitialPathMenuItems;
static BEMenuItem **g_beSelectInitialPathMenuItemsPtrs;
static char *g_beSelectInitialPathMenuItemsStrsBuffer;

BEMenu g_beSelectInitialPathMenu = {
	"Select initial path",
	&g_beSelectGameMenu,
	NULL, // Dynamically allocated, filled later
	// Ignore the rest
};

/*** Select directory menu (basically a dialog that may change often) ***/

BEMenu g_beSelectDirectoryMenu = {
	"Select directory",
	NULL, // SPECIAL (using back button handler)
	NULL, // Dynamically allocated, filled later
	&BE_Launcher_Handler_DirectorySelectionGoPrev, // SPECIAL (back button handler)
	// Ignore the rest
};

/*** Select directory error menu ***/

BEMENUITEM_DEF_HANDLER(g_beSelectDirectoryErrorMenuItem_DisappearedGameHelp, "Try to go up in directories hierachy", &BE_Launcher_Handler_DirectorySelectionGoPrev)

BEMenu g_beSelectDirectoryErrorMenu = {
	"Failed to select directory",
	NULL, // SPECIAL (using back button handler)
	(BEMenuItem *[])
	{
		&g_beSelectDirectoryErrorMenuItem_DisappearedGameHelp,
		NULL
	},
	&BE_Launcher_Handler_DirectorySelectionGoPrev, // SPECIAL (back button handler)
	// Ignore the rest
};

/*** Compatible game found menu ***/

// At this point, all directory-selection related resources are freed
BEMENUITEM_DEF_TARGETMENU(g_beSelectDirectoryFoundGameMenuItem_ShowGamesList, "Show games list", &g_beSelectGameMenu)

BEMenu g_beSelectDirectoryFoundGameMenu = {
	"New compatible game found!",
	&g_beSelectGameMenu,
	(BEMenuItem *[])
	{
		&g_beSelectDirectoryFoundGameMenuItem_ShowGamesList,
		NULL
	},
	// Ignore the rest
};

/*** No additional compatible game found menu ***/

// Here, directory-selection related resources are STILL allocated
BEMENUITEM_DEF_TARGETMENU(g_beSelectDirectoryNoGameFoundMenu_ReturnToDirectory, "Return to directory", &g_beSelectDirectoryMenu)

BEMenu g_beSelectDirectoryNoGameFoundMenu = {
	"No new compatible game found",
	&g_beSelectDirectoryMenu,
	(BEMenuItem *[])
	{
		&g_beSelectDirectoryNoGameFoundMenu_ReturnToDirectory,
		NULL
	},
	// Ignore the rest
};

/*** Settings menu ***/

BEMENUITEM_DEF_TARGETMENU(g_beSettingsMenuItem_VideoSettings, "Video settings", &g_beVideoSettingsMenu)
BEMENUITEM_DEF_TARGETMENU(g_beSettingsMenuItem_SoundSettings, "Sound settings", &g_beSoundSettingsMenu)
BEMENUITEM_DEF_TARGETMENU(g_beSettingsMenuItem_InputSettings, "Input settings", &g_beInputSettingsMenu)

BEMenu g_beSettingsMenu = {
	"Settings",
	&g_beMainMenu,
	(BEMenuItem *[])
	{
		&g_beSettingsMenuItem_VideoSettings,
		&g_beSettingsMenuItem_SoundSettings,
		&g_beSettingsMenuItem_InputSettings,
		NULL
	},
	// Ignore the rest
};

/** Video settings menu ***/

#define BE_LAUNCHER_MAX_NUM_OF_RESOLUTIONS 10

typedef struct {
	int width, height;
} BEMenuItemScreenResPair;

static BEMenuItemScreenResPair g_be_videoSettingsChoices_fullResolutionsVals[BE_LAUNCHER_MAX_NUM_OF_RESOLUTIONS];

static char g_be_videoSettingsChoices_fullResolutionsStrs[BE_LAUNCHER_MAX_NUM_OF_RESOLUTIONS][BE_LAUNCHER_MAX_CHOICE_STRBUFFLEN];
// Need to add an additional desktop/default entry, plus NULL terminator
static const char *g_be_videoSettingsChoices_fullResolutions[BE_LAUNCHER_MAX_NUM_OF_RESOLUTIONS+2];

static const char *g_be_videoSettingsChoices_displayNums[] = {"0","1","2","3","4","5","6","7",NULL};

#define BE_LAUNCHER_MAX_NUM_OF_SDL_RENDERER_DRIVERS 10

static char g_be_videoSettingsChoices_sdlRendererDriversStrs[BE_LAUNCHER_MAX_NUM_OF_SDL_RENDERER_DRIVERS][BE_LAUNCHER_MAX_CHOICE_STRBUFFLEN];
// Need to add additional auto entry, plus NULL terminator
static const char *g_be_videoSettingsChoices_sdlRendererDrivers[BE_LAUNCHER_MAX_NUM_OF_SDL_RENDERER_DRIVERS+2];

static const char *g_be_videoSettingsChoices_vSync[] = {"Auto","Off","On",NULL};
static const char *g_be_videoSettingsChoices_scaleType[] = {"4:3","Fill",NULL};
static const char *g_be_videoSettingsChoices_scaleFactor[] = {"1","2","3","4",NULL};
static const char *g_be_videoSettingsChoices_launcherWindowType[] = {"Default","Fullscreen","Software",NULL};

static void BEL_ST_Launcher_Handler_DisplayNum(BEMenuItem **menuItemP);

BEMENUITEM_DEF_SELECTION(g_beVideoSettingsMenuItem_Fullscreen, "Fullscreen", g_be_settingsChoices_boolean)
BEMENUITEM_DEF_SELECTION(g_beVideoSettingsMenuItem_FullscreenRes, "Fullscreen resolution", g_be_videoSettingsChoices_fullResolutions)
BEMENUITEM_DEF_SELECTION_WITH_HANDLER(g_beVideoSettingsMenuItem_DisplayNum, "Display number", g_be_videoSettingsChoices_displayNums, &BEL_ST_Launcher_Handler_DisplayNum)
BEMENUITEM_DEF_SELECTION(g_beVideoSettingsMenuItem_SDLRenderer, "SDL renderer", g_be_videoSettingsChoices_sdlRendererDrivers)
BEMENUITEM_DEF_SELECTION(g_beVideoSettingsMenuItem_VSync, "VSync", g_be_videoSettingsChoices_vSync)
BEMENUITEM_DEF_SELECTION(g_beVideoSettingsMenuItem_Bilinear, "Bilinear interpolation", g_be_settingsChoices_boolean)
BEMENUITEM_DEF_SELECTION(g_beVideoSettingsMenuItem_ScaleType, "Scale type*", g_be_videoSettingsChoices_scaleType)
BEMENUITEM_DEF_SELECTION(g_beVideoSettingsMenuItem_ScaleFactor, "Scale factor", g_be_videoSettingsChoices_scaleFactor)
BEMENUITEM_DEF_SELECTION(g_beVideoSettingsMenuItem_ForceFullSoftScaling, "Force full software scaling*", g_be_settingsChoices_boolean)
BEMENUITEM_DEF_SELECTION(g_beVideoSettingsMenuItem_LauncherWindowType, "Launcher window type", g_be_videoSettingsChoices_launcherWindowType)
BEMENUITEM_DEF_STATIC(g_beVideoSettingsMenuItem_SoftScalingComment,
"* Full software scaling should be manually toggled. Note that it can lead to great slowdowns with high-resolution windows."
);

BEMenu g_beVideoSettingsMenu = {
	"Video settings",
	&g_beSettingsMenu,
	(BEMenuItem *[])
	{
		&g_beVideoSettingsMenuItem_Fullscreen,
		&g_beVideoSettingsMenuItem_FullscreenRes,
		&g_beVideoSettingsMenuItem_DisplayNum,
		&g_beVideoSettingsMenuItem_SDLRenderer,
		&g_beVideoSettingsMenuItem_VSync,
		&g_beVideoSettingsMenuItem_Bilinear,
		&g_beVideoSettingsMenuItem_ScaleType,
		&g_beVideoSettingsMenuItem_ScaleFactor,
		&g_beVideoSettingsMenuItem_ForceFullSoftScaling,
		&g_beVideoSettingsMenuItem_LauncherWindowType,
		&g_beVideoSettingsMenuItem_SoftScalingComment,
		NULL
	},
	// Ignore the rest
};

/*** Sounds settings menu ***/

static const int g_be_soundsSettingsChoices_sndSampleRateVals[] = {8000, 11025, 12000, 16000, 22050, 32000, 44100, 48000, 49716, 96000};
static const char *g_be_soundsSettingsChoices_sndSampleRate[] = {"8000","11025","12000","16000","22050","32000","44100","48000","49716","96000",NULL};

BEMENUITEM_DEF_SELECTION(g_beSoundSettingsMenuItem_SndSampleRate, "Sound sample rate\n(in Hz)", g_be_soundsSettingsChoices_sndSampleRate)
BEMENUITEM_DEF_SELECTION(g_beSoundSettingsMenuItem_SndSubSystem, "Enable sound subsystem", g_be_settingsChoices_boolean)
BEMENUITEM_DEF_SELECTION(g_beSoundSettingsMenuItem_OPLEmulation, "Enable OPL emulation", g_be_settingsChoices_boolean)

BEMenu g_beSoundSettingsMenu = {
	"Sounds settings",
	&g_beSettingsMenu,
	(BEMenuItem *[])
	{
		&g_beSoundSettingsMenuItem_SndSampleRate,
		&g_beSoundSettingsMenuItem_SndSubSystem,
		&g_beSoundSettingsMenuItem_OPLEmulation,
		NULL
	},
	// Ignore the rest
};

/*** Input settings menu ***/

static const char *g_be_inputSettingsChoices_controllerScheme[] = {"Classic", "Modern", NULL};

BEMENUITEM_DEF_TARGETMENU(g_beInputSettingsMenuItem_ControllerSettings, "Modern controller settings", &g_beControllerSettingsMenu)
BEMENUITEM_DEF_SELECTION(g_beInputSettingsMenuItem_ControllerScheme, "Game controller scheme", g_be_inputSettingsChoices_controllerScheme)
BEMENUITEM_DEF_SELECTION(g_beInputSettingsMenuItem_Autolock, "Autolock mouse cursor\nin fullscreen", g_be_settingsChoices_boolean)

BEMenu g_beInputSettingsMenu = {
	"Input settings",
	&g_beSettingsMenu,
	(BEMenuItem *[])
	{
		&g_beInputSettingsMenuItem_ControllerSettings,
		&g_beInputSettingsMenuItem_ControllerScheme,
		&g_beInputSettingsMenuItem_Autolock,
		NULL
	},
	// Ignore the rest
};

/*** Controller settings menu ***/

static const char *g_be_controllerSettingsChoices_actionButton[] = {"A", "B", "X", "Y", NULL, NULL, NULL, "LStick" ,"RStick", "LShoulder", "RShoulder", NULL, NULL, NULL, NULL, "LTrigger", "RTrigger", "N/A"};
#ifdef REFKEEN_VER_CATACOMB_ALL
static const char *g_be_controllerSettingsChoices_analogMotion[] = {"Keyboard", "Mouse", NULL};
#endif


#ifdef REFKEEN_VER_KDREAMS
BEMENUITEM_DEF_DYNAMIC_SELECTION(g_beControllerSettingsMenuItem_Action_Jump, "Action - Jump", g_be_controllerSettingsChoices_actionButton, &BE_Launcher_Handler_ControllerAction)
BEMENUITEM_DEF_DYNAMIC_SELECTION(g_beControllerSettingsMenuItem_Action_Throw, "Action - Throw", g_be_controllerSettingsChoices_actionButton, &BE_Launcher_Handler_ControllerAction)
BEMENUITEM_DEF_DYNAMIC_SELECTION(g_beControllerSettingsMenuItem_Action_Stats, "Action - Stats", g_be_controllerSettingsChoices_actionButton, &BE_Launcher_Handler_ControllerAction)
#else
BEMENUITEM_DEF_DYNAMIC_SELECTION(g_beControllerSettingsMenuItem_Action_Fire, "Action - Fire", g_be_controllerSettingsChoices_actionButton, &BE_Launcher_Handler_ControllerAction)
BEMENUITEM_DEF_DYNAMIC_SELECTION(g_beControllerSettingsMenuItem_Action_Strafe, "Action - Strafe", g_be_controllerSettingsChoices_actionButton, &BE_Launcher_Handler_ControllerAction)
BEMENUITEM_DEF_DYNAMIC_SELECTION(g_beControllerSettingsMenuItem_Action_Drink, "Action - Drink", g_be_controllerSettingsChoices_actionButton, &BE_Launcher_Handler_ControllerAction)
BEMENUITEM_DEF_DYNAMIC_SELECTION(g_beControllerSettingsMenuItem_Action_Bolt, "Action - Bolt/Zapper", g_be_controllerSettingsChoices_actionButton, &BE_Launcher_Handler_ControllerAction)
BEMENUITEM_DEF_DYNAMIC_SELECTION(g_beControllerSettingsMenuItem_Action_Nuke, "Action - Nuke/Xterminator", g_be_controllerSettingsChoices_actionButton, &BE_Launcher_Handler_ControllerAction)
BEMENUITEM_DEF_DYNAMIC_SELECTION(g_beControllerSettingsMenuItem_Action_FastTurn, "Action - Fast turn", g_be_controllerSettingsChoices_actionButton, &BE_Launcher_Handler_ControllerAction)
#endif
#if (defined REFKEEN_VER_CAT3D) || (defined REFKEEN_VER_CATABYSS)
BEMENUITEM_DEF_DYNAMIC_SELECTION(g_beControllerSettingsMenuItem_Action_Scrolls, "Action - Scrolls", g_be_controllerSettingsChoices_actionButton, &BE_Launcher_Handler_ControllerAction)
#endif
#if (defined REFKEEN_VER_KDREAMS) || (defined REFKEEN_VER_CATADVENTURES)
BEMENUITEM_DEF_DYNAMIC_SELECTION(g_beControllerSettingsMenuItem_Action_FuncKeys, "Action - Function keys", g_be_controllerSettingsChoices_actionButton, &BE_Launcher_Handler_ControllerAction)
#endif
BEMENUITEM_DEF_DYNAMIC_SELECTION(g_beControllerSettingsMenuItem_Action_DebugKeys, "Action - Debug keys", g_be_controllerSettingsChoices_actionButton, &BE_Launcher_Handler_ControllerAction)

BEMENUITEM_DEF_SELECTION(g_beControllerSettingsMenuItem_Dpad, "Use d-pad for motion", g_be_settingsChoices_boolean)
BEMENUITEM_DEF_SELECTION(g_beControllerSettingsMenuItem_LeftStick, "Use left stick", g_be_settingsChoices_boolean)
BEMENUITEM_DEF_SELECTION(g_beControllerSettingsMenuItem_RightStick, "Use right stick", g_be_settingsChoices_boolean)
#ifdef REFKEEN_VER_CATACOMB_ALL
BEMENUITEM_DEF_SELECTION(g_beControllerSettingsMenuItem_AnalogMotion, "Motion emulation mode", g_be_controllerSettingsChoices_analogMotion)
#endif

// TODO (REFKEEN) Add more settings for game controller

BEMenu g_beControllerSettingsMenu = {
	"Modern controller settings",
	&g_beInputSettingsMenu,
	(BEMenuItem *[])
	{
#ifdef REFKEEN_VER_KDREAMS
		&g_beControllerSettingsMenuItem_Action_Jump,
		&g_beControllerSettingsMenuItem_Action_Throw,
		&g_beControllerSettingsMenuItem_Action_Stats,
#else
		&g_beControllerSettingsMenuItem_Action_Fire,
		&g_beControllerSettingsMenuItem_Action_Strafe,
		&g_beControllerSettingsMenuItem_Action_Drink,
		&g_beControllerSettingsMenuItem_Action_Bolt,
		&g_beControllerSettingsMenuItem_Action_Nuke,
		&g_beControllerSettingsMenuItem_Action_FastTurn,
#endif
#if (defined REFKEEN_VER_CAT3D) || (defined REFKEEN_VER_CATABYSS)
		&g_beControllerSettingsMenuItem_Action_Scrolls,
#endif
#if (defined REFKEEN_VER_KDREAMS) || (defined REFKEEN_VER_CATADVENTURES)
		&g_beControllerSettingsMenuItem_Action_FuncKeys,
#endif
		&g_beControllerSettingsMenuItem_Action_DebugKeys,
		&g_beControllerSettingsMenuItem_Dpad,
		&g_beControllerSettingsMenuItem_LeftStick,
		&g_beControllerSettingsMenuItem_RightStick,
#ifdef REFKEEN_VER_CATACOMB_ALL
		&g_beControllerSettingsMenuItem_AnalogMotion,
#endif
		NULL
	},
	// Ignore the rest
};

/*** Show version menu ***/

BEMENUITEM_DEF_STATIC(g_beShowVersionMenuItem_Description, REFKEEN_TITLE_AND_VER_STRING ",\na part of Reflection Keen.")

BEMenu g_beShowVersionMenu = {
	"Version information",
	&g_beMainMenu,
	(BEMenuItem *[])
	{
		&g_beShowVersionMenuItem_Description,
		NULL
	},
	// Ignore the rest
};

/*** Quit confirm menu ***/

BEMENUITEM_DEF_HANDLER(g_beQuitConfirmMenuItem_Yes, "Yes", &BE_Launcher_Handler_MenuQuit)
BEMENUITEM_DEF_TARGETMENU(g_beQuitConfirmMenuItem_No, "No", &g_beMainMenu)

BEMenu g_beQuitConfirmMenu = {
	"Are you sure you want to quit?",
	&g_beMainMenu,
	(BEMenuItem *[])
	{
		&g_beQuitConfirmMenuItem_Yes,
		&g_beQuitConfirmMenuItem_No,
		NULL
	},
	// Ignore the rest
};

static void BEL_ST_Launcher_ResetDisplayModes(int displayNum);
static void BEL_ST_Launcher_SetGfxOutputRect();

void BE_ST_Launcher_Prepare(void)
{
	/*** Prepare ST stuff ***/

	/* Graphics */
	if (!g_refKeenCfg.launcherWinWidth || !g_refKeenCfg.launcherWinHeight || (g_refKeenCfg.launcherWinType == LAUNCHER_WINDOW_SOFTWARE))
	{
		g_refKeenCfg.launcherWinWidth = 2*BE_LAUNCHER_PIX_WIDTH;
		g_refKeenCfg.launcherWinHeight = 2*BE_LAUNCHER_PIX_HEIGHT;
	}

	g_sdlWindow = SDL_CreateWindow(
		REFKEEN_TITLE_AND_VER_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, g_refKeenCfg.launcherWinWidth, g_refKeenCfg.launcherWinHeight,
		(g_refKeenCfg.launcherWinType == LAUNCHER_WINDOW_FULL) ? SDL_WINDOW_FULLSCREEN_DESKTOP : ((g_refKeenCfg.launcherWinType == LAUNCHER_WINDOW_DEFAULT) ? SDL_WINDOW_RESIZABLE : 0)
	);
	if (!g_sdlWindow)
	{
		BE_Cross_LogMessage(BE_LOG_MSG_ERROR, "Failed to create SDL2 window for launcher,\n%s\n", SDL_GetError());
		exit(0);
	}
	g_sdlRenderer = SDL_CreateRenderer(g_sdlWindow, -1, (g_refKeenCfg.launcherWinType == LAUNCHER_WINDOW_SOFTWARE) ? SDL_RENDERER_SOFTWARE : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
	if (!g_sdlRenderer)
	{
		BE_Cross_LogMessage(BE_LOG_MSG_ERROR, "Failed to create SDL2 renderer for launcher,\n%s\n", SDL_GetError());
		//Destroy window?
		exit(0);
	}
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	g_sdlTexture = SDL_CreateTexture(g_sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, BE_LAUNCHER_PIX_WIDTH, BE_LAUNCHER_PIX_HEIGHT);
	if (!g_sdlTexture)
	{
		BE_Cross_LogMessage(BE_LOG_MSG_ERROR, "Failed to (re)create SDL2 texture for launcher,\n%s\n", SDL_GetError());
		//Destroy window and renderer?
		exit(0);
	}

	SDL_SetRenderDrawColor(g_sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE); // For clears in refreshes
	BEL_ST_Launcher_SetGfxOutputRect();

	if (g_refKeenCfg.launcherWinType != LAUNCHER_WINDOW_SOFTWARE)
	{
		// Try, if we fail then simply don't use this
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		g_sdlTargetTexture = SDL_CreateTexture(g_sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 2*BE_LAUNCHER_PIX_WIDTH, 2*BE_LAUNCHER_PIX_HEIGHT);
	}
	/* Game controllers */
	int nOfJoysticks = SDL_NumJoysticks();
	if (nOfJoysticks > BE_ST_MAXJOYSTICKS)
		nOfJoysticks = BE_ST_MAXJOYSTICKS;
	for (int i = 0; i < nOfJoysticks; ++i)
		if (SDL_IsGameController(i))
			g_sdlControllers[i] = SDL_GameControllerOpen(i);

	g_sdlKeyboardLastKeyPressed = SDL_SCANCODE_UNKNOWN;
	g_sdlControllerLastButtonPressed = SDL_CONTROLLER_BUTTON_INVALID;

	// Set fullscreen value
	g_beVideoSettingsMenuItem_Fullscreen.choice = g_refKeenCfg.isFullscreen;
	/*** Prepare displays list ***/
	int nOfDisplays = SDL_GetNumVideoDisplays();
	if (nOfDisplays >= (int)(sizeof(g_be_videoSettingsChoices_displayNums)/sizeof(*g_be_videoSettingsChoices_displayNums)))
		nOfDisplays = sizeof(g_be_videoSettingsChoices_displayNums)/sizeof(*g_be_videoSettingsChoices_displayNums) - 1;
	g_be_videoSettingsChoices_displayNums[nOfDisplays] = NULL;
	g_beVideoSettingsMenuItem_DisplayNum.choice = g_refKeenCfg.displayNum;
	if (g_beVideoSettingsMenuItem_DisplayNum.choice >= nOfDisplays)
		g_beVideoSettingsMenuItem_DisplayNum.choice = 0;
	/*** Prepare fullscreen resolutions list ***/
	BEL_ST_Launcher_ResetDisplayModes(g_refKeenCfg.displayNum);
	/*** Prepare SDL renderer drivers list ***/
	int nOfSDLRendererDrivers = SDL_GetNumRenderDrivers();
	if (nOfSDLRendererDrivers > BE_LAUNCHER_MAX_NUM_OF_SDL_RENDERER_DRIVERS)
		nOfSDLRendererDrivers = BE_LAUNCHER_MAX_NUM_OF_SDL_RENDERER_DRIVERS;
	g_beVideoSettingsMenuItem_SDLRenderer.choices[0] = "auto";
	g_beVideoSettingsMenuItem_SDLRenderer.choices[nOfSDLRendererDrivers+1] = NULL;
	for (int i = 0; i < nOfSDLRendererDrivers; ++i)
	{
		SDL_RendererInfo info;
		SDL_GetRenderDriverInfo(i, &info);
		snprintf(g_be_videoSettingsChoices_sdlRendererDriversStrs[i], sizeof(g_be_videoSettingsChoices_sdlRendererDriversStrs[i]), "%s", info.name);
		g_beVideoSettingsMenuItem_SDLRenderer.choices[i+1] = g_be_videoSettingsChoices_sdlRendererDriversStrs[i];
	}
	if ((g_refKeenCfg.sdlRendererDriver < 0) || (g_refKeenCfg.sdlRendererDriver >= nOfSDLRendererDrivers))
		g_beVideoSettingsMenuItem_SDLRenderer.choice = 0;
	else
		g_beVideoSettingsMenuItem_SDLRenderer.choice = g_refKeenCfg.sdlRendererDriver + 1;
	// Set VSync value
	g_beVideoSettingsMenuItem_VSync.choice = g_refKeenCfg.vSync;
	// Set Bilinear value
	g_beVideoSettingsMenuItem_Bilinear.choice = g_refKeenCfg.isBilinear;
	// Set ScaleType value
	g_beVideoSettingsMenuItem_ScaleType.choice = g_refKeenCfg.scaleType;
	// Set ScaleFactor value
	if ((g_refKeenCfg.scaleFactor <= 0) || (g_refKeenCfg.scaleFactor >= (int)(sizeof(g_be_videoSettingsChoices_scaleFactor)/sizeof(*g_be_videoSettingsChoices_scaleFactor))))
		g_beVideoSettingsMenuItem_ScaleFactor.choice = 0;
	else
		g_beVideoSettingsMenuItem_ScaleFactor.choice = g_refKeenCfg.scaleFactor-1;
	// Set ForceFullSoftScaling value
	g_beVideoSettingsMenuItem_ForceFullSoftScaling.choice = g_refKeenCfg.forceFullSoftScaling;
	// Set LauncherWindowType value
	g_beVideoSettingsMenuItem_LauncherWindowType.choice = g_refKeenCfg.launcherWinType;
	// Set SndSampleRate value
	g_beSoundSettingsMenuItem_SndSampleRate.choice = 8; // FIXME - Better way to grab default off cfg
	for (int i = 0; i < (int)(sizeof(g_be_soundsSettingsChoices_sndSampleRateVals)/sizeof(*g_be_soundsSettingsChoices_sndSampleRateVals)); ++i)
		if (g_be_soundsSettingsChoices_sndSampleRateVals[i] == g_refKeenCfg.sndSampleRate)
		{
			g_beSoundSettingsMenuItem_SndSampleRate.choice = i;
			break;
		}
	// Set SndSubSystem value
	g_beSoundSettingsMenuItem_SndSubSystem.choice = g_refKeenCfg.sndSubSystem;
	// Set OPLEmulation value
	g_beSoundSettingsMenuItem_OPLEmulation.choice = g_refKeenCfg.oplEmulation;
	// Set ControllerScheme value
	g_beInputSettingsMenuItem_ControllerScheme.choice = g_refKeenCfg.altControlScheme.isEnabled;
	// Set Autolock value
	g_beInputSettingsMenuItem_Autolock.choice = g_refKeenCfg.autolockCursor;
	// Set Dpad value
	g_beControllerSettingsMenuItem_Dpad.choice = g_refKeenCfg.altControlScheme.useDpad;
	// Set LeftStick value
	g_beControllerSettingsMenuItem_LeftStick.choice = g_refKeenCfg.altControlScheme.useLeftStick;
	// Set RightStick value
	g_beControllerSettingsMenuItem_RightStick.choice = g_refKeenCfg.altControlScheme.useRightStick;
#ifdef REFKEEN_VER_CATACOMB_ALL
	// Set AnalogMotion value
	g_beControllerSettingsMenuItem_AnalogMotion.choice = g_refKeenCfg.altControlScheme.analogMotion;
#endif
	/*** Set controller button bindings ***/
#ifdef REFKEEN_VER_KDREAMS
	g_beControllerSettingsMenuItem_Action_Jump.choice = g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_JUMP];
	g_beControllerSettingsMenuItem_Action_Throw.choice = g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_THROW];
	g_beControllerSettingsMenuItem_Action_Stats.choice = g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_STATS];
#else
	g_beControllerSettingsMenuItem_Action_Fire.choice = g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_FIRE];
	g_beControllerSettingsMenuItem_Action_Strafe.choice = g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_STRAFE];
	g_beControllerSettingsMenuItem_Action_Drink.choice = g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_DRINK];
	g_beControllerSettingsMenuItem_Action_Bolt.choice = g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_BOLT];
	g_beControllerSettingsMenuItem_Action_Nuke.choice = g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_NUKE];
	g_beControllerSettingsMenuItem_Action_FastTurn.choice = g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_FASTTURN];
#endif
#if (defined REFKEEN_VER_CAT3D) || (defined REFKEEN_VER_CATABYSS)
	g_beControllerSettingsMenuItem_Action_Scrolls.choice = g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_SCROLLS];
#endif
#if (defined REFKEEN_VER_KDREAMS) || (defined REFKEEN_VER_CATADVENTURES)
	g_beControllerSettingsMenuItem_Action_FuncKeys.choice = g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_FUNCKEYS];
#endif
	g_beControllerSettingsMenuItem_Action_DebugKeys.choice = g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_DEBUGKEYS];

	/*** Prepare installed game versions menu ***/
	BE_ST_Launcher_RefreshSelectGameMenuContents();

	/*** Prepare root paths menu ***/
	int nOfRootPaths = BE_Cross_DirSelection_GetNumOfRootPaths();
	const char **rootPathsNames = BE_Cross_DirSelection_GetRootPathsNames();
	g_beSelectInitialPathMenuItems = (BEMenuItem *)malloc(nOfRootPaths*sizeof(BEMenuItem));
	g_beSelectInitialPathMenuItemsPtrs =  (BEMenuItem **)malloc((1+nOfRootPaths)*sizeof(BEMenuItem *));
	g_beSelectInitialPathMenuItemsStrsBuffer = (char *)malloc(nOfRootPaths*BE_LAUNCHER_MENUITEM_STRBUFFER_LEN_BOUND);
	if (!g_beSelectInitialPathMenuItems || !g_beSelectInitialPathMenuItemsPtrs || !g_beSelectInitialPathMenuItemsStrsBuffer)
	{
		BE_Cross_LogMessage(BE_LOG_MSG_ERROR, "BE_ST_Launcher_Prepare: Out of memory!\n");
		// Destroy window, renderer and more?
		exit(0);
	}

	g_beSelectInitialPathMenu.menuItems = g_beSelectInitialPathMenuItemsPtrs;
	char *label = g_beSelectInitialPathMenuItemsStrsBuffer;
	const char **rootPathNamePtr = rootPathsNames;
	for (int i = 0; i < nOfRootPaths; ++i, label += BE_LAUNCHER_MENUITEM_STRBUFFER_LEN_BOUND, ++rootPathNamePtr)
	{
		g_beSelectInitialPathMenuItemsPtrs[i] = &g_beSelectInitialPathMenuItems[i];
		g_beSelectInitialPathMenuItems[i].handler = &BE_Launcher_Handler_RootPathSelection;
		g_beSelectInitialPathMenuItems[i].choices = NULL;
		g_beSelectInitialPathMenuItems[i].targetMenu = NULL;
		g_beSelectInitialPathMenuItems[i].label = label;
		g_beSelectInitialPathMenuItems[i].type = BE_MENUITEM_TYPE_HANDLER;
		BE_Cross_safeandfastcstringcopy(label, label + BE_LAUNCHER_MENUITEM_STRBUFFER_LEN_BOUND, *rootPathNamePtr);
	}
	g_beSelectInitialPathMenuItemsPtrs[nOfRootPaths] = NULL;
}


void BE_ST_Launcher_Shutdown(void)
{
	/*** Free a few launcher-specific resources ***/
	BE_Launcher_ClearDirSelectionMenu();
	/*** Clear ST stuff ***/
	for (int i = 0; i < BE_ST_MAXJOYSTICKS; ++i)
		if (g_sdlControllers[i])
		{
			SDL_GameControllerClose(g_sdlControllers[i]);
			g_sdlControllers[i] = NULL;
		}

	/* Poll events just in case (e.g., clean up old controller events) */
	SDL_Event event;
	bool doExit = false;
	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_QUIT)
			doExit = true;
	}

	SDL_DestroyTexture(g_sdlTexture);
	g_sdlTexture = NULL;
	SDL_DestroyTexture(g_sdlTargetTexture);
	g_sdlTargetTexture = NULL;
	SDL_DestroyRenderer(g_sdlRenderer);
	g_sdlRenderer = NULL;
	SDL_DestroyWindow(g_sdlWindow);
	g_sdlWindow = NULL;

	/*** Save settings if there's any change ***/
	if (!g_be_launcher_wasAnySettingChanged)
		return; // e.g., if there is a cfg value the launcher doesn't cope with (say, out of some range)

	g_refKeenCfg.isFullscreen = g_beVideoSettingsMenuItem_Fullscreen.choice;
	g_refKeenCfg.displayNum = g_beVideoSettingsMenuItem_DisplayNum.choice;

	if (g_beVideoSettingsMenuItem_FullscreenRes.choice > 0)
	{
		g_refKeenCfg.fullWidth = g_be_videoSettingsChoices_fullResolutionsVals[g_beVideoSettingsMenuItem_FullscreenRes.choice - 1].width;
		g_refKeenCfg.fullHeight = g_be_videoSettingsChoices_fullResolutionsVals[g_beVideoSettingsMenuItem_FullscreenRes.choice - 1].height;
	}
	else
	{
		g_refKeenCfg.fullWidth = 0;
		g_refKeenCfg.fullHeight = 0;
	}

	g_refKeenCfg.sdlRendererDriver = g_beVideoSettingsMenuItem_SDLRenderer.choice - 1;
	g_refKeenCfg.vSync = (VSyncSettingType)g_beVideoSettingsMenuItem_VSync.choice;
	g_refKeenCfg.isBilinear = g_beVideoSettingsMenuItem_Bilinear.choice;
	g_refKeenCfg.scaleType = (ScaleTypeSettingType)g_beVideoSettingsMenuItem_ScaleType.choice;
	g_refKeenCfg.scaleFactor = g_beVideoSettingsMenuItem_ScaleFactor.choice + 1;
	g_refKeenCfg.forceFullSoftScaling = g_beVideoSettingsMenuItem_ForceFullSoftScaling.choice;
	g_refKeenCfg.launcherWinType = (LauncherWindowSettingType)g_beVideoSettingsMenuItem_LauncherWindowType.choice;

	g_refKeenCfg.sndSampleRate = g_be_soundsSettingsChoices_sndSampleRateVals[g_beSoundSettingsMenuItem_SndSampleRate.choice];
	g_refKeenCfg.sndSubSystem = g_beSoundSettingsMenuItem_SndSubSystem.choice;
	g_refKeenCfg.oplEmulation = g_beSoundSettingsMenuItem_OPLEmulation.choice;

	g_refKeenCfg.altControlScheme.isEnabled = g_beInputSettingsMenuItem_ControllerScheme.choice;
	g_refKeenCfg.autolockCursor = g_beInputSettingsMenuItem_Autolock.choice;

	g_refKeenCfg.altControlScheme.useDpad = g_beControllerSettingsMenuItem_Dpad.choice;
	g_refKeenCfg.altControlScheme.useLeftStick = g_beControllerSettingsMenuItem_LeftStick.choice;
	g_refKeenCfg.altControlScheme.useRightStick = g_beControllerSettingsMenuItem_RightStick.choice;
#ifdef REFKEEN_VER_CATACOMB_ALL
	g_refKeenCfg.altControlScheme.analogMotion = g_beControllerSettingsMenuItem_AnalogMotion.choice;
#endif

#ifdef REFKEEN_VER_KDREAMS
	g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_JUMP] = g_beControllerSettingsMenuItem_Action_Jump.choice;
	g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_THROW] = g_beControllerSettingsMenuItem_Action_Throw.choice;
	g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_STATS] = g_beControllerSettingsMenuItem_Action_Stats.choice;
#else
	g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_FIRE] = g_beControllerSettingsMenuItem_Action_Fire.choice;
	g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_STRAFE] = g_beControllerSettingsMenuItem_Action_Strafe.choice;
	g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_DRINK] = g_beControllerSettingsMenuItem_Action_Drink.choice;
	g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_BOLT] = g_beControllerSettingsMenuItem_Action_Bolt.choice;
	g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_NUKE] = g_beControllerSettingsMenuItem_Action_Nuke.choice;
	g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_FASTTURN] = g_beControllerSettingsMenuItem_Action_FastTurn.choice;
#endif
#if (defined REFKEEN_VER_CAT3D) || (defined REFKEEN_VER_CATABYSS)
	g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_SCROLLS] = g_beControllerSettingsMenuItem_Action_Scrolls.choice;
#endif
#if (defined REFKEEN_VER_KDREAMS) || (defined REFKEEN_VER_CATADVENTURES)
	g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_FUNCKEYS] = g_beControllerSettingsMenuItem_Action_FuncKeys.choice;
#endif
	g_refKeenCfg.altControlScheme.actionMappings[BE_ST_CTRL_CFG_BUTMAP_DEBUGKEYS] = g_beControllerSettingsMenuItem_Action_DebugKeys.choice;

	if (doExit)
		BE_ST_QuickExit();
}


void BE_ST_Launcher_RefreshSelectGameMenuContents(void)
{
	for (int i = 0; i < g_be_gameinstallations_num; ++i)
	{
		g_beSelectGameMenuItemsPtrs[i] = &g_beSelectGameMenuItems[i];
		g_beSelectGameMenuItems[i].handler = &BE_Launcher_Handler_GameLaunch;
		snprintf(g_beSelectGameMenuItemsStrs[i], sizeof(g_beSelectGameMenuItemsStrs[i]), "%s", BE_Cross_GetGameInstallationDescription(i));
		g_beSelectGameMenuItems[i].label = g_beSelectGameMenuItemsStrs[i];
		g_beSelectGameMenuItems[i].type = BE_MENUITEM_TYPE_HANDLER;
	}
	g_beSelectGameMenuItemsPtrs[g_be_gameinstallations_num] = &g_beSelectGameMenuItem_DisappearedGameHelp;
	g_beSelectGameMenuItemsPtrs[g_be_gameinstallations_num+1] = &g_beSelectGameMenuItem_AddMissingGameVersion;
	g_beSelectGameMenuItemsPtrs[g_be_gameinstallations_num+2] = NULL;
}


static void BEL_ST_Launcher_ResetDisplayModes(int displayNum)
{
	int nOfDisplayModes = SDL_GetNumDisplayModes(displayNum);
	if (nOfDisplayModes > BE_LAUNCHER_MAX_NUM_OF_RESOLUTIONS)
		nOfDisplayModes = BE_LAUNCHER_MAX_NUM_OF_RESOLUTIONS;
	g_beVideoSettingsMenuItem_FullscreenRes.choices[0] = "Desktop";
	g_beVideoSettingsMenuItem_FullscreenRes.choice = 0;
	int actualCounter = 0;
	for (int i = 0; i < nOfDisplayModes; ++i)
	{
		SDL_DisplayMode mode;
		SDL_GetDisplayMode(displayNum, i, &mode);
		// Make sure there's no duplication (this is a possibility, at least when limiting ourselves just to width,height pairs)
		int j;
		for (j = 0; j < actualCounter; ++j)
			if ((g_be_videoSettingsChoices_fullResolutionsVals[j].width == mode.w) && (g_be_videoSettingsChoices_fullResolutionsVals[j].height == mode.h))
				break;
		if (j < actualCounter)
			continue; // Skip duplicated entry

		g_be_videoSettingsChoices_fullResolutionsVals[actualCounter].width = mode.w;
		g_be_videoSettingsChoices_fullResolutionsVals[actualCounter].height = mode.h;
		snprintf(g_be_videoSettingsChoices_fullResolutionsStrs[actualCounter], sizeof(g_be_videoSettingsChoices_fullResolutionsStrs[actualCounter]), "%dx%d", mode.w, mode.h);
		g_beVideoSettingsMenuItem_FullscreenRes.choices[actualCounter+1] =  g_be_videoSettingsChoices_fullResolutionsStrs[actualCounter];
		if ((g_refKeenCfg.fullWidth == mode.w) && (g_refKeenCfg.fullHeight == mode.h))
			g_beVideoSettingsMenuItem_FullscreenRes.choice = actualCounter+1;

		++actualCounter;
	}
	g_beVideoSettingsMenuItem_FullscreenRes.choices[actualCounter+1] = NULL;
}

void BEL_Launcher_DrawMenuItem(BEMenuItem *menuItem);

static void BEL_ST_Launcher_Handler_DisplayNum(BEMenuItem **menuItemP)
{
	BEL_ST_Launcher_ResetDisplayModes((*menuItemP)->choice);
	BEL_Launcher_DrawMenuItem(&g_beVideoSettingsMenuItem_FullscreenRes);
}


static void BEL_ST_Launcher_SetGfxOutputRect(void)
{
	int winWidth, winHeight;
	SDL_GetWindowSize(g_sdlWindow, &winWidth, &winHeight);

	if (g_refKeenCfg.launcherWinType != LAUNCHER_WINDOW_FULL)
	{
		g_refKeenCfg.launcherWinWidth = winWidth;
		g_refKeenCfg.launcherWinHeight = winHeight;
	}

	if (BE_LAUNCHER_PIX_HEIGHT*winWidth < BE_LAUNCHER_PIX_WIDTH*winHeight) // Thinner than BE_LAUNCHER_PIX_WIDTH:BE_LAUNCHER_PIX_HEIGHT
	{
		g_sdlAspectCorrectionBorderedRect.w = winWidth;
		g_sdlAspectCorrectionBorderedRect.h = winWidth*BE_LAUNCHER_PIX_HEIGHT/BE_LAUNCHER_PIX_WIDTH;
		g_sdlAspectCorrectionBorderedRect.x = 0;
		g_sdlAspectCorrectionBorderedRect.y = (winHeight-g_sdlAspectCorrectionBorderedRect.h)/2;
	}
	else // As wide as BE_LAUNCHER_PIX_WIDTH:BE_LAUNCHER_PIX_HEIGHT at the least
	{
		g_sdlAspectCorrectionBorderedRect.w = winHeight*BE_LAUNCHER_PIX_WIDTH/BE_LAUNCHER_PIX_HEIGHT;
		g_sdlAspectCorrectionBorderedRect.h = winHeight;
		g_sdlAspectCorrectionBorderedRect.x = (winWidth-g_sdlAspectCorrectionBorderedRect.w)/2;
		g_sdlAspectCorrectionBorderedRect.y = 0;
	}
}

void BE_ST_Launcher_MarkGfxCache(void)
{
	g_sdlLauncherGfxCacheMarked = true;
}

uint8_t *BE_ST_Launcher_GetGfxPtr(void)
{
	return g_sdlLauncherGfxCache;
}

static void BEL_ST_Launcher_NormalizePos(int *px, int *py)
{
	if (*px < g_sdlAspectCorrectionBorderedRect.x)
		*px = 0;
	if (*px >= g_sdlAspectCorrectionBorderedRect.x + g_sdlAspectCorrectionBorderedRect.w)
		*px = BE_LAUNCHER_PIX_WIDTH;
	else
		*px = BE_LAUNCHER_PIX_WIDTH*(*px-g_sdlAspectCorrectionBorderedRect.x)/g_sdlAspectCorrectionBorderedRect.w;

	if (*py < g_sdlAspectCorrectionBorderedRect.y)
		*py = 0;
	if (*py >= g_sdlAspectCorrectionBorderedRect.y + g_sdlAspectCorrectionBorderedRect.h)
		*py = BE_LAUNCHER_PIX_HEIGHT;
	else
		*py = BE_LAUNCHER_PIX_HEIGHT*(*py-g_sdlAspectCorrectionBorderedRect.y)/g_sdlAspectCorrectionBorderedRect.h;
}


// Maps SDL scancodes to corresponding ASCII chars (including shifted state)
static const char g_sdlScancodeToNonShiftedCharMap[] = "\0\0\0\0abcdefghijklmnopqrstuvwxyz1234567890\0\0\0\0 -=[]\\\0;'`,./";
static const char g_sdlScancodeToShiftedCharMap[] = "\0\0\0\0ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()\0\0\0\0 _+{}|\0:\"~<>?";

static void BE_ST_Launcher_HandleKeyPressEvent(int scancode, bool isShifted)
{
	switch (scancode)
	{
	case SDL_SCANCODE_PAGEUP:
		BE_Launcher_HandleInput_ButtonPageUp();
		break;
	case SDL_SCANCODE_PAGEDOWN:
		BE_Launcher_HandleInput_ButtonPageDown();
		break;
	case SDL_SCANCODE_RIGHT:
		BE_Launcher_HandleInput_ButtonRight();
		break;
	case SDL_SCANCODE_LEFT:
		BE_Launcher_HandleInput_ButtonLeft();
		break;
	case SDL_SCANCODE_DOWN:
		BE_Launcher_HandleInput_ButtonDown();
		break;
	case SDL_SCANCODE_UP:
		BE_Launcher_HandleInput_ButtonUp();
		break;
	case SDL_SCANCODE_RETURN:
		BE_Launcher_HandleInput_ButtonActivate();
		break;
	case SDL_SCANCODE_ESCAPE:
		BE_Launcher_HandleInput_ButtonBack();
		break;
	default:
		if (((scancode >= SDL_SCANCODE_A) && (scancode <= SDL_SCANCODE_Z)) || // Letters
		    ((scancode >= SDL_SCANCODE_1) && (scancode <= SDL_SCANCODE_0)) || // Digits
		    ((scancode >= SDL_SCANCODE_SPACE) && (scancode <= SDL_SCANCODE_SLASH) && (scancode != SDL_SCANCODE_NONUSHASH)) // A few other keys
		)
			BE_Launcher_HandleInput_ASCIIChar((isShifted ? g_sdlScancodeToShiftedCharMap : g_sdlScancodeToNonShiftedCharMap)[scancode]);
		break;
	}
}

static void BE_ST_Launcher_HandleControllerButtonPressEvent(Uint8 but)
{
	switch (but)
	{
	case SDL_CONTROLLER_BUTTON_A:
		BE_Launcher_HandleInput_ButtonActivate();
		break;
	case SDL_CONTROLLER_BUTTON_B:
	case SDL_CONTROLLER_BUTTON_BACK:
		BE_Launcher_HandleInput_ButtonBack();
		break;
	case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
		BE_Launcher_HandleInput_ButtonPageUp();
		break;
	case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
		BE_Launcher_HandleInput_ButtonPageDown();
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_UP:
		BE_Launcher_HandleInput_ButtonUp();
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
		BE_Launcher_HandleInput_ButtonDown();
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
		BE_Launcher_HandleInput_ButtonLeft();
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
		BE_Launcher_HandleInput_ButtonRight();
		break;
	}
}

void BE_ST_Launcher_RunEventLoop(void)
{
	SDL_Event event;
	uint32_t lastRefreshTicks = 0;

	while (1)
	{
		uint32_t ticksBeforePoll = SDL_GetTicks();
		while (SDL_PollEvent(&event))
		{
			void BEL_ST_NormalizeBorderedPos(int *px, int *py);

			switch (event.type)
			{
			case SDL_KEYDOWN:
				if (event.key.repeat)
					break; // Ignore
				g_sdlKeyboardLastKeyPressed = event.key.keysym.scancode;
				g_sdlKeyboardLastKeyPressedIsShifted = (event.key.keysym.mod & KMOD_SHIFT);
				g_sdlInputLastBinaryPressTime = ticksBeforePoll;
				g_sdlInputLastBinaryPressTimeDelay = BE_ST_SDL_CONTROLLER_DELAY_BEFORE_DIGIACTION_REPEAT_MS;
				BE_ST_Launcher_HandleKeyPressEvent(event.key.keysym.scancode, g_sdlKeyboardLastKeyPressedIsShifted);
				break;
			case SDL_KEYUP:
				if (g_sdlKeyboardLastKeyPressed == event.key.keysym.scancode)
					g_sdlKeyboardLastKeyPressed = SDL_SCANCODE_UNKNOWN;
				break;

			case SDL_MOUSEBUTTONDOWN:
				if (event.button.button == SDL_BUTTON_LEFT)
				{
					BEL_ST_Launcher_NormalizePos(&event.button.x, &event.button.y);
					BE_Launcher_HandleInput_PointerSelect(event.button.x, event.button.y, ticksBeforePoll);
				}
				break;
			case SDL_MOUSEBUTTONUP:
				if (event.button.button == SDL_BUTTON_LEFT)
				{
					BEL_ST_Launcher_NormalizePos(&event.button.x, &event.button.y);
					BE_Launcher_HandleInput_PointerRelease(event.button.x, event.button.y, ticksBeforePoll);
				}
				break;
			case SDL_MOUSEMOTION:
				BEL_ST_Launcher_NormalizePos(&event.button.x, &event.button.y);
				BE_Launcher_HandleInput_PointerMotion(event.button.x, event.button.y, ticksBeforePoll);
				break;
			case SDL_MOUSEWHEEL:
				BE_Launcher_HandleInput_PointerVScroll(-10*event.wheel.y, ticksBeforePoll);
				break;

			case SDL_JOYDEVICEADDED:
				if ((event.jdevice.which < BE_ST_MAXJOYSTICKS) && SDL_IsGameController(event.jdevice.which))
					g_sdlControllers[event.jdevice.which] = SDL_GameControllerOpen(event.jdevice.which);
				break;
			case SDL_JOYDEVICEREMOVED:
				for (int i = 0; i < BE_ST_MAXJOYSTICKS; ++i)
					if (g_sdlControllers[i] && (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_sdlControllers[i])) == event.jdevice.which))
					{
						SDL_GameControllerClose(g_sdlControllers[i]);
						g_sdlControllers[i] = NULL;
					}
				break;

			case SDL_CONTROLLERAXISMOTION: // Need this so a pressed trigger is ignored once user gets to choose a button for in-game action
				if ((event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) || (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT))
					g_sdlLauncherTriggerBinaryStates[event.caxis.axis - SDL_CONTROLLER_AXIS_TRIGGERLEFT] = (event.caxis.value >= g_sdlJoystickAxisBinaryThreshold);
				break;
			case SDL_CONTROLLERBUTTONDOWN:
				g_sdlControllerLastButtonPressed = event.cbutton.button;
				g_sdlKeyboardLastKeyPressed = SDL_SCANCODE_UNKNOWN;
				g_sdlInputLastBinaryPressTime = ticksBeforePoll;
				g_sdlInputLastBinaryPressTimeDelay = BE_ST_SDL_CONTROLLER_DELAY_BEFORE_DIGIACTION_REPEAT_MS;
				BE_ST_Launcher_HandleControllerButtonPressEvent(event.cbutton.button);
				break;
			case SDL_CONTROLLERBUTTONUP:
				if (g_sdlControllerLastButtonPressed == event.cbutton.button)
					g_sdlControllerLastButtonPressed = SDL_CONTROLLER_BUTTON_INVALID;
				break;

			case SDL_WINDOWEVENT:
				switch (event.window.event)
				case  SDL_WINDOWEVENT_RESIZED:
				{
					BEL_ST_Launcher_SetGfxOutputRect();
					BE_ST_Launcher_MarkGfxCache();
					break;
				}
				case SDL_WINDOWEVENT_EXPOSED:
					BE_ST_Launcher_MarkGfxCache();
					break;
				break;
			case SDL_QUIT:
				BE_ST_Launcher_Shutdown();
				BE_ST_QuickExit();
				break;
			}
		}

		// Emulate "key repeat" for keyboard/controller buttons
		if (((g_sdlKeyboardLastKeyPressed != SDL_SCANCODE_UNKNOWN) || (g_sdlControllerLastButtonPressed != SDL_CONTROLLER_BUTTON_INVALID)) && (ticksBeforePoll - g_sdlInputLastBinaryPressTime >= g_sdlInputLastBinaryPressTimeDelay))
		{
			g_sdlInputLastBinaryPressTime += g_sdlInputLastBinaryPressTimeDelay;
			g_sdlInputLastBinaryPressTimeDelay = BE_ST_SDL_CONTROLLER_DIGIACTION_REPEAT_RATE_MS;

			if (g_sdlKeyboardLastKeyPressed != SDL_SCANCODE_UNKNOWN)
				BE_ST_Launcher_HandleKeyPressEvent(g_sdlKeyboardLastKeyPressed, g_sdlKeyboardLastKeyPressedIsShifted);
			else
				BE_ST_Launcher_HandleControllerButtonPressEvent(g_sdlControllerLastButtonPressed);
		}

		BE_Launcher_RefreshVerticalScrolling(ticksBeforePoll);

		/*** Draw ***/
		if (g_sdlLauncherGfxCacheMarked)
		{
			SDL_Delay(1);
			g_sdlLauncherGfxCacheMarked = false;
			void *pixels;
			int pitch;
			SDL_LockTexture(g_sdlTexture, NULL, &pixels, &pitch);
			uint32_t *currPixPtr = (uint32_t *)pixels;
			uint8_t *currPalPixPtr = g_sdlLauncherGfxCache;
			for (int pixnum = 0; pixnum < BE_LAUNCHER_PIX_WIDTH*BE_LAUNCHER_PIX_HEIGHT; ++pixnum, ++currPixPtr, ++currPalPixPtr)
			{
				*currPixPtr = g_sdlEGABGRAScreenColors[*currPalPixPtr];
			}

			SDL_UnlockTexture(g_sdlTexture);
			SDL_RenderClear(g_sdlRenderer);
			if (g_sdlTargetTexture)
			{
				SDL_SetRenderTarget(g_sdlRenderer, g_sdlTargetTexture);
				SDL_RenderCopy(g_sdlRenderer, g_sdlTexture, NULL, NULL);
				SDL_SetRenderTarget(g_sdlRenderer, NULL);
				SDL_RenderCopy(g_sdlRenderer, g_sdlTargetTexture, NULL, &g_sdlAspectCorrectionBorderedRect);
			}
			else
			{
				SDL_RenderCopy(g_sdlRenderer, g_sdlTexture, NULL, &g_sdlAspectCorrectionBorderedRect);
			}
			SDL_RenderPresent(g_sdlRenderer);
			lastRefreshTicks = SDL_GetTicks();
		}
		else
		{
			// Refresh graphics from time to time in case a part of the window is overridden by anything,
			// like the Steam Overlay. Sleep for less time so the application is somewhat responsive, though.
			SDL_Delay(10);
			uint32_t currRefreshTicks = SDL_GetTicks();
			if (currRefreshTicks - lastRefreshTicks >= 100)
			{
				SDL_RenderClear(g_sdlRenderer);

				if (g_sdlTargetTexture)
					SDL_RenderCopy(g_sdlRenderer, g_sdlTargetTexture, NULL, &g_sdlAspectCorrectionBorderedRect);
				else
					SDL_RenderCopy(g_sdlRenderer, g_sdlTexture, NULL, &g_sdlAspectCorrectionBorderedRect);

				SDL_RenderPresent(g_sdlRenderer);
				lastRefreshTicks = currRefreshTicks;
			}
		}
	}
}

void BE_ST_Launcher_WaitForControllerButton(BEMenuItem *menuItem)
{
	// HACK - Refresh window and make sure none of it is filled with random data while waiting for button press
	void *pixels;
	int pitch;
	SDL_LockTexture(g_sdlTexture, NULL, &pixels, &pitch);
	uint32_t *currPixPtr = (uint32_t *)pixels;
	uint8_t *currPalPixPtr = g_sdlLauncherGfxCache;
	for (int pixnum = 0; pixnum < BE_LAUNCHER_PIX_WIDTH*BE_LAUNCHER_PIX_HEIGHT; ++pixnum, ++currPixPtr, ++currPalPixPtr)
	{
		*currPixPtr = g_sdlEGABGRAScreenColors[*currPalPixPtr];
	}

	SDL_UnlockTexture(g_sdlTexture);

	if (g_sdlTargetTexture)
	{
		SDL_SetRenderTarget(g_sdlRenderer, g_sdlTargetTexture);
		SDL_RenderCopy(g_sdlRenderer, g_sdlTexture, NULL, NULL);
		SDL_SetRenderTarget(g_sdlRenderer, NULL);
	}

	SDL_Event event;
	bool keepRunning = true;
	const int defaultChoice = SDL_CONTROLLER_BUTTON_MAX + 2/*triggers*/;
	int choice = defaultChoice;

	while (keepRunning)
	{
		while (SDL_PollEvent(&event))
		{
			void BEL_ST_NormalizeBorderedPos(int *px, int *py);

			switch (event.type)
			{
			case SDL_KEYDOWN:
				if (event.key.repeat)
					break; // Ignore
				// Fall-through
			case SDL_MOUSEBUTTONDOWN:
				keepRunning = false;
				break;

			case SDL_JOYDEVICEADDED:
				if ((event.jdevice.which < BE_ST_MAXJOYSTICKS) && SDL_IsGameController(event.jdevice.which))
					g_sdlControllers[event.jdevice.which] = SDL_GameControllerOpen(event.jdevice.which);
				break;
			case SDL_JOYDEVICEREMOVED:
				for (int i = 0; i < BE_ST_MAXJOYSTICKS; ++i)
					if (g_sdlControllers[i] && (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_sdlControllers[i])) == event.jdevice.which))
					{
						SDL_GameControllerClose(g_sdlControllers[i]);
						g_sdlControllers[i] = NULL;
					}
				break;

			case SDL_CONTROLLERAXISMOTION:
				if ((event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) || (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT))
				{
					int triggerNum = event.caxis.axis - SDL_CONTROLLER_AXIS_TRIGGERLEFT;
					bool prevBinaryState = g_sdlLauncherTriggerBinaryStates[triggerNum];
					g_sdlLauncherTriggerBinaryStates[triggerNum] = (event.caxis.value >= g_sdlJoystickAxisBinaryThreshold);
					if (!prevBinaryState && g_sdlLauncherTriggerBinaryStates[triggerNum])
					{
						choice = SDL_CONTROLLER_BUTTON_MAX + triggerNum; // HACK
						keepRunning = false;
					}
				}
				break;
			case SDL_CONTROLLERBUTTONDOWN:
				switch (event.cbutton.button)
				{
					case SDL_CONTROLLER_BUTTON_A:
					case SDL_CONTROLLER_BUTTON_B:
					case SDL_CONTROLLER_BUTTON_X:
					case SDL_CONTROLLER_BUTTON_Y:
					case SDL_CONTROLLER_BUTTON_LEFTSTICK:
					case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
					case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
					case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
						choice = event.cbutton.button;
						// Fall-through
					default:
						keepRunning = false;
						break;
				}
				break;

			case SDL_WINDOWEVENT:
				switch (event.window.event)
				case  SDL_WINDOWEVENT_RESIZED:
				{
					BEL_ST_Launcher_SetGfxOutputRect();
					BE_ST_Launcher_MarkGfxCache();
					break;
				}
				case SDL_WINDOWEVENT_EXPOSED:
					BE_ST_Launcher_MarkGfxCache();
					break;
				break;
			case SDL_QUIT:
				BE_ST_Launcher_Shutdown();
				BE_ST_QuickExit();
				break;
			}
		}

		SDL_Delay(10);
		SDL_RenderClear(g_sdlRenderer);

		if (g_sdlTargetTexture)
			SDL_RenderCopy(g_sdlRenderer, g_sdlTargetTexture, NULL, &g_sdlAspectCorrectionBorderedRect);
		else
			SDL_RenderCopy(g_sdlRenderer, g_sdlTexture, NULL, &g_sdlAspectCorrectionBorderedRect);

		SDL_RenderPresent(g_sdlRenderer);
	}

	if (choice != defaultChoice)
	{
		for (BEMenuItem **menuItemP = g_beControllerSettingsMenu.menuItems; menuItemP != g_beControllerSettingsMenu.menuItems + BE_ST_CTRL_CFG_BUTMAP_AFTERLAST; ++menuItemP)
			if ((*menuItemP != menuItem) && ((*menuItemP)->choice == choice))
				(*menuItemP)->choice = defaultChoice; // Remove duplications
	}
	menuItem->choice = choice;

	// Reset
	g_sdlLauncherGfxCacheMarked = true;

	memset(g_sdlLauncherTriggerBinaryStates, 0, sizeof(g_sdlLauncherTriggerBinaryStates));
	g_sdlControllerLastButtonPressed = SDL_CONTROLLER_BUTTON_INVALID;
	g_sdlKeyboardLastKeyPressed = SDL_SCANCODE_UNKNOWN;
}
