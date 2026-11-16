#include "prop_scale.h"

#include "host.h"
#include "screen_picture.h"

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int s_mode;

void PropScale_Set(int mode) { s_mode = mode; }
int PropScale_Mode() { return s_mode; }

float PropScale_For(U32 w, U32 h, bool wide) {
    const float bw = wide ? 960.0f : 640.0f, bh = wide ? 540.0f : 480.0f;
    const float sx = (float)w / bw, sy = (float)h / bh;
    const float s = sx < sy ? sx : sy;
    return s < 1.0f ? 1.0f : s;
}

void PropScale_Map(U32 w, U32 h, float *s, float *bx, float *by) {
    const U32 lw = Host_LogicalW(), lh = Host_LogicalH();
    if (!s_mode) {
        U32 k = w / lw < h / lh ? w / lw : h / lh;
        if (k < 1)
            k = 1;
        *s = (float)k;
        *bx = (float)((w - k * lw) / 2);
        *by = (float)((h - k * lh) / 2);
        return;
    }
    const float sx = (float)w / (float)lw, sy = (float)h / (float)lh;
    *s = sx < sy ? sx : sy;
    *bx = floorf(((float)w - *s * (float)lw) / 2.0f);
    *by = floorf(((float)h - *s * (float)lh) / 2.0f);
}

// The scale is a whole step (1920x1080, 3840x2160, 640x480): drawn as with whole steps only.
static bool Whole() {
    const U32 lw = Host_LogicalW(), lh = Host_LogicalH(), pw = Host_PhysW(), ph = Host_PhysH();
    return lw && lh && pw % lw == 0 && ph % lh == 0 && pw / lw == ph / lh;
}

// Inside a capture: what the draw calls paints into the scratch, not into a capture of its own.
static bool s_capturing;

bool PropScale_TextOnTop() { return s_mode >= 2 && Host_Pixels() && !Whole() && !s_capturing; }

bool PropScale_CanFit() { return s_mode >= 5 && Host_Pixels() && !s_capturing; }

// ---------------------------------------------------------------------------
// The text layer: per string its ink (palette index per pixel, 0xffff where none),
// its box on the canvas, the clip and a checksum of the canvas under it when drawn,
// so a string something drew over later in the frame is dropped (as the screen font).

struct Item {
    S32 ax, ay;              // the text's start, the anchor in modes 3 and 4
    S32 x0, y0, x1, y1;      // its box, canvas, inclusive
    S32 cx0, cy0, cx1, cy1;  // the clip, canvas, inclusive
    U32 sum;
    float k;                 // painted k times its size from (x0, y0) (PropScale_TextAt), else 1
    size_t at;               // into s_ink
};

enum { MAX_ITEMS = 4096 };
static Item s_items[MAX_ITEMS];
static int s_count;
static U16 *s_ink;
static size_t s_inkUsed, s_inkCap;
static U8 *s_scratch[2];
static size_t s_scratchCap;

void PropScale_BeginFrame() {
    s_count = 0;
    s_inkUsed = 0;
}

static U32 CanvasSum(S32 x0, S32 y0, S32 x1, S32 y1) {
    U32 h = 2166136261u;
    for (S32 y = y0; y <= y1; y++) {
        const U8 *row = (const U8 *)Log + TabOffLine[y];
        for (S32 x = x0; x <= x1; x++)
            h = (h ^ row[x]) * 16777619u;
    }
    return h;
}

static void Capture(S32 x, S32 y, S32 x0, S32 y0, S32 x1, S32 y1, float k, const std::function<void()> &draw);

void PropScale_Text(S32 x, S32 y, S32 x0, S32 y0, S32 x1, S32 y1, const std::function<void()> &draw) {
    Capture(x, y, x0, y0, x1, y1, 1.0f, draw);
}

void PropScale_TextAt(S32 x0, S32 y0, S32 x1, S32 y1, float k, const std::function<void()> &draw) {
    Capture(x0, y0, x0, y0, x1, y1, k, draw);
}

