// The Options menu: pages as data, two layouts, the review's open calls as switches.
#pragma once

#include <stdio.h>

void Menu_Init(bool firstLaunch);
void Menu_Update();  // reads this frame's keys
void Menu_Draw();

// Switches for the open calls (and a few prototype aids). Names are what the
// control channel and the trace use.
//   band     1 = one band (help + legend), 0 = help line + prompt bar   (open call 1)
//   typing   0 = type on every cursor move, 1 = type on page entry only, 2 = no typing (2)
//   chip     8x8 font scale on the logical canvas, 1 or 2                (3)
//   word     preset row word: 0 Settings, 1 Preset, 2 Rules              (6)
//   island   1 = Citadel's palette (Options opened in play), 0 = menu palette
//   help640  1 = help in the dialogue window on every one-column page
//   gameplay 1 = show the Gameplay category
//   overlay  1 = show the prototype's key list
bool Menu_SetSwitch(const char *name, int value);
int Menu_GetSwitch(const char *name);
bool Menu_Set(const char *setting, int value);  // a fake setting by name
bool Menu_Open(const char *page);  // jump to a page by name (cursor on its first row)

// Everything on screen and in the model, as one JSON object on one line.
void Menu_WriteState(FILE *out);
