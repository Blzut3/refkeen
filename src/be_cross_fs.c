/* Copyright (C) 2014-2020 NY00123
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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "refkeen_config.h" // MUST precede other contents due to e.g., endianness-based ifdefs

// For template implementations of reads/writes of enums from/to 16 little-endian integers...
#ifdef REFKEEN_VER_KDREAMS
#include "kd_def.h"
#elif defined REFKEEN_VER_CAT3D
#include "c3_def.h"
#elif defined REFKEEN_VER_CATADVENTURES
#include "def.h"
#else
#error "FATAL ERROR: No Reflection port game macro is defined!"
#endif

#ifdef REFKEEN_PLATFORM_ANDROID
#include <jni.h>
// HACK - Adding a dependency on SDL2 for Android! (Used for external storage path, and for calling Java function)
#include "SDL_system.h"
#endif

//#include "be_st.h" // For BE_ST_ExitWithErrorMsg

#ifdef REFKEEN_PLATFORM_WINDOWS
#include <shlwapi.h> // SHGetValue
#include <direct.h> // _wmkdir
#endif

//#include "be_cross.h"
#include "be_features.h"

#include "crc32/crc32.h"
#include "unlzexe/unlzexe.h"
#ifdef ENABLE_PKLITE
#include "depklite/depklite.h"
#endif

#define BE_CROSS_PATH_LEN_BOUND 256

/*** Common game string versions (for identification in cfg and other locations + directory names) ***/
#ifdef REFKEEN_VER_KDREAMS
#define BE_STR_GAMEVER_KDREAMSE100 "kdreamse100"
#define BE_STR_GAMEVER_KDREAMSC100 "kdreamsc100"
#define BE_STR_GAMEVER_KDREAMSE113 "kdreamse113"
#define BE_STR_GAMEVER_KDREAMSC105 "kdreamsc105"
#define BE_STR_GAMEVER_KDREAMSE193 "kdreamse193"
#define BE_STR_GAMEVER_KDREAMSE120 "kdreamse120"
#define BE_STR_GAMEVER_KDREAMS2015 "kdreams2015"
#endif
#ifdef REFKEEN_VER_CAT3D
#define BE_STR_GAMEVER_CAT3D100 "cat3d100"
#define BE_STR_GAMEVER_CAT3D122 "cat3d122"
#endif
#ifdef REFKEEN_VER_CATABYSS
#define BE_STR_GAMEVER_CATABYSS113 "catabyss113"
#define BE_STR_GAMEVER_CATABYSS124 "catabyss124"
#endif
#ifdef REFKEEN_VER_CATARM
#define BE_STR_GAMEVER_CATARM102 "catarm102"
#endif
#ifdef REFKEEN_VER_CATAPOC
#define BE_STR_GAMEVER_CATAPOC101 "catapoc101"
#endif

/*** Unicode stuff - Using some wide strings on Windows, and narrow strings (possibly UTF-8 encoded) elsewhere ***/
#ifdef REFKEEN_PLATFORM_WINDOWS
#define _T(x) L##x
#define TCHAR wchar_t
#define _tcslen wcslen
#define _tcscmp wcscmp
#define _TDIR _WDIR
#define _tdirent _wdirent
#define _tfopen _wfopen
#define _tremove _wremove
#define _tmkdir _wmkdir
#define _topendir _wopendir
#define _tclosedir _wclosedir
#define _treaddir _wreaddir
#define _trewinddir _wrewinddir
#else
#define _T(x) x
#define TCHAR char
#define _tcslen strlen
#define _tcscmp strcmp
#define _TDIR DIR
#define _tdirent dirent
#define _tfopen fopen
#define _tremove remove
#define _tmkdir mkdir
#define _topendir opendir
#define _tclosedir closedir
#define _treaddir readdir
#define _trewinddir rewinddir
#endif

// Use this in case x is a macro defined to be a narrow string literal
#define CSTR_TO_TCSTR(x) _T(x)

/*** These are similar to functions from header, but ***/
/*** should be usable with wide strings on Windows   ***/

#ifdef REFKEEN_PLATFORM_WINDOWS

static TCHAR *BEL_Cross_safeandfastctstringcopy(TCHAR *dest, TCHAR *destEnd, const TCHAR *src)
{
	TCHAR ch;
	// We assume that initially, destEnd - dest > 0.
	do
	{
		ch = *src++;
		*dest++ = ch; // This includes the null terminator if there's the room
	} while ((dest < destEnd) && ch);
	// These work in case dest == destEnd, and also if not
	--dest;
	*dest = _T('\0');
	return dest; // WARNING: This differs from strcpy!!
}

static TCHAR *BEL_Cross_safeandfastctstringcopy_2strs(TCHAR *dest, TCHAR *destEnd, const TCHAR *src0, const TCHAR *src1)
{
	return BEL_Cross_safeandfastctstringcopy(BEL_Cross_safeandfastctstringcopy(dest, destEnd, src0), destEnd, src1);
}

static TCHAR *BEL_Cross_safeandfastctstringcopy_3strs(TCHAR *dest, TCHAR *destEnd, const TCHAR *src0, const TCHAR *src1, const TCHAR *src2)
{
	return BEL_Cross_safeandfastctstringcopy(BEL_Cross_safeandfastctstringcopy(BEL_Cross_safeandfastctstringcopy(dest, destEnd, src0), destEnd, src1), destEnd, src2);
}

static TCHAR *BEL_Cross_safeandfastctstringcopy_4strs(TCHAR *dest, TCHAR *destEnd, const TCHAR *src0, const TCHAR *src1, const TCHAR *src2, const TCHAR *src3)
{
	return BEL_Cross_safeandfastctstringcopy(BEL_Cross_safeandfastctstringcopy(BEL_Cross_safeandfastctstringcopy(BEL_Cross_safeandfastctstringcopy(dest, destEnd, src0), destEnd, src1), destEnd, src2), destEnd, src3);
}

// Appends a narrow string to a wide string on Windows
static TCHAR *BEL_Cross_safeandfastcstringcopytoctstring(TCHAR *dest, TCHAR *destEnd, const char *src)
{
	TCHAR ch;
	// We assume that initially, destEnd - dest > 0.
	do
	{
		ch = *src++; // Casting to TCHAR
		*dest++ = ch; // This includes the null terminator if there's the room
	} while ((dest < destEnd) && ch);
	// These work in case dest == destEnd, and also if not
	--dest;
	*dest = _T('\0');
	return dest; // WARNING: This differs from strcpy!!
}

// Another function, similar to BE_Cross_strcasecmp, but:
// - The first argument is a TCHAR string.
// - It is assumed BOTH strings are ASCII strings.
static int BEL_Cross_tstr_to_cstr_ascii_casecmp(const TCHAR *s1, const char *s2)
{
	unsigned char uc1;
	unsigned char uc2;
	/* This one is easy. We don't care if a value is signed or not. */
	/* All that matters here is consistency (everything is signed). */
	for (; (*s1) && (BE_Cross_toupper(*s1) == BE_Cross_toupper(*s2)); s1++, s2++);
	/* But now, first we cast from int to char, and only *then* to */
	/* unsigned char, so the correct difference can be calculated. */
	uc1 = (unsigned char)((char)(BE_Cross_toupper(*s1)));
	uc2 = (unsigned char)((char)(BE_Cross_toupper(*s2)));
	/* We should still cast back to int, for a signed difference. */
	/* Assumption: An int can store any unsigned char value.      */
	return ((int)uc1 - (int)uc2);
}

#else

#define BEL_Cross_safeandfastctstringcopy BE_Cross_safeandfastcstringcopy
#define BEL_Cross_safeandfastctstringcopy_2strs BE_Cross_safeandfastcstringcopy_2strs
#define BEL_Cross_safeandfastctstringcopy_3strs BE_Cross_safeandfastcstringcopy_3strs
#define BEL_Cross_safeandfastctstringcopy_4strs BE_Cross_safeandfastcstringcopy_4strs
#define BEL_Cross_safeandfastcstringcopytoctstring BE_Cross_safeandfastcstringcopy
#define BEL_Cross_tstr_to_cstr_ascii_casecmp BE_Cross_strcasecmp

#endif

// Returns pointer to first occurrence of a non-ASCII char,
// or end-of-string NULL terminator in case it's an ASCII string.
static const TCHAR *BEL_Cross_tstr_find_nonascii_ptr(const TCHAR *s)
{
	for (; ((unsigned int)(*s) >= 32) && ((unsigned int)(*s) < 127); ++s)
		;
	return s;
}

#define REFKEEN_ENABLE_EMBEDDED_FILES_CRC32_CHECKS 1

// Describes a file originally embedded somewhere (in an EXE file's image)
typedef struct {
	BE_GameFileDetails_T fileDetails;
	int offset; // Location of file/chunk in the EXE image
} BE_EmbeddedGameFileDetails_T;

typedef enum {
	BE_EXECOMPRESSION_NONE, BE_EXECOMPRESSION_LZEXE9X,
#ifdef ENABLE_PKLITE
	BE_EXECOMPRESSION_PKLITE105,
#endif
} BE_ExeCompression_T;

typedef struct {
	const BE_EmbeddedGameFileDetails_T *embeddedFiles;
	const char *subDescription; // NULL if there's nothing to add
	const char *exeName; // NULL if all we want is a main function pointer
	void (*mainFuncPtr)(void);
	void (*embeddedFilesLoaderFuncPtr)(void);
	int decompExeImageSize;
	BE_ExeCompression_T compressionType;
	bool passArgsToMainFunc;
} BE_EXEFileDetails_T;

typedef struct {
	const BE_GameFileDetails_T *reqFiles;
	const BE_EXEFileDetails_T *exeFiles;
	const TCHAR *writableFilesDir;
	const char *customInstDescription;
	int digiAudioFreq; // Set to a common frequency of input digitized sounds, or to 0 if unused
	BE_GameVer_T verId;
} BE_GameVerDetails_T;

typedef struct {
	const char *descStr;
	TCHAR instPath[BE_CROSS_PATH_LEN_BOUND];
	TCHAR writableFilesPath[BE_CROSS_PATH_LEN_BOUND];
	BE_GameVer_T verId;
} BE_GameInstallation_T;

BE_GameVer_T refkeen_current_gamever;
// These MUST have the same order as in the BE_GameVer_T enum
const char *refkeen_gamever_strs[BE_GAMEVER_LAST] = {
#ifdef REFKEEN_VER_KDREAMS
	BE_STR_GAMEVER_KDREAMSE100,
	BE_STR_GAMEVER_KDREAMSC100,
	BE_STR_GAMEVER_KDREAMSE113,
	BE_STR_GAMEVER_KDREAMSC105,
	BE_STR_GAMEVER_KDREAMSE193,
	BE_STR_GAMEVER_KDREAMSE120,
	BE_STR_GAMEVER_KDREAMS2015,
#endif
#ifdef REFKEEN_VER_CAT3D
	BE_STR_GAMEVER_CAT3D100,
	BE_STR_GAMEVER_CAT3D122,
#endif
#ifdef REFKEEN_VER_CATABYSS
	BE_STR_GAMEVER_CATABYSS113,
	BE_STR_GAMEVER_CATABYSS124,
#endif
#ifdef REFKEEN_VER_CATARM
	BE_STR_GAMEVER_CATARM102,
#endif
#ifdef REFKEEN_VER_CATAPOC
	BE_STR_GAMEVER_CATAPOC101,
#endif
};

const char *refkeen_gamever_descriptions[BE_GAMEVER_LAST] = {
#ifdef REFKEEN_VER_KDREAMS
	"Keen Dreams EGA v1.00",
	"Keen Dreams CGA v1.00",
	"Keen Dreams EGA v1.13",
	"Keen Dreams CGA v1.05",
	"Keen Dreams EGA v1.93",
	"Keen Dreams EGA v1.20",
	"Keen Dreams 2015",
#endif
#ifdef REFKEEN_VER_CAT3D
	"Catacomb 3-D v1.00",
	"Catacomb 3-D v1.22",
#endif
#ifdef REFKEEN_VER_CATABYSS
	"Catacomb Abyss v1.13",
	"Catacomb Abyss v1.24",
#endif
#ifdef REFKEEN_VER_CATARM
	"Catacomb Armageddon v1.02",
#endif
#ifdef REFKEEN_VER_CATAPOC
	"Catacomb Apocalypse v1.01",
#endif
};


static TCHAR g_be_appDataPath[BE_CROSS_PATH_LEN_BOUND];
static TCHAR g_be_appNewCfgPath[BE_CROSS_PATH_LEN_BOUND];

extern const char *be_main_arg_datadir;
extern const char *be_main_arg_newcfgdir;

// A list of "root paths" from which one can choose a game dir (using just ASCII characters)
static TCHAR g_be_rootPaths[BE_CROSS_MAX_ROOT_PATHS][BE_CROSS_PATH_LEN_BOUND];
static const char *g_be_rootPathsKeys[BE_CROSS_MAX_ROOT_PATHS];
static const char *g_be_rootPathsNames[BE_CROSS_MAX_ROOT_PATHS];
static int g_be_rootPathsNum;
#ifdef REFKEEN_PLATFORM_WINDOWS
static const wchar_t *g_be_rootDrivePaths[] = {L"a:",L"b:",L"c:",L"d:",L"e:",L"f:",L"g:",L"h:",L"i:",L"j:",L"k:",L"l:",L"m:",L"n:",L"o:",L"p:",L"q:",L"r:",L"s:",L"t:",L"u:",L"v:",L"w:",L"x:",L"y:",L"z:"};
static const char *g_be_rootDrivePathsNames[] = {"a:","b:","c:","d:","e:","f:","g:","h:","i:","j:","k:","l:","m:","n:","o:","p:","q:","r:","s:","t:","u:","v:","w:","x:","y:","z:"};
#endif

static bool BEL_Cross_IsDir(const TCHAR* path)
{
#ifdef REFKEEN_PLATFORM_WINDOWS
	return PathIsDirectoryW(path);
#endif
#ifdef REFKEEN_PLATFORM_UNIX
	struct stat info;
	return ((stat(path, &info) == 0) && S_ISDIR(info.st_mode));
#endif
}

/*** WARNING: The key and name are assumed to be C STRING LITERALS, and so are *NOT* copied! ***/
static void BEL_Cross_AddRootPath(const TCHAR *rootPath, const char *rootPathKey, const char *rootPathName)
{
	if (g_be_rootPathsNum >= BE_CROSS_MAX_ROOT_PATHS)
		BE_ST_ExitWithErrorMsg("BEL_Cross_AddRootPath: Too many root paths!");

	BEL_Cross_safeandfastctstringcopy(g_be_rootPaths[g_be_rootPathsNum], g_be_rootPaths[g_be_rootPathsNum]+sizeof(g_be_rootPaths[g_be_rootPathsNum])/sizeof(TCHAR), rootPath);
	g_be_rootPathsKeys[g_be_rootPathsNum] = rootPathKey;
	g_be_rootPathsNames[g_be_rootPathsNum] = rootPathName;
	++g_be_rootPathsNum;
}

/*** WARNING: Same as above ***/
static void BEL_Cross_AddRootPathIfDir(const TCHAR *rootPath, const char *rootPathKey, const char *rootPathName)
{
	if (BEL_Cross_IsDir(rootPath))
		BEL_Cross_AddRootPath(rootPath, rootPathKey, rootPathName);
}

static void BEL_Cross_mkdir(const TCHAR *path);

