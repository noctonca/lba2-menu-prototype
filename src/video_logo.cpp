#include "video_logo.h"

#include "host.h"
#include "prop_scale.h"

#include <SVGA/SCREEN.H>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <vector>

extern "C" const unsigned char g_lba2LogoSvg[];
extern "C" const U32 g_lba2LogoSvgSize;
extern "C" const unsigned char g_lba2CcSvg[];
extern "C" const U32 g_lba2CcSvgSize;

// ───────────────────────────── What was measured ─────────────────────────────
//
// Everything below is in the logo's units: one unit is one pixel of the 320x200 video, the
// origin the top left of the settled logo's ink, which sits at (ORIGIN_X, ORIGIN_Y) of the
// video frame. Times are in video frames (15 a second) counted from frame 144, the first
// frame of the build-in.

static const float ORIGIN_X = 33.67f, ORIGIN_Y = 23.08f;
static const float FPS = 15.0f;

// The fill, from the interior of the settled frames (median of 204 to 214, more than 3.5
// pixels inside the edge). The video's palette gives it in flat bands; these are the bands'
// centres along x (the ramp has no vertical term: under 0.04 a pixel in every channel).
struct Stop {
    float x, r, g, b;
};
static const Stop kRamp[] = {
    {-3.67f, 251, 251, 170}, {8.33f, 251, 251, 166},  {24.33f, 251, 247, 138}, {66.33f, 251, 251, 117},
    {101.33f, 251, 251, 117}, {125.33f, 251, 219, 105}, {144.33f, 251, 195, 101}, {163.33f, 251, 174, 101},
    {183.33f, 251, 158, 105}, {209.33f, 243, 125, 97},  {238.33f, 235, 89, 65},   {256.33f, 232, 80, 58},
};

// The bevel: near the edge the fill is lit from the upper left, pale on edges facing left
// and up, red on edges facing down and right. Fitted to the settled frames between 1 and
// 4.5 pixels inside the edge. With a the direction the edge faces (0 right, 90 degrees up)
// and w(a) = k0 + k1 cos a + k2 sin a + k3 cos 2a + k4 sin 2a, times a weight that is 1 up
// to BEVEL_FULL inside the edge and nothing from BEVEL_END: blue goes w of the way to
// BEVEL_PALE, green is scaled by 1 + w (never over the ramp's 251), red stays. Blue's
// error there falls from 46 to 17 of 255, green's from 23 to 19 (the rest is mostly the
// video palette's bands).
static const float BEVEL_FULL = 1.2f, BEVEL_END = 3.3f, BEVEL_PALE = 225.0f, GREEN_TOP = 251.0f;
static const float kBevelBlue[5] = {0.397f, -0.367f, 0.250f, 0.257f, 0.097f};
static const float kBevelGreen[5] = {0.096f, -0.123f, 0.169f, 0.040f, 0.177f};

// The fade (frames 144 to 172, the fill's median over the settled colour in three bands
// across the logo): red, the fill's brightest channel, climbs evenly from nothing at frame
// 144 to full at 160.5, which is the opacity. Green and blue stay at 0.66 of their share
// until frame 160 (a darker orange), then climb to full by frame 170.
static const float FADE_FRAMES = 16.5f;
static const float TINT_DARK = 0.66f, TINT_FROM = 16.0f, TINT_TO = 26.0f;

// A ripple: a ring of water spreading from a centre, bending what it crosses along the
// ring's radius. At radius r the picture is moved by
//   amp * exp(-((r - R) / width)^2 / 2) * sin(2 pi (r - R) / length + phase + drift * (f - from))
// with R = speed * (f - born) the ring's radius at video frame f. Each was fitted by warping
// the settled shape and comparing it with the frames' shapes (mean difference near the
// edge, 0 to 1; frames after the ripples differ by about 0.045 from the video's own blur).
struct Wave {
    float cx, cy;        // the centre
    float ky;            // the ring is an ellipse: r = sqrt(dx^2 + (ky dy)^2)
    float speed, born;   // units a frame; the frame the ring leaves the centre
    float amp, width, length, phase, drift;
    float from, to;      // the frames it is drawn between, faded in and out over FADE_EDGE
};
static const float WAVE_EDGE = 2.0f;
// The first, from above the 2's right, frames 153 to 171: the difference fell from 0.122 to
// 0.108. (A narrower packet, 9.4 wide, 59.7 long, 22 high, reached 0.105; this wider one was
// kept because it bends as many strokes at once as the video does.)
static const Wave kWave1 = {299.39f - ORIGIN_X, -38.03f - ORIGIN_Y, 1.0f, 11.575f, 141.5f - 144.0f, 19.0f, 16.0f, 44.5f,
                            1.265f, -0.106f, 5.0f, 30.0f};
// The second, from where the 2 lands, frames 171 to 185: from 0.083 to 0.071.
static const Wave kWave2 = {263.49f - ORIGIN_X, 33.87f - ORIGIN_Y, 1.0f, 7.58f, 164.0f - 144.0f, 14.6f, 9.7f, 38.7f,
                            0.776f, -0.089f, 21.0f, 42.0f};

// The blue rings ride the first ripple: two bands behind its crest, broken into dashes
// along their length (frames 155 to 171; the bands' place, width and colour read from the
// frames, the dashes drawn to look like them, not fitted).
static const float RING_FROM = 10.0f, RING_TO = 28.5f;
static const float kRingOffset[2] = {2.0f, -36.0f};
static const float kRingWidth[2] = {3.6f, 4.6f};
// The rings show from straight left of the centre down to RING_SECTOR below that, as the
// video's do (top of the frame to its lower left), and are gone RING_SECTOR_FADE further round.
static const float RING_SECTOR = 0.96f, RING_SECTOR_FADE = 0.45f;
static const float kRingBlue[3] = {92, 92, 232}, kRingPale[3] = {120, 170, 200};

