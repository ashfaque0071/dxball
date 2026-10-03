#include "raylib.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets.h"


/* Resolves an asset path to the file that actually shipped.

   The desktop builds load assets/ exactly as it sits in the repository and
   this returns the path unchanged on the first check. The web build packages a
   re-encoded copy instead -- opaque PNGs become JPEG and WAVs become OGG,
   which is what keeps the browser download to a fraction of the 71 MB the
   originals weigh -- so a path ending in .png or .wav may have shipped under a
   different extension. Call sites keep using the original names.

   The returned pointer is either the caller's own string or a static buffer
   that stays valid until the next call, which is all the loaders below need. */
static const char *resolveAssetPath(const char *path)
{
    if (FileExists(path))
        return path;

    static const struct
    {
        const char *from;
        const char *to;
    } swaps[] = {
        {".png", ".jpg"},
        {".wav", ".ogg"},
    };

    static char alternate[512];
    size_t len = strlen(path);

    for (int i = 0; i < (int)(sizeof(swaps) / sizeof(swaps[0])); i++)
    {
        size_t fromLen = strlen(swaps[i].from);
        if (len <= fromLen || !IsFileExtension(path, swaps[i].from))
            continue;

        int written = snprintf(alternate, sizeof(alternate), "%.*s%s",
                               (int)(len - fromLen), path, swaps[i].to);
        if (written < 0 || (size_t)written >= sizeof(alternate))
            break;

        if (FileExists(alternate))
            return alternate;
    }

    return path;
}

Texture2D loadTextureSafe(const char *path)
{
    Texture2D t = {0};
    const char *resolved = resolveAssetPath(path);
    if (FileExists(resolved))
        t = LoadTexture(resolved);
    return t;
}

static void loadTextureOnce(Texture2D *slot, const char *path)
{
    if (slot->id == 0)
        *slot = loadTextureSafe(path);
}

Sound loadSoundSafe(const char *path)
{
    Sound s = {0};
    const char *resolved = resolveAssetPath(path);
    if (FileExists(resolved))
        s = LoadSound(resolved);
    return s;
}

Music loadMusicSafe(const char *path)
{
    Music m = {0};
    const char *resolved = resolveAssetPath(path);
    if (FileExists(resolved))
        m = LoadMusicStream(resolved);
    return m;
}

void playSoundSafe(Sound s)
{
    if (s.frameCount > 0)
        PlaySound(s);
}

void unloadTextureIfLoaded(Texture2D *t)
{
    if (t->id != 0)
        UnloadTexture(*t);
}


static void loadBrickTextures(Assets *a)
{
    loadTextureOnce(&a->brickAtlas, "assets/sprites/bricks.png");
    if (a->brickAtlas.id != 0)
        SetTextureFilter(a->brickAtlas, TEXTURE_FILTER_BILINEAR);
}

void loadLevelAssets(Assets *a, int index)
{
    if (index < 0 || index >= TOTAL_LEVELS)
        return;
    loadTextureOnce(&a->bg[index], TextFormat("assets/backgrounds/level_%d.png", index + 1));
    if (a->bg[index].id != 0)
        SetTextureFilter(a->bg[index], TEXTURE_FILTER_BILINEAR);
    loadTextureOnce(&a->levelTitle[index], TextFormat("assets/ui/level_intros/level_intro_%d.png", index + 1));
    if (a->levelTitle[index].id != 0)
        SetTextureFilter(a->levelTitle[index], TEXTURE_FILTER_BILINEAR);
}

static void loadPlayTextures(Assets *a)
{
    loadTextureOnce(&a->paddleAtlas, "assets/sprites/paddles.png");
    loadTextureOnce(&a->powerAtlas, "assets/sprites/powerups.png");
    loadTextureOnce(&a->heartFull, "assets/ui/heart_full.png");
    if (a->paddleAtlas.id != 0)
        SetTextureFilter(a->paddleAtlas, TEXTURE_FILTER_BILINEAR);
    if (a->powerAtlas.id != 0)
        SetTextureFilter(a->powerAtlas, TEXTURE_FILTER_BILINEAR);
    if (a->heartFull.id != 0)
        SetTextureFilter(a->heartFull, TEXTURE_FILTER_BILINEAR);

    for (int i = 0; i < 16; i++)
    {
        loadTextureOnce(&a->snitch[i], TextFormat("assets/sprites/snitch/snitch_%02d.png", i + 1));
        if (a->snitch[i].id != 0)
            SetTextureFilter(a->snitch[i], TEXTURE_FILTER_BILINEAR);
    }

    for (int i = 0; i < TOTAL_LEVELS; i++)
    {
#if DXBALL_WEB
        if (i > 0)
            break;
#endif
        loadLevelAssets(a, i);
    }
}