void BE_Cross_PrepareAppPaths(void)
{
#ifdef REFKEEN_PLATFORM_WINDOWS
	const wchar_t *homeVar = _wgetenv(L"HOMEPATH");
	const wchar_t *envVar = _wgetenv(L"APPDATA");

	// HACK - Ignore be_main_arg_datadir for now
	if (envVar && *envVar)
	{
		BEL_Cross_safeandfastctstringcopy_2strs(g_be_appDataPath, g_be_appDataPath+sizeof(g_be_appDataPath)/sizeof(TCHAR), envVar, L"\\reflection-keen");
	}
	else
	{
		BE_Cross_LogMessage(BE_LOG_MSG_WARNING, "APPDATA environment variable is not properly defined.\n");
		if (homeVar && *homeVar)
		{
			BEL_Cross_safeandfastctstringcopy_2strs(g_be_appDataPath, g_be_appDataPath+sizeof(g_be_appDataPath)/sizeof(TCHAR), homeVar, L"\\AppData\\Roaming\\reflection-keen");
		}
		else
		{
			BE_Cross_LogMessage(BE_LOG_MSG_WARNING, "HOMEPATH environment variable is not properly defined.\n");
			BEL_Cross_safeandfastctstringcopy(g_be_appDataPath, g_be_appDataPath+sizeof(g_be_appDataPath)/sizeof(TCHAR), L".");
		}
	}

	if (be_main_arg_newcfgdir)
	{
		BEL_Cross_safeandfastcstringcopytoctstring(g_be_appNewCfgPath, g_be_appNewCfgPath+sizeof(g_be_appNewCfgPath)/sizeof(TCHAR), be_main_arg_newcfgdir);
	}
	else // This is why be_main_arg_datadir has been ignored (using g_be_appDataPath as a temporary buffer)
	{
		memcpy(g_be_appNewCfgPath, g_be_appDataPath, sizeof(g_be_appDataPath));
	}

	if (be_main_arg_datadir) // Now checking be_main_arg_datadir
	{
		BEL_Cross_safeandfastcstringcopytoctstring(g_be_appDataPath, g_be_appDataPath+sizeof(g_be_appDataPath)/sizeof(TCHAR), be_main_arg_datadir);
	}

	/*** Root paths ***/

	// List of drives
	DWORD drivesBitMasks = GetLogicalDrives();
	for (int driveNum = 0; driveNum < 26; ++driveNum)
		if (drivesBitMasks & (1 << driveNum))
			BEL_Cross_AddRootPath(g_be_rootDrivePaths[driveNum], g_be_rootDrivePathsNames[driveNum], g_be_rootDrivePathsNames[driveNum]);

	// Home dir
	if (homeVar && *homeVar)
		BEL_Cross_AddRootPathIfDir(homeVar, "home", "Home dir");

	TCHAR path[BE_CROSS_PATH_LEN_BOUND];
	DWORD dwType;
	DWORD dwSize;
	LSTATUS status;

	// Steam installation dir
	dwType = 0;
	dwSize = sizeof(path);
	status = SHGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\VALVE\\STEAM", L"INSTALLPATH", &dwType, path, &dwSize);
	if ((status == ERROR_SUCCESS) && (dwType == REG_SZ))
		BEL_Cross_AddRootPathIfDir(path, "steam", "Steam (installation)");
	
	// GOG.com installation dir
	dwType = 0;
	dwSize = sizeof(path);
	status = SHGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\GOG.COM", L"DEFAULTPACKPATH", &dwType, path, &dwSize);
	if ((status == ERROR_SUCCESS) && (dwType == REG_SZ))
		BEL_Cross_AddRootPathIfDir(path, "gog", "GOG Games (default)");
#endif

#ifdef REFKEEN_PLATFORM_ANDROID
	// HACK - Adding a dependency on SDL2 for Android!

	// FIXME - These environment variables don't seem to be shown in any
	// official documentation for Android, but at least EXTERNAL_STORAGE
	// appears to do the job, and they're simple to use from C/C++.

	// We still need to check/ask for permission to access the storage, on Android 6.0 and later

	// Prepare to call Javaj function
	JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
	jobject activity = (jobject)SDL_AndroidGetActivity();
	jclass clazz = (*env)->GetObjectClass(env, activity);
	jmethodID method_id = (*env)->GetMethodID(env, clazz, "requestReadExternalStoragePermission", "()I");
	// Do call it
	bool haveReadPermission = (bool)((*env)->CallIntMethod(env, activity, method_id));
	// Clean up references
	(*env)->DeleteLocalRef(env, activity);
	(*env)->DeleteLocalRef(env, clazz);

	if (haveReadPermission)
	{
		const char *primaryStorage = getenv("EXTERNAL_STORAGE");
		if (primaryStorage && *primaryStorage)
			BEL_Cross_AddRootPathIfDir(primaryStorage, "externalstorage", "Primary storage");
		else
			BE_Cross_LogMessage(BE_LOG_MSG_WARNING, "EXTERNAL_STORAGE environment variable is not properly defined.\n");
		// Let's ignore SECONDARY_STORAGE for now, since we may get a colon-delimited list of paths
		const char *externalSDCardStorage = getenv("EXTERNAL_SDCARD_STORAGE");
		if (externalSDCardStorage && *externalSDCardStorage)
			BEL_Cross_AddRootPathIfDir(externalSDCardStorage, "externalsdcardstorage", "External SD Card storage");
	}

	char path[BE_CROSS_PATH_LEN_BOUND];

	const char *externalStoragePath = SDL_AndroidGetExternalStoragePath();
	if (externalStoragePath && *externalStoragePath)
	{
		BEL_Cross_safeandfastctstringcopy_2strs(g_be_appDataPath, g_be_appDataPath+sizeof(g_be_appDataPath)/sizeof(TCHAR), externalStoragePath, "/appdata");
		memcpy(g_be_appNewCfgPath, g_be_appDataPath, sizeof(g_be_appDataPath));
		// Let's add this just in case (sdcard directory cannot be naively opened on Android 7.0)
		BE_Cross_safeandfastcstringcopy_2strs(path, path+sizeof(path)/sizeof(TCHAR), externalStoragePath, "/user_gameinsts");
		// In contrary to other root paths, we should create this one on first launch
		BEL_Cross_mkdir(externalStoragePath); // Non-recursive
		BEL_Cross_mkdir(path);
		BEL_Cross_AddRootPathIfDir(path, "appgameinsts", "App-local game installations dir");
		// HACK - We don't look at arguments set by the user (for modifying e.g., g_be_appDataPath), but then these are never sent on Android...
	}
#elif (defined REFKEEN_PLATFORM_UNIX)
	const char *homeVar = getenv("HOME");

#ifndef REFKEEN_PLATFORM_MACOS
	const char *envVar;
#endif

	if (!homeVar || !(*homeVar))
	{
		BE_Cross_LogMessage(BE_LOG_MSG_WARNING, "HOME environment variable is not properly defined.\n");
	}

	if (be_main_arg_datadir)
	{
		BE_Cross_safeandfastcstringcopy(g_be_appDataPath, g_be_appDataPath+sizeof(g_be_appDataPath)/sizeof(TCHAR), be_main_arg_datadir);
	}
	else
	{
#ifdef REFKEEN_PLATFORM_MACOS
		// FIXME - Handle sandboxing?
		BE_Cross_safeandfastcstringcopy_2strs(g_be_appDataPath, g_be_appDataPath+sizeof(g_be_appDataPath)/sizeof(TCHAR), homeVar, "/Library/Application Support/reflection-keen");
#else
		envVar = getenv("XDG_DATA_HOME");
		if (envVar && *envVar)
		{
			BE_Cross_safeandfastcstringcopy_2strs(g_be_appDataPath, g_be_appDataPath+sizeof(g_be_appDataPath)/sizeof(TCHAR), envVar, "/reflection-keen");
		}
		else if (homeVar && *homeVar)
		{
			BE_Cross_safeandfastcstringcopy_2strs(g_be_appDataPath, g_be_appDataPath+sizeof(g_be_appDataPath)/sizeof(TCHAR), homeVar, "/.local/share/reflection-keen");
		}
		else
		{
			BE_Cross_safeandfastcstringcopy(g_be_appDataPath, g_be_appDataPath+sizeof(g_be_appDataPath)/sizeof(TCHAR), ".");
		}
#endif
	}

	if (be_main_arg_newcfgdir)
	{
		BE_Cross_safeandfastcstringcopy(g_be_appNewCfgPath, g_be_appNewCfgPath+sizeof(g_be_appNewCfgPath)/sizeof(TCHAR), be_main_arg_newcfgdir);
	}
	else
	{
#ifdef REFKEEN_PLATFORM_MACOS
		// FIXME - Handle sandboxing?
		BE_Cross_safeandfastcstringcopy_2strs(g_be_appNewCfgPath, g_be_appNewCfgPath+sizeof(g_be_appNewCfgPath)/sizeof(TCHAR), homeVar, "/Library/Application Support/reflection-keen");
#else
		envVar = getenv("XDG_CONFIG_HOME");
		if (envVar && *envVar)
		{
			BE_Cross_safeandfastcstringcopy_2strs(g_be_appNewCfgPath, g_be_appNewCfgPath+sizeof(g_be_appNewCfgPath)/sizeof(TCHAR), envVar, "/reflection-keen");
		}
		else if (homeVar && *homeVar)
		{
			BE_Cross_safeandfastcstringcopy_2strs(g_be_appNewCfgPath, g_be_appNewCfgPath+sizeof(g_be_appNewCfgPath)/sizeof(TCHAR), homeVar, "/.config/reflection-keen");
		}
		else
		{
			BE_Cross_safeandfastcstringcopy(g_be_appNewCfgPath, g_be_appNewCfgPath+sizeof(g_be_appNewCfgPath)/sizeof(TCHAR), ".");
		}
#endif
	}

	/*** Root paths ***/

	char path[BE_CROSS_PATH_LEN_BOUND];

	if (homeVar && *homeVar) // Should be set, otherwise there's a big problem (but warning is printed)
	{
		// Home dir
		BEL_Cross_AddRootPathIfDir(homeVar, "home", "Home dir");
#ifdef REFKEEN_PLATFORM_MACOS
		// Steam installation dir
		BE_Cross_safeandfastcstringcopy_2strs(path, path+sizeof(path)/sizeof(TCHAR), homeVar, "/Library/Application Support/Steam");
		BEL_Cross_AddRootPathIfDir(path, "steam", "Steam (installation)");
#else
		// Steam installation dir
		BE_Cross_safeandfastcstringcopy_2strs(path, path+sizeof(path)/sizeof(TCHAR), homeVar, "/.steam/steam");
		BEL_Cross_AddRootPathIfDir(path, "steam", "Steam (installation)");
		// GOG.com installation dir
		BE_Cross_safeandfastcstringcopy_2strs(path, path+sizeof(path)/sizeof(TCHAR), homeVar, "/GOG Games");
		BEL_Cross_AddRootPathIfDir(path, "gog", "GOG Games (default)");
#endif
	}
	// Finally the root itself (better keep it at the bottom of the list)
	BEL_Cross_AddRootPathIfDir("/", "/", "/");
#endif
}

static const BE_GameInstallation_T *g_be_selectedGameInstallation;
static const BE_EXEFileDetails_T *g_be_current_exeFileDetails;

#define BE_CROSS_MAX_GAME_INSTALLATIONS 7
static BE_GameInstallation_T g_be_gameinstallations[BE_CROSS_MAX_GAME_INSTALLATIONS];
int g_be_gameinstallations_num;

static BE_GameInstallation_T *g_be_gameinstallationsbyver[BE_GAMEVER_LAST];

const char *BE_Cross_GetGameInstallationDescription(int num)
{
	return g_be_gameinstallations[num].descStr;
}

int BE_Cross_GetGameVerFromInstallation(int num)
{
	return g_be_gameinstallations[num].verId;
}

// Main functions prototypes
void kdreams_exe_main(void);
int loadscn2_main(int argc, const char **argv);
void cat3d_exe_main(void);
void abysgame_exe_main(void);
void armgame_exe_main(void);
void apocgame_exe_main(void);
void intro_exe_main(void);
void slidecat_exe_main(void);
void loadscn_exe_main(void);
// Embedded resources loader functions prototypes
void RefKeen_Load_Embedded_Resources_From_kdreams_exe(void);
void RefKeen_Load_Embedded_Resources_From_catacombs_exe(void);
void RefKeen_Load_Embedded_Resources_From_slidecat_exe(void);

#ifdef REFKEEN_VER_KDREAMS
/*** v1.00 Registered EGA ***/
static const BE_GameFileDetails_T g_be_reqgameverfiles_kdreamse100[] = {
	{"AUDIO.KDR", 3498, 0x80ac85e5},
	{"EGAGRAPH.KDR", 213045, 0x2dc94687},
	{"GAMEMAPS.KDR", 63497, 0x7b517fa0},
	{"KDREAMS.EXE", 77694, 0xc73b8cb2},
	{0}
};

static const BE_EXEFileDetails_T g_be_exefiles_kdreamse100[] = {
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"AUDIODCT.KDR", 1024, 0x8b6116d7, 0x28984},
			{"AUDIOHHD.KDR", 340, 0x499e0cbf, 0x1f370},
			{"CONTEXT.KDR", 4759, 0x5bae2337, 0x1f4d0},
			{"EGADICT.KDR", 1024, 0xa69af202, 0x28188},
			{"EGAHEAD.KDR", 12068, 0xb9d789ee, 0x19610},
			{"GAMETEXT.KDR", 4686, 0x046c5328, 0x20770},
			{"MAPDICT.KDR", 1020, 0x8aa7334b, 0x28588},
			{"MAPHEAD.KDR", 11824, 0x4b9c9ebe, 0x1c540},
			{"PIRACY.BIN", 4001, 0x94458def, 0x14970}, // A bit different from PIRACY.SCN
			{"STORY.KDR", 2487, 0xed0ea5fe, 0x219c0},
			{0}
		},

		NULL,
		"KDREAMS.EXE",
		&kdreams_exe_main,
		&RefKeen_Load_Embedded_Resources_From_kdreams_exe,
		175424 - 0x1a00,
		BE_EXECOMPRESSION_PKLITE105,
		false
	},
	{0}
};

static const BE_GameVerDetails_T g_be_gamever_kdreamse100 = {
	g_be_reqgameverfiles_kdreamse100,
	g_be_exefiles_kdreamse100,
	CSTR_TO_TCSTR(BE_STR_GAMEVER_KDREAMSE100),
	"Keen Dreams EGA v1.00 (Custom)",
	0,
	BE_GAMEVER_KDREAMSE100
};

/*** v1.00 CGA ***/
static const BE_GameFileDetails_T g_be_reqgameverfiles_kdreamsc100[] = {
	{"AUDIO.KDR", 3498, 0x80ac85e5},
	{"CGAGRAPH.KDR", 134691, 0x05e32626},
	{"GAMEMAPS.KDR", 63497, 0x7b517fa0},
	{"KDREAMS.EXE", 75015, 0xb6ff595a},
	{0}
};

static const BE_EXEFileDetails_T g_be_exefiles_kdreamsc100[] = {
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"AUDIODCT.KDR", 1024, 0x8b6116d7, 0x281c2},
			{"AUDIOHHD.KDR", 340, 0x499e0cbf, 0x1e6b0},
			{"CGADICT.KDR", 1024, 0xaba89759, 0x279c6},
			{"CGAHEAD.KDR", 12068, 0x36d48226, 0x18950},
			{"CONTEXT.KDR", 4759, 0x5bae2337, 0x1e810},
			{"GAMETEXT.KDR", 4686, 0x046c5328, 0x1fab0},
			{"MAPDICT.KDR", 1020, 0x8aa7334b, 0x27dc6},
			{"MAPHEAD.KDR", 11824, 0x4b9c9ebe, 0x1b880},
			{"PIRACY.BIN", 4001, 0x94458def, 0x13cb0}, // A bit different from PIRACY.SCN
			{"STORY.KDR", 2487, 0xed0ea5fe, 0x20d00},
			{0}
		},

		NULL,
		"KDREAMS.EXE",
		&kdreams_exe_main,
		&RefKeen_Load_Embedded_Resources_From_kdreams_exe,
		172896 - 0x1800,
		BE_EXECOMPRESSION_PKLITE105,
		false
	},
	{0}
};

static const BE_GameVerDetails_T g_be_gamever_kdreamsc100 = {
	g_be_reqgameverfiles_kdreamsc100,
	g_be_exefiles_kdreamsc100,
	CSTR_TO_TCSTR(BE_STR_GAMEVER_KDREAMSC100),
	"Keen Dreams CGA v1.00 (Custom)",
	0,
	BE_GAMEVER_KDREAMSC100
};

/*** v1.13 (Shareware) ***/
static const BE_GameFileDetails_T g_be_reqgameverfiles_kdreamse113[] = {
	{"KDREAMS.AUD", 3498, 0x80ac85e5},
	{"KDREAMS.CMP", 14189, 0x97628ca0},
	{"KDREAMS.EGA", 213045, 0x2dc94687},
	{"KDREAMS.EXE", 81619, 0x9dce0a39},
	{"KDREAMS.MAP", 65673, 0x8dce09af},
	{"LAST.SHL", 1634, 0xc0a3560f},
	{0}
};

static const BE_EXEFileDetails_T g_be_exefiles_kdreamse113[] = {
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"AUDIODCT.KDR", 1024, 0x8b6116d7, 0x2a042},
			{"AUDIOHHD.KDR", 340, 0x499e0cbf, 0x22880},
			{"CONTEXT.KDR", 1283, 0x5a33439d, 0x229e0},
			{"EGADICT.KDR", 1024, 0xa69af202, 0x29846},
			{"EGAHEAD.KDR", 12068, 0xb9d789ee, 0x1cb20},
			{"GAMETEXT.KDR", 413, 0xb0df2792, 0x22ef0},
			{"MAPDICT.KDR", 1020, 0x9faa7213, 0x29c46},
			{"MAPHEAD.KDR", 11824, 0xb2f36c60, 0x1fa50},
			{"STORY.KDR", 2526, 0xcafc1d15, 0x23090},
			{0}
		},

		NULL,
		"KDREAMS.EXE",
		&kdreams_exe_main,
		&RefKeen_Load_Embedded_Resources_From_kdreams_exe,
		213536 - 0x1c00,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{
		NULL,

		NULL,
		"LOADSCN.EXE",
		(void (*)(void))&loadscn2_main,
		NULL,
		17712 - 0x200,
		BE_EXECOMPRESSION_LZEXE9X,
		true
	},
	{0}
};

