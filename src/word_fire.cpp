#include "word_fire.h"

#include "host.h"
#include "screen_font.h"

#include <string.h>

// The menu palette's plasma bank 15 is its fire ramp, but entry 240 is black and 246 to 250
// are all white: the letters use 241 to 246, orange (231,95,0) to white. Each letter has a
// rim in the wordmark's gold (247,187,87).
enum { FIRE_BANK = 15, FIRE_LO = 1, FIRE_HI = 6, INK_RIM = 107 };

// Flames: heat at the logical pixel grain over the letters' box, fed along its bottom row and
// carried up a row a step, cooling and drifting sideways as it climbs (the demo-scene fire).
// The cooling varies slowly along the row, so some columns stay hot higher than their
// neighbours: tongues that rise and fall. Stepped 5 times a second from the menu's clock, so
// a fixed-clock run repeats (chosen from 60, 30, 20, 14, 12, 10, 8, 7 and 5). Between two steps
// the letters show the heat moved up the part of a row the flames have climbed, a blend of the
// step before seen that far higher and the step after seen the rest lower, so the flames rise
// smoothly at so few steps rather than cross-fading the two.
enum { FIRE_W = 256, FIRE_H = 64, FIRE_HZ = 5 };
// The cooling over the letters' height: half of what left a third of the heat at the top of
// LBA's capitals, so the yellow reaches near the top and the orange stays in the tips (chosen
// from 1, 0.7, 0.5 and 0.35).
static const float kCool = 1.6f * 0.5f;

static float s_heat[FIRE_H][FIRE_W], s_prev[FIRE_H][FIRE_W];
static float s_blend;           // how far from s_prev to s_heat the letters are, 0 to 1
static float s_wander[FIRE_W];  // each column's drift, 0 to 1
static float s_tongue[FIRE_W];  // the drift smoothed across neighbours, scaling the cooling
static S32 s_fx0, s_fy0, s_fw, s_fh;
static U32 s_rng = 1997, s_lastMs;
static float s_acc;  // milliseconds owed to the next step
static bool s_started;

static float Rand01() {
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return (s_rng & 0xffffff) / 16777216.0f;
}

static void StepTongues(S32 w) {
    for (S32 x = 0; x < w; x++) {
        const float v = s_wander[x] + (Rand01() - 0.5f) * 0.25f;
        s_wander[x] = v < 0 ? -v : v > 1 ? 2 - v : v;  // bounced back inside 0 to 1
    }
    enum { REACH = 3 };  // a tongue about 7 logical pixels wide
    for (S32 x = 0; x < w; x++) {
        float sum = 0;
        int n = 0;
        for (S32 i = x - REACH; i <= x + REACH; i++)
            if (i >= 0 && i < w)
                sum += s_wander[i], n++;
        // Spread the smoothed drift out again (averaging pulls it to the middle): 0.15 to 2.
        const float d = (sum / n - 0.5f) * 3.0f + 0.5f;
        s_tongue[x] = 0.15f + 1.85f * (d < 0 ? 0 : d > 1 ? 1 : d);
    }
}

static void StepFlames() {
    const S32 w = s_fw, h = s_fh;
    StepTongues(w);
    // The fuel: the bottom row flickers near full heat.
    for (S32 x = 0; x < w; x++)
        s_heat[h - 1][x] = 0.6f + 0.4f * Rand01();
    // Each row takes the heat under it (not yet stepped), a little to one side, less a cooling.
    const float cool = kCool / h;
    for (S32 y = 0; y < h - 1; y++)
        for (S32 x = 0; x < w; x++) {
            const S32 from = x + (S32)(Rand01() * 3.0f) - 1;
            const S32 sx = from < 0 ? 0 : from >= w ? w - 1 : from;
            const float t = s_heat[y + 1][sx] - cool * Rand01() * 1.6f * s_tongue[x];
            s_heat[y][x] = t < 0 ? 0 : t;
        }
}

// The heat at logical (x, y) as a level 0 to 10, between the two steps and between texels.
static float FlameLevel(float x, float y) {
    float u = x - s_fx0, v = y - s_fy0;
    u = u < 0 ? 0 : u > s_fw - 1 ? (float)(s_fw - 1) : u;
    v = v < 0 ? 0 : v > s_fh - 1 ? (float)(s_fh - 1) : v;
    const int u0 = (int)u, v0 = (int)v;
    const int u1 = u0 + 1 < s_fw ? u0 + 1 : u0, v1 = v0 + 1 < s_fh ? v0 + 1 : v0;
    const float au = u - u0, av = v - v0;
    // Each step carries the heat up a row: s_prev seen s_blend of a row higher, s_heat the rest lower.
    const float vp = v + s_blend, vh = v + s_blend - 1;
    const float pv = vp > s_fh - 1 ? (float)(s_fh - 1) : vp, hv = vh < 0 ? 0 : vh;
    const int p0 = (int)pv, h0 = (int)hv;
    const int p1 = p0 + 1 < s_fh ? p0 + 1 : p0, h1 = h0 + 1 < s_fh ? h0 + 1 : h0;
    const float ap = pv - p0, ah = hv - h0;
    float c[2];
    for (int i = 0; i < 2; i++) {
        const int xx = i ? u1 : u0;
        const float pr = s_prev[p0][xx] + (s_prev[p1][xx] - s_prev[p0][xx]) * ap;
        const float hr = s_heat[h0][xx] + (s_heat[h1][xx] - s_heat[h0][xx]) * ah;
        c[i] = pr + (hr - pr) * s_blend;
    }
    const float t = c[0] * (1 - au) + c[1] * au;
    return 10.0f * (t > 1 ? 1 : t);
}

void WordFire_Frame(S32 x0, S32 y0, S32 x1, S32 y1) {
    S32 w = x1 - x0 + 1, h = y1 - y0 + 1;
    w = w > FIRE_W ? FIRE_W : w;
    h = h > FIRE_H ? FIRE_H : h;
    const U32 now = Host_TimeMs();
    if (!s_started || w != s_fw || h != s_fh) {  // a new box: start again, warmed up
        memset(s_heat, 0, sizeof s_heat);
        s_fw = w;
        s_fh = h;
        s_rng = 1997;
        for (S32 x = 0; x < FIRE_W; x++)
            s_wander[x] = Rand01();
        for (int i = 0; i < h * 2; i++)
            StepFlames();
        memcpy(s_prev, s_heat, sizeof s_heat);
        s_lastMs = now;
        s_acc = 0;
        s_started = true;
    }
    s_fx0 = x0;
    s_fy0 = y0;
    // As many steps as the clock owes, a few at most after a stall.
    const float period = 1000.0f / FIRE_HZ;
    s_acc += (float)(now - s_lastMs);
    s_lastMs = now;
    for (int n = 0; s_acc >= period; n++) {
        if (n == 8) {
            s_acc = 0;
            break;
        }
        memcpy(s_prev, s_heat, sizeof s_heat);
        StepFlames();
        s_acc -= period;
    }
    s_blend = s_acc / period;
    ScreenFont_SetFill(FlameLevel, FIRE_BANK, FIRE_LO, FIRE_HI, INK_RIM);
}
