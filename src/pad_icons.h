// A pad's buttons drawn as the pad shows them, in place of their names on chips:
// PlayStation's four shapes; Xbox's coloured letters, View and Menu. The names stay the
// binding's identity (the trace, the self test, the voice's "<CROSS>"); only a chip's
// drawing changes, and only on a chip that names a pad's button: the keyboard's X key
// stays the letter.
#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

extern int g_padIcons;  // 1: the pad's look on chips (the default), 0: the names; switch padicons, F12

// pad: the family whose buttons s names (1 Xbox, 2 PlayStation, 3 Nintendo), 0 for a key.
// The width of a chip's text in the 8x8 font, a button taking one letter's cell.
S32 PadIcons_Width(const char *s, S32 scale, int pad);
// The text as Ui_Small draws it, each button's name (a whole word) drawn as the button.
// coloured: in the pad's colours (a plain chip); otherwise in colour, as the letters
// (white on a plasma chip).
void PadIcons_Text(S32 x, S32 y, const char *s, U8 colour, S32 scale, bool coloured, int pad);