static const BE_GameVerDetails_T g_be_gamever_kdreamse113 = {
	g_be_reqgameverfiles_kdreamse113,
	g_be_exefiles_kdreamse113,
	CSTR_TO_TCSTR(BE_STR_GAMEVER_KDREAMSE113),
	"Keen Dreams EGA v1.13 (Custom)",
	0,
	BE_GAMEVER_KDREAMSE113
};

/*** v1.05 (CGA) ***/
static const BE_GameFileDetails_T g_be_reqgameverfiles_kdreamsc105[] = {
	{"AUDIO.KDR", 3498, 0x80ac85e5},
	{"CGAGRAPH.KDR", 134691, 0x05e32626},
	{"GAMEMAPS.KDR", 65736, 0x4b92df5f},
	{"KDREAMS.EXE", 78253, 0x5af7ce2b},
	{0}
};

static const BE_EXEFileDetails_T g_be_exefiles_kdreamsc105[] = {
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"AUDIODCT.KDR", 1024, 0x8b6116d7, 0x28490},
			{"AUDIOHHD.KDR", 340, 0x499e0cbf, 0x1eb50},
			{"CGADICT.KDR", 1024, 0xaba89759, 0x27c94},
			{"CGAHEAD.KDR", 12068, 0x36d48226, 0x18df0},
			{"CONTEXT.KDR", 4759, 0x5bae2337, 0x1ecb0},
			{"GAMETEXT.KDR", 4686, 0x046c5328, 0x1ff50},
			{"MAPDICT.KDR", 1020, 0xfa8362f3, 0x28094},
			{"MAPHEAD.KDR", 11824, 0x66c122b4, 0x1bd20},
			{"STORY.KDR", 2487, 0xed0ea5fe, 0x211a0},
			{0}
		},

		NULL,
		"KDREAMS.EXE",
		&kdreams_exe_main,
		&RefKeen_Load_Embedded_Resources_From_kdreams_exe,
		202320 - 0x1800,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{0}
};

static const BE_GameVerDetails_T g_be_gamever_kdreamsc105 = {
	g_be_reqgameverfiles_kdreamsc105,
	g_be_exefiles_kdreamsc105,
	CSTR_TO_TCSTR(BE_STR_GAMEVER_KDREAMSC105),
	"Keen Dreams CGA v1.05 (Custom)",
	0,
	BE_GAMEVER_KDREAMSC105
};

// Keen Dreams v1.93 and v1.20 actually share the exact same game data,
// minus the EXE (and v1.92 is also the same, except for KDREAMS.CMP).

/*** v1.93 (Registered EGA) ***/
static const BE_GameFileDetails_T g_be_reqgameverfiles_kdreamse193[] = {
	{"KDREAMS.AUD", 3498, 0x80ac85e5},
	{"KDREAMS.CMP", 14620, 0x1681bc21},
	{"KDREAMS.EGA", 213045, 0x2dc94687},
	{"KDREAMS.EXE", 80715, 0x3d0cd9ed},
	{"KDREAMS.MAP", 65674, 0x78bd24fc},
	{0}
};

static const BE_EXEFileDetails_T g_be_exefiles_kdreamse193[] = {
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"AUDIODCT.KDR", 1024, 0x8b6116d7, 0x29fba},
			{"AUDIOHHD.KDR", 340, 0x499e0cbf, 0x21990},
			{"CONTEXT.KDR", 1283, 0x5a33439d, 0x21af0},
			{"EGADICT.KDR", 1024, 0xa69af202, 0x297be},
			{"EGAHEAD.KDR", 12068, 0xb9d789ee, 0x1bc30},
			{"GAMETEXT.KDR", 4256, 0xbfe72f94, 0x22000},
			{"MAPDICT.KDR", 1020, 0x6bb0de32, 0x29bbe},
			{"MAPHEAD.KDR", 11824, 0x2b821e29, 0x1eb60},
			{"STORY.KDR", 2526, 0xcafc1d15, 0x230a0},
			{0}
		},

		NULL,
		"KDREAMS.EXE",
		&kdreams_exe_main,
		&RefKeen_Load_Embedded_Resources_From_kdreams_exe,
		213200 - 0x1c00,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{0}
};

static const BE_GameVerDetails_T g_be_gamever_kdreamse193 = {
	g_be_reqgameverfiles_kdreamse193,
	g_be_exefiles_kdreamse193,
	CSTR_TO_TCSTR(BE_STR_GAMEVER_KDREAMSE193),
	"Keen Dreams EGA v1.93 (Custom)",
	0,
	BE_GAMEVER_KDREAMSE193
};

/*** v1.20 (Shareware) ***/
static const BE_GameFileDetails_T g_be_reqgameverfiles_kdreamse120[] = {
	{"KDREAMS.AUD", 3498, 0x80ac85e5},
	{"KDREAMS.CMP", 14620, 0x1681bc21},
	{"KDREAMS.EGA", 213045, 0x2dc94687},
	{"KDREAMS.EXE", 81519, 0xf36f0edf},
	{"KDREAMS.MAP", 65674, 0x78bd24fc},
	{0}
};

static const BE_EXEFileDetails_T g_be_exefiles_kdreamse120[] = {
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"AUDIODCT.KDR", 1024, 0x8b6116d7, 0x2a66c},
			{"AUDIOHHD.KDR", 340, 0x499e0cbf, 0x21d70},
			{"CONTEXT.KDR", 1283, 0x5a33439d, 0x21ed0},
			{"EGADICT.KDR", 1024, 0xa69af202, 0x29e70},
			{"EGAHEAD.KDR", 12068, 0xb9d789ee, 0x1c010},
			{"GAMETEXT.KDR", 4256, 0xbfe72f94, 0x223e0},
			{"MAPDICT.KDR", 1020, 0x6bb0de32, 0x2a270},
			{"MAPHEAD.KDR", 11824, 0x2b821e29, 0x1ef40},
			{"STORY.KDR", 2526, 0xcafc1d15, 0x23480},
			{0}
		},

		NULL,
		"KDREAMS.EXE",
		&kdreams_exe_main,
		&RefKeen_Load_Embedded_Resources_From_kdreams_exe,
		214912 - 0x1c00,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{0}
};

static const BE_GameVerDetails_T g_be_gamever_kdreamse120 = {
	g_be_reqgameverfiles_kdreamse120,
	g_be_exefiles_kdreamse120,
	CSTR_TO_TCSTR(BE_STR_GAMEVER_KDREAMSE120),
	"Keen Dreams EGA v1.20 (Custom)",
	0,
	BE_GAMEVER_KDREAMSE120
};

/*** 2015 edition - All files are external here ***/
static const BE_GameFileDetails_T g_be_reqgameverfiles_kdreams2015[] = {
	{"CONTEXT.KDR", 1331, 0x70920ec3},
	{"EGADICT.CGA", 1024, 0x75a458f2},
	{"EGADICT.KDR", 1024, 0xfe21f5c4},
	{"EGAHEAD.CGA", 12068, 0x38c15a01},
	{"EGAHEAD.KDR", 12068, 0xdeb318b3},
	{"GAMETEXT.KDR", 4314, 0x0996e7f2},
	{"KDREAMS.CGA", 179725, 0x83b0c6c4},
	{"KDREAMS.CMP", 40878, 0xf8a33e2f},
	{"KDREAMS.EGA", 213221, 0xee1024b9},
	{"KDREAMS.MAP", 65674, 0x934cd898},
	{"KDREAMS.SND", 1449826, 0xfb74b9a2},
	{"MAPDICT.KDR", 1020, 0x6bb0de32},
	{"MAPHEAD.KDR", 11824, 0xff335e8c},
	{"SOUNDDCT.KDR", 1024, 0x2217ceb9},
	{"SOUNDHHD.KDR", 228, 0xb8b39c5c},
	{"STORY.KDR", 2526, 0xcafc1d15},
	{0}
};

static const BE_EXEFileDetails_T g_be_exefiles_kdreams2015[] = {
	{
		NULL,

		NULL,
		NULL, // No EXE file
		&kdreams_exe_main,
		NULL, // No EXE file
		0, // No EXE file
		BE_EXECOMPRESSION_NONE, // No EXE file
		false
	},
	{0}
};

static const BE_GameVerDetails_T g_be_gamever_kdreams2015 = {
	g_be_reqgameverfiles_kdreams2015,
	g_be_exefiles_kdreams2015,
	CSTR_TO_TCSTR(BE_STR_GAMEVER_KDREAMS2015),
	"Keen Dreams 2015 (Custom)",
	44100, // Digitized sounds have such sample rate (in Hz)
	BE_GAMEVER_KDREAMS2015
};
#endif

#ifdef REFKEEN_VER_CAT3D
/*** v1.00 ***/
static const BE_GameFileDetails_T g_be_reqgameverfiles_cat3d100[] = {
	{"AUDIO.C3D", 5062, 0x074f7525},
	{"EGAGRAPH.C3D", 258007, 0x7b756d01},
	{"GAMEMAPS.C3D", 14288, 0x1d8d501e},
	{"CAT3D.EXE", 78483, 0xe20992c7},
	{0}
};

static const BE_EXEFileDetails_T g_be_exefiles_cat3d100[] = {
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"AUDIODCT.C3D", 1024, 0xd3dbe849, 0x22c64},
			{"AUDIOHHD.C3D", 368, 0xb83933bc, 0x1aa60},
			{"EGADICT.C3D", 1024, 0xab94fb6c, 0x23064},
			{"EGAHEAD.C3D", 1437, 0x33772bb0, 0x1abd0},
			{"INTROSCN.SCN", 4008, 0xec236c5c, 0x192c0},
			{"MTEMP.TMP", 618, 0x6b7cc556, 0x1b170},
			{0}
		},

		NULL,
		"CAT3D.EXE",
		&cat3d_exe_main,
		&RefKeen_Load_Embedded_Resources_From_catacombs_exe,
		191536 - 0x1400,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{0}
};

static const BE_GameVerDetails_T g_be_gamever_cat3d100 = {
	g_be_reqgameverfiles_cat3d100,
	g_be_exefiles_cat3d100,
	CSTR_TO_TCSTR(BE_STR_GAMEVER_CAT3D100),
	"Catacomb 3-D v1.00 (Custom)",
	0,
	BE_GAMEVER_CAT3D100
};

/*** v1.22 ***/
static const BE_GameFileDetails_T g_be_reqgameverfiles_cat3d122[] = {
	{"AUDIO.C3D", 5062, 0x074f7525},
	{"EGAGRAPH.C3D", 256899, 0xbc122f40},
	{"GAMEMAPS.C3D", 14288, 0x1ecb107b},
	{"CAT3D.EXE", 78285, 0xfaa27410},
	{0}
};

static const BE_EXEFileDetails_T g_be_exefiles_cat3d122[] = {
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"AUDIODCT.C3D", 1024, 0xd3dbe849, 0x22bd8},
			{"AUDIOHHD.C3D", 368, 0xb83933bc, 0x1a710},
			{"EGADICT.C3D", 1024, 0xb26a70a6, 0x22fd8},
			{"EGAHEAD.C3D", 1437, 0x3fde00c4, 0x1a880},
			// INTROSCN.SCN isn't displayed in vanilla v1.22, but it's still
			// allocated and in use, so it's safer to require this chunk
			{"INTROSCN.SCN", 4008, 0xcf9696af, 0x18f70},
			{"MTEMP.TMP", 618, 0x6b7cc556, 0x1ae20},
			{0}
		},

		NULL,
		"CAT3D.EXE",
		&cat3d_exe_main,
		&RefKeen_Load_Embedded_Resources_From_catacombs_exe,
		191904 - 0x1600,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{0}
};

static const BE_GameVerDetails_T g_be_gamever_cat3d122 = {
	g_be_reqgameverfiles_cat3d122,
	g_be_exefiles_cat3d122,
	CSTR_TO_TCSTR(BE_STR_GAMEVER_CAT3D122),
	"Catacomb 3-D v1.22 (Custom)",
	0,
	BE_GAMEVER_CAT3D122
};
#endif

#ifdef REFKEEN_VER_CATABYSS
/*** v1.13 (Shareware) ***/
static const BE_GameFileDetails_T g_be_reqgameverfiles_catabyss113[] = {
	{"ARMAPC.ABS", 10489, 0x9562020e},
	{"AUDIO.ABS", 6904, 0x462d2eb2},
	{"CATABYSS.EXE", 82662, 0xbc3b015e},
	{"EGAGRAPH.ABS", 336143, 0x3b2464a2},
	{"GAMEMAPS.ABS", 20433, 0x97ddb354},
	{"HELP.TXT", 15491, 0x7f8c4a59},
	{"LAST.ABS", 1290, 0xc36ba646},
	{"SHP01.ABS", 1376, 0xe04a6d1e},
	{"SHP02.ABS", 15181, 0x0198b8f7},
	{"SHP03.ABS", 13158, 0x0d6b4986},
	{"SHP04.ABS", 12648, 0x3bd5ccbc},
	{"SHP05.ABS", 724, 0x857573eb},
	{"SHP06.ABS", 9182, 0x86ce4004},
	{"SHP07.ABS", 9930, 0x6a22743f},
	{"SHP08.ABS", 398, 0x99404417},
	{"SHP09.ABS", 417, 0x201133a8},
	{"SHP10.ABS", 329, 0xaa51f92a},
	{"SHP11.ABS", 10595, 0x5a95691e},
	{"SHP12.ABS", 9182, 0x378b5984},
	// Looks like DEMOCAT stuff; Let's require these.
	{"ALTAR.CAT", 15014, 0x833d1ca7},
	{"APC.CAT", 12828, 0x009ec08a},
	{"CEMETRY.CAT", 14983, 0x8cfb3741},
	{"COOKIE.CAT", 16837, 0xee83c4f4},
	{"CRYSTAL.CAT", 15320, 0x16fa18f0},
	{"DEMOCAT.EXE", 32066, 0xce326ea2},
	{"DRAGONS.CAT", 7107, 0x54ea7eb5},
	{"EGYPT.CAT", 8863, 0xc78451d7},
	{"FLAME.CAT", 7062, 0xe422aa64},
	{"FOLLOW.CAT", 10437, 0xbec4bd75},
	{"GRATE.CAT", 8306, 0x8f7cb569},
	{"GROUPIC.CAT", 20563, 0x93531dd8},
	{"HOMEBBS.CAT", 3233, 0xaf3d0e16},
	{"INFERNO.CAT", 13934, 0x3ddd36e9},
	{"INVITED.CAT", 9723, 0xacc2ce73},
	{"LAST.CAT", 1247, 0x73f1ec9e},
	{"LEGEND.CAT", 10977, 0xc99a5540},
	{"LOUNGE.CAT", 10665, 0xb799f029},
	{"OPEN.CAT", 4445, 0xe90e743b},
	{"ORCMINE.CAT", 8903, 0xf8821eda},
	{"QUEST.CAT", 13076, 0x252de058},
	{"REDHALL.CAT", 8214, 0x0d9bd58e},
	{"SCRIPT.CAT", 174, 0x45973aa6},
	{"TRILOGY.CAT", 11397, 0xe09a1995},
	{"VAULT.CAT", 8737, 0x43d6dba7},
	{"VICTORY.CAT", 7194, 0x70846fc6},
	{0}
};

static const BE_EXEFileDetails_T g_be_exefiles_catabyss113[] = {
	// Intro EXE is the first one we begin from
	{
		NULL,

		NULL,
		"INTRO.EXE",
		(void (*)(void))&intro_exe_main,
		NULL,
		36560 - 0x600,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"AUDIODCT.ABS", 1024, 0xe9088011, 0x2554c},
			{"AUDIOHHD.ABS", 416, 0xfbfff495, 0x1a210},
			{"EGADICT.ABS", 1024, 0xbb760f1d, 0x2594c},
			{"EGAHEAD.ABS", 1881, 0xe31e1c3b, 0x1a3b0},
			{"MTEMP.TMP", 834, 0x5d9ccfb3, 0x1ab10},
			{0}
		},

		NULL,
		"CATABYSS.EXE",
		&abysgame_exe_main,
		&RefKeen_Load_Embedded_Resources_From_catacombs_exe,
		201120 - 0x1a00,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"TEXTSCN.SCN", 4000, 0xf7773f42, 0xbf70},
			{0}
		},

		"Electronic Catalog v1.00",
		"DEMOCAT.EXE",
		&slidecat_exe_main,
		&RefKeen_Load_Embedded_Resources_From_slidecat_exe,
		62800 - 0x800,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{
		NULL,

		NULL,
		"LOADSCN.EXE",
		&loadscn_exe_main,
		NULL,
		28992 - 0x400,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{0}
};

