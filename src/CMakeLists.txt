list(APPEND be_src
	src/actual_main.c
	src/backend/audio/be_audio_digi.c
	src/backend/audio/be_audio_init.c
	src/backend/audio/be_audio_main_thread.c
	src/backend/audio/be_audio_mixer.c
	src/backend/audio/be_audio_opl.c
	src/backend/audio/be_audio_pcspkr.c
	src/backend/audio/be_audio_resampling.c
	src/backend/audio/be_audio_sdl.c
	src/backend/filesystem/be_filesystem_app_paths.c
	src/backend/filesystem/be_filesystem_dirselection.c
	src/backend/filesystem/be_filesystem_file.c
	src/backend/filesystem/be_filesystem_file_ops.c
	src/backend/filesystem/be_filesystem_file_lists.c
	src/backend/filesystem/be_filesystem_gameinst.c
	src/backend/filesystem/be_filesystem_root_paths.c
	src/backend/gamedefs/be_gamedefs_accessors.c
	src/backend/gamedefs/be_gamedefs_prep.c
	src/backend/misc/be_misc_steamcfg.c
	src/backend/startup/be_startup.c
	src/backend/startup/be_startup_embedded_data.c
	src/backend/timing/be_timing.c
	src/backend/timing/be_timing_callback.c
	src/be_cross.c
	src/be_cross_doszeroseg.c
	src/be_cross_emm.c
	src/be_cross_mem.c
	src/be_cross_xmm.c
	src/be_st_sdl.c
	src/be_st_sdl_graphics.c
	src/be_textmode_fonts.c
	src/crc32/crc32.c
	src/depklite/depklite.c
	src/nukedopl/opl3.c
	src/unlzexe/unlzexe.c
)

if (WIN32)
	list(APPEND be_src src/backend/filesystem/be_filesystem_dir_win32.c)
	list(APPEND be_src src/backend/misc/be_misc_winreg.c)
else ()
	list(APPEND be_src src/backend/filesystem/be_filesystem_dir_unix.c)
endif ()

if (REFKEEN_ENABLE_LAUNCHER)
	list(APPEND be_src src/be_launcher.c src/be_st_sdl_launcher.c)
endif ()