// The 2: nothing until frame 165, whole at 166.5 and wound into a thin swirl larger than
// itself; it unwinds, turning back and forth past its place and squashing wide and tall by
// turns, fills out and is still by frame 178 (read from frames 165 to 178, drawn to look
// like them, not fitted).
static const float TWO_X = 263.9f - ORIGIN_X, TWO_Y = 40.5f - ORIGIN_Y, TWO_REACH = 36.0f;
static const float TWO_FROM = 21.0f, TWO_FADE = 1.5f, TWO_TO = 34.0f;
static const float TWO_TWIST = 2.2f, TWO_GROW = 0.35f, TWO_THIN = 1.2f, TWO_SQUASH = 0.3f;

// When the build-in is over, and how the idle ripple replays it: every 30 seconds the
// moments from RIPPLE_FROM to BUILD_END again, the logo whole and bright.
static const float BUILD_END = 42.0f, RIPPLE_FROM = 6.0f;
static const U32 RIPPLE_EVERY_MS = 30000;
static const float RIPPLE_GENTLE = 0.5f;  // the gentle idle ripple's height against the video's

// The cc of LBA2 Classic Community (assets/logo/cc.svg): ours, not the video's, so nothing
// here is measured. It lies over the logo under the 2, in the ramp's colours with the
// logo's bevel (chosen in captures over a pale blue, which had less contrast over the
// planet; the maintainer, 2026-11-17). It comes last, when the second ripple has left: each c fades in over CC_FADE frames while it settles from CC_GROW larger over
// CC_SETTLE, the second c CC_GAP frames after the first (the maintainer, 2026-11-17: the
// intro's build-in stays as it is and the cc arrives as its own short beat).
static const float CC_FROM = 40.0f, CC_GAP = 2.5f, CC_FADE = 2.5f, CC_SETTLE = 4.5f, CC_GROW = 0.35f;
static const float CC_END = CC_FROM + CC_GAP + CC_SETTLE;

// ───────────────────────────── The field ─────────────────────────────

static const int TEX = 3;          // texels a unit
static const float PAD = 8.0f;     // units of field round the ink
static const float RANGE = 6.0f;   // distances are exact up to this far from the edge

struct Texel {
    float d;  // units inside the edge (negative outside)
    U8 r, g, b, pad;
};
// A shape's distances and colours over a grid of texels.
struct Field {
    std::vector<Texel> t;
    int gw = 0, gh = 0;
    float ox = 0, oy = 0;  // the grid's top left corner, in units
    float ex = 0, ey = 0;  // and its bottom right
    float cx = 0, cy = 0;  // the middle of the shape's box
};
static Field s_main;      // the traced logo
static Field s_cc[2];     // the two c's
static float s_inkW, s_inkH;
static bool s_ready, s_ccReady;
static bool s_ccOn;       // the cc is drawn

struct Seg {
    float x0, y0, x1, y1;
};

// The path data of each <path> in an SVG, flattened to line segments. The files are our
// own: only M, C and Z, absolute.
static bool ParseSvg(const unsigned char *data, U32 size, std::vector<std::vector<Seg>> &paths, float *w, float *h) {
    const char *s = (const char *)data, *end = s + size;
    static const char kBox[] = "viewBox=\"";
    const char *vb = std::search(s, end, kBox, kBox + 9);
    if (vb == end)
        return false;
    float bx, by;
    if (sscanf(vb + 9, "%f %f %f %f", &bx, &by, w, h) != 4)
        return false;
    static const char kD[] = " d=\"";
    for (const char *p = s;;) {
        p = std::search(p, end, kD, kD + 4);
        if (p == end)
            break;
        p += 4;
        paths.emplace_back();
        std::vector<Seg> &segs = paths.back();
        float cx = 0, cy = 0, sx = 0, sy = 0;
        while (p < end && *p != '"') {
            const char c = *p;
            if (c == 'M' || c == 'C') {
                float v[6];
                const int want = c == 'M' ? 2 : 6;
                char *q = (char *)p + 1;
                for (int i = 0; i < want; i++)
                    v[i] = strtof(q, &q);
                p = q;
                if (c == 'M') {
                    cx = sx = v[0], cy = sy = v[1];
                    continue;
                }
                enum { STEPS = 10 };
                float px = cx, py = cy;
                for (int i = 1; i <= STEPS; i++) {
                    const float t = (float)i / STEPS, u = 1 - t;
                    const float x = u * u * u * cx + 3 * u * u * t * v[0] + 3 * u * t * t * v[2] + t * t * t * v[4];
                    const float y = u * u * u * cy + 3 * u * u * t * v[1] + 3 * u * t * t * v[3] + t * t * t * v[5];
                    segs.push_back({px, py, x, y});
                    px = x, py = y;
                }
                cx = v[4], cy = v[5];
            } else if (c == 'Z') {
                if (cx != sx || cy != sy)
                    segs.push_back({cx, cy, sx, sy});
                cx = sx, cy = sy;
                p++;
            } else {
                p++;
            }
        }
    }
    return !paths.empty() && !paths[0].empty();
}

static void RampAt(float x, float rgb[3]) {
    const int n = (int)(sizeof kRamp / sizeof kRamp[0]);
    int i = 0;
    while (i < n - 2 && x > kRamp[i + 1].x)
        i++;
    const Stop &a = kRamp[i], &b = kRamp[i + 1];
    float t = (x - a.x) / (b.x - a.x);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    rgb[0] = a.r + (b.r - a.r) * t;
    rgb[1] = a.g + (b.g - a.g) * t;
    rgb[2] = a.b + (b.b - a.b) * t;
}

