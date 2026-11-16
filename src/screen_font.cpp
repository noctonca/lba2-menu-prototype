#include "screen_font.h"

#include "host.h"
#include "ui.h"
#include "prop_scale.h"

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// Font bytes baked in at build time (cmake/embed_binary.cmake).
extern "C" const unsigned char g_zillaSlabBoldTtf[];

// The game font's cell, in logical pixels (RESS.HQR entry 1: 'H' is 22 tall from
// y + 7, so the baseline is y + 29; 'g' reaches y + 36).
enum { GAME_CAP = 22, GAME_BASELINE = 29, CELL_H = TEXT_LINE_H };

// CP850 128-255 to Unicode, as TEXT.HQR's text is CP850.
static const U16 kCp850[128] = {
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE,
    0x00EC, 0x00C4, 0x00C5, 0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6,
    0x00DC, 0x00F8, 0x00A3, 0x00D8, 0x00D7, 0x0192, 0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA,
    0x00BA, 0x00BF, 0x00AE, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB, 0x2591, 0x2592, 0x2593, 0x2502,
    0x2524, 0x00C1, 0x00C2, 0x00C0, 0x00A9, 0x2563, 0x2551, 0x2557, 0x255D, 0x00A2, 0x00A5, 0x2510, 0x2514,
    0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x00E3, 0x00C3, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550,
    0x256C, 0x00A4, 0x00F0, 0x00D0, 0x00CA, 0x00CB, 0x00C8, 0x0131, 0x00CD, 0x00CE, 0x00CF, 0x2518, 0x250C,
    0x2588, 0x2584, 0x00A6, 0x00CC, 0x2580, 0x00D3, 0x00DF, 0x00D4, 0x00D2, 0x00F5, 0x00D5, 0x00B5, 0x00FE,
    0x00DE, 0x00DA, 0x00DB, 0x00D9, 0x00FD, 0x00DD, 0x00AF, 0x00B4, 0x00AD, 0x00B1, 0x2017, 0x00BE, 0x00B6,
    0x00A7, 0x00F7, 0x00B8, 0x00B0, 0x00A8, 0x00B7, 0x00B9, 0x00B3, 0x00B2, 0x25A0, 0x00A0};

static int Unicode(U8 c) { return c < 128 ? c : kCp850[c - 128]; }

static stbtt_fontinfo s_font[1];  // bold
static bool s_ready;
static int s_face = SCREEN_FONT_GAME;
// Capitals as tall as the game font's (22 logical pixels).
static const int s_sizePercent = 100;
static float s_unit[1];  // font units to logical pixels

static void SetSizePercent(int percent);

void ScreenFont_Init() {
    if (s_ready)
        return;
    if (!stbtt_InitFont(&s_font[0], g_zillaSlabBoldTtf, stbtt_GetFontOffsetForIndex(g_zillaSlabBoldTtf, 0)))
        return;
    s_ready = true;
    SetSizePercent(s_sizePercent);
}

// Advances and kerning in logical pixels, looked up once: stb_truetype searches the font's
// tables on every call, and widths are asked for every frame (wrapping the help asks for
// each prefix of a line). Filled as letters are met; SetSizePercent empties them.
static float s_advance[1][256];
static bool s_advanceKnown[1][256];
static float s_kern[1][256][256];
static bool s_kernKnown[1][256][256];

static void SetSizePercent(int percent) {
    if (!s_ready)
        return;
    for (int f = 0; f < 1; f++) {
        int x0, y0, x1, y1;
        stbtt_GetCodepointBox(&s_font[f], 'H', &x0, &y0, &x1, &y1);
        s_unit[f] = (float)GAME_CAP * percent / 100.0f / (float)y1;
    }
    memset(s_advanceKnown, 0, sizeof s_advanceKnown);
    memset(s_kernKnown, 0, sizeof s_kernKnown);
}

void ScreenFont_Set(int face) { s_face = s_ready ? face : SCREEN_FONT_GAME; }
int ScreenFont_Face() { return s_face; }

static int FaceIndex() { return 0; }

// The narrowest space in small text, in logical pixels. Zilla's own is 2 at the 1x size
// (capitals 7 tall), and the letters' soft edges either side close it at 640x480.
static const float SMALL_SPACE_MIN = 3.0f;

