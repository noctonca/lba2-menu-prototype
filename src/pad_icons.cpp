#include "pad_icons.h"

#include "ui.h"

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

#include <string.h>

extern "C" U8 Font8x8[256 * 8];

int g_padIcons = 1;

enum { PAD_XBOX = 1, PAD_PLAYSTATION = 2, PAD_NINTENDO = 3 };

// Each button 7x7 in an 8x8 cell, the top 7 rows as a capital of the 8x8 font sits.
// colour 0: the letters' ink (a button printed without colour). A button with no rows is
// its name's letter in the 8x8 font, in its colour.
static const struct {
    int pad;
    const char *name;
    U8 colour;
    U8 rows[7];
} kButtons[] = {
    // PlayStation: outlines one pixel wide, as the pad prints them; at the font's weight
    // (uprights two wide) they read as the letters X, O and I (tried 2026-11-12). The
    // PS1-PS4 print's colours, the nearest the menu palette has (green 139,203,147;
    // red 215,103,99; blue 131,183,215; pink 255,179,175).
    {PAD_PLAYSTATION, "Triangle", 141, {0x10, 0x28, 0x28, 0x44, 0x44, 0x82, 0xFE}},
    {PAD_PLAYSTATION, "Circle", 76, {0x38, 0x44, 0x82, 0x82, 0x82, 0x44, 0x38}},
    {PAD_PLAYSTATION, "Cross", 205, {0x82, 0x44, 0x28, 0x10, 0x28, 0x44, 0x82}},
    {PAD_PLAYSTATION, "Square", 79, {0xFE, 0x82, 0x82, 0x82, 0x82, 0x82, 0xFE}},
    // Xbox: the letter in its colour, as the One and Series pads print it on a dark button
    // (green 119,183,127; red 191,59,55; blue 79,155,191; yellow 255,207,0). A letter cut
    // out of a coloured disc was tried and did not read at 7 pixels (2026-11-12). View: two
    // squares, one over the other; Menu: three lines. Guide stays a word: its button is the
    // Xbox logo.
    {PAD_XBOX, "A", 140, {0}},
    {PAD_XBOX, "B", 74, {0}},
    {PAD_XBOX, "X", 203, {0}},
    {PAD_XBOX, "Y", 244, {0}},
    {PAD_XBOX, "View", 0, {0xF0, 0x90, 0x90, 0xFE, 0x12, 0x12, 0x1E}},
    {PAD_XBOX, "Menu", 0, {0x00, 0xFE, 0x00, 0xFE, 0x00, 0xFE, 0x00}},
    // Nintendo: the Switch's Pro Controller and Joy-Con print B, A, Y and X without colour,
    // so those stay the font's letters, as do - and +, which the font draws as the pad
    // prints them. Home: a house with a door, as its button shows.
    {PAD_NINTENDO, "Home", 0, {0x10, 0x38, 0x7C, 0xFE, 0x7C, 0x6C, 0x6C}},
};
enum { BUTTON_COUNT = sizeof kButtons / sizeof kButtons[0] };

// The button whose name is the whole word at s (s at a word's start), or -1; *len its length.
static int ButtonAt(const char *s, int pad, size_t *len) {
    if (!g_padIcons || !pad)
        return -1;
    for (int i = 0; i < BUTTON_COUNT; i++) {
        if (kButtons[i].pad != pad)
            continue;
        size_t n = strlen(kButtons[i].name);
        if (!strncmp(s, kButtons[i].name, n) && (s[n] == 0 || s[n] == ' ')) {
            *len = n;
            return i;
        }
    }
    return -1;
}

static bool HasRows(int b) {
    for (int r = 0; r < 7; r++)
        if (kButtons[b].rows[r])
            return true;
    return false;
}

static void DrawButton(S32 x, S32 y, int b, U8 colour, S32 scale) {
    if (!HasRows(b)) {
        char letter[2] = {kButtons[b].name[0], 0};
        Ui_Small(x, y, letter, colour, scale);
        return;
    }
    U8 *log = (U8 *)Log;
    for (S32 row = 0; row < 7; row++)
        for (S32 col = 0; col < 8; col++) {
            if (!(kButtons[b].rows[row] & (0x80 >> col)))
                continue;
            for (S32 dy = 0; dy < scale; dy++) {
                S32 py = y + row * scale + dy;
                if (py < ClipYMin || py > ClipYMax)
                    continue;
                for (S32 dx = 0; dx < scale; dx++) {
                    S32 px = x + col * scale + dx;
                    if (px >= ClipXMin && px <= ClipXMax)
                        log[TabOffLine[py] + px] = colour;
                }
            }
        }
}

// Walks s a cell at a time, drawing when draw is set; returns the cells taken.
static S32 Walk(const char *s, S32 x, S32 y, U8 colour, S32 scale, bool coloured, int pad, bool draw) {
    S32 cells = 0;
    char one[2] = {0, 0};
    for (const char *p = s; *p;) {
        size_t len;
        int b = (p == s || p[-1] == ' ') ? ButtonAt(p, pad, &len) : -1;
        S32 cx = x + cells * 8 * scale;
        if (b >= 0) {
            if (draw)
                DrawButton(cx, y, b, coloured && kButtons[b].colour ? kButtons[b].colour : colour, scale);
            p += len;
        } else {
            if (draw) {
                one[0] = *p;
                Ui_Small(cx, y, one, colour, scale);
            }
            p++;
        }
        cells++;
    }
    return cells;
}

S32 PadIcons_Width(const char *s, S32 scale, int pad) {
    return Walk(s, 0, 0, 0, scale, false, pad, false) * 8 * scale;
}

void PadIcons_Text(S32 x, S32 y, const char *s, U8 colour, S32 scale, bool coloured, int pad) {
    Walk(s, x, y, colour, scale, coloured, pad, true);
}

// The columns one cell inks: a button's drawing, or the font's glyph.
static U8 CellInk(int b, U8 ch) {
    U8 bits = 0;
    if (b >= 0 && HasRows(b)) {
        for (int r = 0; r < 7; r++)
            bits |= kButtons[b].rows[r];
        return bits;
    }
    if (b >= 0)
        ch = (U8)kButtons[b].name[0];
    for (int r = 0; r < 8; r++)
        bits |= Font8x8[ch * 8 + r];
    return bits;
}

bool PadIcons_InkColumns(const char *s, int pad, S32 *left, S32 *right) {
    S32 l = -1, r = -1, cell = 0;
    for (const char *p = s; *p; cell++) {
        size_t len;
        int b = (p == s || p[-1] == ' ') ? ButtonAt(p, pad, &len) : -1;
        U8 bits = CellInk(b, (U8)*p);
        p += b >= 0 ? len : 1;
        for (S32 col = 0; col < 8; col++)
            if (bits & (0x80 >> col)) {
                if (l < 0)
                    l = cell * 8 + col;
                r = cell * 8 + col;
            }
    }
    *left = l, *right = r;
    return l >= 0;
}

bool PadIcons_IsColour(U8 c) {
    for (int i = 0; i < BUTTON_COUNT; i++)
        if (kButtons[i].colour && kButtons[i].colour == c)
            return true;
    return false;
}