static const BE_GameVerDetails_T g_be_gamever_catabyss113 = {
	g_be_reqgameverfiles_catabyss113,
	g_be_exefiles_catabyss113,
	CSTR_TO_TCSTR(BE_STR_GAMEVER_CATABYSS113),
	"Catacomb Abyss v1.13 (Custom)",
	0,
	BE_GAMEVER_CATABYSS113
};

/*** v1.24 (Registered) ***/
static const BE_GameFileDetails_T g_be_reqgameverfiles_catabyss124[] = {
	{"ABYSGAME.EXE", 82479, 0x5f0319db},
	{"AUDIO.ABS", 6904, 0x462d2eb2},
	{"EGAGRAPH.ABS", 335994, 0xfd9995ad},
	{"GAMEMAPS.ABS", 20433, 0x97ddb354},
	{"HELP.TXT", 15042, 0xc47ce06e},
	{"SHP01.ABS", 1376, 0xe04a6d1e},
	{"SHP02.ABS", 14912, 0xffc72620},
	{"SHP03.ABS", 13186, 0x7b73bb7b},
	{"SHP04.ABS", 12648, 0x3bd5ccbc},
	{"SHP05.ABS", 724, 0x857573eb},
	{"SHP06.ABS", 9182, 0x86ce4004},
	{"SHP07.ABS", 9930, 0x6a22743f},
	{"SHP08.ABS", 398, 0x99404417},
	{"SHP09.ABS", 417, 0x201133a8},
	{"SHP10.ABS", 329, 0xaa51f92a},
	{"SHP11.ABS", 10328, 0x08ee65f4},
	{"SHP12.ABS", 1652, 0xbeb87fbd},
	// HINTCAT stuff similar to DEMOCAT; Require these as well.
	{"AQUDUCT.HNT", 7886, 0x66a11ac1},
	{"CEMETRY.HNT", 10376, 0xf7b3f888},
	{"COVEN.HNT", 13550, 0xc3fdd8a4},
	{"CRYPT.HNT", 11233, 0x46ca0a4b},
	{"DEN.HNT", 10520, 0xbea64562},
	{"GARDEN.HNT", 11125, 0x8c2385b6},
	{"HAUNT.HNT", 7743, 0x2b9ef944},
	{"HINTCAT.EXE", 16328, 0x2bd569ac},
	{"HINTGO1.HNT", 9751, 0x4fa00485},
	{"HINTGO2.HNT", 9902, 0x6640c7b6},
	{"HINTGO3.HNT", 8372, 0x8e743b2c},
	{"INFERNO.HNT", 11658, 0x9d59623a},
	{"MAUSGND.HNT", 11305, 0x70655f30},
	{"MAUSOL.HNT", 12075, 0xee9b8745},
	{"ORCMINE.HNT", 12664, 0x35502713},
	{"PASSAGE.HNT", 10159, 0x126e01a5},
	{"SANCTUM.HNT", 12030, 0x16c785e6},
	{"SCRIPT.HNT", 155, 0x985da39a},
	{"SUBVALT.HNT", 12110, 0xfb4d589d},
	{"TITANS.HNT", 9821, 0xd491ac71},
	{"TROLLS.HNT", 13409, 0x68a6a6e9},
	{"VICTORY.HNT", 7301, 0xb2a16e89},
	{0}
};

static const BE_EXEFileDetails_T g_be_exefiles_catabyss124[] = {
	// Again, intro EXE is the first one we begin from
	{
		NULL,

		NULL,
		"CATABYSS.EXE",
		(void (*)(void))&intro_exe_main,
		NULL,
		36064 - 0x600,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"AUDIODCT.ABS", 1024, 0xe9088011, 0x2543a},
			{"AUDIOHHD.ABS", 416, 0xfbfff495, 0x1a140},
			{"EGADICT.ABS", 1024, 0x63eb06d3, 0x2583a},
			{"EGAHEAD.ABS", 1881, 0x94967205, 0x1a2e0},
			{"MTEMP.TMP", 834, 0x5d9ccfb3, 0x1aa40},
			{0}
		},

		NULL,
		"ABYSGAME.EXE",
		&abysgame_exe_main,
		&RefKeen_Load_Embedded_Resources_From_catacombs_exe,
		200848 - 0x1a00,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"TEXTSCN.SCN", 4000, 0xf7773f42, 0x6f60},
			{0}
		},

		"Catacomb Abyss 3-D Hint Book v1.01",
		"HINTCAT.EXE",
		&slidecat_exe_main,
		&RefKeen_Load_Embedded_Resources_From_slidecat_exe,
		39968 - 0x600,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{0}
};

static const BE_GameVerDetails_T g_be_gamever_catabyss124 = {
	g_be_reqgameverfiles_catabyss124,
	g_be_exefiles_catabyss124,
	CSTR_TO_TCSTR(BE_STR_GAMEVER_CATABYSS124),
	"Catacomb Abyss v1.24 (Custom)",
	0,
	BE_GAMEVER_CATABYSS124
};
#endif

#ifdef REFKEEN_VER_CATARM
static const BE_GameFileDetails_T g_be_reqgameverfiles_catarm102[] = {
	{"ARMGAME.EXE", 82198, 0x7bf5e3d5},
	// Looks like ARM_SLIB.ARM is mentioned in DEF.H but unused
	{"AUDIO.ARM", 7104, 0xb6c28bc7},
	{"EGAGRAPH.ARM", 355832, 0x8eea3210},
	{"GAMEMAPS.ARM", 17653, 0x787794bf},
	{"SHP10.ARM", 9930, 0x6a22743f},
	{"SHP11.ARM", 398, 0x99404417},
	{"SHP12.ARM", 417, 0x201133a8},
	{"SHP13.ARM", 329, 0xaa51f92a},
	{"SHP14.ARM", 1688, 0x98e0af99},
	{"SHP1.ARM", 1376, 0xe04a6d1e},
	{"SHP2.ARM", 3991, 0x1b4bb85b},
	{"SHP3.ARM", 15036, 0xe7c6a5a3},
	{"SHP4.ARM", 15568, 0x9fa3faee},
	{"SHP5.ARM", 13234, 0x3f90b11d},
	{"SHP6.ARM", 10861, 0x42faf6e5},
	{"SHP7.ARM", 9414, 0x1a969278},
	{"SHP8.ARM", 749, 0x086438f0},
	{"SHP9.ARM", 9182, 0x86ce4004},
	// HINTCAT stuff
	{"HINT01.HNT", 9664, 0xb6ecee18},
	{"HINT02.HNT", 9833, 0xa01e9cba},
	{"HINT03.HNT", 12060, 0xa8857173},
	{"HINT04.HNT", 10438, 0x60866c28},
	{"HINT05.HNT", 11555, 0x0973da05},
	{"HINT06.HNT", 10339, 0x1e5089d5},
	{"HINT07.HNT", 12888, 0x0bc7121d},
	{"HINT08.HNT", 10400, 0x0b33d707},
	{"HINT09.HNT", 10754, 0x4f787690},
	{"HINT10.HNT", 10262, 0xbddb06e5},
	{"HINT11.HNT", 12006, 0x47104911},
	{"HINT12.HNT", 12757, 0x340d8383},
	{"HINT13.HNT", 11662, 0xe2e1890f},
	{"HINT14.HNT", 9271, 0x74bbe8c7},
	{"HINT15.HNT", 11624, 0x8e842f18},
	{"HINT16.HNT", 12067, 0xffccf406},
	{"HINT17.HNT", 12342, 0xf24371ec},
	{"HINT18.HNT", 8896, 0x18567f5d},
	{"HINT19.HNT", 9308, 0xdb5b1b65},
	{"HINT20.HNT", 7301, 0xb2a16e89},
	{"HINTCAT.EXE", 15973, 0x79cc6ee3},
	{"SCRIPT.HNT", 90, 0x79a7f746},
	{0}
};

static const BE_EXEFileDetails_T g_be_exefiles_catarm102[] = {
	{
		NULL,

		NULL,
		"CATARM.EXE",
		(void (*)(void))&intro_exe_main,
		NULL,
		36448 - 0x600,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"AUDIODCT.ARM", 1024, 0x8f1d4dd2, 0x240b0},
			{"AUDIOHHD.ARM", 428, 0x5f863ad2, 0x1bb20},
			{"EGADICT.ARM", 1024, 0xab662db8, 0x244b0},
			{"EGAHEAD.ARM", 1977, 0x711cbf10, 0x1bcd0},
			{"MTEMP.TMP", 834, 0x546f00d1, 0x1b7d0},
			{0}
		},

		NULL,
		"ARMGAME.EXE",
		&armgame_exe_main,
		&RefKeen_Load_Embedded_Resources_From_catacombs_exe,
		198304 - 0x2000,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"TEXTSCN.SCN", 4000, 0xf7773f42, 0x6dd0},
			{0}
		},

		"Catacomb Armageddon 3-D Hint Book v1.12",
		"HINTCAT.EXE",
		&slidecat_exe_main,
		&RefKeen_Load_Embedded_Resources_From_slidecat_exe,
		39296 - 0x600,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{0}
};

static const BE_GameVerDetails_T g_be_gamever_catarm102 = {
	g_be_reqgameverfiles_catarm102,
	g_be_exefiles_catarm102,
	CSTR_TO_TCSTR(BE_STR_GAMEVER_CATARM102),
	"Catacomb Armageddon v1.02 (Custom)",
	0,
	BE_GAMEVER_CATARM102
};
#endif

#ifdef REFKEEN_VER_CATAPOC
static const BE_GameFileDetails_T g_be_reqgameverfiles_catapoc101[] = {
	{"APOCGAME.EXE", 82386, 0x48ca6808},
	// No APC_SLIB.APC file was found, although it is mentioned in DEF.H
	{"AUDIO.APC", 7366, 0x23934cec},
	{"EGAGRAPH.APC", 382078, 0x291893c3},
	{"GAMEMAPS.APC", 18855, 0xd317b2c9},
	{"SHP10.APC", 9930, 0x6a22743f},
	{"SHP11.APC", 398, 0x99404417},
	{"SHP12.APC", 417, 0x201133a8},
	{"SHP13.APC", 329, 0xaa51f92a},
	{"SHP14.APC", 1668, 0xa4a8e44e},
	{"SHP1.APC", 1376, 0xe04a6d1e},
	{"SHP2.APC", 6329, 0xff48bc57},
	{"SHP3.APC", 13187, 0x5800b7f5},
	{"SHP4.APC", 13592, 0x1c498a90},
	{"SHP5.APC", 12698, 0xf2fa1802},
	{"SHP6.APC", 11131, 0x110da23b},
	{"SHP7.APC", 13266, 0x9ec74514},
	{"SHP8.APC", 640, 0x864e6d0c},
	{"SHP9.APC", 9182, 0x86ce4004},
	// HINTCAT stuff
	{"HINT01.HNT", 9664, 0x1641c8f0},
	{"HINT02.HNT", 9823, 0x0d319717},
	{"HINT03.HNT", 10822, 0x994d94cd},
	{"HINT04.HNT", 11654, 0xe754554f},
	{"HINT05.HNT", 8021, 0xfd819cab},
	{"HINT06.HNT", 9839, 0x13f601ce},
	{"HINT07.HNT", 10314, 0xa049c277},
	{"HINT08.HNT", 9616, 0xa5990371},
	{"HINT09.HNT", 9074, 0x61e1681e},
	{"HINT10.HNT", 9744, 0x38ae11c9},
	{"HINT11.HNT", 14563, 0x7c233efa},
	{"HINT12.HNT", 9261, 0x714bd15a},
	{"HINT13.HNT", 10056, 0xebf72992},
	{"HINT14.HNT", 6552, 0x5039141c},
	{"HINT15.HNT", 10738, 0x60fd9f81},
	{"HINT16.HNT", 8302, 0xcace81b8},
	{"HINT17.HNT", 8348, 0x00783edb},
	{"HINT18.HNT", 9990, 0x04e56bbe},
	{"HINT19.HNT", 7301, 0xb2a16e89},
	{"HINTCAT.EXE", 16215, 0xfefa177c},
	{"SCRIPT.HNT", 88, 0x8dd529b2},
	{0}
};

static const BE_EXEFileDetails_T g_be_exefiles_catapoc101[] = {
	{
		NULL,

		NULL,
		"CATAPOC.EXE",
		(void (*)(void))&intro_exe_main,
		NULL,
		40208 - 0x600,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"AUDIODCT.APC", 1024, 0x26658498, 0x2439c},
			{"AUDIOHHD.APC", 452, 0x76adb051, 0x1bd80},
			{"EGADICT.APC", 1024, 0xb2ed57fd, 0x2479c},
			{"EGAHEAD.APC", 2049, 0xd7548ed8, 0x1bf50},
			{"MTEMP.TMP", 834, 0x90742162, 0x1ba30},
			{0}
		},

		NULL,
		"APOCGAME.EXE",
		&apocgame_exe_main,
		&RefKeen_Load_Embedded_Resources_From_catacombs_exe,
		200064 - 0x2200,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{
		(const BE_EmbeddedGameFileDetails_T []) {
			{"TEXTSCN.SCN", 4000, 0xf7773f42, 0x6ef0},
			{0}
		},


		"Catacomb Apocalypse 3-D Hint Book v1.13",
		"HINTCAT.EXE",
		&slidecat_exe_main,
		&RefKeen_Load_Embedded_Resources_From_slidecat_exe,
		39568 - 0x600,
		BE_EXECOMPRESSION_LZEXE9X,
		false
	},
	{0}
};

static const BE_GameVerDetails_T g_be_gamever_catapoc101 = {
	g_be_reqgameverfiles_catapoc101,
	g_be_exefiles_catapoc101,
	CSTR_TO_TCSTR(BE_STR_GAMEVER_CATAPOC101),
	"Catacomb Apocalypse v1.01 (Custom)",
	0,
	BE_GAMEVER_CATAPOC101
};
#endif


static const BE_GameVerDetails_T *g_be_gamever_ptrs[] = {
#ifdef REFKEEN_VER_KDREAMS
	&g_be_gamever_kdreamse100,
	&g_be_gamever_kdreamsc100,
	&g_be_gamever_kdreamse113,
	&g_be_gamever_kdreamsc105,
	&g_be_gamever_kdreamse193,
	&g_be_gamever_kdreamse120,
	&g_be_gamever_kdreams2015,
#endif
#ifdef REFKEEN_VER_CAT3D
	&g_be_gamever_cat3d100,
	&g_be_gamever_cat3d122,
#endif
#ifdef REFKEEN_VER_CATABYSS
	&g_be_gamever_catabyss113,
	&g_be_gamever_catabyss124,
#endif
#ifdef REFKEEN_VER_CATARM
	&g_be_gamever_catarm102,
#endif
#ifdef REFKEEN_VER_CATAPOC
	&g_be_gamever_catapoc101,
#endif
};

const BE_GameFileDetails_T *g_be_gamefiledetails_ptrs[]  = {
#ifdef REFKEEN_VER_KDREAMS
	g_be_reqgameverfiles_kdreamse100,
	g_be_reqgameverfiles_kdreamsc100,
	g_be_reqgameverfiles_kdreamse113,
	g_be_reqgameverfiles_kdreamsc105,
	g_be_reqgameverfiles_kdreamse193,
	g_be_reqgameverfiles_kdreamse120,
	g_be_reqgameverfiles_kdreams2015,
#endif
#ifdef REFKEEN_VER_CAT3D
	g_be_reqgameverfiles_cat3d100,
	g_be_reqgameverfiles_cat3d122,
#endif
#ifdef REFKEEN_VER_CATABYSS
	g_be_reqgameverfiles_catabyss113,
	g_be_reqgameverfiles_catabyss124,
#endif
#ifdef REFKEEN_VER_CATARM
	g_be_reqgameverfiles_catarm102,
#endif
#ifdef REFKEEN_VER_CATAPOC
	g_be_reqgameverfiles_catapoc101,
#endif
};


// C99
BE_FILE_T BE_Cross_IsFileValid(BE_FILE_T fp);
BE_FILE_T BE_Cross_GetNilFile(void);
int BE_Cross_seek(BE_FILE_T fp, long int offset, int origin);
int BE_Cross_putc(int character, BE_FILE_T fp);
int BE_Cross_getc(BE_FILE_T fp);
void BE_Cross_close(BE_FILE_T fp);



// WARNING: Do *not* assume this is recursive!!
static void BEL_Cross_mkdir(const TCHAR *path)
{
#ifdef REFKEEN_PLATFORM_WINDOWS
	_tmkdir(path);
#else
	_tmkdir(path, 0755);
#endif
}