// Advance of c (and kerning before the next letter) in logical pixels, before scale.
static float Advance(int f, U8 c, U8 next, float scale = 1.0f) {
    if (!s_advanceKnown[f][c]) {
        int aw, lsb;
        stbtt_GetCodepointHMetrics(&s_font[f], Unicode(c), &aw, &lsb);
        s_advance[f][c] = aw * s_unit[f];
        s_advanceKnown[f][c] = true;
    }
    float a = s_advance[f][c];
    if (c == ' ' && scale < 1.0f && a * scale < SMALL_SPACE_MIN)
        a = SMALL_SPACE_MIN / scale;
    if (next) {
        if (!s_kernKnown[f][c][next]) {
            s_kern[f][c][next] = stbtt_GetCodepointKernAdvance(&s_font[f], Unicode(c), Unicode(next)) * s_unit[f];
            s_kernKnown[f][c][next] = true;
        }
        a += s_kern[f][c][next];
    }
    return a;
}

static float WidthF(const char *s, float scale = 1.0f) {
    const int f = FaceIndex();
    float w = 0;
    for (const U8 *p = (const U8 *)s; *p; p++)
        w += Advance(f, *p, p[1], scale);
    return w;
}

// The small size: capitals cap logical pixels tall, as the 8x8 font's are 7 a step.
static float SmallScale(S32 cap) { return (float)cap / GAME_CAP; }

S32 ScreenFont_Width(const char *s) { return (S32)ceilf(WidthF(s)); }

S32 ScreenFont_InkWidth(const char *s) {
    size_t n = strlen(s);
    if (!n)
        return 0;
    // The last letter's ink ends at its box, not its advance.
    const int f = FaceIndex();
    int x0, y0, x1, y1;
    U8 last = (U8)s[n - 1];
    float w = WidthF(s) - Advance(f, last, 0);
    if (stbtt_GetCodepointBox(&s_font[f], Unicode(last), &x0, &y0, &x1, &y1))
        w += x1 * s_unit[f];
    return (S32)ceilf(w);
}

S32 ScreenFont_Advance(U8 c) { return (S32)floorf(Advance(FaceIndex(), c, 0) + 0.5f); }

// ---------------------------------------------------------------------------
// The queue.

enum { MAX_ITEMS = 512, MAX_LEN = 160 };

struct Item {
    S32 x, y;
    U8 ink;
    bool filled;    // coloured by the fill (ScreenFont_SetFill), not the ink
    float rim;      // a filled item's rim, logical pixels
    float scale;    // 1, or the small size's (capitals scale x 22 tall)
    S32 baseline;   // below y
    S32 cx0, cy0, cx1, cy1;  // the clip when queued, logical, inclusive
    S32 bx0, by0, bx1, by1;  // the canvas box checked for overdraw
    U32 sum;
    char s[MAX_LEN];
};

static Item s_items[MAX_ITEMS];
static int s_count;

void ScreenFont_BeginFrame() { s_count = 0; }

static U32 CanvasSum(S32 x0, S32 y0, S32 x1, S32 y1) {
    U32 h = 2166136261u;
    for (S32 y = y0; y <= y1; y++) {
        const U8 *row = (const U8 *)Log + TabOffLine[y];
        for (S32 x = x0; x <= x1; x++)
            h = (h ^ row[x]) * 16777619u;
    }
    return h;
}

static void Queue(S32 x, S32 y, const char *s, U8 ink, S32 width, float scale = 1.0f,
                  S32 baseline = GAME_BASELINE, S32 cellH = CELL_H) {
    if (s_count >= MAX_ITEMS || !*s)
        return;
    Item &it = s_items[s_count];
    it.x = x;
    it.y = y;
    it.ink = ink;
    it.filled = false;
    it.scale = scale;
    it.baseline = baseline;
    it.cx0 = ClipXMin;
    it.cy0 = ClipYMin;
    it.cx1 = ClipXMax;
    it.cy1 = ClipYMax;
    strncpy(it.s, s, MAX_LEN - 1);
    it.s[MAX_LEN - 1] = 0;
    it.bx0 = x > it.cx0 ? x : it.cx0;
    it.by0 = y > it.cy0 ? y : it.cy0;
    it.bx1 = x + width < it.cx1 ? x + width : it.cx1;
    it.by1 = y + cellH < it.cy1 ? y + cellH : it.cy1;
    if (it.bx0 > it.bx1 || it.by0 > it.by1)
        return;  // clipped away
    // Only Paint reads the sum, and with pixels off (the self test) nothing is painted.
    it.sum = Host_Pixels() ? CanvasSum(it.bx0, it.by0, it.bx1, it.by1) : 0;
    s_count++;
}

void ScreenFont_Text(S32 x, S32 y, const char *s, U8 ink) { Queue(x, y, s, ink, ScreenFont_Width(s)); }

