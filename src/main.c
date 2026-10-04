#include "config.h"

#include "raylib.h"

#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#if DXBALL_WEB
#include <emscripten/emscripten.h>
#endif

#include "types.h"
#include "assets.h"
#include "audio.h"
#include "game.h"
#include "gameplay.h"
#include "hud.h"
#include "input.h"
#include "menus.h"
#include "render.h"
#include "storage.h"


#if DXBALL_WEB

/* A browser tab can be closed or reloaded at any moment, and the code that
   follows the desktop main loop never gets a chance to run. The web build
   therefore writes the resumable-progress file on a timer while a run is in
   progress, so a refresh cannot lose the current game. */
#define WEB_AUTOSAVE_SECONDS 5.0f
#endif


typedef struct
{
    RenderTexture2D screen;
    Camera2D designCamera;

    Assets assets;
    Audio audio;
    Game game;

    const char *capturePath;
    int captureFrames;

    int shouldClose;

#if DXBALL_WEB
    float autosaveTimer;
    int coreReady;
    int coreFailed;
    int corePercent;
    int deferredReady;
    int deferredFailed;
    int deferredPercent;
    int loadedLevelCount;
#endif
} App;

static App app;

#if DXBALL_WEB
EMSCRIPTEN_KEEPALIVE int dxballMobileState(void)
{
    Game *g = &app.game;
    return (g->showNameEntry ? 1 : 0) |
           (g->gameStarted ? 2 : 0) |
           (g->paused ? 4 : 0) |
           (g->levelIntro ? 8 : 0) |
           ((g->levelComplete || g->gameOver || g->gameWon) ? 16 : 0) |
           (g->ballLaunched ? 32 : 0) |
           ((g->showNameEntry || g->showLevelSelect || g->showResumePrompt ||
             g->showHowToPlay || g->showHighScore || g->showSettings ||
             g->showCredits) ? 64 : 0);
}

EMSCRIPTEN_KEEPALIVE void dxballMobileName(const char *name)
{
    if (!app.game.showNameEntry || name == NULL)
        return;

    int length = 0;
    for (const unsigned char *p = (const unsigned char *)name;
         *p != '\0' && length < MAX_NAME_CHARS; p++)
    {
        if (*p < 128 && isalnum(*p))
            app.game.playerName[length++] = (char)toupper(*p);
    }
    app.game.playerName[length] = '\0';
    confirmPlayerName(&app.game, &app.audio);
}

EMSCRIPTEN_KEEPALIVE void dxballCoreReady(void)
{
    app.coreReady = 1;
}

EMSCRIPTEN_KEEPALIVE void dxballCoreStatus(int percent, int failed)
{
    app.corePercent = percent;
    app.coreFailed = failed;
}

EMSCRIPTEN_KEEPALIVE void dxballAssetsReady(void)
{
    app.deferredReady = 1;
}

EMSCRIPTEN_KEEPALIVE void dxballAssetsStatus(int percent, int failed)
{
    app.deferredPercent = percent;
    app.deferredFailed = failed;
}

static int requestedLevel(Game *g)
{
    if (g->returnToMenuRequested)
        return 0;
    if (g->startLevelRequested)
        return g->selectedStartLevel;
    if (g->resumeRequested || g->restartLevelRequested)
        return g->level;
    if (g->nextLevelRequested && g->level < TOTAL_LEVELS)
        return g->level + 1;
    if (g->levelComplete && !g->gameWon && !g->gameOver &&
        g->level < TOTAL_LEVELS && IsKeyPressed(KEY_ENTER))
    {
        g->nextLevelRequested = 1;
        return g->level + 1;
    }
    return 0;
}
#endif