static void Capture(S32 x, S32 y, S32 x0, S32 y0, S32 x1, S32 y1, float k, const std::function<void()> &draw) {
    if (x0 < ClipXMin) x0 = ClipXMin;
    if (y0 < ClipYMin) y0 = ClipYMin;
    if (x1 > ClipXMax) x1 = ClipXMax;
    if (y1 > ClipYMax) y1 = ClipYMax;
    if (x0 > x1 || y0 > y1 || s_count >= MAX_ITEMS)
        return;
    const size_t bytes = (size_t)TabOffLine[Host_LogicalH() - 1] + Host_LogicalW();
    if (bytes > s_scratchCap) {
        s_scratchCap = bytes;
        for (int p = 0; p < 2; p++)
            s_scratch[p] = (U8 *)realloc(s_scratch[p], bytes);
    }
    void *const log = Log;
    s_capturing = true;
    for (int p = 0; p < 2; p++) {
        for (S32 j = y0; j <= y1; j++)
            memset(s_scratch[p] + TabOffLine[j] + x0, p ? 0xff : 0x00, (size_t)(x1 - x0 + 1));
        Log = s_scratch[p];
        draw();
    }
    s_capturing = false;
    Log = log;
    const size_t n = (size_t)(x1 - x0 + 1) * (size_t)(y1 - y0 + 1);
    if (s_inkUsed + n > s_inkCap) {
        s_inkCap = (s_inkUsed + n) * 2;
        s_ink = (U16 *)realloc(s_ink, s_inkCap * sizeof *s_ink);
    }
    Item &it = s_items[s_count++];
    it.ax = x, it.ay = y;
    it.x0 = x0, it.y0 = y0, it.x1 = x1, it.y1 = y1;
    it.cx0 = ClipXMin, it.cy0 = ClipYMin, it.cx1 = ClipXMax, it.cy1 = ClipYMax;
    it.k = k;
    it.at = s_inkUsed;
    U16 *o = s_ink + s_inkUsed;
    for (S32 j = y0; j <= y1; j++)
        for (S32 i = x0; i <= x1; i++) {
            const U8 a = s_scratch[0][TabOffLine[j] + i], b = s_scratch[1][TabOffLine[j] + i];
            *o++ = a == b ? a : 0xffff;
        }
    s_inkUsed += n;
    it.sum = CanvasSum(x0, y0, x1, y1);
}

// ---------------------------------------------------------------------------
// Painting.

void PropScale_PaintLinear(U32 *argb, U32 pitchBytes, U32 w, U32 h, const U8 *palette) {
    if (!s_mode || Whole())
        return;
    float s, bx, by;
    PropScale_Map(w, h, &s, &bx, &by);
    const U32 lw = Host_LogicalW(), lh = Host_LogicalH();
    const U8 *chain = ScreenPicture_ChainAt(w, h);
    for (U32 y = 0; y < h; y++) {
        const float v = ((float)y + 0.5f - by) / s - 0.5f;
        if (v < -0.5f || v > (float)lh - 0.5f)
            continue;
        S32 j0 = (S32)floorf(v);
        const float fv = v - (float)j0;
        S32 j1 = j0 + 1;
        j0 = j0 < 0 ? 0 : j0;
        j1 = j1 >= (S32)lh ? (S32)lh - 1 : j1;
        const U8 *r0 = (const U8 *)Log + TabOffLine[j0], *r1 = (const U8 *)Log + TabOffLine[j1];
        U32 *o = (U32 *)((U8 *)argb + (size_t)y * pitchBytes);
        const U8 *ca = chain ? chain + (size_t)y * w : NULL;
        for (U32 x = 0; x < w; x++) {
            if (ca && ca[x] != SCREEN_PICTURE_NONE)
                continue;  // the picture, at the screen's resolution already
            const float u = ((float)x + 0.5f - bx) / s - 0.5f;
            if (u < -0.5f || u > (float)lw - 0.5f)
                continue;
            S32 i0 = (S32)floorf(u);
            const float fu = u - (float)i0;
            S32 i1 = i0 + 1;
            i0 = i0 < 0 ? 0 : i0;
            i1 = i1 >= (S32)lw ? (S32)lw - 1 : i1;
            const U8 *c00 = palette + r0[i0] * 3, *c10 = palette + r0[i1] * 3;
            const U8 *c01 = palette + r1[i0] * 3, *c11 = palette + r1[i1] * 3;
            U32 px = 0xff000000u;
            for (int ch = 0; ch < 3; ch++) {
                const float top = c00[ch] + (c10[ch] - c00[ch]) * fu;
                const float bot = c01[ch] + (c11[ch] - c01[ch]) * fu;
                const int val = (int)lrintf(top + (bot - top) * fv);
                px |= (U32)(val < 0 ? 0 : val > 255 ? 255 : val) << (16 - 8 * ch);
            }
            o[x] = px;
        }
    }
}

