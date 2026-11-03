// The original's menu drawing pieces. Copied from lba2cc where they sit in the
// game layer (INVENT.CPP, FLOW_A.CPP, GAMEMENU.CPP), which this prototype does
// not link; LIB386 itself is used from the submodule.
#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// Plasma banks (GAMEMENU.CPP): 12 the cursor, 4 the slider, 8 the current value.
enum { PLASMA_CURSOR = 12, PLASMA_SLIDER = 4, PLASMA_CURRENT = 8 };

void Cadre_InitPlasma();
// Animate the plasma (call once a frame) and fill a rectangle with a bank.
void Cadre_AnimatePlasma();
void Cadre_FireBar(S32 x0, S32 y0, S32 x1, S32 y1, S32 bank);

// INVENT.CPP DrawCadre with ALL_ANGLES: the 1px two-tone frame with chamfers.
// Call Cadre_BackupCorners before filling, Cadre_Frame after (DrawOneChoice).
void Cadre_BackupCorners(S32 x0, S32 y0, S32 x1, S32 y1);
void Cadre_Frame(S32 x0, S32 y0, S32 x1, S32 y1, bool red = false);
// Darken, do not blend (FLOW_A.CPP ShadeBoxBlk).
void Cadre_Shade(S32 x0, S32 y0, S32 x1, S32 y1, S32 level);

// Frame colours (INVENT.CPP) and shade levels (DEFINES.H).
#define COUL_DOWN_BLUE ((10 * 16) + 9)
#define COUL_UP_BLUE ((10 * 16) + 15)
#define COUL_DOWN_RED ((4 * 16) + 9)
#define COUL_UP_RED ((4 * 16) + 15)
#define MENU_SHADE_LVL 11
#define PANEL_SHADE_LVL 8
