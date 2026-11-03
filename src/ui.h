// The design's vocabulary, drawn with the original's pieces.
#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// Palette indices (design system tokens.json, lba2 theme).
enum {
    INK_BLACK = 0,
    INK_WHITE = 15,             // LBAWHITE, COUL_TEXT_MENU
    INK_DISABLED = 3 * 16 + 11, // COUL_TEXT_MENU_DISABLE
    INK_COUNT = 158,            // changed values: bank 9, three steps brighter than the
                                // design's 155 so it holds 3:1 on a shaded row
    FILL_SELECTED = 166,        // COUL_SELECT_MENU
    INK_CONFIG_HEAD = 45,       // 8x8 headers, status line
    INK_CONFIG_MENU = 174,      // 8x8 menu entries, legend
    INK_CONFIG_TEXT = 203,
    DIALOGUE_HERO = 12,         // hero bank: 192..204
};

enum RowFill { FILL_SHADE, FILL_CURSOR, FILL_CURRENT };

// Menu row height and gap (GAMEMENU.CPP HAUTEUR_STANDARD, MENU_SPACE). A row spans
// its centre +-25, so it is ROW_H + 1 pixels tall: y0 .. y0 + ROW_H.
enum { ROW_H = 50, ROW_GAP = 6, ROW_PITCH = 56 };

void Ui_RowBackground(S32 x0, S32 y0, S32 x1, S32 y1, RowFill fill);
// Frame + darkened inside: PANEL_SHADE_LVL, the 1997 dialogue window's level, or
// BAND_SHADE_LVL for the wide band, darker (the rows' MENU_SHADE_LVL) so the
// voice holds 3:1 and the legend 4.5:1 over light skies.
enum { BAND_SHADE_LVL = 11 };
void Ui_Panel(S32 x0, S32 y0, S32 x1, S32 y1, S32 level);

S32 Ui_TextWidth(const char *s);
// The width of the ink alone: Ui_TextWidth less the spacing after the last letter.
S32 Ui_TextInkWidth(const char *s);
void Ui_Text(S32 x, S32 y, const char *s, U8 colour);
void Ui_TextShadow(S32 x, S32 y, const char *s, U8 colour);  // dialogue shadow +2,+4

// Two solid triangles, kin of the corner arrow sprites, either side of a value.
void Ui_ChangeMarks(S32 leftX, S32 rightX, S32 yCentre);
// Framed steps for a value with few of them (detail level 0..3 shows 3 steps):
// lit steps take the slider plasma, unlit ones stay dark. They end on xRight.
void Ui_Steps(S32 xRight, S32 yCentre, S32 count, S32 lit, S32 *outLeft);
// A small slider for the value column, x0..x1. Dimmed: no plasma, the filled part
// flat in the dimmed label's grey (3:1 from the live plasma), the rest dark.
void Ui_MiniSlider(S32 x0, S32 x1, S32 yCentre, S32 value, S32 max, bool dim);

// 8x8 font, drawn at a whole-step scale.
S32 Ui_SmallWidth(const char *s, S32 scale);
void Ui_Small(S32 x, S32 y, const char *s, U8 colour, S32 scale);
// A key chip: the key name in the 8x8 font inside a frame. Returns its width.
S32 Ui_ChipWidth(const char *key, S32 scale);
S32 Ui_Chip(S32 x, S32 y, const char *key, S32 scale, bool lit = false);

// A binding chip of a fixed width, the name centred: shade, the cursor plasma,
// the current-value plasma while it waits for a key, or empty (a row of dots).
// red: the alert pair, for a key on two actions. Returns false when the name
// does not fit inside (it is clipped).
enum ChipFill { CHIP_SHADE, CHIP_CURSOR, CHIP_WAITING };
bool Ui_KeyChip(S32 x, S32 y, S32 w, const char *name, S32 scale, ChipFill fill, bool red);
S32 Ui_KeyChipHeight(S32 scale);

// The game's voice: help typed letter by letter with the 13-step colour ramp
// (MESSAGE.CPP AffAllCar). Wraps into at most maxLines of the game font.
struct Voice {
    char text[512];
    U32 start;
    bool typed;  // false: shown whole
};
void Voice_Say(Voice &v, const char *text, bool typed);
// How many lines of the game font the text wraps into at that width.
S32 Voice_Lines(const char *text, S32 width);
// Returns the lines drawn; *needed gets how many the whole text takes.
S32 Voice_Draw(const Voice &v, S32 x0, S32 y0, S32 width, S32 maxLines, S32 *needed);
