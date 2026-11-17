#include "game_scene.h"

#include "host.h"

#include <math.h>
#include <string.h>

// Colours are chosen in RGB and put in the menu palette through a table of the nearest index
// for each 5-bit RGB, with a 4x4 ordered dither so the sky's gradient does not band.

namespace {

struct Rgb {
    float r, g, b;
};

Rgb Mix(Rgb a, Rgb b, float k) {
    k = k < 0 ? 0 : k > 1 ? 1 : k;
    return {a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k};
}

float Smooth(float e0, float e1, float x) {
    float k = (x - e0) / (e1 - e0);
    k = k < 0 ? 0 : k > 1 ? 1 : k;
    return k * k * (3 - 2 * k);
}

U8 s_near[32 * 32 * 32];
const U8 *s_nearFor;

void BuildNear() {
    const U8 *pal = Host_Palette();
    if (s_nearFor == pal)
        return;
    for (int i = 0; i < 32 * 32 * 32; i++) {
        const int r = ((i >> 10) & 31) * 255 / 31, g = ((i >> 5) & 31) * 255 / 31, b = (i & 31) * 255 / 31;
        int best = 0, bestD = 1 << 30;
        for (int c = 1; c < 256; c++) {  // 0 stays the menu's black
            const int dr = pal[c * 3] - r, dg = pal[c * 3 + 1] - g, db = pal[c * 3 + 2] - b;
            const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
            if (d < bestD)
                best = c, bestD = d;
        }
        s_near[i] = (U8)best;
    }
    s_nearFor = pal;
}

const int kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

U8 Index(Rgb c, U32 x, U32 y) {
    const float d = (kBayer[y & 3][x & 3] - 7.5f) * 1.6f;
    auto q = [&](float v) {
        int i = (int)((v + d) * 31.0f / 255.0f + 0.5f);
        return i < 0 ? 0 : i > 31 ? 31 : i;
    };
    return s_near[(q(c.r) << 10) | (q(c.g) << 5) | q(c.b)];
}

U32 Hash(U32 a, U32 b) {
    U32 h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    return h ^ (h >> 12);
}

struct Look {
    Rgb skyTop, skyLow, seaFar, seaNear, landTop, landLow, sand;
    bool moon;  // no sea and no sun: stars, and Twinsun in the sky
};

const Look kLooks[5] = {
    // Citadel Island: a clear day, green hills
    {{70, 130, 210}, {190, 220, 240}, {60, 120, 175}, {25, 70, 130}, {95, 160, 75}, {45, 95, 45}, {220, 200, 150}, false},
    // Desert Island: a hot sky, dunes
    {{200, 140, 80}, {250, 215, 150}, {70, 140, 170}, {30, 90, 140}, {225, 185, 115}, {170, 120, 70}, {235, 205, 140}, false},
    // Emerald Moon: night, grey rock
    {{5, 5, 20}, {25, 35, 70}, {0, 0, 0}, {0, 0, 0}, {150, 150, 160}, {70, 70, 85}, {0, 0, 0}, true},
    // Otringal: overcast, dark stone
    {{95, 110, 140}, {170, 180, 195}, {55, 75, 100}, {25, 40, 65}, {130, 115, 100}, {70, 60, 55}, {170, 160, 140}, false},
    // Celebration Island: a green-blue haze
    {{60, 150, 160}, {185, 230, 210}, {50, 140, 150}, {20, 85, 105}, {125, 140, 70}, {70, 80, 40}, {215, 210, 150}, false},
};

}  // namespace