// Opens a (possibly-existing) file from given directory in a case-insensitive manner
//
// OPTIONAL ARGUMENT (used internally): outfullpath, if not NULL, should point to an out buffer which is BE_CROSS_PATH_LEN_BOUND chars long.
static BE_FILE_T BEL_Cross_open_from_dir(const char *filename, bool isOverwriteRequest, const TCHAR *searchdir, TCHAR *outfullpath)
{
	_TDIR *dir;
	struct _tdirent *direntry;
	dir = _topendir(searchdir);
	if (!dir)
		return NULL;

	TCHAR fullpath[BE_CROSS_PATH_LEN_BOUND];

	for (direntry = _treaddir(dir); direntry; direntry = _treaddir(dir))
	{
		/*** Ignore non-ASCII filenames ***/
		if (*BEL_Cross_tstr_find_nonascii_ptr(direntry->d_name))
		{
			continue;
		}
		if (!BEL_Cross_tstr_to_cstr_ascii_casecmp(direntry->d_name, filename))
		{
			// Just a little sanity check
			if (_tcslen(searchdir) + 1 + _tcslen(direntry->d_name) >= BE_CROSS_PATH_LEN_BOUND)
			{
				_tclosedir(dir);
				return NULL;
			}
			TCHAR *fullpathEnd = fullpath + sizeof(fullpath)/sizeof(TCHAR);
			BEL_Cross_safeandfastctstringcopy_3strs(fullpath, fullpathEnd, searchdir, _T("/"), direntry->d_name);

			_tclosedir(dir);
			if (outfullpath)
			{
				memcpy(outfullpath, fullpath, sizeof(fullpath));
			}
			return _tfopen(fullpath, isOverwriteRequest ? _T("wb") : _T("rb"));
		}
	}
	_tclosedir(dir);
	// If tried to open for reading, we return NULL, otherwise we attempt create new file
	if (!isOverwriteRequest)
		return NULL;
	TCHAR *fullpathEnd = fullpath + sizeof(fullpath)/sizeof(TCHAR);
	TCHAR *fullpathPtr = BEL_Cross_safeandfastctstringcopy_2strs(fullpath, fullpathEnd, searchdir, _T("/"));
	// Create actual new files with a lower case, just because that's a common pattern in Unix-like setups
	// (basically a modified BE_Cross_safeandfastcstringcopy, also copying a narrow string to a wide string).
	//
	// Note: fullpathPtr should initially point to an instance of '\0', so fullpathPtr < fullpathEnd.
	char ch;
	do
	{
		ch = *filename++;
		*fullpathPtr++ = BE_Cross_tolower(ch); // This includes the null terminator if there's the room
	} while ((fullpathPtr < fullpathEnd) && ch);
	// These work in case fullpathPtr == fullpathEnd, and also if not
	--fullpathPtr;
	*fullpathPtr = _T('\0');

	if (outfullpath)
	{
		memcpy(outfullpath, fullpath, sizeof(fullpath));
	}
	return _tfopen(fullpath, _T("wb"));
}

// Returns 0 if not found, 1 if found with some data mismatch, or 2 otherwise
//
// OPTIONAL ARGUMENT (used internally): outfullpath, if not NULL, should point to an out buffer which is BE_CROSS_PATH_LEN_BOUND chars long.
static int BEL_Cross_CheckGameFileDetails(const BE_GameFileDetails_T *details, const TCHAR *searchdir, TCHAR *outfullpath)
{
	BE_FILE_T fp = BEL_Cross_open_from_dir(details->filename, false, searchdir, outfullpath);
	if (!fp)
		return 0;

	if (details->filesize == BE_Cross_FileLengthFromHandle(fp))
	{
		uint32_t crc32;
		if (!Crc32_ComputeFile(fp, &crc32) && (crc32 == details->crc32))
		{
			BE_Cross_close(fp);
			return 2;
		}
	}
	BE_Cross_close(fp);
	return 1;
}

// ***ASSUMPTION: descStr points to a C string literal which is never modified nor deleted!!!***
static void BEL_Cross_ConditionallyAddGameInstallation_WithReturnedErrMsg(const BE_GameVerDetails_T *details, const TCHAR *searchdir, const char *descStr, BE_TryAddGameInstallation_ErrorMsg_T *outErrMsg)
{
	char errorMsg[256];

	if (g_be_gameinstallationsbyver[details->verId])
		return;

	if (g_be_gameinstallations_num >= BE_CROSS_MAX_GAME_INSTALLATIONS)
		BE_ST_ExitWithErrorMsg("BEL_Cross_ConditionallyAddGameInstallation_WithReturnedErrMsg: Too many game installations!");

	BE_GameInstallation_T *gameInstallation = &g_be_gameinstallations[g_be_gameinstallations_num];
	// If used correctly then these SHOULD have enough space
	BEL_Cross_safeandfastctstringcopy(gameInstallation->instPath, gameInstallation->instPath+sizeof(gameInstallation->instPath)/sizeof(TCHAR), searchdir);
	gameInstallation->verId = details->verId;
	gameInstallation->descStr = descStr; // ASSUMPTION: This is a C string literal!!!

	TCHAR tempFullPath[BE_CROSS_PATH_LEN_BOUND];

	TCHAR *endPtr = gameInstallation->writableFilesPath + sizeof(gameInstallation->writableFilesPath)/sizeof(TCHAR);
	BEL_Cross_safeandfastctstringcopy_3strs(gameInstallation->writableFilesPath, endPtr, g_be_appDataPath, _T("/"), details->writableFilesDir);

	for (const BE_GameFileDetails_T *fileDetailsBuffer = details->reqFiles; fileDetailsBuffer->filename; ++fileDetailsBuffer)
	{
		// Check in writableFilesPath first. If WRONG file is found, REMOVE(!)
		switch (BEL_Cross_CheckGameFileDetails(fileDetailsBuffer, gameInstallation->writableFilesPath, tempFullPath))
		{
		case 2: // Match found
			continue;
		case 1: // Wrong file found in writableFilesPath: DELETE, then verify it's actually deleted (if there are multiple files differing just by case, this is an error, too.)
		{
			// Actually, there's a special case in which we don't delete and even accept the different file...
			// Each EXE must be unmodified, though (used for version identification, and possibly also the extraction of embedded resources)
			// But it's also possible to have no EXE (e.g., Keen Dreams, 2015 release) so check this.
			//
			// KNOWN LIMITATION: Resources embedded in an EXE may *not* be modified.
			if (g_refKeenCfg.manualGameVerMode)
			{
				const BE_EXEFileDetails_T *exeFileDetailsBuffer;
				for (exeFileDetailsBuffer = details->exeFiles; exeFileDetailsBuffer->mainFuncPtr; ++exeFileDetailsBuffer)
					if (!BE_Cross_strcasecmp(fileDetailsBuffer->filename, exeFileDetailsBuffer->exeName))
						break;

				if (!exeFileDetailsBuffer->mainFuncPtr) // fileDetailsBuffer does not refer to an EXE file, so it may be skipped
					continue;
			}

			_tremove(tempFullPath);
			BE_FILE_T fp = BEL_Cross_open_from_dir(fileDetailsBuffer->filename, false, gameInstallation->writableFilesPath, NULL);
			if (fp)
			{
				fclose(fp);
				snprintf(errorMsg, sizeof(errorMsg), "BEL_Cross_ConditionallyAddGameInstallation_WithReturnedErrMsg: Cannot remove file with\nunexpected contents! Alternatively, one such file has been removed, but there's\nanother one differing just by case.\nFilename: %s", fileDetailsBuffer->filename);
				BE_ST_ExitWithErrorMsg(errorMsg);
			}
			break;
		}
		}
		// No match found (and possibly deleted wrong file from writableFilesPath), recheck in installation path
		switch (BEL_Cross_CheckGameFileDetails(fileDetailsBuffer, searchdir, NULL))
		{
		case 2: // Match found
			continue;
		case 1: // Wrong file found
			if (g_refKeenCfg.manualGameVerMode)
				continue; // A special case again (where a wrong file is acceptable)
			// Fall-through
		default:
			// (Matching, and in manual mode, also wrong) file not found, we cannot add a new game installation
			if (outErrMsg)
				BE_Cross_safeandfastcstringcopy_2strs(*outErrMsg, (*outErrMsg) + sizeof(*outErrMsg), "Wrong or missing file ", fileDetailsBuffer->filename);
			return;
		}
	}

	++g_be_gameinstallations_num; // We KNOW we have the required data at this point

	// Create dirs in case we need just writableFilesPath (creation isn't recursive)
	BEL_Cross_mkdir(g_be_appDataPath); // Non-recursive
	BEL_Cross_mkdir(gameInstallation->writableFilesPath);

	// Finish with this
	g_be_gameinstallationsbyver[details->verId] = gameInstallation;
}

// ***ASSUMPTION: Again, descStr points to a C string literal which is never modified nor deleted!!!***
static void BEL_Cross_ConditionallyAddGameInstallation(const BE_GameVerDetails_T *details, const TCHAR *searchdir, const char *descStr)
{
	BEL_Cross_ConditionallyAddGameInstallation_WithReturnedErrMsg(details, searchdir, descStr, NULL);
}

static void BEL_Cross_CreateTrimmedFilename(const char *inFilename, char (*outFileName)[13])
{
	// Remove trailing spaces (required for filenames stored in SCRIPT.HNT The Catacomb Armageddon v1.02 (used by HINTCAT.EXE))
	char *fixedFilenamePtr = BE_Cross_safeandfastcstringcopy((*outFileName), (*outFileName) + sizeof(*outFileName), inFilename);
	if (fixedFilenamePtr != *outFileName) // Copied string isn't empty (which would actually be bad, anyway...)
	{
		--fixedFilenamePtr;
		do
		{
			if (*fixedFilenamePtr != ' ')
				break;
			*fixedFilenamePtr = '\0';
			// Technically, checking *(fixedFilenamePtr-1) leads to undefined behaviors, and even
			// the pointer itself doesn't have a clear manner, so do the following check (of the *original ptr)
			if (fixedFilenamePtr-- == *outFileName)
				break;
		}
		while (true);
	}
}

// Opens a read-only file for reading from a "search path" in a case-insensitive manner
BE_FILE_T BE_Cross_open_readonly_for_reading(const char *filename)
{
	char trimmedFilename[13];
	BEL_Cross_CreateTrimmedFilename(filename, &trimmedFilename);
	// Trying writableFilesPath first, and then instPath in case of failure
	BE_FILE_T fp = BEL_Cross_open_from_dir(trimmedFilename, false, g_be_selectedGameInstallation->writableFilesPath, NULL);
	if (fp)
		return fp;
	return BEL_Cross_open_from_dir(trimmedFilename, false, g_be_selectedGameInstallation->instPath, NULL);
}

// Opens a rewritable file for reading in a case-insensitive manner, checking just a single path
BE_FILE_T BE_Cross_open_rewritable_for_reading(const char *filename)
{
	char trimmedFilename[13];
	BEL_Cross_CreateTrimmedFilename(filename, &trimmedFilename);
	return BEL_Cross_open_from_dir(trimmedFilename, false, g_be_selectedGameInstallation->writableFilesPath, NULL);
}

// Opens a rewritable file for overwriting in a case-insensitive manner, checking just a single path
BE_FILE_T BE_Cross_open_rewritable_for_overwriting(const char *filename)
{
	char trimmedFilename[13];
	BEL_Cross_CreateTrimmedFilename(filename, &trimmedFilename);
	return BEL_Cross_open_from_dir(trimmedFilename, true, g_be_selectedGameInstallation->writableFilesPath, NULL);
}

// Used for e.g., the RefKeen cfg file
BE_FILE_T BE_Cross_open_additionalfile_for_reading(const char *filename)
{
	return BEL_Cross_open_from_dir(filename, false, g_be_appNewCfgPath, NULL);
}

BE_FILE_T BE_Cross_open_additionalfile_for_overwriting(const char *filename)
{
	// Do this just in case, but note that this isn't recursive
	BEL_Cross_mkdir(g_be_appNewCfgPath); // Non-recursive

	return BEL_Cross_open_from_dir(filename, true, g_be_appNewCfgPath, NULL);
}

// Loads a file originally embedded into the EXE (for DOS) to a newly allocated
// chunk of memory. Should be freed with BE_Cross_free_mem_loaded_embedded_rsrc.
// Returns chunk size if successful, or a negative number in case of failure.
int BE_Cross_load_embedded_rsrc_to_mem(const char *filename, void **ptr)
{
	BE_FILE_T fp = BE_Cross_open_readonly_for_reading(filename);
	if (!fp)
		return -1;

	int filesize = BE_Cross_FileLengthFromHandle(fp);
	*ptr = malloc(filesize);
	bool success = (*ptr) && (fread(*ptr, filesize, 1, fp) == 1);
	if (!success)
		free(*ptr); // This is OK with NULL

	fclose(fp);
	return success ? filesize : -1;
}

// Frees file loaded using BE_Cross_load_embedded_rsrc_to_mem. Accepts a NULL pointer.
void BE_Cross_free_mem_loaded_embedded_rsrc(void *ptr)
{
	free(ptr);
}

static unsigned char *g_be_current_exeImage;

void *BE_Cross_BmallocFromEmbeddedData(const char *name, uint16_t *pSize)
{
	const BE_EmbeddedGameFileDetails_T *embeddedFile = g_be_current_exeFileDetails->embeddedFiles;
	if (embeddedFile)
		for (; embeddedFile->fileDetails.filename && BE_Cross_strcasecmp(name, embeddedFile->fileDetails.filename); ++embeddedFile)
			;

	if (!embeddedFile || !(embeddedFile->fileDetails.filename))
		BE_ST_ExitWithErrorMsg("BE_Cross_BmallocFromEmbeddedData: Unrecognized embedded data name!");

#ifdef REFKEEN_ENABLE_EMBEDDED_FILES_CRC32_CHECKS
	if (Crc32_ComputeBuf(0, g_be_current_exeImage + embeddedFile->offset, embeddedFile->fileDetails.filesize) != embeddedFile->fileDetails.crc32)
		BE_ST_ExitWithErrorMsg("BE_Cross_BmallocFromEmbeddedData: Unexpectedly got the wrong CRC32!");
#endif

	void *ptr = BE_Cross_Bmalloc(embeddedFile->fileDetails.filesize);
	if (ptr)
	{
		memcpy(ptr, g_be_current_exeImage + embeddedFile->offset, embeddedFile->fileDetails.filesize);
		if (pSize)
			*pSize = embeddedFile->fileDetails.filesize;
	}
	return ptr;
}

void *BE_Cross_BfarmallocFromEmbeddedData(const char *name, uint32_t *pSize)
{
	const BE_EmbeddedGameFileDetails_T *embeddedFile = g_be_current_exeFileDetails->embeddedFiles;
	if (embeddedFile)
		for (; embeddedFile->fileDetails.filename && BE_Cross_strcasecmp(name, embeddedFile->fileDetails.filename); ++embeddedFile)
			;

	if (!embeddedFile || !(embeddedFile->fileDetails.filename))
		BE_ST_ExitWithErrorMsg("BE_Cross_BfarmallocFromEmbeddedData: Unrecognized embedded data name!");

#ifdef REFKEEN_ENABLE_EMBEDDED_FILES_CRC32_CHECKS
	if (Crc32_ComputeBuf(0, g_be_current_exeImage + embeddedFile->offset, embeddedFile->fileDetails.filesize) != embeddedFile->fileDetails.crc32)
		BE_ST_ExitWithErrorMsg("BE_Cross_BfarmallocFromEmbeddedData: Unexpectedly got the wrong CRC32!");
#endif

	void *ptr = BE_Cross_Bfarmalloc(embeddedFile->fileDetails.filesize);
	if (ptr)
	{
		memcpy(ptr, g_be_current_exeImage + embeddedFile->offset, embeddedFile->fileDetails.filesize);
		if (pSize)
			*pSize = embeddedFile->fileDetails.filesize;
	}
	return ptr;
}

