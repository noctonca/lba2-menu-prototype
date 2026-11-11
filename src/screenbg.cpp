#include "screenbg.h"

#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int s_mode;

void ScreenBg_SetMode(int mode) { s_mode = mode; }
int ScreenBg_Mode() { return s_mode; }

// The canvas as the picture left it, and per canvas pixel the darkening it has
// had since: a chain of fog rows, 0 for none, NONE when there were too many.
static U8 *s_canvas, *s_mask;
static U32 s_lw, s_lh, s_cap;
static bool s_have;  // the canvas holds the picture this frame
static U8 s_fill;    // what it holds instead

enum { MAX_CHAINS = 32, MAX_DEPTH = 6, NONE = 255 };
struct Chain {
    int depth;
    U8 rows[MAX_DEPTH];
};
static Chain s_chains[MAX_CHAINS] = {{0, {0}}};
static int s_chainCount = 1;
static S16 s_next[MAX_CHAINS][256];
static bool s_nextInit;

// Index to index through a chain's fog rows, for the clut in use.
static U8 s_map[MAX_CHAINS][256];
static const U8 *s_mapClut;
static int s_mapCount;

static int Extend(int chain, U32 row) {
    if (!s_nextInit) {
        memset(s_next, 0xff, sizeof s_next);
        s_nextInit = true;
    }
    if (chain == NONE)
        return NONE;
    S16 &n = s_next[chain][row & 255];
    if (n >= 0)
        return n;
    const Chain &c = s_chains[chain];
    if (c.depth == MAX_DEPTH || s_chainCount == MAX_CHAINS)
        return NONE;
    Chain &d = s_chains[s_chainCount];
    d = c;
    d.rows[d.depth++] = (U8)row;
    n = (S16)s_chainCount++;
    return n;
}

static void UpdateMaps() {
    if (s_mapClut == PtrCLUTGouraud && s_mapCount == s_chainCount)
        return;
    for (int c = 0; c < s_chainCount; c++) {
        for (int i = 0; i < 256; i++) {
            U8 v = (U8)i;
            for (int r = 0; r < s_chains[c].depth; r++)
                v = PtrCLUTGouraud[(U32)s_chains[c].rows[r] * 256 + v];
            s_map[c][i] = v;
        }
    }
    s_mapClut = PtrCLUTGouraud;
    s_mapCount = s_chainCount;
}

void ScreenBg_BeginFrame(U32 lw, U32 lh) {
    if (!s_mode)
        return;
    if (lw * lh > s_cap) {
        s_cap = lw * lh;
        s_canvas = (U8 *)realloc(s_canvas, s_cap);
        s_mask = (U8 *)realloc(s_mask, s_cap);
    }
    s_lw = lw;
    s_lh = lh;
    s_have = true;
    for (U32 y = 0; y < lh; y++)
        memcpy(s_canvas + y * lw, (const U8 *)Log + TabOffLine[y], lw);
    memset(s_mask, 0, lw * lh);
}

void ScreenBg_NoPicture(U8 fill) {
    s_have = false;
    s_fill = fill;
}

void ScreenBg_NoteShade(S32 x0, S32 y0, S32 x1, S32 y1, U32 row) {
    if (!s_mode || !s_have || !s_mask)
        return;
    for (S32 y = y0; y <= y1; y++) {
        U8 *m = s_mask + (U32)y * s_lw;
        for (S32 x = x0; x <= x1; x++)
            m[x] = (U8)Extend(m[x], row);
    }
}

// Per screen pixel: the chain its picture pixel went through, NONE where the menu drew.
static U8 *s_chainAt;
static U32 s_chainAtCap;

void ScreenBg_Compose(U8 *out, U32 pitch, U32 pw, U32 ph, U32 lw, U32 lh, U32 k, const U8 *picture) {
    if (pw * ph > s_chainAtCap) {
        s_chainAtCap = pw * ph;
        s_chainAt = (U8 *)realloc(s_chainAt, s_chainAtCap);
    }
    UpdateMaps();
    const bool on = s_mode && s_have && s_canvas && s_lw == lw && s_lh == lh;
    const U8 outside = s_mode && !s_have ? s_fill : 0;
    // The picture at the smallest whole factor that covers the screen, centred.
    U32 n = (pw + 639) / 640, ny = (ph + 479) / 480;
    if (ny > n)
        n = ny;
    const U32 px0 = (640 * n - pw) / 2, py0 = (480 * n - ph) / 2;
    // The canvas at its whole step, centred.
    const U32 bx = (pw - k * lw) / 2, by = (ph - k * lh) / 2;
    static U32 *colPic, *colLog, colCap;
    if (pw > colCap) {
        colCap = pw;
        colPic = (U32 *)realloc(colPic, colCap * sizeof *colPic);
        colLog = (U32 *)realloc(colLog, colCap * sizeof *colLog);
    }
    for (U32 x = 0; x < pw; x++) {
        colPic[x] = (x + px0) / n;
        colLog[x] = (x >= bx && x - bx < k * lw) ? (x - bx) / k : 0xffffffffu;
    }
    for (U32 y = 0; y < ph; y++) {
        U8 *o = out + (size_t)y * pitch;
        U8 *ca = s_chainAt + (size_t)y * pw;
        const U8 *pic = picture + ((y + py0) / n) * 640;
        const bool rowIn = y >= by && y - by < k * lh;
        const U32 ly = rowIn ? (y - by) / k : 0;
        const U8 *logRow = (const U8 *)Log + TabOffLine[ly];
        const U8 *canRow = on ? s_canvas + ly * lw : NULL;
        const U8 *maskRow = on ? s_mask + ly * lw : NULL;
        for (U32 x = 0; x < pw; x++) {
            const U32 lx = colLog[x];
            if (!rowIn || lx == 0xffffffffu) {  // outside the canvas: the picture, the fill, or black when off
                o[x] = on ? pic[colPic[x]] : outside;
                ca[x] = on ? 0 : NONE;
                continue;
            }
            const U8 v = logRow[lx];
            if (on) {
                const U8 m = maskRow[lx];
                if (m != NONE && v == s_map[m][canRow[lx]]) {
                    o[x] = s_map[m][pic[colPic[x]]];
                    ca[x] = m;
                    continue;
                }
            }
            o[x] = v;
            ca[x] = NONE;
        }
    }
}

