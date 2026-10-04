#ifndef INPUT_H
#define INPUT_H

#include "game.h"
#include "audio.h"


void handleInput(Game *g, Audio *au);
void confirmPlayerName(Game *g, Audio *au);

/* Shared pointer input. Web touch events are bridged from the page; desktop
   builds continue to use raylib's mouse and keyboard input. */
int inputPressed(void);
Vector2 inputPosition(void);
int inputTouchActive(void);
int inputLaunchPressed(void);
int inputBackPressed(void);
int inputTouchMode(void);
void inputEndFrame(void);

#endif