// MICRO-OPTIMIZATION: Not needed for all games
#ifdef BE_CROSS_ENABLE_SORTED_FILENAMES_FUNC
int BE_Cross_GetSortedRewritableFilenames_AsUpperCase(char *outFilenames, int maxNum, int strLenBound, const char *suffix)
{
	struct _tdirent *direntry;
	size_t sufLen = strlen(suffix);
	char *nextFilename = outFilenames, *outFilenamesEnd = outFilenames + maxNum*strLenBound, *outFilenamesLast = outFilenamesEnd - strLenBound;
	char *checkFilename, *checkCh, *dnameCStr;
	// For the sake of consistency we look for files just in this path
	_TDIR * dir = _topendir(g_be_selectedGameInstallation->writableFilesPath);
	if (!dir)
	{
		return 0;
	}
	for (direntry = _treaddir(dir); direntry; direntry = _treaddir(dir))
	{
		size_t len = _tcslen(direntry->d_name);
		TCHAR *tchPtr;
		/*** Ignore non-ASCII filenames ***/
		if (*BEL_Cross_tstr_find_nonascii_ptr(direntry->d_name))
		{
			continue;
		}
		if ((len < sufLen) || BEL_Cross_tstr_to_cstr_ascii_casecmp(direntry->d_name+len-sufLen, suffix))
		{
			continue;
		}
		len -= sufLen;
		/*** Possibly a HACK - Modify d_name itself ***/
		len = (len >= (size_t)strLenBound) ? (strLenBound-1) : len;
		direntry->d_name[len] = _T('\0');
		/*** Another HACK - Further convert d_name from wide string on Windows (and watch out due to strict aliasing rules) ***/
		tchPtr = direntry->d_name;
		dnameCStr = (char *)tchPtr;
		for (checkCh = dnameCStr; *tchPtr; ++checkCh, ++tchPtr)
		{
			*checkCh = BE_Cross_toupper(*tchPtr); // Even if *tchPtr is a wide char, we know it's an ASCII char at this point
		}
#ifdef REFKEEN_PLATFORM_WINDOWS
		*checkCh = '\0'; // Required if converted from wide string
#endif
		// This is basically insertion-sort, but we store
		// the *last* entries if there isn't enough room.
		for (checkFilename = outFilenames; checkFilename < nextFilename; checkFilename += strLenBound)
		{
			if (strcmp(checkFilename, dnameCStr) > 0)
			{
				break;
			}
		}
		// Gone over all inserted entries
		if (checkFilename == nextFilename)
		{
			if (nextFilename < outFilenamesEnd)
			{
				memcpy(nextFilename, dnameCStr, 1+len);
				nextFilename += strLenBound;
			}
			else
			{
				memmove(outFilenames, outFilenames+strLenBound, strLenBound*(maxNum-1));
				memcpy(outFilenamesLast, dnameCStr, 1+len);
			}
		}
		// Shift existing entries and insert new one
		else
		{
			// If there's room for another entry, shift "forward"
			if (nextFilename < outFilenamesEnd)
			{
				memmove(checkFilename + strLenBound, checkFilename, outFilenamesEnd-checkFilename-strLenBound);
				memcpy(checkFilename, dnameCStr, 1+len);
				nextFilename += strLenBound;
			}
			// Otherwise shift "backwards", but only if there's already an entry "smaller" than current one
			else if (checkFilename != outFilenames)
			{
				memmove(outFilenames, outFilenames+strLenBound, (checkFilename-strLenBound)-outFilenames);
				memcpy(checkFilename-strLenBound, dnameCStr, 1+len);
			}
		};
	}
	_tclosedir(dir);
	return (nextFilename-outFilenames)/strLenBound;
}
#endif

// SPECIAL - Attempts to open config/config.vdf file from Steam
// installation, used for importing controller mappings.
//
// Do *NOT* assume any specific format for the newline chars!
// (May be LF or CR-LF)
#ifdef REFKEEN_CONFIG_CHECK_FOR_STEAM_INSTALLATION
BE_FILE_T BE_Cross_open_steamcfg_for_reading(void)
{
	TCHAR steam_config_path[BE_CROSS_PATH_LEN_BOUND];

#ifdef REFKEEN_PLATFORM_WINDOWS

	DWORD dwType = 0;
	DWORD dwSize = sizeof(steam_config_path);
	LSTATUS status = SHGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\VALVE\\STEAM", L"INSTALLPATH", &dwType, steam_config_path, &dwSize);
	if ((status != ERROR_SUCCESS) || (dwType != REG_SZ))
		return NULL;
	BEL_Cross_safeandfastctstringcopy(steam_config_path + _tcslen(steam_config_path), steam_config_path + sizeof(steam_config_path)/sizeof(TCHAR), L"\\config\\config.vdf");

#elif (defined REFKEEN_PLATFORM_UNIX)

	const char *homeVar = getenv("HOME");
	if (!homeVar || !(*homeVar))
		return NULL;

#ifdef REFKEEN_PLATFORM_MACOS
	BE_Cross_safeandfastcstringcopy_2strs(steam_config_path, steam_config_path+sizeof(steam_config_path)/sizeof(TCHAR), homeVar, "/Library/Application Support/Steam/config/config.vdf");
#else
	BE_Cross_safeandfastcstringcopy_2strs(steam_config_path, steam_config_path+sizeof(steam_config_path)/sizeof(TCHAR), homeVar, "/.steam/steam/config/config.vdf");
#endif

#endif // UNIX
	return _tfopen(steam_config_path, _T("rb"));
}
#endif // REFKEEN_CONFIG_CHECK_FOR_STEAM_INSTALLATION

#if (defined REFKEEN_VER_CATACOMB_ALL) && ((defined REFKEEN_PLATFORM_WINDOWS) || (defined REFKEEN_PLATFORM_MACOS))
#define BE_CHECK_GOG_INSTALLATIONS

#ifdef REFKEEN_PLATFORM_WINDOWS
static const TCHAR *g_be_catacombs_gog_subdirnames_withdirsep[] = {_T("\\Cat3D"), _T("\\Abyss"), _T("\\Armageddon"), _T("\\Apocalypse")};
#endif
#ifdef REFKEEN_PLATFORM_MACOS
static const TCHAR *g_be_catacombs_gog_subdirnames_withdirsep[] = {_T("/2CAT3D"), _T("/3CABYSS"), _T("/4CATARM"), _T("/5APOC")};
#endif

#endif

void BE_Cross_PrepareGameInstallations(void)
{
	/*** Reset these ***/
	for (int i = 0; i < BE_GAMEVER_LAST; ++i)
		g_be_gameinstallationsbyver[i] = NULL;

	/*** Some misc. preparation ***/

	TCHAR path[BE_CROSS_PATH_LEN_BOUND];
	TCHAR *pathEnd = path + sizeof(path)/sizeof(TCHAR);

	if (!g_refKeenCfg.manualGameVerMode)
	{
#ifdef REFKEEN_VER_CATACOMB_ALL

#ifdef REFKEEN_PLATFORM_WINDOWS
		TCHAR gog_catacombs_paths[1][BE_CROSS_PATH_LEN_BOUND];
		DWORD dwType = 0;
		DWORD dwSize = sizeof(gog_catacombs_paths[0]);
		LSTATUS status = SHGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\GOG.COM\\GOGCATACOMBSPACK", L"PATH", &dwType, gog_catacombs_paths[0], &dwSize);
		int numOfGogPathsToCheck = ((status == ERROR_SUCCESS) && (dwType == REG_SZ)) ? 1 : 0;
		TCHAR *path_gog_catacombs_prefix_ends[1];
		if (numOfGogPathsToCheck)
		{
			path_gog_catacombs_prefix_ends[0] = path/*NOT gog_catacombs_paths[0]*/ + _tcslen(gog_catacombs_paths[0]);
		}
#endif
#ifdef REFKEEN_PLATFORM_MACOS
		int numOfGogPathsToCheck = 1;
		char gog_catacombs_paths[2][BE_CROSS_PATH_LEN_BOUND] = {
			"/Applications/Catacombs Pack/Catacomb Pack.app/Contents/Resources/Catacomb Pack.boxer/C 1 CATACOMB.harddisk",
			"" // Fill this very soon
		};
		char *path_gog_catacombs_prefix_ends[2] = {path/*NOT gog_catacombs_paths[0]*/ + strlen(gog_catacombs_paths[0]), NULL};
		const char *homeVar = getenv("HOME");
		if (homeVar && *homeVar)
		{
			BE_Cross_safeandfastcstringcopy_2strs(gog_catacombs_paths[1], gog_catacombs_paths[1]+sizeof(gog_catacombs_paths[1])/sizeof(TCHAR), homeVar, gog_catacombs_paths[0]);
			path_gog_catacombs_prefix_ends[1] = path/*NOT gog_catacombs_paths[1]*/ + strlen(gog_catacombs_paths[1]);
			++numOfGogPathsToCheck;
		}
#endif

#endif

		/*** Now handling each version separately ***/

#ifdef REFKEEN_VER_KDREAMS
		BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_kdreamse100, _T("."), "Keen Dreams EGA v1.00 (Local)");
		BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_kdreamsc100, _T("."), "Keen Dreams CGA v1.00 (Local)");
		BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_kdreamse113, _T("."), "Keen Dreams EGA v1.13 (Local)");
		BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_kdreamsc105, _T("."), "Keen Dreams CGA v1.05 (Local)");
		BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_kdreamse193, _T("."), "Keen Dreams EGA v1.93 (Local)");
		BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_kdreamse120, _T("."), "Keen Dreams EGA v1.20 (Local)");
		BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_kdreams2015, _T("."), "Keen Dreams 2015 (Local)");
#ifdef REFKEEN_CONFIG_CHECK_FOR_STEAM_INSTALLATION

		TCHAR steam_kdreams_path[BE_CROSS_PATH_LEN_BOUND];

#ifdef REFKEEN_PLATFORM_WINDOWS
		DWORD dwType = 0;
		DWORD dwSize = sizeof(steam_kdreams_path);
		LSTATUS status = SHGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\MICROSOFT\\WINDOWS\\CURRENTVERSION\\UNINSTALL\\STEAM APP 356200", L"INSTALLLOCATION", &dwType, steam_kdreams_path, &dwSize);
		if ((status == ERROR_SUCCESS) && (dwType == REG_SZ))
			BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_kdreams2015, steam_kdreams_path, "Keen Dreams 2015 (Steam)");
#elif (defined REFKEEN_PLATFORM_UNIX)
		const char *homeVar = getenv("HOME");
		if (homeVar && *homeVar)
		{
#ifdef REFKEEN_PLATFORM_MACOS
			BE_Cross_safeandfastcstringcopy_2strs(steam_kdreams_path, steam_kdreams_path+sizeof(steam_kdreams_path)/sizeof(TCHAR), homeVar, "/Library/Application Support/Steam/SteamApps/common/Keen Dreams/KDreams.app/Contents/Resources");
			BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_kdreams2015, steam_kdreams_path, "Keen Dreams 2015 (Steam)");
#else
			// They changed from SteamApps to steamapps at some point, so check both two
			BE_Cross_safeandfastcstringcopy_2strs(steam_kdreams_path, steam_kdreams_path+sizeof(steam_kdreams_path)/sizeof(TCHAR), homeVar, "/.steam/steam/SteamApps/common/Keen Dreams");
			BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_kdreams2015, steam_kdreams_path, "Keen Dreams 2015 (Steam)");
			BE_Cross_safeandfastcstringcopy_2strs(steam_kdreams_path, steam_kdreams_path+sizeof(steam_kdreams_path)/sizeof(TCHAR), homeVar, "/.steam/steam/steamapps/common/Keen Dreams");
			BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_kdreams2015, steam_kdreams_path, "Keen Dreams 2015 (Steam)");
#endif
		}
#endif // REFKEEN_PLATFORM

#endif // REFKEEN_CONFIG_CHECK_FOR_STEAM_INSTALLATION

#endif // REFKEEN_VER_KDREAMS

#ifdef REFKEEN_VER_CAT3D
		BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_cat3d100, _T("."), "Catacomb 3-D v1.00 (Local)");
		BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_cat3d122, _T("."), "Catacomb 3-D v1.22 (Local)");
#ifdef BE_CHECK_GOG_INSTALLATIONS
		for (int i = 0; i < numOfGogPathsToCheck; ++i)
		{
			memcpy(path, gog_catacombs_paths[i], sizeof(path));
			BEL_Cross_safeandfastctstringcopy(path_gog_catacombs_prefix_ends[i], pathEnd, g_be_catacombs_gog_subdirnames_withdirsep[0]);
			BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_cat3d122, path, "Catacomb 3-D v1.22 (GOG.com)");
		}
#endif
#endif

#ifdef REFKEEN_VER_CATABYSS
		BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_catabyss113, _T("."), "Catacomb Abyss v1.13 (Local)");
		BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_catabyss124, _T("."), "Catacomb Abyss v1.24 (Local)");
#ifdef BE_CHECK_GOG_INSTALLATIONS
		for (int i = 0; i < numOfGogPathsToCheck; ++i)
		{
			memcpy(path, gog_catacombs_paths[i], sizeof(path));
			BEL_Cross_safeandfastctstringcopy(path_gog_catacombs_prefix_ends[i], pathEnd, g_be_catacombs_gog_subdirnames_withdirsep[1]);
			BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_catabyss124, path, "Catacomb Abyss v1.24 (GOG.com)");
		}
#endif
#endif

#ifdef REFKEEN_VER_CATARM
		BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_catarm102, _T("."), "Catacomb Armageddon v1.02 (Local)");
#ifdef BE_CHECK_GOG_INSTALLATIONS
		for (int i = 0; i < numOfGogPathsToCheck; ++i)
		{
			memcpy(path, gog_catacombs_paths[i], sizeof(path));
			BEL_Cross_safeandfastctstringcopy(path_gog_catacombs_prefix_ends[i], pathEnd, g_be_catacombs_gog_subdirnames_withdirsep[2]);
			BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_catarm102, path, "Catacomb Armageddon v1.02 (GOG.com)");
		}
#endif
#endif

#ifdef REFKEEN_VER_CATAPOC
		BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_catapoc101, _T("."), "Catacomb Apocalypse v1.01 (Local)");
#ifdef BE_CHECK_GOG_INSTALLATIONS
		for (int i = 0; i < numOfGogPathsToCheck; ++i)
		{
			memcpy(path, gog_catacombs_paths[i], sizeof(path));
			BEL_Cross_safeandfastctstringcopy(path_gog_catacombs_prefix_ends[i], pathEnd, g_be_catacombs_gog_subdirnames_withdirsep[3]);
			BEL_Cross_ConditionallyAddGameInstallation(&g_be_gamever_catapoc101, path, "Catacomb Apocalypse v1.01 (GOG.com)");
		}
#endif
#endif
	}
	/*** Finally check any custom dir ***/
	char buffer[2*BE_CROSS_PATH_LEN_BOUND];
	for (int i = 0; i < BE_GAMEVER_LAST; ++i)
	{
		const BE_GameVerDetails_T *details = g_be_gamever_ptrs[i];
		if (g_be_gameinstallationsbyver[details->verId])
			continue;

		BEL_Cross_safeandfastctstringcopy_4strs(path, pathEnd, g_be_appDataPath, _T("/"), details->writableFilesDir, _T(".txt"));
		FILE *fp = _tfopen(path, _T("r"));
		if (!fp)
			continue;

		bool lineRead = fgets(buffer, sizeof(buffer), fp);
		fclose(fp);

		if (!lineRead)
			continue;

		for (char *ptr = buffer; *ptr; ++ptr)
		{
			if (*ptr == '\n')
			{
				*ptr = '\0';
				break;
			}
			if ((*ptr < 32) || (*ptr >= 127)) // Non-ASCII?
				continue;
		}

		int j;
		for (j = 0; j < g_be_rootPathsNum; ++j)
			if (!strncmp(buffer, g_be_rootPathsKeys[j], strlen(g_be_rootPathsKeys[j])))
				break;
		if (j == g_be_rootPathsNum)
			continue;

		const char *dirWithoutRoot = buffer + strlen(g_be_rootPathsKeys[j]);
		if (*dirWithoutRoot == '\0')
			continue;
		++dirWithoutRoot;
		// Finally fill the path and conditionally add it
		BEL_Cross_safeandfastcstringcopytoctstring(BEL_Cross_safeandfastctstringcopy_2strs(path, pathEnd, g_be_rootPaths[j], _T("/")), pathEnd, dirWithoutRoot);
		BEL_Cross_ConditionallyAddGameInstallation(details, path, details->customInstDescription);
	}
	/*** To finish, apply a simple bubble sort on the game installations, by game version (done for visual consistency in the launcher) ***/
	for (int count = g_be_gameinstallations_num - 1; count > 0; --count)
		for (int i = 0; i < count; ++i)
			if (g_be_gameinstallations[i].verId > g_be_gameinstallations[i+1].verId)
			{
				// FIXME - Copies are just a little bit expensive, but not a lot...
				BE_GameInstallation_T tempInst = g_be_gameinstallations[i];
				g_be_gameinstallations[i] = g_be_gameinstallations[i+1];
				g_be_gameinstallations[i+1] = tempInst;

				g_be_gameinstallationsbyver[g_be_gameinstallations[i].verId] = &g_be_gameinstallations[i];
				g_be_gameinstallationsbyver[g_be_gameinstallations[i+1].verId] = &g_be_gameinstallations[i+1];
			}
}


int BE_Cross_DirSelection_GetNumOfRootPaths(void)
{
	return g_be_rootPathsNum;
}

const char **BE_Cross_DirSelection_GetRootPathsNames(void)
{
	return g_be_rootPathsNames;
}