// Mode 2: the picture through each chain, smoothly scaled to cover the screen.
struct Smooth {
    U32 *argb;
    U32 w, h;
    U8 palette[768];
    const U8 *clut;
};
static Smooth s_smooth[MAX_CHAINS];

static float CatmullRom(float t, int i) {
    // Weights for taps -1, 0, 1, 2 at fraction t.
    const float t2 = t * t, t3 = t2 * t;
    switch (i) {
    case 0: return -0.5f * t3 + t2 - 0.5f * t;
    case 1: return 1.5f * t3 - 2.5f * t2 + 1.0f;
    case 2: return -1.5f * t3 + 2.0f * t2 + 0.5f * t;
    default: return 0.5f * t3 - 0.5f * t2;
    }
}

static void BuildSmooth(Smooth &s, int chain, U32 pw, U32 ph, const U8 *palette, const U8 *picture) {
    const float f = fmaxf((float)pw / 640.0f, (float)ph / 480.0f);
    const float ox = (640.0f * f - (float)pw) / 2.0f, oy = (480.0f * f - (float)ph) / 2.0f;
    float *src = (float *)malloc(sizeof(float) * 3 * 640 * 480);
    for (U32 i = 0; i < 640 * 480; i++) {
        const U8 *rgb = palette + s_map[chain][picture[i]] * 3;
        src[i * 3] = rgb[0];
        src[i * 3 + 1] = rgb[1];
        src[i * 3 + 2] = rgb[2];
    }
    // Across, then down.
    float *mid = (float *)malloc(sizeof(float) * 3 * pw * 480);
    for (U32 x = 0; x < pw; x++) {
        const float sx = ((float)x + ox + 0.5f) / f - 0.5f;
        const int x0 = (int)floorf(sx);
        const float t = sx - (float)x0;
        float w[4];
        int xi[4];
        for (int i = 0; i < 4; i++) {
            w[i] = CatmullRom(t, i);
            int c = x0 - 1 + i;
            xi[i] = c < 0 ? 0 : c > 639 ? 639 : c;
        }
        for (U32 y = 0; y < 480; y++)
            for (int ch = 0; ch < 3; ch++) {
                float a = 0;
                for (int i = 0; i < 4; i++)
                    a += w[i] * src[(y * 640 + (U32)xi[i]) * 3 + ch];
                mid[(y * pw + x) * 3 + ch] = a;
            }
    }
    free(src);
    s.argb = (U32 *)realloc(s.argb, sizeof(U32) * pw * ph);
    for (U32 y = 0; y < ph; y++) {
        const float sy = ((float)y + oy + 0.5f) / f - 0.5f;
        const int y0 = (int)floorf(sy);
        const float t = sy - (float)y0;
        float w[4];
        U32 yi[4];
        for (int i = 0; i < 4; i++) {
            w[i] = CatmullRom(t, i);
            int c = y0 - 1 + i;
            yi[i] = (U32)(c < 0 ? 0 : c > 479 ? 479 : c);
        }
        for (U32 x = 0; x < pw; x++) {
            U32 px = 0xff000000u;
            for (int ch = 0; ch < 3; ch++) {
                float a = 0;
                for (int i = 0; i < 4; i++)
                    a += w[i] * mid[(yi[i] * pw + x) * 3 + ch];
                int v = (int)lrintf(a);
                v = v < 0 ? 0 : v > 255 ? 255 : v;
                px |= (U32)v << (16 - 8 * ch);
            }
            s.argb[y * pw + x] = px;
        }
    }
    free(mid);
    s.w = pw;
    s.h = ph;
    memcpy(s.palette, palette, 768);
    s.clut = PtrCLUTGouraud;
}

void ScreenBg_PaintSmooth(U32 *argb, U32 pitchBytes, U32 pw, U32 ph, const U8 *palette, const U8 *picture) {
    if (s_mode != 2 || !s_chainAt || pw * ph > s_chainAtCap)
        return;
    UpdateMaps();
    bool checked[MAX_CHAINS] = {false};
    for (U32 y = 0; y < ph; y++) {
        U32 *o = (U32 *)((U8 *)argb + (size_t)y * pitchBytes);
        const U8 *ca = s_chainAt + (size_t)y * pw;
        for (U32 x = 0; x < pw; x++) {
            const U8 c = ca[x];
            if (c == NONE)
                continue;
            Smooth &s = s_smooth[c];
            if (!checked[c]) {
                if (!s.argb || s.w != pw || s.h != ph || s.clut != PtrCLUTGouraud || memcmp(s.palette, palette, 768))
                    BuildSmooth(s, c, pw, ph, palette, picture);
                checked[c] = true;
            }
            o[x] = s.argb[y * pw + x];
        }
    }
}