static U8 Byte(float v) { return (U8)(v < 0 ? 0 : v > 255 ? 255 : v + 0.5f); }

// The field of the shape `segs` outline, whose box is w by h units from (x0, y0).
static void BuildField(Field &F, const std::vector<Seg> &segs, float x0, float y0, float w, float h) {
    F.ox = x0 - PAD, F.oy = y0 - PAD;
    F.cx = x0 + w * 0.5f, F.cy = y0 + h * 0.5f;
    const int gw = F.gw = (int)ceilf((w + 2 * PAD) * TEX);
    const int gh = F.gh = (int)ceilf((h + 2 * PAD) * TEX);
    F.ex = F.ox + (float)gw / TEX, F.ey = F.oy + (float)gh / TEX;
    std::vector<Texel> &field = F.t;
    field.assign((size_t)gw * gh, Texel{-RANGE, 0, 0, 0, 0});

    // Inside or outside, a row at a time: the crossings of the row's centre line, even-odd.
    std::vector<U8> inside((size_t)gw * gh, 0);
    std::vector<float> xs;
    for (int j = 0; j < gh; j++) {
        const float y = (j + 0.5f) / TEX + F.oy;
        xs.clear();
        for (const Seg &s : segs)
            if ((s.y0 <= y) != (s.y1 <= y))
                xs.push_back(s.x0 + (s.x1 - s.x0) * (y - s.y0) / (s.y1 - s.y0));
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2) {
            int i0 = (int)ceilf((xs[k] - F.ox) * TEX - 0.5f), i1 = (int)floorf((xs[k + 1] - F.ox) * TEX - 0.5f);
            i0 = i0 < 0 ? 0 : i0;
            i1 = i1 >= gw ? gw - 1 : i1;
            for (int i = i0; i <= i1; i++)
                inside[(size_t)j * gw + i] = 1;
        }
    }

    // The exact distance to the nearest segment, found through a grid of cells RANGE wide.
    const float CELL = RANGE;
    const int cw = (int)ceilf((w + 2 * PAD) / CELL) + 1, ch = (int)ceilf((h + 2 * PAD) / CELL) + 1;
    std::vector<std::vector<int>> cells((size_t)cw * ch);
    for (size_t n = 0; n < segs.size(); n++) {
        const Seg &s = segs[n];
        const int a0 = (int)floorf((std::min(s.x0, s.x1) - F.ox) / CELL), a1 = (int)floorf((std::max(s.x0, s.x1) - F.ox) / CELL);
        const int b0 = (int)floorf((std::min(s.y0, s.y1) - F.oy) / CELL), b1 = (int)floorf((std::max(s.y0, s.y1) - F.oy) / CELL);
        for (int b = std::max(b0, 0); b <= std::min(b1, ch - 1); b++)
            for (int a = std::max(a0, 0); a <= std::min(a1, cw - 1); a++)
                cells[(size_t)b * cw + a].push_back((int)n);
    }
    for (int j = 0; j < gh; j++) {
        const float y = (j + 0.5f) / TEX + F.oy;
        for (int i = 0; i < gw; i++) {
            const float x = (i + 0.5f) / TEX + F.ox;
            const int ca = (int)floorf((x - F.ox) / CELL), cb = (int)floorf((y - F.oy) / CELL);
            float best = RANGE * RANGE, nx = 0, ny = 0;
            for (int b = std::max(cb - 1, 0); b <= std::min(cb + 1, ch - 1); b++)
                for (int a = std::max(ca - 1, 0); a <= std::min(ca + 1, cw - 1); a++)
                    for (int n : cells[(size_t)b * cw + a]) {
                        const Seg &s = segs[n];
                        const float ex = s.x1 - s.x0, ey = s.y1 - s.y0, len2 = ex * ex + ey * ey;
                        float t = len2 > 0 ? ((x - s.x0) * ex + (y - s.y0) * ey) / len2 : 0;
                        t = t < 0 ? 0 : t > 1 ? 1 : t;
                        const float dx = x - (s.x0 + ex * t), dy = y - (s.y0 + ey * t), d2 = dx * dx + dy * dy;
                        if (d2 < best)
                            best = d2, nx = dx, ny = dy;
                    }
            const bool in = inside[(size_t)j * gw + i] != 0;
            const float dist = sqrtf(best);
            Texel &t = field[(size_t)j * gw + i];
            t.d = in ? dist : -dist;
            // The colour: the ramp, and near the edge the bevel by the way the edge faces
            // (from the nearest point outwards).
            float rgb[3];
            RampAt(x, rgb);
            if (dist < RANGE && dist > 1e-4f) {
                const float sgn = in ? -1.0f : 1.0f;
                const float c = sgn * nx / dist, s = -sgn * ny / dist;  // y grows downwards
                float wgt = in ? (BEVEL_END - dist) / (BEVEL_END - BEVEL_FULL) : 1.0f;
                wgt = wgt < 0 ? 0 : wgt > 1 ? 1 : wgt;
                const float turn[5] = {1, c, s, c * c - s * s, 2 * c * s};
                float wb = 0, wg = 0;
                for (int k = 0; k < 5; k++)
                    wb += kBevelBlue[k] * turn[k], wg += kBevelGreen[k] * turn[k];
                rgb[2] += wgt * wb * (BEVEL_PALE - rgb[2]);
                rgb[1] *= 1 + wgt * wg;
                rgb[1] = rgb[1] > GREEN_TOP ? GREEN_TOP : rgb[1];
            }
            t.r = Byte(rgb[0]), t.g = Byte(rgb[1]), t.b = Byte(rgb[2]);
        }
    }
}