static TCHAR g_be_dirSelection_currPath[BE_CROSS_PATH_LEN_BOUND];
static TCHAR * const g_be_dirSelection_currPathEnd = g_be_dirSelection_currPath + BE_CROSS_PATH_LEN_BOUND;
static TCHAR *g_be_dirSelection_separatorPtrs[BE_CROSS_PATH_LEN_BOUND];
static TCHAR ** const g_be_dirSelection_separatorPtrsEnd = g_be_dirSelection_separatorPtrs + BE_CROSS_PATH_LEN_BOUND;
static TCHAR **g_be_dirSelection_currSeparatorPtrPtr;
static int g_be_dirSelection_rootPathIndex;
// Dynamically allocated memory
static char *g_be_dirSelection_dirnamesBuffer = NULL;
static char **g_be_dirSelection_dirnamesBufferPtrPtrs = NULL;

static void BEL_Cross_DirSelection_ClearResources(void)
{
	free(g_be_dirSelection_dirnamesBuffer);
	free(g_be_dirSelection_dirnamesBufferPtrPtrs);
	g_be_dirSelection_dirnamesBuffer = NULL;
	g_be_dirSelection_dirnamesBufferPtrPtrs = NULL;
}

static const char **BEL_Cross_DirSelection_PrepareDirsAndGetNames(int *outNumOfSubDirs)
{
	_TDIR *dir;
	struct _tdirent *direntry;
	dir = _topendir(g_be_dirSelection_currPath);
	if (!dir)
		return NULL;

	TCHAR fullpath[BE_CROSS_PATH_LEN_BOUND];
	TCHAR *fullpathEnd = fullpath + sizeof(fullpath)/sizeof(TCHAR);

	int numOfSubDirs = 0;
	int charsToAllocateForNames = 0;
	for (direntry = _treaddir(dir); direntry; direntry = _treaddir(dir))
	{
		/*** Ignore non-ASCII dirnames or any of a few special entries ***/
		if (*BEL_Cross_tstr_find_nonascii_ptr(direntry->d_name) || !_tcscmp(direntry->d_name, _T(".")) || !_tcscmp(direntry->d_name, _T("..")))
			continue;

		BEL_Cross_safeandfastctstringcopy_3strs(fullpath, fullpathEnd, g_be_dirSelection_currPath, _T("/"), direntry->d_name);
		if (!BEL_Cross_IsDir(fullpath))
			continue;

		++numOfSubDirs;

		charsToAllocateForNames += 1 + _tcslen(direntry->d_name);
	}

	g_be_dirSelection_dirnamesBuffer = (char *)malloc(charsToAllocateForNames);
	g_be_dirSelection_dirnamesBufferPtrPtrs = (char **)malloc(numOfSubDirs*sizeof(char *));
	if (!(g_be_dirSelection_dirnamesBuffer && g_be_dirSelection_dirnamesBufferPtrPtrs))
	{
		BEL_Cross_DirSelection_ClearResources();
		_tclosedir(dir);
		BE_ST_ExitWithErrorMsg("BEL_Cross_DirSelection_PrepareDirsAndGetNames: Out of memory!");
	}

	// Re-scan, and be ready for the case directory contents have changed
	int repeatedNumOfSubDirs = 0;
	int repeatedCharsToAllocateForNames = 0;
	_trewinddir(dir);
	char *dirnameBufferPtr = g_be_dirSelection_dirnamesBuffer;
	for (direntry = _treaddir(dir); direntry; direntry = _treaddir(dir))
	{
		/*** Ignore non-ASCII dirnames or any of a few special entries ***/
		if (*BEL_Cross_tstr_find_nonascii_ptr(direntry->d_name) || !_tcscmp(direntry->d_name, _T(".")) || !_tcscmp(direntry->d_name, _T("..")))
			continue;

		BEL_Cross_safeandfastctstringcopy_3strs(fullpath, fullpathEnd, g_be_dirSelection_currPath, _T("/"), direntry->d_name);
		if (!BEL_Cross_IsDir(fullpath))
			continue;

		size_t len = _tcslen(direntry->d_name);
		if ((++repeatedNumOfSubDirs > numOfSubDirs) || ((repeatedCharsToAllocateForNames += 1 + len) > charsToAllocateForNames))
		{
			--repeatedNumOfSubDirs;
			break;
		}

		char *currDirnameBufferPtr = dirnameBufferPtr;

		TCHAR *tchPtr = direntry->d_name;
		for (size_t i = 0; i <= len; ++i)
			*currDirnameBufferPtr++ = *tchPtr++; // Possibly convert (ASCII only) wchar_t to char here

		// Basically insertion-sort
		char **dirnameBufferPtrPtr = g_be_dirSelection_dirnamesBufferPtrPtrs;
		int j;
		for (j = 0; j < repeatedNumOfSubDirs - 1; ++j, ++dirnameBufferPtrPtr)
			if (BE_Cross_strcasecmp(*dirnameBufferPtrPtr, dirnameBufferPtr) > 0)
			{
				memmove(dirnameBufferPtrPtr+1, dirnameBufferPtrPtr, (repeatedNumOfSubDirs - 1 - j)*sizeof(char*));
				break;
			}
		*dirnameBufferPtrPtr = dirnameBufferPtr;

		dirnameBufferPtr = currDirnameBufferPtr;
	}

	*outNumOfSubDirs = repeatedNumOfSubDirs;
	return (const char **)g_be_dirSelection_dirnamesBufferPtrPtrs;
}

const char **BE_Cross_DirSelection_Start(int rootPathIndex, int *outNumOfSubDirs) // Start dir selection
{
	g_be_dirSelection_separatorPtrs[0] = BEL_Cross_safeandfastctstringcopy(g_be_dirSelection_currPath, g_be_dirSelection_currPathEnd, g_be_rootPaths[rootPathIndex]);
	g_be_dirSelection_rootPathIndex = rootPathIndex;
	g_be_dirSelection_currSeparatorPtrPtr = &g_be_dirSelection_separatorPtrs[0];
	return BEL_Cross_DirSelection_PrepareDirsAndGetNames(outNumOfSubDirs);
}

void BE_Cross_DirSelection_Finish(void) // Finish dir selection
{
	BEL_Cross_DirSelection_ClearResources();
}

const char **BE_Cross_DirSelection_GetNext(int dirIndex, int *outNumOfSubDirs) // Enter dir by index into last array
{
	TCHAR *nextSeparatorPtr =
	// HACK for taking care of root path on Linux (/); No check for Windows-specific backslash, though!
	((*g_be_dirSelection_currSeparatorPtrPtr != g_be_dirSelection_currPath) && (*((*g_be_dirSelection_currSeparatorPtrPtr)-1) == _T('/')))
	? BEL_Cross_safeandfastcstringcopytoctstring(*g_be_dirSelection_currSeparatorPtrPtr, g_be_dirSelection_currPathEnd, g_be_dirSelection_dirnamesBufferPtrPtrs[dirIndex])
	: BEL_Cross_safeandfastcstringcopytoctstring(BEL_Cross_safeandfastctstringcopy(*g_be_dirSelection_currSeparatorPtrPtr, g_be_dirSelection_currPathEnd, _T("/")), g_be_dirSelection_currPathEnd, g_be_dirSelection_dirnamesBufferPtrPtrs[dirIndex]);

	BEL_Cross_DirSelection_ClearResources(); // Safe to do so now since copy has already been done
	if ((nextSeparatorPtr == g_be_dirSelection_currPathEnd) || (++g_be_dirSelection_currSeparatorPtrPtr == g_be_dirSelection_separatorPtrsEnd))
	{
		BE_ST_ExitWithErrorMsg("BEL_Cross_DirSelection_GetNext: Buffer overflow, or too deep hierarchy!");
	}
	*g_be_dirSelection_currSeparatorPtrPtr = nextSeparatorPtr;
	return BEL_Cross_DirSelection_PrepareDirsAndGetNames(outNumOfSubDirs);
}

const char **BE_Cross_DirSelection_GetPrev(int *outNumOfSubDirs) // Go up in the filesystem hierarchy
{
	BEL_Cross_DirSelection_ClearResources();
	if (g_be_dirSelection_currSeparatorPtrPtr == &g_be_dirSelection_separatorPtrs[0])
		return NULL;

	**(--g_be_dirSelection_currSeparatorPtrPtr) = _T('\0'); // Replace separator with this
	return BEL_Cross_DirSelection_PrepareDirsAndGetNames(outNumOfSubDirs);
}


const char *BE_Cross_GetEXEFileDescriptionStrForGameVer(const char *exeFileName, int verId)
{
	const BE_EXEFileDetails_T *exeFile = g_be_gamever_ptrs[verId]->exeFiles;
	for (; exeFile->mainFuncPtr && (!exeFile->exeName || strcmp(exeFile->exeName, exeFileName)); ++exeFile)
		;
	return (exeFile->mainFuncPtr ? exeFile->subDescription : NULL); // subDescription may also be NULL
}

void (*BE_Cross_GetAccessibleMainFuncPtrForGameVer(const char *exeFileName, int verId))(void)
{
	const BE_EXEFileDetails_T *exeFile = g_be_gamever_ptrs[verId]->exeFiles;
	for (; exeFile->mainFuncPtr && (!exeFile->exeName || strcmp(exeFile->exeName, exeFileName)); ++exeFile)
		;
	return ((exeFile->mainFuncPtr && exeFile->subDescription) ? exeFile->mainFuncPtr : g_be_gamever_ptrs[verId]->exeFiles->mainFuncPtr);
}

int BE_Cross_GetAccessibleEXEsCountForGameVer(int verId)
{
	int nOfEXEs = 1;
	for (const BE_EXEFileDetails_T *exeFile = 1 + g_be_gamever_ptrs[verId]->exeFiles; exeFile->mainFuncPtr; ++exeFile) // Skip first entry, which is the "default" accessible main function
		if (exeFile->subDescription)
			++nOfEXEs;
	return nOfEXEs;
}

void BE_Cross_FillAccessibleEXEFileNamesForGameVer(int verId, const char **outStrs)
{
	*outStrs++ = "Just launch the game normally";
	for (const BE_EXEFileDetails_T *exeFile = 1 + g_be_gamever_ptrs[verId]->exeFiles; exeFile->mainFuncPtr; ++exeFile) // Skip first entry
		if (exeFile->subDescription)
			*outStrs++ = exeFile->subDescription;
}

void (*BE_Cross_GetAccessibleEXEFuncPtrForGameVerByIndex(int index, int verId))(void)
{
	if (!index)
		return g_be_gamever_ptrs[verId]->exeFiles->mainFuncPtr;

	const BE_EXEFileDetails_T *exeFile = g_be_gamever_ptrs[verId]->exeFiles + 1;
	for (int i = 0; true; ++exeFile)
		if (exeFile->subDescription)
			if (++i == index)
				break;

	return exeFile->mainFuncPtr;
}


// Attempt to add a game installation from currently selected dir;
// Returns BE_GAMEVER_LAST if no new supported game version is found; Otherwise game version id is returned.
// The given array is used in order to report an error for each checked version, in case of failure.
//
// Array MUST have at least BE_GAMEVER_LAST elements.
int BE_Cross_DirSelection_TryAddGameInstallation(BE_TryAddGameInstallation_ErrorMsg_T errorMsgsArray[])
{
	int verId;
	for (verId = 0; verId < BE_GAMEVER_LAST; ++verId)
	{
		if (g_be_gameinstallationsbyver[verId])
		{
			BE_Cross_safeandfastcstringcopy(errorMsgsArray[verId], errorMsgsArray[verId] + sizeof(errorMsgsArray[verId]), "Already available");
			continue;
		}

		const BE_GameVerDetails_T *details = g_be_gamever_ptrs[verId];
		BEL_Cross_ConditionallyAddGameInstallation_WithReturnedErrMsg(details, g_be_dirSelection_currPath, details->customInstDescription, &errorMsgsArray[verId]);
		if (g_be_gameinstallationsbyver[verId]) // Match found and added
		{
			TCHAR path[BE_CROSS_PATH_LEN_BOUND];
			TCHAR * const pathEnd = path + BE_CROSS_PATH_LEN_BOUND;
			BEL_Cross_safeandfastctstringcopy_4strs(path, pathEnd, g_be_appDataPath, _T("/"), details->writableFilesDir, _T(".txt"));
			FILE *fp = _tfopen(path, _T("w"));
			if (!fp)
			{
				BE_Cross_LogMessage(BE_LOG_MSG_WARNING, "BE_Cross_DirSelection_TryAddGameInstallation: Can't add directory to txt file.\n");
				break;
			}

			// HACK
#ifdef REFKEEN_PLATFORM_WINDOWS
			fprintf(fp, "%s %ls\n", g_be_rootPathsKeys[g_be_dirSelection_rootPathIndex], 1+g_be_dirSelection_separatorPtrs[0]);
#else
			fprintf(fp, "%s %s\n", g_be_rootPathsKeys[g_be_dirSelection_rootPathIndex], 1+g_be_dirSelection_separatorPtrs[0]);
#endif

			fclose(fp);
			break; // Finish
		}
	}
	return verId;
}


int BE_Cross_GetSelectedGameVerSampleRate(void)
{
	return g_be_gamever_ptrs[refkeen_current_gamever]->digiAudioFreq;
}


// gameVer should be BE_GAMEVER_LAST if no specific version is desired
static void BEL_Cross_SelectGameInstallation(int gameVerVal)
{
	if (gameVerVal == BE_GAMEVER_LAST)
	{
		if (!g_be_gameinstallations_num)
		{
			BE_ST_ExitWithErrorMsg("BEL_Cross_SelectGameInstallation: No compatible game installation found!");
		}
		g_be_selectedGameInstallation = &g_be_gameinstallations[0];
	}
	else
	{
		g_be_selectedGameInstallation = g_be_gameinstallationsbyver[gameVerVal];
		if (!g_be_selectedGameInstallation)
		{
			char errorBuffer[80];
			BE_Cross_safeandfastcstringcopy_2strs(errorBuffer, errorBuffer+sizeof(errorBuffer), "BEL_Cross_SelectGameInstallation: Can't find game installation: ",  refkeen_gamever_strs[gameVerVal]);
			BE_ST_ExitWithErrorMsg(errorBuffer);
		}
	}

	g_refKeenCfg.lastSelectedGameVer = refkeen_current_gamever = g_be_selectedGameInstallation->verId;

#ifdef REFKEEN_VER_KDREAMS
	// MUST be the first patched file (at least for Keen Dreams)
	extern void RefKeen_Patch_id_ca(void);
	RefKeen_Patch_id_ca();
#endif
	extern void RefKeen_Patch_id_us(void);
	RefKeen_Patch_id_us();
#ifdef REFKEEN_VER_KDREAMS
	extern void RefKeen_Patch_id_rf(void);
	RefKeen_Patch_id_rf();
	extern void RefKeen_Patch_id_rf_a(void);
	RefKeen_Patch_id_rf_a();
	extern void RefKeen_Patch_id_vw(void);
	RefKeen_Patch_id_vw();
	extern void RefKeen_Patch_id_vw_ac(void);
	RefKeen_Patch_id_vw_ac();
	extern void RefKeen_Patch_id_vw_ae(void);
	RefKeen_Patch_id_vw_ae();
	extern void RefKeen_Patch_kd_demo(void);
	RefKeen_Patch_kd_demo();
	extern void RefKeen_Patch_kd_keen(void);
	RefKeen_Patch_kd_keen();
	extern void RefKeen_Patch_kd_play(void);
	RefKeen_Patch_kd_play();
#endif
#ifdef REFKEEN_VER_CAT3D
	extern void RefKeen_Patch_c3_game(void);
	RefKeen_Patch_c3_game();
	extern void RefKeen_Patch_c3_play(void);
	RefKeen_Patch_c3_play();
#endif
#ifdef REFKEEN_VER_CATABYSS
	extern void RefKeen_Patch_c4_main(void);
	RefKeen_Patch_c4_main();
	extern void RefKeen_Patch_c4_play(void);
	RefKeen_Patch_c4_play();
	extern void RefKeen_Patch_gelib(void);
	RefKeen_Patch_gelib();
#endif
#ifdef REFKEEN_VER_CATADVENTURES
	extern void RefKeen_Patch_intro(void);
	RefKeen_Patch_intro();
	extern void RefKeen_Patch_slidecat(void);
	RefKeen_Patch_slidecat();
#endif
	extern void RefKeen_FillObjStatesWithDOSPointers(void);
	RefKeen_FillObjStatesWithDOSPointers(); // Saved games compatibility
	extern void RefKeen_PrepareAltControllerScheme(void);
	RefKeen_PrepareAltControllerScheme(); // Alternative controller scheme stuff
}

// Here the magic happens - used to clear a portion of the stack before
// changing to another "main" function (in case we get a loop).
// In a way, this should be similar to C++ exception handling,
// just C-compatible.
/*static*/ jmp_buf g_be_mainFuncJumpBuffer;