static void loadUITextures(Assets *a)
{
    loadTextureOnce(&a->menuBackground, "assets/backgrounds/menu.png");
    loadTextureOnce(&a->logo, "assets/ui/logo.png");
    loadTextureOnce(&a->buttonPlate, "assets/ui/button.png");
    loadTextureOnce(&a->settingsBackground, "assets/ui/settings.png");
    loadTextureOnce(&a->levelSelectBackground, "assets/ui/level_select.png");
    loadTextureOnce(&a->highScoreBackground, "assets/ui/high_scores.png");
    loadTextureOnce(&a->bookPanel, "assets/ui/book_panel.png");
    loadTextureOnce(&a->howToPlayScreen, "assets/ui/help_controls.png");
    loadTextureOnce(&a->howToPlayPowerups, "assets/ui/help_powerups.png");
    loadTextureOnce(&a->howToPlayBricks, "assets/ui/help_bricks.png");
    if (a->menuBackground.id != 0)
        SetTextureFilter(a->menuBackground, TEXTURE_FILTER_BILINEAR);
    Texture2D *ui[] = {&a->logo, &a->buttonPlate, &a->settingsBackground,
                       &a->levelSelectBackground, &a->highScoreBackground, &a->bookPanel,
                       &a->howToPlayScreen, &a->howToPlayPowerups, &a->howToPlayBricks};
    for (int i = 0; i < (int)(sizeof(ui) / sizeof(ui[0])); i++)
        if (ui[i]->id != 0)
            SetTextureFilter(*ui[i], TEXTURE_FILTER_BILINEAR);
}

void loadAssets(Assets *a)
{
    loadBrickTextures(a);
    loadPlayTextures(a);
    loadUITextures(a);
}


static void smoothFontAtlas(Font *font)
{
    GenTextureMipmaps(&font->texture);
    SetTextureFilter(font->texture, TEXTURE_FILTER_TRILINEAR);
}

Font loadUIFont(int *loaded)
{
    *loaded = 0;

    if (FileExists("assets/fonts/Cinzel.ttf"))
    {
        Font font = LoadFontEx("assets/fonts/Cinzel.ttf", 72, NULL, 0);
        if (font.texture.id != 0)
        {
            smoothFontAtlas(&font);
            *loaded = 1;
            return font;
        }
    }

    return GetFontDefault();
}

Font loadTitleFont(int *loaded)
{
    *loaded = 0;
    if (FileExists("assets/fonts/CinzelDecorative-Regular.ttf"))
    {
        Font font = LoadFontEx("assets/fonts/CinzelDecorative-Regular.ttf", 96, NULL, 0);
        if (font.texture.id != 0)
        {
            smoothFontAtlas(&font);
            *loaded = 1;
            return font;
        }
    }
    return GetFontDefault();
}

void unloadAssets(Assets *a)
{


    Texture2D *bricks[] = {
        &a->red, &a->blue, &a->green, &a->yellow, &a->purple,
        &a->ice, &a->fire, &a->lightning, &a->rune, &a->wood,
        &a->stone, &a->stoneHit1, &a->stoneHit2,
        &a->locked, &a->lockedHit1, &a->lockedHit2,
        &a->skull, &a->book,
        &a->redBreak, &a->blueBreak, &a->greenBreak, &a->purpleBreak,
        &a->iceBreak, &a->fireBreak, &a->lightningBreak, &a->runeBreak, &a->woodBreak,
        &a->stoneBreak, &a->lockedBreak, &a->skullBreak, &a->bookBreak, &a->goldBreak};

    Texture2D *play[] = {
        &a->paddleNormal, &a->paddleWide, &a->paddleGolden, &a->paddleShield,
        &a->powerLife, &a->powerSpeed, &a->powerMulti, &a->powerWide,
        &a->powerInvincible, &a->powerAntilife, &a->heartFull};

    Texture2D *ui[] = {
        &a->levelClear, &a->gameOver, &a->youWin,
        &a->buttonPause, &a->buttonResume, &a->buttonRestart, &a->buttonMainMenu,
        &a->menuBackground, &a->menuLogo, &a->menuPlay, &a->menuHowToPlay,
        &a->menuSettings, &a->menuHighScore, &a->menuQuit,
        &a->howToPlayScreen, &a->howToPlayPowerups, &a->howToPlayBricks,
        &a->highScoreScreen, &a->panelSettings,
        &a->btnPillSmall, &a->btnPillBrown, &a->btnSquareSmall,
        &a->btnHelpNext, &a->btnHelpPrevious, &a->btnHelpBack, &a->btnSettingsBack,
        &a->btnMainMenu, &a->btnRetry, &a->btnNextLevel,
        &a->btnNextLevelNavy, &a->btnMainMenuNavy};

    unloadTextureIfLoaded(&a->brickAtlas);
    unloadTextureIfLoaded(&a->paddleAtlas);
    unloadTextureIfLoaded(&a->powerAtlas);
    unloadTextureIfLoaded(&a->logo);
    unloadTextureIfLoaded(&a->buttonPlate);
    unloadTextureIfLoaded(&a->settingsBackground);
    unloadTextureIfLoaded(&a->levelSelectBackground);
    unloadTextureIfLoaded(&a->highScoreBackground);
    unloadTextureIfLoaded(&a->bookPanel);
    for (int i = 0; i < 16; i++)
        unloadTextureIfLoaded(&a->snitch[i]);

    for (int i = 0; i < (int)(sizeof(bricks) / sizeof(bricks[0])); i++)
        unloadTextureIfLoaded(bricks[i]);

    for (int i = 0; i < (int)(sizeof(play) / sizeof(play[0])); i++)
        unloadTextureIfLoaded(play[i]);

    for (int i = 0; i < (int)(sizeof(ui) / sizeof(ui[0])); i++)
        unloadTextureIfLoaded(ui[i]);

    for (int i = 0; i < TOTAL_LEVELS; i++)
    {
        unloadTextureIfLoaded(&a->bg[i]);
        unloadTextureIfLoaded(&a->levelTitle[i]);
    }
}
