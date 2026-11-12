#include "pad_icons.h"

#include "ui.h"

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

#include <string.h>

int g_padIcons = 1;

// The four shapes, 7x7 in an 8x8 cell, the top 7 rows as a capital of the 8x8 font sits.
// Outlines one pixel wide, as the pad prints them: at the font's weight (uprights two wide)
// they read as the letters X, O and I (tried 2026-11-12).
// Colours: the PS1-PS4 pads' print, the nearest the menu palette has (green 139,203,147;
// red 215,103,99; blue 131,183,215; pink 255,179,175).
static const struct {
    const char *name;
    U8 colour;
    U8 rows[7];
} kShapes[] = {
    {"Triangle", 141, {0x10, 0x28, 0x28, 0x44, 0x44, 0x82, 0xFE}},
    {"Circle", 76, {0x38, 0x44, 0x82, 0x82, 0x82, 0x44, 0x38}},
    {"Cross", 205, {0x82, 0x44, 0x28, 0x10, 0x28, 0x44, 0x82}},
    {"Square", 79, {0xFE, 0x82, 0x82, 0x82, 0x82, 0x82, 0xFE}},
};
enum { SHAPE_COUNT = sizeof kShapes / sizeof kShapes[0] };

// The shape whose name is the whole word at s (s at a word's start), or -1; *len its length.
static int ShapeAt(const char *s, size_t *len) {
    if (!g_padIcons)
        return -1;
    for (int i = 0; i < SHAPE_COUNT; i++) {
        size_t n = strlen(kShapes[i].name);
        if (!strncmp(s, kShapes[i].name, n) && (s[n] == 0 || s[n] == ' ')) {
            *len = n;
            return i;
        }
    }
    return -1;
}

static void DrawShape(S32 x, S32 y, int shape, U8 colour, S32 scale) {
    U8 *log = (U8 *)Log;
    for (S32 row = 0; row < 7; row++)
        for (S32 col = 0; col < 8; col++) {
            if (!(kShapes[shape].rows[row] & (0x80 >> col)))
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

// Walks s a cell at a time: draw (when x is not null) and returns the cells taken.
static S32 Walk(const char *s, S32 x, S32 y, U8 colour, S32 scale, bool coloured, bool draw) {
    S32 cells = 0;
    char one[2] = {0, 0};
    for (const char *p = s; *p;) {
        size_t len;
        int shape = (p == s || p[-1] == ' ') ? ShapeAt(p, &len) : -1;
        S32 cx = x + cells * 8 * scale;
        if (shape >= 0) {
            if (draw)
                DrawShape(cx, y, shape, coloured ? kShapes[shape].colour : colour, scale);
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

S32 PadIcons_Width(const char *s, S32 scale) { return Walk(s, 0, 0, 0, scale, false, false) * 8 * scale; }

void PadIcons_Text(S32 x, S32 y, const char *s, U8 colour, S32 scale, bool coloured) {
    Walk(s, x, y, colour, scale, coloured, true);
}
