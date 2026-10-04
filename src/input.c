#include "raylib.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "input.h"
#include "assets.h"
#include "render.h"
#include "storage.h"

#if DXBALL_WEB
#include <emscripten/emscripten.h>

static Vector2 touchPosition;
static int touchActive;
static int touchPressed;
static int touchMode;
static int launchPressed;
static int backPressed;
static double lastTouchTime = -1000.0;

EMSCRIPTEN_KEEPALIVE void dxballTouchMode(int enabled)
{
    touchMode = enabled != 0;
}

/* phase: 0 down, 1 move, 2 up, 3 cancel. A tap is delivered on release so a
   drag can position the paddle without also launching the ball. */
EMSCRIPTEN_KEEPALIVE void dxballTouchPointer(int phase, int x, int y, int tap)
{
    touchPosition = (Vector2){(float)x, (float)y};
    lastTouchTime = GetTime();
    if (phase == 0 || phase == 1)
        touchActive = 1;
    else
    {
        touchActive = 0;
        if (phase == 2 && tap)
            touchPressed = 1;
    }
}

EMSCRIPTEN_KEEPALIVE void dxballTouchAction(int action)
{
    if (action == 1)
        launchPressed = 1;
    else if (action == 2)
        backPressed = 1;
}
#endif

int inputPressed(void)
{
#if DXBALL_WEB
    return touchPressed ||
           (GetTime() - lastTouchTime > 0.7 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON));
#else
    return IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
#endif
}

Vector2 inputPosition(void)
{
#if DXBALL_WEB
    if (touchActive || touchPressed)
        return touchPosition;
#endif
    return getMouseDesignPosition();
}

int inputTouchActive(void)
{
#if DXBALL_WEB
    return touchActive;
#else
    return 0;
#endif
}

int inputLaunchPressed(void)
{
#if DXBALL_WEB
    return launchPressed;
#else
    return 0;
#endif
}

int inputBackPressed(void)
{
#if DXBALL_WEB
    return backPressed;
#else
    return 0;
#endif
}

int inputTouchMode(void)
{
#if DXBALL_WEB
    return touchMode;
#else
    return 0;
#endif
}

void inputEndFrame(void)
{
#if DXBALL_WEB
    touchPressed = 0;
    launchPressed = 0;
    backPressed = 0;
#endif
}

static int clickedIn(Vector2 mouse, Rectangle r)
{
    return CheckCollisionPointRec(mouse, r);
}

static void handleEscape(Game *g, Audio *au)
{
    if (!IsKeyPressed(KEY_ESCAPE) && !inputBackPressed())
        return;

    if (g->gameStarted && !g->levelIntro && !g->levelComplete && !g->gameOver && !g->gameWon)
    {
        g->paused = !g->paused;
        playSoundSafe(au->ui);
        return;
    }
    if (g->gameStarted)
    {
        g->returnToMenuRequested = 1;
        playSoundSafe(au->ui);
        return;
    }
    if (g->showNameEntry)
    {
        g->showNameEntry = 0;
        playSoundSafe(au->ui);
    }
    else if (g->showLevelSelect)
    {
        g->showLevelSelect = 0;
        playSoundSafe(au->ui);
    }
    else if (g->showResumePrompt)
    {
        g->showResumePrompt = 0;
        playSoundSafe(au->ui);
    }
    else if (g->showHowToPlay)
    {
        g->showHowToPlay = 0;
        g->howToPlayPage = 0;
        playSoundSafe(au->ui);
    }
    else if (g->showHighScore)
    {
        g->showHighScore = 0;
        playSoundSafe(au->ui);
    }
    else if (g->showSettings)
    {
        g->showSettings = 0;
        playSoundSafe(au->ui);
    }
    else if (g->showCredits)
    {
        g->showCredits = 0;
        playSoundSafe(au->ui);
    }
    else
    {
#if !DXBALL_WEB
        g->quitRequested = 1;
#endif
        /* A browser tab cannot close itself, so Escape on the main menu does
           nothing in the web build. */
    }
}

void confirmPlayerName(Game *g, Audio *au)
{
    if (g->playerName[0] == '\0')
        snprintf(g->playerName, MAX_NAME, "PLAYER");

    g->showNameEntry = 0;
    g->nameConfirmed = 1;
    g->unlockedLevel = loadUnlockedLevel(g->playerName);
    loadLevelHighScores(g->playerName, g->levelHighScores);
    if (loadProgress(g->playerName, &g->savedLevel, &g->savedScore, &g->savedLives,
                     &g->savedLevelStartScore, g->savedBricks))
    {
        if (g->savedLevel > g->unlockedLevel)
        {
            g->unlockedLevel = g->savedLevel;
            saveUnlockedLevel(g->playerName, g->unlockedLevel);
        }
        g->showResumePrompt = 1;
    }
    else
    {
        g->showLevelSelect = 1;
    }
    playSoundSafe(au->ui);
}