static inline void Fill(U32 *argb, U32 pitchBytes, S32 x0, S32 y0, S32 x1, S32 y1, U32 c) {
    for (S32 y = y0; y < y1; y++) {
        U32 *row = (U32 *)((U8 *)argb + (size_t)y * pitchBytes);
        for (S32 x = x0; x < x1; x++)
            row[x] = c;
    }
}

// Scale2x (EPX): each pixel into four, a corner taking a neighbour's value where two
// neighbours agree and the others do not, so diagonals and curves step by half a pixel.
static void Scale2x(const U16 *in, S32 w, S32 h, U16 *out) {
    auto at = [&](S32 x, S32 y) -> U16 { return x < 0 || y < 0 || x >= w || y >= h ? 0xffff : in[y * w + x]; };
    for (S32 y = 0; y < h; y++)
        for (S32 x = 0; x < w; x++) {
            const U16 p = at(x, y), a = at(x, y - 1), b = at(x + 1, y), c = at(x - 1, y), d = at(x, y + 1);
            U16 *o0 = out + (2 * y) * (2 * w) + 2 * x, *o1 = o0 + 2 * w;
            o0[0] = (c == a && c != d && a != b) ? a : p;
            o0[1] = (a == b && a != c && b != d) ? b : p;
            o1[0] = (d == c && d != b && c != a) ? c : p;
            o1[1] = (b == d && b != a && d != c) ? d : p;
        }
}

// Modes 5 and 6: every texel of the grid (the ink, or its Scale2x at f 2) drawn as its exact
// square on the screen; a screen pixel takes the share of each texel it covers, so strokes
// keep one weight and their edges are one pixel soft.
static void PaintCoverage(const Item &it, const U16 *ink, U32 *argb, U32 pitchBytes, S32 kx0, S32 ky0, S32 kx1,
                          S32 ky1, float s, float bx, float by, const U8 *palette) {
    static U16 *big;
    static size_t bigCap;
    const S32 bw = it.x1 - it.x0 + 1, bh = it.y1 - it.y0 + 1;
    const int f = s_mode == 6 ? 2 : 1;
    const U16 *grid = ink;
    if (f == 2) {
        const size_t n = (size_t)bw * bh * 4;
        if (n > bigCap) {
            bigCap = n;
            big = (U16 *)realloc(big, n * sizeof *big);
        }
        Scale2x(ink, bw, bh, big);
        grid = big;
    }
    const S32 gw = bw * f, gh = bh * f;
    const float t = s * it.k / (float)f;  // screen pixels a texel
    const float ox = bx + (float)it.x0 * s, oy = by + (float)it.y0 * s;
    S32 X0 = (S32)floorf(ox), Y0 = (S32)floorf(oy);
    S32 X1 = (S32)ceilf(ox + (float)gw * t), Y1 = (S32)ceilf(oy + (float)gh * t);
    X0 = X0 < kx0 ? kx0 : X0, Y0 = Y0 < ky0 ? ky0 : Y0;
    X1 = X1 > kx1 ? kx1 : X1, Y1 = Y1 > ky1 ? ky1 : Y1;
    const float area = t * t;
    for (S32 Y = Y0; Y < Y1; Y++) {
        const float g0 = ((float)Y - oy) / t, g1 = ((float)Y + 1 - oy) / t;
        S32 j0 = (S32)floorf(g0), j1 = (S32)ceilf(g1);
        j0 = j0 < 0 ? 0 : j0, j1 = j1 > gh ? gh : j1;
        U32 *row = (U32 *)((U8 *)argb + (size_t)Y * pitchBytes);
        for (S32 X = X0; X < X1; X++) {
            const float h0 = ((float)X - ox) / t, h1 = ((float)X + 1 - ox) / t;
            S32 i0 = (S32)floorf(h0), i1 = (S32)ceilf(h1);
            i0 = i0 < 0 ? 0 : i0, i1 = i1 > gw ? gw : i1;
            float a = 0, r = 0, g = 0, b = 0;
            for (S32 j = j0; j < j1; j++) {
                const float lo = g0 > (float)j ? g0 : (float)j, hi = g1 < (float)(j + 1) ? g1 : (float)(j + 1);
                const float wy = hi - lo;
                if (wy <= 0)
                    continue;
                for (S32 i = i0; i < i1; i++) {
                    const U16 v = grid[j * gw + i];
                    if (v == 0xffff)
                        continue;
                    const float l2 = h0 > (float)i ? h0 : (float)i, r2 = h1 < (float)(i + 1) ? h1 : (float)(i + 1);
                    const float wgt = (r2 - l2) * wy;
                    if (wgt <= 0)
                        continue;
                    const U8 *c = palette + v * 3;
                    a += wgt, r += wgt * c[0], g += wgt * c[1], b += wgt * c[2];
                }
            }
            if (a <= 0)
                continue;
            float al = a * area;  // the share of this screen pixel the ink covers
            al = al > 1.0f ? 1.0f : al;
            const U32 d = row[X];
            const float cr = r / a, cg = g / a, cb = b / a;
            const int nr = (int)lrintf(cr * al + (float)((d >> 16) & 0xff) * (1 - al));
            const int ng = (int)lrintf(cg * al + (float)((d >> 8) & 0xff) * (1 - al));
            const int nb = (int)lrintf(cb * al + (float)(d & 0xff) * (1 - al));
            row[X] = 0xff000000u | (U32)nr << 16 | (U32)ng << 8 | (U32)nb;
        }
    }
}