void VideoLogo_Init() {
    std::vector<std::vector<Seg>> paths;
    if (!ParseSvg(g_lba2LogoSvg, g_lba2LogoSvgSize, paths, &s_inkW, &s_inkH))
        return;
    std::vector<Seg> all;
    for (const std::vector<Seg> &path : paths)
        all.insert(all.end(), path.begin(), path.end());
    BuildField(s_main, all, 0, 0, s_inkW, s_inkH);
    s_ready = true;

    // The cc: a field for each c.
    std::vector<std::vector<Seg>> cc;
    float w, h;
    if (!ParseSvg(g_lba2CcSvg, g_lba2CcSvgSize, cc, &w, &h) || cc.size() != 2 || cc[1].empty())
        return;
    for (int k = 0; k < 2; k++) {
        float x0 = cc[k][0].x0, y0 = cc[k][0].y0, x1 = x0, y1 = y0;
        for (const Seg &g : cc[k])
            x0 = std::min(x0, g.x1), y0 = std::min(y0, g.y1), x1 = std::max(x1, g.x1), y1 = std::max(y1, g.y1);
        BuildField(s_cc[k], cc[k], x0, y0, x1 - x0, y1 - y0);
    }
    s_ccReady = true;
}

bool VideoLogo_Ready() { return s_ready; }
float VideoLogo_Aspect() { return s_ready ? s_inkW / s_inkH : 1.5f; }

// The distance at (x, y) units, between texels.
static inline float FieldD(const Field &F, float x, float y, int *i0, int *j0, float *ax, float *ay) {
    const int gw = F.gw, gh = F.gh;
    float u = (x - F.ox) * TEX - 0.5f, v = (y - F.oy) * TEX - 0.5f;
    u = u < 0 ? 0 : u > gw - 1.001f ? gw - 1.001f : u;
    v = v < 0 ? 0 : v > gh - 1.001f ? gh - 1.001f : v;
    const int i = (int)u, j = (int)v;
    const float a = u - i, b = v - j;
    const Texel *t = &F.t[(size_t)j * gw + i];
    *i0 = i, *j0 = j, *ax = a, *ay = b;
    return (t[0].d * (1 - a) + t[1].d * a) * (1 - b) + (t[gw].d * (1 - a) + t[gw + 1].d * a) * b;
}

static inline void FieldRgb(const Field &F, int i, int j, float a, float b, float rgb[3]) {
    const int gw = F.gw;
    const Texel *t = &F.t[(size_t)j * gw + i];
    const float w00 = (1 - a) * (1 - b), w10 = a * (1 - b), w01 = (1 - a) * b, w11 = a * b;
    rgb[0] = t[0].r * w00 + t[1].r * w10 + t[gw].r * w01 + t[gw + 1].r * w11;
    rgb[1] = t[0].g * w00 + t[1].g * w10 + t[gw].g * w01 + t[gw + 1].g * w11;
    rgb[2] = t[0].b * w00 + t[1].b * w10 + t[gw].b * w01 + t[gw + 1].b * w11;
}

// ───────────────────────────── A moment ─────────────────────────────

static inline float Clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static inline float Smooth(float v) {
    v = Clamp01(v);
    return v * v * (3 - 2 * v);
}

// What a moment needs, worked out once a frame.
struct Moment {
    float opacity, tint;
    bool wave[2];
    float waveR[2], waveAmp[2], wavePhase[2];
    bool ring;
    float ringGain;
    float twoAlpha, twoTwist, twoScale, twoThin, twoWide;
    bool cc;  // the cc is part of the logo
    float ccAlpha[2], ccScale[2];
    bool still;  // nothing moves: the settled logo
};

static void WaveAt(const Wave &w, float f, bool *on, float *R, float *amp, float *phase) {
    *on = f > w.from && f < w.to;
    *R = w.speed * (f - w.born);
    *amp = w.amp * Smooth((f - w.from) / WAVE_EDGE) * Smooth((w.to - f) / WAVE_EDGE);
    *phase = w.phase + w.drift * (f - w.from);
}

// f: video frames since 144. ripple: an idle ripple, the logo already whole.
// gain: the ripples' height, 1 as the video has it.
static Moment MomentAt(float f, bool ripple, bool ring, float gain = 1) {
    Moment m;
    m.opacity = ripple ? 1 : Clamp01(f / FADE_FRAMES);
    m.tint = ripple ? 1 : TINT_DARK + (1 - TINT_DARK) * Clamp01((f - TINT_FROM) / (TINT_TO - TINT_FROM));
    WaveAt(kWave1, f, &m.wave[0], &m.waveR[0], &m.waveAmp[0], &m.wavePhase[0]);
    WaveAt(kWave2, f, &m.wave[1], &m.waveR[1], &m.waveAmp[1], &m.wavePhase[1]);
    m.waveAmp[0] *= gain, m.waveAmp[1] *= gain;
    m.ring = ring && f > RING_FROM && f < RING_TO;
    m.ringGain = Smooth((f - RING_FROM) / 2) * Smooth((RING_TO - f) / 3);
    const float t = Clamp01((f - TWO_FROM) / (TWO_TO - TWO_FROM)), left = (1 - t) * (1 - t);
    m.twoAlpha = ripple ? 1 : Clamp01((f - TWO_FROM) / TWO_FADE);
    m.twoTwist = ripple ? 0 : TWO_TWIST * (1 - t) * sqrtf(1 - t) * cosf(t * 7.853982f);
    m.twoWide = ripple ? 0 : TWO_SQUASH * (1 - t) * sinf(t * 12.566371f);
    m.twoScale = ripple ? 1 : 1 + TWO_GROW * left;
    m.twoThin = ripple ? 0 : TWO_THIN * left;
    m.cc = s_ccOn && s_ccReady;
    for (int k = 0; k < 2; k++) {
        const float since = f - CC_FROM - k * CC_GAP, left = 1 - Clamp01(since / CC_SETTLE);
        m.ccAlpha[k] = !m.cc ? 0 : ripple ? 1 : Clamp01(since / CC_FADE);
        m.ccScale[k] = ripple ? 1 : 1 + CC_GROW * left * left;
    }
    m.still = !m.wave[0] && !m.wave[1] && !m.ring && m.opacity >= 1 && m.tint >= 1 && m.twoAlpha >= 1 &&
              (ripple || (f >= TWO_TO && (!m.cc || f >= CC_END)));
    if (f >= TWO_TO)
        m.twoTwist = m.twoWide = m.twoThin = 0, m.twoScale = 1;
    return m;
}