S32 ScreenFont_SmallWidth(const char *s, S32 cap) { return (S32)ceilf(WidthF(s, SmallScale(cap)) * SmallScale(cap)); }

void ScreenFont_SmallText(S32 x, S32 y, const char *s, U8 ink, S32 cap) {
    // Descenders reach about a third of the capitals' height below the baseline.
    Queue(x, y, s, ink, ScreenFont_SmallWidth(s, cap), SmallScale(cap), cap, cap + cap / 2);
}

static ScreenFontLevel s_fillLevel;
static U8 s_fillBank, s_fillLo, s_fillHi, s_fillRim;

void ScreenFont_SetFill(ScreenFontLevel level, U8 bank, U8 lo, U8 hi, U8 rim) {
    s_fillLevel = level;
    s_fillBank = bank;
    s_fillLo = lo;
    s_fillHi = hi;
    s_fillRim = rim;
}

void ScreenFont_SmallTextFilled(S32 x, S32 y, const char *s, U8 ink, S32 cap, float rim) {
    const int before = s_count;
    ScreenFont_SmallText(x, y, s, ink, cap);
    if (s_count > before && s_fillLevel)
        s_items[before].filled = true, s_items[before].rim = rim;
}

S32 ScreenFont_Char(S32 x, S32 y, U8 c, U8 ink) {
    S32 a = ScreenFont_Advance(c);
    if (c != ' ') {
        char s[2] = {(char)c, 0};
        Queue(x, y, s, ink, a);
    }
    return a;
}

// ---------------------------------------------------------------------------
// Painting.

// Glyphs rasterised at the current screen scale, by size (a few sizes kept) and CP850 code.
struct Glyph {
    unsigned char *bmp;
    int w, h, xo, yo;
    bool done;
};
enum { GLYPH_SIZES = 4 };
enum { SUBPIXELS = 4 };  // letters placed at quarters of a screen pixel (switch zsub)
static Glyph s_glyphs[GLYPH_SIZES][SUBPIXELS][256];
static int s_subpixel;
void ScreenFont_SetSubpixel(bool on) { s_subpixel = on; }
static float s_glyphScale[GLYPH_SIZES];
static float s_glyphK;
static int s_glyphPercent;

static void ForgetSize(int i) {
    for (int q = 0; q < SUBPIXELS; q++)
        for (int j = 0; j < 256; j++) {
            if (s_glyphs[i][q][j].bmp)
                stbtt_FreeBitmap(s_glyphs[i][q][j].bmp, NULL);
            memset(&s_glyphs[i][q][j], 0, sizeof(Glyph));
        }
    s_glyphScale[i] = 0;
}

static const Glyph &GetGlyph(int f, U8 c, float k, float scale, int q = 0) {
    if (k != s_glyphK || s_sizePercent != s_glyphPercent) {
        for (int i = 0; i < GLYPH_SIZES; i++)
            ForgetSize(i);
        s_glyphK = k;
        s_glyphPercent = s_sizePercent;
    }
    int slot = 0;
    while (slot < GLYPH_SIZES && s_glyphScale[slot] != 0 && s_glyphScale[slot] != scale)
        slot++;
    if (slot == GLYPH_SIZES) {  // more sizes than kept: start the last one again
        slot = GLYPH_SIZES - 1;
        ForgetSize(slot);
    }
    s_glyphScale[slot] = scale;
    Glyph &g = s_glyphs[slot][q][c];
    if (!g.done) {
        const float px = k * s_unit[f] * scale;
        g.bmp = stbtt_GetCodepointBitmapSubpixel(&s_font[f], px, px, (float)q / SUBPIXELS, 0.0f, Unicode(c), &g.w, &g.h,
                                                 &g.xo, &g.yo);
        g.done = true;
    }
    return g;
}