static void applyCaptureScene(Game *game)
{
    const char *captureScene = getenv("DXBALL_CAPTURE_SCENE");
    if (captureScene == NULL)
        return;

    if (captureScene[0] == 'g')
    {
        game->gameStarted = 1;
        game->levelIntro = 0;
        game->ballLaunched = 1;
    }
    else if (captureScene[0] == 'h')
    {
        game->showHowToPlay = 1;
        game->howToPlayPage = (captureScene[1] >= '0' && captureScene[1] <= '2')
                                  ? captureScene[1] - '0'
                                  : 1;
    }
    else if (captureScene[0] == 'i')
    {
        int previewLevel = captureScene[1] - '0';
        game->gameStarted = 1;
        game->levelIntro = 1;
        game->level = (previewLevel >= 1 && previewLevel <= TOTAL_LEVELS) ? previewLevel : 1;
    }
    else if (captureScene[0] == 's')
    {
        game->showSettings = 1;
    }
    else if (captureScene[0] == 'l')
    {
        game->showLevelSelect = 1;
        game->unlockedLevel = TOTAL_LEVELS;
        TextCopy(game->playerName, "HARRY");
    }
    else if (captureScene[0] == 'c')
    {
        game->showCredits = 1;
    }
    else if (captureScene[0] == 'b')
    {
        static const char *previewNames[] = {"HARRY", "HERMIONE", "RON", "LUNA", "NEVILLE", "GINNY"};
        game->showHighScore = 1;
        game->boardCount = 6;
        TextCopy(game->playerName, "HARRY");
        for (int p = 0; p < game->boardCount; p++)
        {
            TextCopy(game->boardPlayers[p].name, previewNames[p]);
            game->boardPlayers[p].total = 0;
            for (int l = 0; l < TOTAL_LEVELS; l++)
            {
                game->boardPlayers[p].levels[l] = (8 - p) * 120 + l * 35;
                game->boardPlayers[p].total += game->boardPlayers[p].levels[l];
            }
        }
    }
    else if (captureScene[0] == 'n')
    {
        game->showNameEntry = 1;
        TextCopy(game->playerName, "HERMIONE");
    }
    else if (captureScene[0] == 'r')
    {
        game->showResumePrompt = 1;
        game->savedLevel = 4;
        TextCopy(game->playerName, "HARRY");
    }
    else if (captureScene[0] == 'p')
    {
        game->gameStarted = 1;
        game->levelIntro = 0;
        game->paused = 1;
    }
    else if (captureScene[0] == 'e')
    {
        game->gameStarted = 1;
        game->levelIntro = 0;
        game->gameOver = 1;
        game->score = 4280;
    }
}


/* Creates the window, the audio device and every loaded resource. Returns 0 on
   failure so main() can report it the same way the original code did. */
static int appInit(void)
{
    app.capturePath = getenv("DXBALL_CAPTURE");
    app.captureFrames = 0;
    app.shouldClose = 0;

#if !DXBALL_WEB

    /* The canvas keeps a fixed 1200x900 (4:3) backbuffer on the web and the
       page scales it with CSS, so raylib must not resize it to the browser
       window. Desktop windows stay freely resizable. */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif

    InitWindow(WINDOW_W, WINDOW_H, "DX BALL - Wizarding World");
    if (!IsWindowReady())
        return 0;

#if !DXBALL_WEB
    SetWindowMinSize(SCREEN_W / 2, SCREEN_H / 2);
#endif

    InitAudioDevice();
    SetTargetFPS(60);
    SetRandomSeed((unsigned int)time(NULL));

    app.screen = LoadRenderTexture(INTERNAL_W, INTERNAL_H);
    SetTextureFilter(app.screen.texture, TEXTURE_FILTER_BILINEAR);

    app.designCamera = (Camera2D){0};
    app.designCamera.zoom = (float)INTERNAL_SCALE;

    loadAssets(&app.assets);
    loadAudio(&app.audio);

#if DXBALL_WEB
    app.loadedLevelCount = 0;
#endif

    gameInit(&app.game);
    app.game.font = loadUIFont(&app.game.fontLoaded);
    app.game.titleFont = loadTitleFont(&app.game.titleFontLoaded);

    applyCaptureScene(&app.game);

    return 1;
}


/* One update-and-render frame. This is the body of the desktop while loop and
   the callback handed to emscripten_set_main_loop(), so neither platform has a
   private copy of the gameplay sequence. */