// How far the ripples move the picture at (x, y) units: it is looked up at (x - ux, y - uy).
static inline void RippleAt(const Moment &m, float x, float y, float *ux, float *uy) {
    const Wave *const def[2] = {&kWave1, &kWave2};
    *ux = *uy = 0;
    for (int k = 0; k < 2; k++) {
        if (!m.wave[k])
            continue;
        const Wave &w = *def[k];
        const float rx = x - w.cx, ry = (y - w.cy) * w.ky, r = sqrtf(rx * rx + ry * ry) + 1e-3f;
        const float q = (r - m.waveR[k]) / w.width;
        if (q > 3.5f || q < -3.5f)
            continue;
        const float u = m.waveAmp[k] * expf(-0.5f * q * q) * sinf(6.2831853f * (r - m.waveR[k]) / w.length + m.wavePhase[k]);
        // Along the ellipse's normal.
        const float nx = rx, ny = ry * w.ky, n = sqrtf(nx * nx + ny * ny) + 1e-3f;
        *ux += u * nx / n, *uy += u * ny / n;
    }
}

// Where a c is read for the place (x, y): about its middle, shrunk by how much larger it is drawn.
static inline void CcPlace(const Moment &m, int k, const Field &F, float x, float y, float *gx, float *gy) {
    *gx = F.cx + (x - F.cx) / m.ccScale[k], *gy = F.cy + (y - F.cy) / m.ccScale[k];
}

// The cc's distance at (x, y) units: the nearer c's, as it is drawn at this moment.
static inline float CcD(const Moment &m, float x, float y) {
    float best = -RANGE;
    for (int k = 0; k < 2; k++) {
        if (m.ccAlpha[k] <= 0)
            continue;
        const Field &F = s_cc[k];
        float gx, gy, ax, ay;
        int i, j;
        CcPlace(m, k, F, x, y, &gx, &gy);
        const float d = FieldD(F, gx, gy, &i, &j, &ax, &ay) * m.ccScale[k];
        best = d > best ? d : best;
    }
    return best;
}

// The cc laid over what the logo gave at (x, y): rgb and coverage a in, both out.
static inline float CcOver(const Moment &m, float x, float y, float soft, float rgb[3], float a) {
    for (int k = 0; k < 2; k++) {
        if (m.ccAlpha[k] <= 0)
            continue;
        const Field &F = s_cc[k];
        float gx, gy, ax, ay;
        int i, j;
        CcPlace(m, k, F, x, y, &gx, &gy);
        if (gx < F.ox || gy < F.oy || gx > F.ex || gy > F.ey)
            continue;
        const float d = FieldD(F, gx, gy, &i, &j, &ax, &ay) * m.ccScale[k];
        const float c = Clamp01(d / soft + 0.5f) * m.ccAlpha[k];
        if (c <= 0)
            continue;
        float ink[3];
        FieldRgb(F, i, j, ax, ay, ink);
        const float under = a * (1 - c), out = c + under;
        for (int n = 0; n < 3; n++)
            rgb[n] = a > 0 ? (ink[n] * c + rgb[n] * under) / out : ink[n];
        a = out;
    }
    return a;
}

// The logo looked up at (lx, ly) units at a moment: colour and coverage, `soft` units of
// edge softness (half a screen pixel either side).
static inline float LogoAt(const Moment &m, float lx, float ly, float soft, float rgb[3]) {
    const float ccX = lx, ccY = ly;  // the 2's swirl does not turn the cc
    float alpha = m.opacity, thin = 0;
    // The 2 in its swirl: looked up turned and shrunk about its centre.
    const float tx = lx - TWO_X, ty = ly - TWO_Y, tr2 = tx * tx + ty * ty;
    if (tr2 < TWO_REACH * TWO_REACH) {
        alpha *= m.twoAlpha;
        thin = m.twoThin;
        if (alpha > 0 && (m.twoTwist != 0 || m.twoScale != 1 || m.twoWide != 0)) {
            // All of it eases to nothing at the reach, so the swirl has no rim.
            const float fall = Smooth(1 - tr2 / (TWO_REACH * TWO_REACH));
            const float a = m.twoTwist * fall, c = cosf(a), s = sinf(a);
            const float kx = 1 / (1 + (m.twoScale - 1 + m.twoWide) * fall), ky = 1 / (1 + (m.twoScale - 1 - m.twoWide) * fall);
            lx = TWO_X + (tx * c - ty * s) * kx, ly = TWO_Y + (tx * s + ty * c) * ky;
            thin *= fall;
        }
    }
    float a = 0;
    if (alpha > 0) {
        int i, j;
        float ax, ay;
        const float d = FieldD(s_main, lx, ly, &i, &j, &ax, &ay) - thin;
        const float cover = Clamp01(d / soft + 0.5f);
        if (cover > 0) {
            FieldRgb(s_main, i, j, ax, ay, rgb);
            rgb[1] *= m.tint, rgb[2] *= m.tint;
            a = cover * alpha;
        }
    }
    return m.cc ? CcOver(m, ccX, ccY, soft, rgb, a) : a;
}

