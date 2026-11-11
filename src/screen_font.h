#pragma once

// The menu's text in Zilla Slab (SIL OFL, assets/fonts) at the screen's resolution,
// in place of the game font (asked for 2026-11-11; the faces are the ones lba2cc's
// parked HD UI overlay chose, branch feat/hd-ui-scale).
//
// The menu is still laid out on the logical canvas. When the font is on, the Ui_*
// text helpers do not draw into the canvas: they queue each string here with its
// logical position, palette colour and clip. When the frame is presented, the
// queue is drawn in RGB over the screen-size frame, scaled by the canvas's whole
// step, through LIB386's overlay pre-present hook. Widths come from the font's
// own metrics, scaled so its capitals are as tall as the game font's (22 logical
// pixels, baseline 29 below the text's y).
//
// A string that something later in the frame drew over (a confirm box over the
// rows, the preset screen over the hub) is dropped: each entry remembers a
// checksum of the canvas under it when queued, and is drawn only if that part
// of the canvas is unchanged at present.
//
// The self test and the sweeps read ink from the canvas, so they run with the
// game font; `shot` (the canvas) shows no text with this on, `shotscreen` does.

#include <SYSTEM/ADELINE_TYPES.H>

enum { SCREEN_FONT_GAME = 0, SCREEN_FONT_ZILLA = 1 };  // Zilla Slab Bold

void ScreenFont_Init();
void ScreenFont_Set(int face);  // SCREEN_FONT_GAME draws with the game font as before
int ScreenFont_Face();
inline bool ScreenFont_On() { return ScreenFont_Face() != SCREEN_FONT_GAME; }

// Start of a frame: forget last frame's text.
void ScreenFont_BeginFrame();

// Logical widths, CP850 text, as SizeFont gives them.
S32 ScreenFont_Width(const char *s);
S32 ScreenFont_InkWidth(const char *s);
// One letter's advance (a space included), for typed text.
S32 ScreenFont_Advance(U8 c);

// Queue text at logical (x, y), the top of the game font's cell, in palette colour `ink`.
void ScreenFont_Text(S32 x, S32 y, const char *s, U8 ink);
// One letter; returns its advance.
S32 ScreenFont_Char(S32 x, S32 y, U8 c, U8 ink);

// Paint the queue over a presented frame (w x h, pitch in bytes) through `palette`.
// The canvas (lw x lh) sits centred at its whole step.
void ScreenFont_Paint(U32 *argb, U32 pitch, U32 w, U32 h, const U8 *palette);
