// A pad's button shapes in place of their names on chips: PlayStation's four face buttons
// for now. The names stay the binding's identity (the trace, the self test, the voice's
// "<CROSS>"); only a chip's drawing changes.
#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

extern int g_padIcons;  // 1: shapes on chips (the default), 0: the names; switch padicons, F12

// The width of a chip's text in the 8x8 font, a shape taking one letter's cell.
S32 PadIcons_Width(const char *s, S32 scale);
// The text as Ui_Small draws it, each shape's name (a whole word) drawn as the shape.
// coloured: the shapes in their pad colours (a plain chip); otherwise in colour, as the
// letters (white on a plasma chip).
void PadIcons_Text(S32 x, S32 y, const char *s, U8 colour, S32 scale, bool coloured);
