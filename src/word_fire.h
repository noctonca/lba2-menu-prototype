#pragma once

// Fire inside the letters of the game menu's LBA² wordmark (the maintainer, 2026-11-16). The
// letters are painted at the screen's resolution (screen_font.h), so the fire is a colour per
// screen pixel inside the glyphs, nothing drawn on the canvas: flames rising through the menu
// palette's fire ramp, in tongues, slowed, with a gold rim round each letter.

#include <SYSTEM/ADELINE_TYPES.H>

// Once a frame the wordmark is drawn, with its letters' box (logical, inclusive): moves the
// flames on and hands the font painter its fill.
void WordFire_Frame(S32 x0, S32 y0, S32 x1, S32 y1);