// The blue rings' light at (x, y) units: 0 to 1, and how pale (0 blue, 1 pale).
static inline float RingAt(const Moment &m, float x, float y, float *pale) {
    const float rx = x - kWave1.cx, ry = (y - kWave1.cy) * kWave1.ky, r = sqrtf(rx * rx + ry * ry);
    float light = 0;
    *pale = 0;
    for (int k = 0; k < 2; k++) {
        const float q = (r - (m.waveR[0] + kRingOffset[k])) / kRingWidth[k];
        if (q > 3 || q < -3)
            continue;
        // Beads along the ring: the arc length cut into unequal pieces, the same across the band.
        const float turn = atan2f(ry, -rx);  // 0 straight left of the centre, growing downwards
        const float sector = Smooth((RING_SECTOR + RING_SECTOR_FADE - turn) / RING_SECTOR_FADE) * Smooth((turn + 0.5f) / 0.3f);
        if (sector <= 0)
            continue;
        const float along = turn * (m.waveR[0] + kRingOffset[k]);
        const float bead = 0.5f + 0.5f * sinf(along * (k ? 0.19f : 0.23f) + (k ? 1.3f : 0.2f) + sinf(along * 0.067f) * 1.9f);
        float band = 1.5f * expf(-0.5f * q * q) * Smooth(bead * 2.4f - 0.2f) * sector;
        band = band > 1 ? 1 : band;
        if (band > light)
            light = band, *pale = Smooth(bead * 2.6f - 1.3f) * expf(-1.5f * q * q);
    }
    return light * m.ringGain;
}

// ───────────────────────────── Painting ─────────────────────────────

struct Placed {
    float x0, y0, s;  // the ink's top left on the frame, and screen pixels a unit
};

// What moves, at one point of the lattice Render works the ripples and rings out on.
struct Moved {
    float ux, uy, light, pale;
    float lx, ly, d;  // where the field is read for this point, and its distance there
};

// Draw a moment over `argb`, the ink placed by `at`, inside the clip. The ripples' push and
// the rings' light change slowly across the screen, so they are worked out on a lattice
// `step` screen pixels apart and blended between; the field is read at every pixel. A cell
// of the lattice with no light on its corners and all four deep outside the ink is left
// alone (distances cannot change faster than the place they are read at moves).
static void Render(U32 *argb, U32 pitch, S32 cx0, S32 cy0, S32 cx1, S32 cy1, const Placed &at, const Moment &m) {
    if (cx1 <= cx0 || cy1 <= cy0)
        return;
    const float inv = 1 / at.s, soft = inv;
    const int step = at.s >= 3.1f ? 4 : at.s >= 2.3f ? 3 : 2;
    // Past this far from the ink the rings fade to nothing, so they never end at a clip.
    const float mx = s_inkW * 0.5f, my = s_inkH * 0.5f, reachX = mx + s_inkW * 0.3f, reachY = my + s_inkH * 0.3f;
    const bool swirl = m.twoTwist != 0 || m.twoScale != 1 || m.twoWide != 0;
    const int cols = (cx1 - cx0 + step - 1) / step + 1;
    static std::vector<Moved> rows[2];
    rows[0].resize((size_t)cols), rows[1].resize((size_t)cols);
    const auto fill = [&](std::vector<Moved> &row, S32 py) {
        const float y = (py + 0.5f - at.y0) * inv;
        for (int c = 0; c < cols; c++) {
            const float x = (cx0 + c * step + 0.5f - at.x0) * inv;
            Moved &v = row[(size_t)c];
            RippleAt(m, x, y, &v.ux, &v.uy);
            v.lx = x - v.ux, v.ly = y - v.uy;
            int i, j;
            float ax, ay;
            v.d = FieldD(s_main, v.lx, v.ly, &i, &j, &ax, &ay);
            if (m.cc)
                v.d = std::max(v.d, CcD(m, v.lx, v.ly));
            v.light = v.pale = 0;
            if (m.ring) {
                v.light = RingAt(m, x, y, &v.pale);
                if (v.light > 0) {
                    const float ex = fabsf(x - mx) / reachX, ey = fabsf(y - my) / reachY;
                    v.light *= Smooth((1 - (ex > ey ? ex : ey)) * 4);
                }
            }
        }
    };
    int top = 0;  // rows[top] is the lattice row at the top of the cells, rows[!top] the one under them
    fill(rows[0], cy0);
    for (S32 rowY = cy0; rowY < cy1; rowY += step, top = !top) {
        fill(rows[!top], rowY + step);
        const Moved *ra = rows[top].data(), *rb = rows[!top].data();
        const S32 yEnd = rowY + step < cy1 ? rowY + step : cy1;
        for (int c = 0; c + 1 < cols; c++) {
            const Moved &a0 = ra[c], &a1 = ra[c + 1], &b0 = rb[c], &b1 = rb[c + 1];
            const S32 cellX = cx0 + c * step, xEnd = cellX + step < cx1 ? cellX + step : cx1;
            const bool lit = a0.light > 0 || a1.light > 0 || b0.light > 0 || b1.light > 0;
            if (!lit) {
                // Every place read inside the cell lies between the four corners' places: no
                // further from one of them than they spread. Not where the 2's swirl turns them.
                const float lo = std::min(std::min(a0.lx, a1.lx), std::min(b0.lx, b1.lx)), hi = std::max(std::max(a0.lx, a1.lx), std::max(b0.lx, b1.lx));
                const float lo2 = std::min(std::min(a0.ly, a1.ly), std::min(b0.ly, b1.ly)), hi2 = std::max(std::max(a0.ly, a1.ly), std::max(b0.ly, b1.ly));
                const float spread = (hi - lo) + (hi2 - lo2) + soft + 0.5f;
                const float deep = std::max(std::max(a0.d, a1.d), std::max(b0.d, b1.d));
                bool nearTwo = false;
                if (swirl) {
                    const float tx = a0.lx - TWO_X, ty = a0.ly - TWO_Y, reach = TWO_REACH + spread;
                    nearTwo = tx * tx + ty * ty < reach * reach;
                }
                if (deep < -spread && !nearTwo)
                    continue;
            }
            for (S32 py = rowY; py < yEnd; py++) {
                const float fy = (float)(py - rowY) / step;
                U32 *row = (U32 *)((U8 *)argb + (size_t)py * pitch);
                for (S32 px = cellX; px < xEnd; px++) {
                    const float fx = (float)(px - cellX) / step;
                    const float w00 = (1 - fx) * (1 - fy), w10 = fx * (1 - fy), w01 = (1 - fx) * fy, w11 = fx * fy;
                    const float lx = a0.lx * w00 + a1.lx * w10 + b0.lx * w01 + b1.lx * w11;
                    const float ly = a0.ly * w00 + a1.ly * w10 + b0.ly * w01 + b1.ly * w11;
                    float rgb[3];
                    const float a = LogoAt(m, lx, ly, soft, rgb);
                    const float light = lit ? a0.light * w00 + a1.light * w10 + b0.light * w01 + b1.light * w11 : 0;
                    if (a <= 0 && light <= 0)
                        continue;
                    const U32 d = row[px];
                    float r = (float)((d >> 16) & 0xff), g = (float)((d >> 8) & 0xff), b = (float)(d & 0xff);
                    if (a > 0)
                        r += (rgb[0] - r) * a, g += (rgb[1] - g) * a, b += (rgb[2] - b) * a;
                    if (light > 0) {
                        // Light added as a screen: over the fill it goes to white, as the video has it.
                        const float pale = a0.pale * w00 + a1.pale * w10 + b0.pale * w01 + b1.pale * w11;
                        const float lr = kRingBlue[0] + (kRingPale[0] - kRingBlue[0]) * pale;
                        const float lg = kRingBlue[1] + (kRingPale[1] - kRingBlue[1]) * pale;
                        const float lb = kRingBlue[2] + (kRingPale[2] - kRingBlue[2]) * pale;
                        r += (255 - r) * lr * light / 255, g += (255 - g) * lg * light / 255, b += (255 - b) * lb * light / 255;
                    }
                    row[px] = 0xff000000u | (U32)Byte(r) << 16 | (U32)Byte(g) << 8 | Byte(b);
                }
            }
        }
    }
}

