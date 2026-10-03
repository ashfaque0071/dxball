#ifndef CONFIG_H
#define CONFIG_H


/* DXBALL_WEB is 1 for the Emscripten/WebAssembly build and 0 for the native
   desktop builds. Emscripten always defines __EMSCRIPTEN__; PLATFORM_WEB is
   accepted as well because raylib's own web build uses that name. */
#if defined(__EMSCRIPTEN__) || defined(PLATFORM_WEB)
#define DXBALL_WEB 1
#else
#define DXBALL_WEB 0
#endif


/* A browser tab cannot close itself, so the web build drops the QUIT row from
   the main menu instead of offering an action that cannot work. */
#if DXBALL_WEB
#define MAIN_MENU_ROWS 4
#else
#define MAIN_MENU_ROWS 5
#endif


#define SCREEN_W 800
#define SCREEN_H 600


#define INTERNAL_SCALE 2
#define INTERNAL_W (SCREEN_W * INTERNAL_SCALE)
#define INTERNAL_H (SCREEN_H * INTERNAL_SCALE)


#define WINDOW_W 1200
#define WINDOW_H 900


#define END_MENU_BUTTON_W 230
#define END_MENU_BUTTON_H 52
#define END_MENU_BUTTON_Y 420
#define END_RETRY_BUTTON_X 260
#define END_MENU_BUTTON_X 540


#define HUD_PAUSE_BUTTON_X 550
#define HUD_PAUSE_BUTTON_Y 26


#define MAX_NAME 16
#define MAX_NAME_CHARS 12
#define MAX_SCORES 5


#define SAVE_DIR "saves"

#define MAX_PLAYERS 16
#define SCOREBOARD_ROWS 8

#define ROWS 9
#define COLS 10
#define TOTAL_LEVELS 7
#define MUSIC_THEMES 3
#define MAX_BALLS 8
#define MAX_POWERUPS 20
#define MAX_BREAK_FX 30

#endif