static void handleNameEntry(Game *g, Audio *au)
{
    if (!g->showNameEntry)
        return;
    int typed = GetCharPressed();
    while (typed > 0)
    {
        int len = (int)strlen(g->playerName);
        if (len < MAX_NAME_CHARS && typed < 128 && isalnum(typed))
        {
            g->playerName[len] = (char)toupper(typed);
            g->playerName[len + 1] = '\0';
        }
        typed = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE))
    {
        int len = (int)strlen(g->playerName);
        if (len > 0)
            g->playerName[len - 1] = '\0';
    }

    if (IsKeyPressed(KEY_ENTER))
        confirmPlayerName(g, au);
}

static void handlePauseKeys(Game *g, Audio *au)
{
    if (IsKeyPressed(KEY_P) && g->gameStarted && !g->levelIntro &&
        !g->gameWon && !g->gameOver && !g->levelComplete)
    {
        g->paused = !g->paused;
        playSoundSafe(au->ui);
    }

    if (g->paused && IsKeyPressed(KEY_R))
        g->restartLevelRequested = 1;
}


static void handleLevelClearShortcut(Game *g)
{
    if (!g->gameStarted || g->paused || g->levelIntro ||
        g->gameWon || g->gameOver || g->levelComplete)
        return;

    if (!(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ||
        !IsKeyPressed(KEY_L))
        return;

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            g->bricks[r][c].active = 0;
}

static void handleHudPauseClick(Game *g, Audio *au)
{
    if (!g->gameStarted || g->paused || g->levelIntro || g->gameWon || g->gameOver || g->levelComplete)
        return;
    if (!inputPressed())
        return;

    Rectangle rect = hudPauseButtonRect();

    if (clickedIn(inputPosition(), rect))
    {
        g->paused = 1;
        g->clickConsumed = 1;
        playSoundSafe(au->ui);
    }
}

static void handlePauseMenuClick(Game *g, Audio *au)
{
    if (!g->paused || !inputPressed())
        return;

    Vector2 mouse = inputPosition();

    Rectangle rows[3];
    for (int i = 0; i < 3; i++)
        rows[i] = pauseMenuButtonRect(i);

    if (clickedIn(mouse, rows[0]))
    {
        g->paused = 0;
        g->clickConsumed = 1;
        playSoundSafe(au->ui);
    }
    else if (clickedIn(mouse, rows[1]))
    {
        g->restartLevelRequested = 1;
        g->clickConsumed = 1;
    }
    else if (clickedIn(mouse, rows[2]))
    {
        g->returnToMenuRequested = 1;
        g->clickConsumed = 1;
    }
}


static void requestPlay(Game *g, Audio *au)
{
    if (g->nameConfirmed)
    {
        g->showLevelSelect = 1;
    }
    else
    {

        g->playerName[0] = '\0';
        g->showNameEntry = 1;
    }
    playSoundSafe(au->ui);
}

static void handleMenuEnter(Game *g, Audio *au)
{
    if (g->gameStarted || g->showResumePrompt || g->showNameEntry || g->showLevelSelect ||
        g->showHowToPlay || g->showHighScore || g->showSettings || g->showCredits)
        return;

    if (IsKeyPressed(KEY_ENTER))
        requestPlay(g, au);
}


static void handleResumePromptClick(Game *g, Audio *au, Vector2 mouse)
{
    Rectangle yesRect = resumePromptButtonRect(0);
    Rectangle noRect = resumePromptButtonRect(1);

    if (clickedIn(mouse, yesRect))
    {
        g->level = g->savedLevel;
        g->score = g->savedScore;
        g->lives = g->savedLives;
        g->showResumePrompt = 0;
        g->resumeRequested = 1;
    }
    else if (clickedIn(mouse, noRect))
    {
        g->showResumePrompt = 0;
        g->showLevelSelect = 1;
        playSoundSafe(au->ui);
    }
}

static void handleLevelSelectClick(Game *g, Audio *au, Vector2 mouse)
{
    Rectangle backRect = bottomActionRect(0, 1);

    if (clickedIn(mouse, backRect))
    {
        g->showLevelSelect = 0;
        playSoundSafe(au->ui);
        return;
    }

    for (int i = 0; i < TOTAL_LEVELS; i++)
    {

        if (i + 1 > g->unlockedLevel)
            continue;

        if (clickedIn(mouse, levelSelectTileRect(i)))
        {
            g->selectedStartLevel = i + 1;
            g->startLevelRequested = 1;
            g->showLevelSelect = 0;
            playSoundSafe(au->ui);
            return;
        }
    }
}

static void handleHowToPlayClick(Game *g, Audio *au, Vector2 mouse)
{


    Rectangle previousRect = helpNavRect(g->howToPlayPage, HELP_NAV_PREVIOUS);
    Rectangle backRect = helpNavRect(g->howToPlayPage, HELP_NAV_BACK);
    Rectangle nextRect = helpNavRect(g->howToPlayPage, HELP_NAV_NEXT);

    if (g->howToPlayPage < 2 && clickedIn(mouse, nextRect))
    {
        g->howToPlayPage++;
        playSoundSafe(au->ui);
    }
    else if (g->howToPlayPage > 0 && clickedIn(mouse, previousRect))
    {
        g->howToPlayPage--;
        playSoundSafe(au->ui);
    }
    else if (clickedIn(mouse, backRect))
    {
        g->showHowToPlay = 0;
        g->howToPlayPage = 0;
        playSoundSafe(au->ui);
    }
}


static void applyDifficulty(Game *g, int difficulty)
{
    static const int livesFor[3] = {4, 3, 2};
    static const float speedFor[3] = {0.85f, 1.0f, 1.25f};

    g->difficulty = difficulty;
    g->startLives = livesFor[difficulty];
    g->ballSpeedSetting = speedFor[difficulty];
}


static void resetEverything(Game *g)
{
    g->soundOn = 1;
    SetMasterVolume(1.0f);
    g->mouseControlEnabled = 0;
    g->paddleSpeed = 7.0f;
    applyDifficulty(g, 1);

    clearProgress(g->playerName);
    clearHighScore();
    clearLevelHighScores(g->playerName);
    clearUnlockedLevel(g->playerName);

    memset(g->levelHighScores, 0, sizeof(g->levelHighScores));
    g->unlockedLevel = 1;
    g->boardCount = 0;
    g->lives = g->startLives;
    g->score = 0;
    g->level = 1;
    g->showResumePrompt = 0;
}

static void handleSettingsClick(Game *g, Audio *au, Vector2 mouse)
{


    Rectangle soundToggleRect = settingsToggleRect(0);
    Rectangle ballSpeedMinusRect = settingsStepRect(1, 0);
    Rectangle ballSpeedPlusRect = settingsStepRect(1, 1);
    Rectangle paddleSpeedMinusRect = settingsStepRect(2, 0);
    Rectangle paddleSpeedPlusRect = settingsStepRect(2, 1);
    Rectangle difficultyEasyRect = settingsDifficultyRect(0);
    Rectangle difficultyNormalRect = settingsDifficultyRect(1);
    Rectangle difficultyHardRect = settingsDifficultyRect(2);
    Rectangle mouseToggleRect = settingsToggleRect(4);
    Rectangle resetButtonRect = bottomActionRect(0, 2);
    Rectangle backRect = bottomActionRect(1, 2);

    if (clickedIn(mouse, soundToggleRect))
    {
        g->soundOn = !g->soundOn;
        SetMasterVolume(g->soundOn ? 1.0f : 0.0f);
        playSoundSafe(au->ui);
    }
    else if (clickedIn(mouse, ballSpeedMinusRect))
    {
        g->ballSpeedSetting -= 0.1f;
        if (g->ballSpeedSetting < 0.5f)
            g->ballSpeedSetting = 0.5f;
        playSoundSafe(au->ui);
    }
    else if (clickedIn(mouse, ballSpeedPlusRect))
    {
        g->ballSpeedSetting += 0.1f;
        if (g->ballSpeedSetting > 2.0f)
            g->ballSpeedSetting = 2.0f;
        playSoundSafe(au->ui);
    }
    else if (clickedIn(mouse, paddleSpeedMinusRect))
    {
        g->paddleSpeed -= 1.0f;
        if (g->paddleSpeed < 3.0f)
            g->paddleSpeed = 3.0f;
        playSoundSafe(au->ui);
    }
    else if (clickedIn(mouse, paddleSpeedPlusRect))
    {
        g->paddleSpeed += 1.0f;
        if (g->paddleSpeed > 14.0f)
            g->paddleSpeed = 14.0f;
        playSoundSafe(au->ui);
    }
    else if (clickedIn(mouse, difficultyEasyRect))
    {
        applyDifficulty(g, 0);
        playSoundSafe(au->ui);
    }
    else if (clickedIn(mouse, difficultyNormalRect))
    {
        applyDifficulty(g, 1);
        playSoundSafe(au->ui);
    }
    else if (clickedIn(mouse, difficultyHardRect))
    {
        applyDifficulty(g, 2);
        playSoundSafe(au->ui);
    }
    else if (clickedIn(mouse, mouseToggleRect))
    {
        g->mouseControlEnabled = !g->mouseControlEnabled;
        playSoundSafe(au->ui);
    }
    else if (clickedIn(mouse, resetButtonRect))
    {
        resetEverything(g);
        playSoundSafe(au->ui);
    }
    else if (clickedIn(mouse, backRect))
    {
        g->showSettings = 0;
        playSoundSafe(au->ui);
    }


    saveSettings(g->soundOn, g->mouseControlEnabled, g->difficulty,
                 g->ballSpeedSetting, g->paddleSpeed, g->startLives);
}

static void handleMainMenuClick(Game *g, Audio *au, Vector2 mouse)
{
    Rectangle credits = creditsButtonRect();
    if (clickedIn(mouse, credits))
    {
        g->showCredits = 1;
        playSoundSafe(au->ui);
        return;
    }

    enum
    {
        ROW_PLAY,
        ROW_HOW_TO_PLAY,
        ROW_SETTINGS,
        ROW_HIGH_SCORE,
        ROW_QUIT
    };

    int row = -1;
    for (int i = 0; i < MAIN_MENU_ROWS; i++)
    {
        Rectangle rect = mainMenuButtonRect(i);
        if (clickedIn(mouse, rect))
        {
            row = i;
            break;
        }
    }

    switch (row)
    {
    case ROW_PLAY:
        requestPlay(g, au);
        break;

    case ROW_HOW_TO_PLAY:
        g->showHowToPlay = 1;
        g->howToPlayPage = 0;
        playSoundSafe(au->ui);
        break;

    case ROW_SETTINGS:
        g->showSettings = 1;
        playSoundSafe(au->ui);
        break;

    case ROW_HIGH_SCORE:
        g->showHighScore = 1;
        g->boardCount = loadAllPlayerScores(g->boardPlayers, MAX_PLAYERS);
        playSoundSafe(au->ui);
        break;

    case ROW_QUIT:
        /* Unreachable on the web, where MAIN_MENU_ROWS omits this row. */
        g->quitRequested = 1;
        break;

    default:
        break;
    }
}

static void handleMenuClick(Game *g, Audio *au)
{
    if (g->gameStarted || !inputPressed())
        return;

    Vector2 mouse = inputPosition();

    if (g->showNameEntry)
    {

        return;
    }

    if (g->showResumePrompt)
        handleResumePromptClick(g, au, mouse);
    else if (g->showLevelSelect)
        handleLevelSelectClick(g, au, mouse);
    else if (g->showHowToPlay)
        handleHowToPlayClick(g, au, mouse);
    else if (g->showHighScore)
    {
        Rectangle backRect = bottomActionRect(0, 1);
        if (clickedIn(mouse, backRect))
        {
            g->showHighScore = 0;
            playSoundSafe(au->ui);
        }
    }
    else if (g->showSettings)
        handleSettingsClick(g, au, mouse);
    else if (g->showCredits)
    {
        Rectangle backRect = bottomActionRect(0, 1);
        if (clickedIn(mouse, backRect))
        {
            g->showCredits = 0;
            playSoundSafe(au->ui);
        }
    }
    else
        handleMainMenuClick(g, au, mouse);
}


static void handleEndScreenInput(Game *g)
{

    if (g->gameOver && IsKeyPressed(KEY_R))
        g->restartLevelRequested = 1;

    if (!g->gameStarted || !(g->levelComplete || g->gameOver || g->gameWon))
        return;

    int clicked = inputPressed();
    Vector2 mouse = inputPosition();


    float menuButtonX = g->gameWon ? SCREEN_W / 2.0f : END_MENU_BUTTON_X;

    Rectangle mainMenuRect = {
        menuButtonX - END_MENU_BUTTON_W / 2.0f,
        END_MENU_BUTTON_Y - END_MENU_BUTTON_H / 2.0f,
        END_MENU_BUTTON_W,
        END_MENU_BUTTON_H};

    if ((clicked && clickedIn(mouse, mainMenuRect)) || IsKeyPressed(KEY_M))
    {
        g->returnToMenuRequested = 1;
        return;
    }


    Rectangle leftRect = {
        END_RETRY_BUTTON_X - END_MENU_BUTTON_W / 2.0f,
        END_MENU_BUTTON_Y - END_MENU_BUTTON_H / 2.0f,
        END_MENU_BUTTON_W,
        END_MENU_BUTTON_H};

    if (!clicked || !clickedIn(mouse, leftRect))
        return;

    if (g->levelComplete)
        g->nextLevelRequested = 1;
    else if (g->gameOver)
        g->restartLevelRequested = 1;
}


void handleInput(Game *g, Audio *au)
{
    g->clickConsumed = 0;

    handleEscape(g, au);
    handleNameEntry(g, au);
    handlePauseKeys(g, au);
    handleLevelClearShortcut(g);
    handleHudPauseClick(g, au);
    handlePauseMenuClick(g, au);
    handleMenuEnter(g, au);
    handleMenuClick(g, au);
    handleEndScreenInput(g);
}