// A filled letter's colour at glyph pixel (gx, gy), logical (lx, ly): the fill's level through
// the bank's ramp, blended between neighbouring colours; with a rim, its ink where the glyph's
// coverage falls off within rimPx (the glyph less its erosion by that many pixels).
static void FillColour(const U8 *palette, const Glyph &g, int gx, int gy, int rimPx, float lx, float ly, U32 *r,
                       U32 *gr, U32 *b) {
    float l = s_fillLevel(lx, ly);
    l = l < 0 ? 0 : l > 10 ? 10 : l;
    l = s_fillLo + l * (s_fillHi - s_fillLo) / 10.0f;
    const int l0 = (int)l, l1 = l0 < s_fillHi ? l0 + 1 : l0;
    const float t = l - l0;
    const U8 *c0 = palette + (s_fillBank * 16 + l0) * 3, *c1 = palette + (s_fillBank * 16 + l1) * 3;
    float fr = c0[0] + (c1[0] - c0[0]) * t, fg = c0[1] + (c1[1] - c0[1]) * t, fb = c0[2] + (c1[2] - c0[2]) * t;
    if (s_fillRim && rimPx > 0) {
        U32 inner = 255;
        for (int dy = -rimPx; dy <= rimPx && inner; dy++)
            for (int dx = -rimPx; dx <= rimPx; dx++) {
                const int x = gx + dx, y = gy + dy;
                const U32 a = (x < 0 || y < 0 || x >= g.w || y >= g.h) ? 0 : g.bmp[y * g.w + x];
                if (a < inner)
                    inner = a;
            }
        const float rim = 1.0f - inner / 255.0f;
        const U8 *c = palette + s_fillRim * 3;
        fr += (c[0] - fr) * rim;
        fg += (c[1] - fg) * rim;
        fb += (c[2] - fb) * rim;
    }
    *r = (U32)(fr + 0.5f);
    *gr = (U32)(fg + 0.5f);
    *b = (U32)(fb + 0.5f);
}

void ScreenFont_Paint(U32 *argb, U32 pitch, U32 w, U32 h, const U8 *palette) {
    if (!ScreenFont_On() || !s_count)
        return;
    float k, bxf, byf;
    PropScale_Map(w, h, &k, &bxf, &byf);
    const S32 bx = (S32)bxf, by = (S32)byf;
    const int f = FaceIndex();
    for (int i = 0; i < s_count; i++) {
        const Item &it = s_items[i];
        if (CanvasSum(it.bx0, it.by0, it.bx1, it.by1) != it.sum)
            continue;  // drawn over later in the frame
        // The clip on the screen.
        const S32 sx0 = bx + (S32)floorf(it.cx0 * k), sy0 = by + (S32)floorf(it.cy0 * k);
        const S32 sx1 = bx + (S32)floorf((it.cx1 + 1) * k), sy1 = by + (S32)floorf((it.cy1 + 1) * k);
        const U8 *rgb = palette + it.ink * 3;
        const U32 cr = rgb[0], cg = rgb[1], cb = rgb[2];
        const int rimPx = it.filled ? (int)floorf(k * it.rim + 0.5f) : 0;
        const S32 base = by + (S32)floorf((it.y + it.baseline) * k);
        float pen = (float)bx + it.x * k;
        for (const U8 *p = (const U8 *)it.s; *p; p++) {
            // Whole pixels as before, or the nearest quarter with zsub on.
            const float at = s_subpixel ? floorf(pen * SUBPIXELS + 0.5f) / SUBPIXELS : floorf(pen + 0.5f);
            const int q = (int)((at - floorf(at)) * SUBPIXELS + 0.5f) % SUBPIXELS;
            const Glyph &g = GetGlyph(f, *p, k, it.scale, q);
            const S32 gx = (S32)floorf(at) + g.xo, gy = base + g.yo;
            for (int j = 0; j < g.h; j++) {
                const S32 y = gy + j;
                if (y < sy0 || y >= sy1 || y < 0 || y >= (S32)h)
                    continue;
                U32 *dst = (U32 *)((U8 *)argb + (size_t)y * pitch);
                const unsigned char *src = g.bmp + j * g.w;
                for (int i2 = 0; i2 < g.w; i2++) {
                    const S32 x = gx + i2;
                    const U32 a = src[i2];
                    if (!a || x < sx0 || x >= sx1 || x < 0 || x >= (S32)w)
                        continue;
                    U32 pr = cr, pg = cg, pb = cb;
                    if (it.filled && s_fillLevel)
                        FillColour(palette, g, i2, j, rimPx, ((float)x + 0.5f - bxf) / k, ((float)y + 0.5f - byf) / k,
                                   &pr, &pg, &pb);
                    const U32 ia = 255 - a, d = dst[x];
                    const U32 r = (pr * a + ((d >> 16) & 0xff) * ia) / 255;
                    const U32 gg = (pg * a + ((d >> 8) & 0xff) * ia) / 255;
                    const U32 b = (pb * a + (d & 0xff) * ia) / 255;
                    dst[x] = 0xff000000u | r << 16 | gg << 8 | b;
                }
            }
            pen += Advance(f, *p, p[1], it.scale) * k * it.scale;
        }
    }
}
