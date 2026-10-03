#include "raylib.h"

#include "audio.h"
#include "assets.h"
#include "levels.h"

static void loadSoundOnce(Sound *slot, const char *path)
{
    if (slot->frameCount == 0)
        *slot = loadSoundSafe(path);
}

static void loadMusicOnce(Music *slot, const char *path)
{
    if (slot->frameCount == 0)
        *slot = loadMusicSafe(path);
}


void loadAudio(Audio *au)
{
    loadSoundOnce(&au->brick, "assets/sounds/brick_magic_hit.wav");
    loadSoundOnce(&au->brickBreak, "assets/sounds/brick_break_crystal.wav");
    loadSoundOnce(&au->paddle, "assets/sounds/ball_soft_hit.wav");
    loadSoundOnce(&au->lifeLost, "assets/sounds/life_lost_gentle.wav");
    loadSoundOnce(&au->gameOver, "assets/sounds/game_over_soft.wav");
    loadSoundOnce(&au->levelStart, "assets/sounds/level_start_cozy.wav");
    loadSoundOnce(&au->levelComplete, "assets/sounds/level_complete_warm.wav");
    loadSoundOnce(&au->victory, "assets/sounds/victory_magical_finish.wav");
    loadSoundOnce(&au->powerLife, "assets/sounds/extra_life_warm.wav");
    loadSoundOnce(&au->powerSpeed, "assets/sounds/speed_magic.wav");
    loadSoundOnce(&au->powerMulti, "assets/sounds/multiball_arcane.wav");
    loadSoundOnce(&au->powerWide, "assets/sounds/wide_paddle_glow.wav");
    loadSoundOnce(&au->powerInvincible, "assets/sounds/invincible_aura.wav");
    loadSoundOnce(&au->skull, "assets/sounds/skull_dark_chime.wav");
    loadSoundOnce(&au->book, "assets/sounds/magic_book_open.wav");
    loadSoundOnce(&au->locked, "assets/sounds/locked_mystery.wav");
    loadSoundOnce(&au->ui, "assets/sounds/ui_soft_chime.wav");


    au->powerSounds[0] = au->powerLife;
    au->powerSounds[1] = au->powerSpeed;
    au->powerSounds[2] = au->powerMulti;
    au->powerSounds[3] = au->powerWide;
    au->powerSounds[4] = au->powerInvincible;

    loadMusicOnce(&au->music[0], "assets/sounds/level1_cozy_magic_loop.wav");
    loadMusicOnce(&au->music[1], "assets/sounds/level2_arcane_library_loop.wav");
    loadMusicOnce(&au->music[2], "assets/sounds/level3_mysterious_castle_loop.wav");

    loadMusicOnce(&au->menuMusic, "assets/sounds/menu_theme.wav");
}

void unloadAudio(Audio *au)
{
    for (int i = 0; i < MUSIC_THEMES; i++)
    {
        if (au->music[i].frameCount > 0)
            UnloadMusicStream(au->music[i]);
    }

    if (au->menuMusic.frameCount > 0)
        UnloadMusicStream(au->menuMusic);

    Sound *sounds[] = {
        &au->brick, &au->brickBreak, &au->paddle, &au->lifeLost,
        &au->gameOver, &au->levelStart, &au->levelComplete, &au->victory,
        &au->powerLife, &au->powerSpeed, &au->powerMulti, &au->powerWide,
        &au->powerInvincible, &au->skull, &au->book, &au->locked, &au->ui};

    for (int i = 0; i < (int)(sizeof(sounds) / sizeof(sounds[0])); i++)
        if (sounds[i]->frameCount > 0)
            UnloadSound(*sounds[i]);
}

void updateMusic(Audio *au, int level, int gameStarted)
{
    int theme = musicThemeIndex(level);

    if (au->music[theme].frameCount > 0)
    {
        UpdateMusicStream(au->music[theme]);
        if (gameStarted && !IsMusicStreamPlaying(au->music[theme]))
            PlayMusicStream(au->music[theme]);
    }

    if (au->menuMusic.frameCount > 0)
    {
        if (!gameStarted)
        {
            UpdateMusicStream(au->menuMusic);
            if (!IsMusicStreamPlaying(au->menuMusic))
                PlayMusicStream(au->menuMusic);
        }
        else if (IsMusicStreamPlaying(au->menuMusic))
        {
            StopMusicStream(au->menuMusic);
        }
    }
}

void stopLevelMusic(Audio *au)
{
    for (int i = 0; i < MUSIC_THEMES; i++)
    {
        if (au->music[i].frameCount > 0 && IsMusicStreamPlaying(au->music[i]))
            StopMusicStream(au->music[i]);
    }
}
