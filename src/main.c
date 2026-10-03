#include "config.h"

#include "raylib.h"

#include <stdlib.h>
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
#endif
} App;

static App app;


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

    handleInput(game, audio);
    if (game->quitRequested)
    {
        app.shouldClose = 1;
        return;
    }

    applyTransitions(game, audio);
    updateGameplay(game, audio, dt);
    updateAnimations(game, dt);

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
    appFrame();

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