// The settled logo, kept at the size last painted: coverage and colour a screen pixel.
static std::vector<U32> s_still;  // alpha in the top byte, colour not multiplied
static S32 s_stillW, s_stillH;
static float s_stillS, s_stillFx, s_stillFy;

static void PaintStill(U32 *argb, U32 pitch, S32 cx0, S32 cy0, S32 cx1, S32 cy1, const Placed &at) {
    // The cache starts at the whole pixel left of the ink, one pixel of margin round it.
    const S32 ox = (S32)floorf(at.x0) - 1, oy = (S32)floorf(at.y0) - 1;
    const float fx = at.x0 - ox, fy = at.y0 - oy;
    const S32 w = (S32)ceilf(s_inkW * at.s) + 3, h = (S32)ceilf(s_inkH * at.s) + 3;
    if (s_still.empty() || w != s_stillW || h != s_stillH || at.s != s_stillS || fx != s_stillFx || fy != s_stillFy) {
        s_still.assign((size_t)w * h, 0);
        s_stillW = w, s_stillH = h, s_stillS = at.s, s_stillFx = fx, s_stillFy = fy;
        const Moment m = MomentAt(BUILD_END, true, false);
        const float inv = 1 / at.s;
        for (S32 j = 0; j < h; j++)
            for (S32 i = 0; i < w; i++) {
                float rgb[3];
                const float a = LogoAt(m, (i + 0.5f - fx) * inv, (j + 0.5f - fy) * inv, inv, rgb);
                if (a > 0)
                    s_still[(size_t)j * w + i] =
                        (U32)Byte(a * 255) << 24 | (U32)Byte(rgb[0]) << 16 | (U32)Byte(rgb[1]) << 8 | Byte(rgb[2]);
            }
    }
    for (S32 py = std::max(cy0, oy); py < std::min(cy1, oy + h); py++) {
        U32 *row = (U32 *)((U8 *)argb + (size_t)py * pitch);
        const U32 *src = &s_still[(size_t)(py - oy) * w];
        for (S32 px = std::max(cx0, ox); px < std::min(cx1, ox + w); px++) {
            const U32 c = src[px - ox], a = c >> 24;
            if (!a)
                continue;
            const U32 d = row[px], ia = 255 - a;
            const U32 r = (((c >> 16) & 0xff) * a + ((d >> 16) & 0xff) * ia + 127) / 255;
            const U32 g = (((c >> 8) & 0xff) * a + ((d >> 8) & 0xff) * ia + 127) / 255;
            const U32 b = ((c & 0xff) * a + (d & 0xff) * ia + 127) / 255;
            row[px] = 0xff000000u | r << 16 | g << 8 | b;
        }
    }
}