static void BEL_Cross_LoadEXEImageToMem(void)
{
	char errorMsg[256];
	FILE *exeFp = BEL_Cross_open_from_dir(g_be_current_exeFileDetails->exeName, false, g_be_selectedGameInstallation->instPath, NULL);
	if (!exeFp)
	{
		snprintf(errorMsg, sizeof(errorMsg), "BEL_Cross_LoadEXEImageToMem: Can't open EXE after checking it!\nFilename: %s", g_be_current_exeFileDetails->exeName);
		BE_ST_ExitWithErrorMsg(errorMsg);
	}

	// FIXME - Use BE_Cross_Bmalloc/farmalloc and shuffle things around
	g_be_current_exeImage = (unsigned char *)malloc(g_be_current_exeFileDetails->decompExeImageSize);
	if (!g_be_current_exeImage)
	{
		fclose(exeFp);
		snprintf(errorMsg, sizeof(errorMsg), "BEL_Cross_LoadEXEImageToMem: Can't allocate memory for unpacked EXE copy!\nFilename: %s", g_be_current_exeFileDetails->exeName);
		BE_ST_ExitWithErrorMsg(errorMsg);
	}

	bool success;
	switch (g_be_current_exeFileDetails->compressionType)
	{
	case BE_EXECOMPRESSION_NONE:
	{
		uint16_t offsetInPages;
		fseek(exeFp, 8, SEEK_SET);
		BE_Cross_readInt16LE(exeFp, &offsetInPages);
		fseek(exeFp, 16*offsetInPages, SEEK_SET);
		success = (fread(g_be_current_exeImage, g_be_current_exeFileDetails->decompExeImageSize, 1, exeFp) == 1);
		break;
	}
	case BE_EXECOMPRESSION_LZEXE9X:
		success = Unlzexe_unpack(exeFp, g_be_current_exeImage, g_be_current_exeFileDetails->decompExeImageSize);
		break;
#ifdef ENABLE_PKLITE
	case BE_EXECOMPRESSION_PKLITE105:
		success = depklite_unpack(exeFp, g_be_current_exeImage, g_be_current_exeFileDetails->decompExeImageSize);
		break;
#endif
	}

	fclose(exeFp);
	if (!success)
	{
		free(g_be_current_exeImage);
		snprintf(errorMsg, sizeof(errorMsg), "decompExeImage: Failed to copy EXE in unpacked form!\nFilename: %s", g_be_current_exeFileDetails->exeName);
		BE_ST_ExitWithErrorMsg(errorMsg);
	}
}

static void BEL_Cross_FreeEXEImageFromMem(void)
{
	free(g_be_current_exeImage);
}

static void BEL_Cross_DoCallMainFunc(void)
{
	setjmp(g_be_mainFuncJumpBuffer); // Ignore returned value, always doing the same thing

	// Do start!
	BE_ST_AltControlScheme_Reset();

	if (g_be_current_exeFileDetails->embeddedFiles)
	{
		BEL_Cross_LoadEXEImageToMem();
		g_be_current_exeFileDetails->embeddedFilesLoaderFuncPtr();
		BEL_Cross_FreeEXEImageFromMem();
	}

	be_lastSetMainFuncPtr = g_be_current_exeFileDetails->mainFuncPtr;
	if (g_be_current_exeFileDetails->passArgsToMainFunc)
		((void (*)(int, const char **))be_lastSetMainFuncPtr)(g_be_argc, g_be_argv); // HACK
	else
		be_lastSetMainFuncPtr();
}

// When a new main function is called in the middle (BE_Cross_Bexecv),
// by default the current (non-new) main function is stored as
// a "one time" function that shall *never* be called again.
//
// To prevent this, BE_Cross_Bexecv should get a finalizer function pointer,
// used to reset the sub-program as represented by current main function
// to its original state. (Emphasis on global and static variables.)
#define MAX_NUM_OF_ONE_TIME_MAIN_FUNCS 4

static int g_be_numOfOneTimeMainFuncs = 0;
static void (*g_be_oneTimeMainFuncs[MAX_NUM_OF_ONE_TIME_MAIN_FUNCS])(void);

void BE_Cross_Bexecv(void (*mainFunc)(void), const char **argv, void (*finalizer)(void), bool passArgsToMainFunc)
{
	for (int i = 0; i < g_be_numOfOneTimeMainFuncs; ++i)
		if (g_be_oneTimeMainFuncs[i] == mainFunc)
			BE_ST_ExitWithErrorMsg("BE_Cross_Bexecv - One-time function unexpectedly called again!");

	if (finalizer)
		finalizer();
	else
	{
		if (g_be_numOfOneTimeMainFuncs == MAX_NUM_OF_ONE_TIME_MAIN_FUNCS)
			BE_ST_ExitWithErrorMsg("BE_Cross_Bexecv - Too many one-time functions called!");
		g_be_oneTimeMainFuncs[g_be_numOfOneTimeMainFuncs++] = be_lastSetMainFuncPtr;
	}

	// Note this does NOT work for memory not managed by us (e.g., simple calls to malloc)
	void BEL_Cross_ClearMemory(void);
	BEL_Cross_ClearMemory();

	g_be_argv = argv;
	for (g_be_argc = 0; *argv; ++g_be_argc, ++argv)
		;

	// Locate the right EXE
	for (g_be_current_exeFileDetails = g_be_gamever_ptrs[refkeen_current_gamever]->exeFiles; g_be_current_exeFileDetails->mainFuncPtr && (g_be_current_exeFileDetails->mainFuncPtr != mainFunc); ++g_be_current_exeFileDetails)
		;
	if (!g_be_current_exeFileDetails->mainFuncPtr)
		BE_ST_ExitWithErrorMsg("BE_Cross_Bexecv - Unrecognized main function!");

	longjmp(g_be_mainFuncJumpBuffer, 0); // A little bit of magic
}

void BE_Cross_StartGame(int gameVerVal, int argc, char **argv, void (*mainFuncPtr)(void))
{
	// Some additional preparation required
	BE_ST_PrepareForGameStartupWithoutAudio();

	BEL_Cross_SelectGameInstallation(gameVerVal);

	BE_ST_InitAudio(); // Do this now, since we can tell if we want digi audio out or not

	// Prepare arguments for ported game code
	g_be_argc = argc;
	// HACK: In Keen Dreams CGA v1.05, even if argc == 1, argv[1] is accessed...
	// Furthermore, in Keen Dreams Shareware v1.13, argc, argv[1], argv[2] and argv[3] are all modified...
	// And then in Catacomb Abyss, argv[3] is compared to "1". In its INTROSCN.EXE argv[4] is compared...

	// REFKEEN - As long as argv[0] isn't used, use a placeholder
	const char *our_workaround_argv[] = { "PROG.EXE", "", "", "", "", "", "", "", "", NULL };
	if (argc < 10)
	{
		for (int currarg = 1; currarg < argc; ++currarg)
		{
			our_workaround_argv[currarg] = argv[currarg];
		}
		g_be_argv = our_workaround_argv;
	}
	else
	{
		// REFKEEN - Hack, but we don't access argv directly anyway...
		g_be_argv = (const char **)argv;
	}

	// Locate the right EXE
	for (g_be_current_exeFileDetails = g_be_gamever_ptrs[refkeen_current_gamever]->exeFiles; g_be_current_exeFileDetails->mainFuncPtr && (g_be_current_exeFileDetails->mainFuncPtr != mainFuncPtr); ++g_be_current_exeFileDetails)
		;
	if (!g_be_current_exeFileDetails->mainFuncPtr)
		BE_ST_ExitWithErrorMsg("BE_Cross_StartGame - Unrecognized main function!");

	snprintf(g_refKeenCfg.lastSelectedGameExe, sizeof(g_refKeenCfg.lastSelectedGameExe), "%s", g_be_current_exeFileDetails->exeName ? g_be_current_exeFileDetails->exeName : "");

	BEL_Cross_DoCallMainFunc(); // Do a bit more preparation and then begin
}

int32_t BE_Cross_FileLengthFromHandle(BE_FILE_T fp)
{
	long int origOffset = ftell(fp);
	fseek(fp, 0, SEEK_END);
	long int len = ftell(fp);
	fseek(fp, origOffset, SEEK_SET);
	return len;
}

size_t BE_Cross_readInt8LE(BE_FILE_T fp, void *ptr)
{
	return fread(ptr, 1, 1, fp);
}

size_t BE_Cross_readInt16LE(BE_FILE_T fp, void *ptr)
{
	size_t bytesread = fread(ptr, 1, 2, fp);
#ifdef REFKEEN_ARCH_BIG_ENDIAN
	if (bytesread == 2)
	{
		*(uint16_t *)ptr = BE_Cross_Swap16LE(*(uint16_t *) ptr);
	}
#endif
	return bytesread;
}

size_t BE_Cross_readInt32LE(BE_FILE_T fp, void *ptr)
{
	size_t bytesread = fread(ptr, 1, 4, fp);
#ifdef REFKEEN_ARCH_BIG_ENDIAN
	if (bytesread == 4)
	{
		*(uint32_t *)ptr = BE_Cross_Swap32LE(*(uint32_t *) ptr);
	}
#endif
	return bytesread;
}

size_t BE_Cross_readInt8LEBuffer(BE_FILE_T fp, void *ptr, size_t nbyte)
{
	return fread(ptr, 1, nbyte, fp);
}

size_t BE_Cross_readInt16LEBuffer(BE_FILE_T fp, void *ptr, size_t nbyte)
{
#ifndef REFKEEN_ARCH_BIG_ENDIAN
	return fread(ptr, 1, nbyte, fp);
#else
	size_t result = fread(ptr, 1, nbyte, fp);
	for (uint16_t *currptr = (uint16_t *)ptr, *endptr = currptr + result/2; currptr < endptr; ++currptr)
	{
		*currptr = BE_Cross_Swap16LE(*currptr);
	}
	return result;
#endif
}

size_t BE_Cross_readInt32LEBuffer(BE_FILE_T fp, void *ptr, size_t nbyte)
{
#ifndef REFKEEN_ARCH_BIG_ENDIAN
	return fread(ptr, 1, nbyte, fp);
#else
	size_t result = fread(ptr, 1, nbyte, fp);
	for (uint32_t *currptr = (uint32_t *)ptr, *endptr = currptr + result/4; currptr < endptr; ++currptr)
	{
		*currptr = BE_Cross_Swap32LE(*currptr);
	}
	return result;
#endif
}

// This exists for the EGAHEADs from the Catacombs
size_t BE_Cross_readInt24LEBuffer(BE_FILE_T fp, void *ptr, size_t nbyte)
{
#ifndef REFKEEN_ARCH_BIG_ENDIAN
	return fread(ptr, 1, nbyte, fp);
#else
	size_t result = fread(ptr, 1, nbyte, fp);
	uint8_t tempbyte;
	// Let's ensure there's no buffer overflow in case nbyte is not divisible by 3
	for (uint8_t *currbyteptr = (uint8_t *)ptr, *endbyteptr = currbyteptr + (nbyte/3)*3; currbyteptr < endbyteptr; currbyteptr += 3)
	{
		tempbyte = *currbyteptr;
		*currbyteptr = *(currbyteptr+2);
		*(currbyteptr+2) = tempbyte;
	}
	return result;
#endif
}

size_t BE_Cross_writeInt8LE(BE_FILE_T fp, const void *ptr)
{
	return fwrite(ptr, 1, 1, fp);
}

size_t BE_Cross_writeInt16LE(BE_FILE_T fp, const void *ptr)
{
#ifndef REFKEEN_ARCH_BIG_ENDIAN
	return fwrite(ptr, 1, 2, fp);
#else
	uint16_t val = BE_Cross_Swap16LE(*(uint16_t *) ptr);
	return fwrite(&val, 1, 2, fp);
#endif
}

size_t BE_Cross_writeInt32LE(BE_FILE_T fp, const void *ptr)
{
#ifndef REFKEEN_ARCH_BIG_ENDIAN
	return fwrite(ptr, 1, 4, fp);
#else
	uint32_t val = BE_Cross_Swap32LE(*(uint32_t *) ptr);
	return fwrite(&val, 1, 4, fp);
#endif
}

size_t BE_Cross_writeInt8LEBuffer(BE_FILE_T fp, const void *ptr, size_t nbyte)
{
	return fwrite(ptr, 1, nbyte, fp);
}

size_t BE_Cross_writeInt16LEBuffer(BE_FILE_T fp, const void *ptr, size_t nbyte)
{
#ifndef REFKEEN_ARCH_BIG_ENDIAN
	return fwrite(ptr, 1, nbyte, fp);
#else
	size_t result = 0;
	for (uint16_t *currptr = (uint16_t *)ptr, *endptr = currptr + nbyte/2; currptr < endptr; ++currptr)
	{
		uint16_t val = BE_Cross_Swap16LE(*currptr);
		size_t bytesread = fwrite(&val, 1, 2, fp);
		result += bytesread;
		if (bytesread < 2)
		{
			break;
		}
	}
	return result;
#endif
}

BE_CROSS_IMPLEMENT_FP_READWRITE_16LE_FUNCS(classtype)
#ifdef REFKEEN_VER_CATACOMB_ALL
BE_CROSS_IMPLEMENT_FP_READWRITE_16LE_FUNCS(dirtype)
#endif

size_t BE_Cross_read_boolean_From16LE(BE_FILE_T fp, bool *ptr)
{
	uint16_t val;
	size_t bytesread = fread(&val, 1, 2, fp);
	if (bytesread == 2)
	{
		*ptr = val; // No need to swap byte-order here
	}
	return bytesread;
}

size_t BE_Cross_read_boolean_From32LE(BE_FILE_T fp, bool *ptr)
{
	uint32_t val;
	size_t bytesread = fread(&val, 1, 4, fp);
	if (bytesread == 4)
	{
		*ptr = val; // No need to swap byte-order here
	}
	return bytesread;
}

size_t BE_Cross_read_booleans_From16LEBuffer(BE_FILE_T fp, bool *ptr, size_t nbyte)
{
	uint16_t val;
	size_t totalbytesread = 0, currbytesread;
	for (size_t curbyte = 0; curbyte < nbyte; curbyte += 2, ++ptr)
	{
		currbytesread = fread(&val, 1, 2, fp);
		totalbytesread += currbytesread;
		if (currbytesread < 2)
		{
			return totalbytesread;
		}
		*ptr = val; // No need to swap byte-order here
	}
	return totalbytesread;
}

size_t BE_Cross_read_booleans_From32LEBuffer(BE_FILE_T fp, bool *ptr, size_t nbyte)
{
	uint32_t val;
	size_t totalbytesread = 0, currbytesread;
	for (size_t curbyte = 0; curbyte < nbyte; curbyte += 4, ++ptr)
	{
		currbytesread = fread(&val, 1, 4, fp);
		totalbytesread += currbytesread;
		if (currbytesread < 4)
		{
			return totalbytesread;
		}
		*ptr = val; // No need to swap byte-order here
	}
	return totalbytesread;
}


size_t BE_Cross_write_boolean_To16LE(BE_FILE_T fp, const bool *ptr)
{
	uint16_t val = BE_Cross_Swap16LE((uint16_t)(*ptr)); // Better to swap just in case...
	return fwrite(&val, 1, 2, fp);
}

size_t BE_Cross_write_boolean_To32LE(BE_FILE_T fp, const bool *ptr)
{
	uint32_t val = BE_Cross_Swap32LE((uint32_t)(*ptr)); // Better to swap just in case...
	return fwrite(&val, 1, 4, fp);
}

size_t BE_Cross_write_booleans_To16LEBuffer(BE_FILE_T fp, const bool *ptr, size_t nbyte)
{
	uint16_t val;
	size_t totalbyteswritten = 0, currbyteswritten;
	for (size_t curbyte = 0; curbyte < nbyte; curbyte += 2, ++ptr)
	{
		val = BE_Cross_Swap16LE((uint16_t)(*ptr)); // Better to swap just in case...
		currbyteswritten = fwrite(&val, 1, 2, fp);
		totalbyteswritten += currbyteswritten;
		if (currbyteswritten < 2)
		{
			return totalbyteswritten;
		}
	}
	return totalbyteswritten;
}

size_t BE_Cross_write_booleans_To32LEBuffer(BE_FILE_T fp, const bool *ptr, size_t nbyte)
{
	uint32_t val;
	size_t totalbyteswritten = 0, currbyteswritten;
	for (size_t curbyte = 0; curbyte < nbyte; curbyte += 4, ++ptr)
	{
		val = BE_Cross_Swap16LE((uint32_t)(*ptr)); // Better to swap just in case...
		currbyteswritten = fwrite(&val, 1, 4, fp);
		totalbyteswritten += currbyteswritten;
		if (currbyteswritten < 4)
		{
			return totalbyteswritten;
		}
	}
	return totalbyteswritten;
}