static void appFrame(void)
{
    Game *game = &app.game;
    Audio *audio = &app.audio;

    float dt = GetFrameTime();

    updateMusic(audio, game->level, game->gameStarted);

    int waitingForLevel = 0;
#if DXBALL_WEB
    if (requestedLevel(game) > app.loadedLevelCount)
    {
        if (IsKeyPressed(KEY_ESCAPE) || inputBackPressed())
        {
            if (game->gameStarted)
                game->returnToMenuRequested = 1;
            else
            {
                game->startLevelRequested = 0;
                game->resumeRequested = 0;
                game->restartLevelRequested = 0;
                game->showLevelSelect = 1;
            }
        }
    }
    else
#endif
        handleInput(game, audio);
    if (game->quitRequested)
    {
        app.shouldClose = 1;
        return;
    }

#if DXBALL_WEB
    waitingForLevel = requestedLevel(game) > app.loadedLevelCount;
#endif
    if (!waitingForLevel)
    {
        applyTransitions(game, audio);
        updateGameplay(game, audio, dt);
        updateAnimations(game, dt);
    }

    game->justStartedGame = 0;

#if DXBALL_WEB
    if (game->gameStarted && !game->gameOver && !game->gameWon && !game->levelComplete)
    {
        app.autosaveTimer += dt;
        if (app.autosaveTimer >= WEB_AUTOSAVE_SECONDS)
        {
            app.autosaveTimer = 0.0f;
            saveProgress(game->playerName, game->level, game->score, game->lives,
                         game->levelStartScore, game->bricks);
        }
    }
    else
    {
        app.autosaveTimer = 0.0f;
    }
#endif

    BeginTextureMode(app.screen);
    ClearBackground(BLACK);
    BeginMode2D(app.designCamera);

    if (game->gameStarted)
        drawPlayScreens(game, &app.assets);
    else
        drawMenuScreens(game, &app.assets);

#if DXBALL_WEB
    int waitingForScreen = app.loadedLevelCount == 0 &&
        (game->showHowToPlay || game->showSettings || game->showHighScore ||
         game->showCredits || game->showLevelSelect);
    if (waitingForLevel || waitingForScreen)
    {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.86f));
        int waitingForCore = app.loadedLevelCount == 0;
        int failed = waitingForCore ? app.coreFailed : app.deferredFailed;
        int percent = waitingForCore ? app.corePercent : app.deferredPercent;
        drawCenteredFontText(game->titleFont,
                             failed ? "ARTWORK DOWNLOAD FAILED"
                                    : waitingForLevel ? "PREPARING CHAMBER" : "PREPARING SCREEN",
                             SCREEN_W / 2.0f, 270, 25, 0.8f, GOLD);
        drawCenteredFontText(game->font,
                             failed ? "CHECK YOUR CONNECTION AND RELOAD"
                                    : TextFormat("DOWNLOADING ARTWORK... %d%%", percent),
                             SCREEN_W / 2.0f, 319, 14, 0.5f, RAYWHITE);
        drawCenteredFontText(game->font, "ESC TO RETURN TO THE MENU",
                             SCREEN_W / 2.0f, 352, 12, 0.5f, LIGHTGRAY);
    }
#endif

    EndMode2D();
    EndTextureMode();

    presentScreen(app.screen);

    if (app.capturePath && app.capturePath[0] != '\0' && ++app.captureFrames >= 4)
    {
        TakeScreenshot(app.capturePath);
        app.shouldClose = 1;
    }
}


static void appShutdown(void)
{
    Game *game = &app.game;

    if (game->gameStarted && !game->gameOver && !game->gameWon && !game->levelComplete)
        saveProgress(game->playerName, game->level, game->score, game->lives,
                     game->levelStartScore, game->bricks);

    unloadAudio(&app.audio);
    unloadAssets(&app.assets);

    if (game->fontLoaded)
        UnloadFont(game->font);
    if (game->titleFontLoaded)
        UnloadFont(game->titleFont);

    UnloadRenderTexture(app.screen);

    CloseAudioDevice();
    CloseWindow();
}


#if DXBALL_WEB

static void webFrame(void)
{
    if (app.coreReady && app.loadedLevelCount == 0)
    {
        loadAssets(&app.assets);
        loadAudio(&app.audio);
        app.loadedLevelCount = 1;
    }
    /* Decode one newly downloaded chamber per frame to avoid a long pause. */
    if (app.deferredReady && app.loadedLevelCount < TOTAL_LEVELS)
    {
        loadLevelAssets(&app.assets, app.loadedLevelCount);
        app.loadedLevelCount++;
    }
    appFrame();
    inputEndFrame();

    /* Nothing in the web build sets quitRequested, but honour it anyway rather
       than keeping a dead frame callback alive. */
    if (app.shouldClose)
    {
        appShutdown();
        emscripten_cancel_main_loop();
    }
}
#endif


int main(void)
{
    if (!appInit())
        return 1;

#if DXBALL_WEB

    /* The browser owns the event loop: a blocking while loop would never yield
       to it and the page would freeze. Requesting 0 fps lets the browser drive
       the callback from requestAnimationFrame. */
    emscripten_set_main_loop(webFrame, 0, 1);

    /* Not reached: emscripten_set_main_loop() with simulate_infinite_loop set
       unwinds out of main() and keeps the runtime alive. */
    return 0;
#else
    while (!WindowShouldClose() && !app.shouldClose)
        appFrame();

    appShutdown();
    return 0;
#endif
}