void GameScene_Draw(U8 *dst, U32 pitch, U32 w, U32 h, const GameSpot &spot) {
    BuildNear();
    const int island = spot.island < 0 || spot.island > 4 ? 0 : spot.island;
    const Look &L = kLooks[island];
    const U32 seed = Hash((U32)island, (U32)spot.scene);
    const float t = (float)spot.t / 1000.0f;
    const float H = (float)h;
    const float horizon = 0.58f;
    // The island: where it starts and ends, and its hills.
    const float left = -0.95f + 0.35f * (float)(seed % 7) / 6.0f, right = 0.55f + 0.4f * (float)((seed >> 3) % 7) / 6.0f;
    const float f1 = 3.0f + (float)((seed >> 6) % 5), f2 = 8.0f + (float)((seed >> 9) % 6);
    const float p1 = (float)((seed >> 12) % 628) / 100.0f, p2 = (float)((seed >> 18) % 628) / 100.0f;
    const float tall = 0.16f + 0.1f * (float)((seed >> 24) % 5) / 4.0f;
    const float sunU = -0.5f + (float)((seed >> 4) % 9) / 8.0f;
    // A building on the hills, by island: Citadel's lighthouse, Otringal's towers.
    const float towerU = left + (right - left) * (0.25f + 0.5f * (float)((seed >> 20) % 5) / 4.0f);

    for (U32 x = 0; x < w; x++) {
        const float u = ((float)x + 0.5f - (float)w / 2) / H;
        const float env = Smooth(left, left + 0.2f, u) * (1 - Smooth(right - 0.2f, right, u));
        const float shape = 0.55f + 0.3f * sinf(u * f1 + p1) + 0.15f * sinf(u * f2 + p2);
        float top = L.moon ? 0.62f - 0.05f * (0.5f + 0.5f * sinf(u * f1 + p1)) : horizon - env * tall * shape;
        // Buildings stand on the hill under them.
        bool building = false;
        if (!L.moon && env > 0.6f) {
            if (island == 0 && fabsf(u - towerU) < 0.018f) {
                building = true;
            } else if (island == 3) {
                for (int k = 0; k < 3; k++)
                    if (fabsf(u - (towerU - 0.12f + 0.12f * (float)k)) < 0.022f)
                        building = true;
            }
        }
        const float roof = building ? top - (island == 0 ? 0.14f : 0.09f) : top;
        const float shore = horizon + 0.035f * env;
        const float beach = 0.86f + 0.025f * sinf(u * 5.0f + p1);
        for (U32 y = 0; y < h; y++) {
            const float v = ((float)y + 0.5f) / H;
            Rgb c;
            if (v < roof || (!L.moon && v >= shore && v < beach) || (!L.moon && env <= 0.02f && v >= horizon && v < beach)) {
                if (v < horizon || L.moon) {  // the sky
                    c = Mix(L.skyTop, L.skyLow, v / horizon);
                    if (L.moon) {
                        // stars, fixed to the scene's own units so the picture has the same ones
                        const U32 su = (U32)(int)floorf((u + 4) * 180), sv = (U32)(int)floorf(v * 180);
                        if (Hash(su, sv + spot.scene * 977u) % 211 == 0)
                            c = {230, 230, 255};
                        // Twinsun, its ocean and a lit edge
                        const float du = u - 0.35f, dv = v - 0.22f, r2 = du * du + dv * dv;
                        if (r2 < 0.16f * 0.16f) {
                            const float lit = Smooth(0.16f, -0.05f, sqrtf((du + 0.05f) * (du + 0.05f) + (dv + 0.04f) * (dv + 0.04f)));
                            c = Mix({20, 50, 90}, {90, 170, 210}, lit);
                        }
                    } else {
                        const float du = u - sunU, dv = v - 0.17f, r = sqrtf(du * du + dv * dv);
                        c = Mix(c, {255, 250, 215}, Smooth(0.14f, 0.0f, r) * 0.5f);
                        if (r < 0.045f)
                            c = {255, 245, 190};
                        // three clouds drifting right
                        for (int k = 0; k < 3; k++) {
                            float cu = -1.3f + fmodf(0.9f * (float)k + 0.6f * (float)((seed >> (k * 5)) % 4) + t * 0.01f, 2.6f);
                            const float cv = 0.09f + 0.07f * (float)k;
                            float in = 0;
                            for (int e = -1; e <= 1; e++) {
                                const float eu = (u - cu - 0.06f * (float)e) / (0.08f - 0.02f * (float)(e * e));
                                const float ev = (v - cv + 0.012f * (float)(e == 0)) / 0.028f;
                                in = fmaxf(in, Smooth(1.0f, 0.7f, eu * eu + ev * ev));
                            }
                            c = Mix(c, {245, 245, 250}, in * 0.9f);
                        }
                    }
                } else {  // the sea
                    c = Mix(L.seaFar, L.seaNear, (v - horizon) / (beach - horizon));
                    // glints: short dashes on rows that thin out towards the horizon, drifting
                    const float depth = (v - horizon) / (beach - horizon);
                    const float rows = 14.0f + 40.0f * (1 - depth), gu = (u + 4 + t * 0.02f) * (6.0f + 18.0f * depth);
                    const U32 ru = (U32)(int)floorf(gu), rv = (U32)(int)floorf(v * rows * 3);
                    const float fu = gu - floorf(gu), fv = v * rows * 3 - floorf(v * rows * 3);
                    const U32 g = Hash(ru, rv);
                    if (g % 5 == 0 && fv < 0.35f && fabsf(fu - 0.5f) < 0.3f * (0.5f + 0.5f * sinf(t * 1.3f + (float)(g % 97))))
                        c = Mix(c, {235, 245, 255}, 0.45f + 0.25f * depth);
                }
            } else if (v >= beach && !L.moon) {  // the near beach
                c = Mix(L.sand, Mix(L.sand, {0, 0, 0}, 0.35f), (v - beach) / (1 - beach));
            } else if (v < top) {  // a building
                c = island == 0 ? (((int)((v - roof) * 60.0f) & 1) ? Rgb{235, 235, 230} : Rgb{200, 60, 50})
                                : Mix(Rgb{150, 145, 150}, Rgb{90, 85, 95}, (v - roof) / (top - roof));
                if (island == 3 && fmodf((v - roof) * 40.0f, 1.0f) < 0.25f && fabsf(fmodf(u * 200.0f, 2.0f) - 1) < 0.4f)
                    c = {255, 220, 140};  // lit windows
            } else {  // the land
                c = Mix(L.landTop, L.landLow, (v - top) / 0.3f);
                if (L.moon) {  // craters
                    const U32 cu = (U32)(int)floorf((u + 4) * 9), cv = (U32)(int)floorf(v * 9);
                    if (Hash(cu, cv + seed) % 3 == 0) {
                        const float fu = (u + 4) * 9 - floorf((u + 4) * 9) - 0.5f, fv = v * 9 - floorf(v * 9) - 0.5f;
                        if (fu * fu + fv * fv * 4 < 0.09f)
                            c = Mix(c, {40, 40, 50}, 0.5f);
                    }
                }
            }
            dst[(size_t)y * pitch + x] = Index(c, x, y);
        }
    }
}

void GameScene_Picture(U8 *out, const GameSpot &spot) {
    GameScene_Draw(out, SAVE_PICTURE_W, SAVE_PICTURE_W, SAVE_PICTURE_H, spot);
}