void PropScale_PaintText(U32 *argb, U32 pitchBytes, U32 w, U32 h, const U8 *palette) {
    if (s_mode < 2 || !s_count)  // a fitted legend needs 5 or 6 (PropScale_CanFit)
        return;
    float s, bx, by;
    PropScale_Map(w, h, &s, &bx, &by);
    const S32 m = s_mode == 3 ? (S32)floorf(s + 1e-4f) : (S32)ceilf(s - 1e-4f);
    auto sx = [&](S32 x) { return (S32)floorf(bx + (float)x * s); };
    auto sy = [&](S32 y) { return (S32)floorf(by + (float)y * s); };
    for (int n = 0; n < s_count; n++) {
        const Item &it = s_items[n];
        if (CanvasSum(it.x0, it.y0, it.x1, it.y1) != it.sum)
            continue;  // drawn over later in the frame
        // The clip on the screen, and the frame.
        S32 kx0 = sx(it.cx0), ky0 = sy(it.cy0), kx1 = sx(it.cx1 + 1), ky1 = sy(it.cy1 + 1);
        kx0 = kx0 < 0 ? 0 : kx0, ky0 = ky0 < 0 ? 0 : ky0;
        kx1 = kx1 > (S32)w ? (S32)w : kx1, ky1 = ky1 > (S32)h ? (S32)h : ky1;
        const U16 *ink = s_ink + it.at;
        if (s_mode >= 5 || it.k != 1.0f) {
            PaintCoverage(it, ink, argb, pitchBytes, kx0, ky0, kx1, ky1, s, bx, by, palette);
            continue;
        }
        const S32 bw = it.x1 - it.x0 + 1;
        const S32 ox = sx(it.ax), oy = sy(it.ay);
        for (S32 j = it.y0; j <= it.y1; j++)
            for (S32 i = it.x0; i <= it.x1; i++) {
                const U16 v = ink[(j - it.y0) * bw + (i - it.x0)];
                if (v == 0xffff)
                    continue;
                const U8 *rgb = palette + v * 3;
                const U32 c = 0xff000000u | (U32)rgb[0] << 16 | (U32)rgb[1] << 8 | rgb[2];
                S32 x0, y0, x1, y1;
                if (s_mode == 2) {
                    x0 = sx(i), x1 = sx(i + 1), y0 = sy(j), y1 = sy(j + 1);
                } else {
                    x0 = ox + (i - it.ax) * m, x1 = x0 + m;
                    y0 = oy + (j - it.ay) * m, y1 = y0 + m;
                }
                x0 = x0 < kx0 ? kx0 : x0, y0 = y0 < ky0 ? ky0 : y0;
                x1 = x1 > kx1 ? kx1 : x1, y1 = y1 > ky1 ? ky1 : y1;
                if (x0 < x1 && y0 < y1)
                    Fill(argb, pitchBytes, x0, y0, x1, y1, c);
            }
    }
}