// ───────────────────────────── The queue and the clock ─────────────────────────────

static bool s_queued, s_queuedStill, s_noteOver;
static S32 s_bx0, s_by0, s_bx1, s_by1;
static U32 s_sum;
static bool s_started;
static U32 s_startMs;
static int s_idle = 1;

void VideoLogo_BeginFrame() { s_queued = false; }
void VideoLogo_Restart() { s_started = false; }
void VideoLogo_SetIdle(int mode) { s_idle = mode; }
void VideoLogo_SetCc(bool on) {
    if (on == s_ccOn)
        return;
    s_ccOn = on;
    s_still.clear();  // the settled logo kept for painting is another picture now
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

static U32 QueuedSum() {
    const S32 a0 = s_bx0 < 0 ? 0 : s_bx0, b0 = s_by0 < 0 ? 0 : s_by0;
    const S32 a1 = s_bx1 > (S32)ModeDesiredX - 1 ? (S32)ModeDesiredX - 1 : s_bx1,
              b1 = s_by1 > (S32)ModeDesiredY - 1 ? (S32)ModeDesiredY - 1 : s_by1;
    return CanvasSum(a0, b0, a1, b1);
}

void VideoLogo_NoteBegin() { s_noteOver = s_queued && Host_Pixels() && QueuedSum() == s_sum; }
void VideoLogo_NoteEnd() {
    if (s_noteOver)
        s_sum = QueuedSum();
    s_noteOver = false;
}

void VideoLogo_Queue(S32 x0, S32 y0, S32 x1, S32 y1, bool still) {
    if (!s_ready)
        return;
    s_queuedStill = still;
    if (!s_started && !still) {
        s_started = true;
        s_startMs = Host_TimeMs();
    }
    s_queued = true;
    s_bx0 = x0, s_by0 = y0, s_bx1 = x1, s_by1 = y1;
    s_sum = Host_Pixels() ? QueuedSum() : 0;
}

// When the build-in is over: the cc's beat comes after the video's.
static float BuildEnd() { return s_ccOn && s_ccReady ? CC_END : BUILD_END; }

void VideoLogo_RippleNow() {
    s_started = true;
    s_startMs = Host_TimeMs() - (U32)(BuildEnd() * 1000.0f / FPS) - RIPPLE_EVERY_MS;
}

// The moment the clock shows: the build-in once, then a ripple every 30 seconds.
static Moment MomentNow() {
    const U32 ms = Host_TimeMs() - s_startMs;
    const float f = ms * FPS / 1000.0f;
    const float end = BuildEnd();
    if (f < end)
        return MomentAt(f, false, true);
    const U32 since = ms - (U32)(end * 1000.0f / FPS);
    const float rf = RIPPLE_FROM + (since % RIPPLE_EVERY_MS) * FPS / 1000.0f;
    if (since >= RIPPLE_EVERY_MS && rf < BUILD_END)
        return MomentAt(rf, true, s_idle == 3, s_idle == 1 ? RIPPLE_GENTLE : 1);
    return MomentAt(BUILD_END, true, false);
}

void VideoLogo_Paint(U32 *argb, U32 pitch, U32 w, U32 h) {
    if (!s_queued || !s_ready)
        return;
    if (QueuedSum() != s_sum)
        return;  // drawn over later in the frame
    float k, bx, by;
    PropScale_Map(w, h, &k, &bx, &by);
    // The box on the screen, and the ink fitted into it, centred.
    const float sx0 = bx + s_bx0 * k, sy0 = by + s_by0 * k, sw = (s_bx1 - s_bx0 + 1) * k, sh = (s_by1 - s_by0 + 1) * k;
    Placed at;
    at.s = std::min(sw / s_inkW, sh / s_inkH);
    at.x0 = sx0 + (sw - s_inkW * at.s) * 0.5f;
    at.y0 = sy0 + (sh - s_inkH * at.s) * 0.5f;
    const Moment m = s_queuedStill ? MomentAt(BUILD_END, true, false) : MomentNow();
    if (m.still) {
        PaintStill(argb, pitch, 0, 0, (S32)w, (S32)h, at);
        return;
    }
    // The rings reach past the ink: 0.3 of its size each way.
    const float rx = s_inkW * 0.3f * at.s, ry = s_inkH * 0.3f * at.s;
    S32 cx0 = (S32)floorf(at.x0 - rx), cy0 = (S32)floorf(at.y0 - ry);
    S32 cx1 = (S32)ceilf(at.x0 + s_inkW * at.s + rx), cy1 = (S32)ceilf(at.y0 + s_inkH * at.s + ry);
    cx0 = cx0 < 0 ? 0 : cx0, cy0 = cy0 < 0 ? 0 : cy0;
    cx1 = cx1 > (S32)w ? (S32)w : cx1, cy1 = cy1 > (S32)h ? (S32)h : cy1;
    Render(argb, pitch, cx0, cy0, cx1, cy1, at, m);
}

void VideoLogo_RenderVideoFrame(U32 *argb, U32 scale, float videoFrame, bool withRing) {
    if (!s_ready)
        return;
    Placed at;
    at.s = (float)scale;
    at.x0 = ORIGIN_X * scale, at.y0 = ORIGIN_Y * scale;
    const float f = videoFrame - 144.0f;
    const Moment m = MomentAt(f < 0 ? 0 : f, false, withRing);
    Render(argb, 320 * scale * 4, 0, 0, (S32)(320 * scale), (S32)(200 * scale), at, m);
}
