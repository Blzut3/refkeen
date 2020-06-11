/* This header must be included *only* from be_gamedef_catadventures.h */

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