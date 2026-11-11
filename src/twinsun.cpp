#include "twinsun.h"

#include <3D.H>
#include <POLYGON/POLY.H>
#include <POLYGON/TESTVUE.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SYSTEM/HQRLOAD.H>
#include <SYSTEM/LE.H>
#include <SYSTEM/LOADMALL.H>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// As HOLO.H.
#define RAYON_PLANET 1000
#define ZOOM_PLANET 8000
#define STEP_ANGLE (32 * 4)
#define COLS ((4096 / STEP_ANGLE) + 1)  // 33 longitudes, the last closing the seam
#define ROWS ((2048 / STEP_ANGLE) + 1)  // 17 latitudes, pole to pole
#define SLIG (COLS * 2)
#define HQR_COORMAPP_HMM 0
#define HQR_TWINSUN_HMT 1
#define HQR_TWINSUN_HMG 2

// One turn in three minutes: one and two were too fast at 140% (play tests, 2026-11-11).
#define TURN_MS 180000
// The holomap's own tilt is wherever FindAngleIsland lands for the island; this
// looks up at the south pole, the cloud belt a halo above it (play test, 2026-11-11).
#define VIEW_ALPHA 3328
// 140% of the holomap's size (play test, 2026-11-11).
#define SIZE_PERCENT 140
// The holomap's projection factor at 480 lines; scaled with the canvas height.
#define PROJ_FACTOR 1024

#define STARS 220

static U16 *s_mapping;  // CoorMapp.HMM: u, v per grid point
static U8 *s_alt;       // Twinsun .HMT: a height per grid point (255 = cloud belt)
static U8 *s_texture;   // Twinsun .HMG: 256 x 256, menu palette
static S16 s_coor[COLS * ROWS * 3];
static U16 s_proj[COLS * ROWS * 2];
static U32 s_tri[(COLS - 1) * (ROWS - 1)];  // depth << 16 | offset into s_proj
static bool s_ready;

static struct Star {
    U16 x, y;  // in 1/65536 of the canvas
    U8 colour;
} s_stars[STARS];

// ComputeCoorGlobe from HOLOGLOB.CPP: the grid's points in the planet's frame.
static void ComputeCoor() {
    S16 *c = s_coor;
    const U8 *alt = s_alt;
    SetAngleCamera(0, 0, 0);
    for (S32 alpha = -1024; alpha <= 1024; alpha += STEP_ANGLE) {
        const U8 *first = alt;
        for (S32 beta = 0; beta < 4096; beta += STEP_ANGLE) {
            const S32 r = *alt < 255 ? RAYON_PLANET + *alt * 2 : RAYON_PLANET + 500;
            alt++;
            Rotate(r, 0, alpha);
            const S32 x = X0, y = Z0;
            Rotate(x, 0, beta);
            WorldRotatePoint(X0, y, Z0);
            *c++ = (S16)X0;
            *c++ = (S16)Y0;
            *c++ = (S16)Z0;
        }
        Rotate(RAYON_PLANET + *first * 2, 0, alpha);
        const S32 x = X0, y = Z0;
        Rotate(x, 0, 0);
        WorldRotatePoint(X0, y, Z0);
        *c++ = (S16)X0;
        *c++ = (S16)Y0;
        *c++ = (S16)Z0;
    }
}

static void InitStars() {
    // A fixed seed, so captures repeat; mostly dim greys (menu palette 52-63).
    U32 seed = 0x7457u;
    for (int i = 0; i < STARS; i++) {
        seed = seed * 1103515245u + 12345u;
        s_stars[i].x = (U16)(seed >> 16);
        seed = seed * 1103515245u + 12345u;
        s_stars[i].y = (U16)(seed >> 16);
        seed = seed * 1103515245u + 12345u;
        const U32 r = (seed >> 16) % 100;
        s_stars[i].colour = r < 60 ? 52 + r % 4 : r < 92 ? 57 + r % 4 : 63;
    }
}

bool Twinsun_Init(const char *path) {
    s_mapping = (U16 *)LoadMalloc_HQR(path, HQR_COORMAPP_HMM);
    s_alt = (U8 *)LoadMalloc_HQR(path, HQR_TWINSUN_HMT);
    s_texture = (U8 *)LoadMalloc_HQR(path, HQR_TWINSUN_HMG);
    if (!s_mapping || !s_alt || !s_texture) {
        fprintf(stderr, "no holomap.hqr: the Twinsun background is black\n");
        return false;
    }
    FromLE16InPlace(s_mapping, COLS * ROWS * 2);
    ComputeCoor();
    InitStars();
    s_ready = true;
    return true;
}

bool Twinsun_Ready() { return s_ready; }

static int ByDepthFarFirst(const void *a, const void *b) {
    const U32 da = *(const U32 *)a >> 16, db = *(const U32 *)b >> 16;
    return da < db ? 1 : da > db ? -1 : 0;
}

void Twinsun_Draw(U32 w, U32 h, U32 timeMs) {
    U8 *log = (U8 *)Log;
    for (U32 y = 0; y < h; y++)
        memset(log + TabOffLine[y], 0, w);
    for (int i = 0; i < STARS; i++)
        log[TabOffLine[s_stars[i].y * h >> 16] + (s_stars[i].x * w >> 16)] = s_stars[i].colour;
    if (!s_ready)
        return;

    const S32 factor = PROJ_FACTOR * (S32)h * SIZE_PERCENT / (480 * 100);
    SetProjection((S32)w / 2, (S32)h / 2, 128, factor, factor);
    const S32 beta = (S32)((U64)(timeMs % TURN_MS) * 4096 / TURN_MS);
    SetFollowCamera(0, 0, 0, VIEW_ALPHA, beta, 0, ZOOM_PLANET);

    // ComputeGlobeProj from HOLOGLOB.CPP; the quads are sorted far first here
    // instead of going through the holomap's z-buffer.
    const S16 *c = s_coor;
    U16 *p = s_proj;
    U32 *t = s_tri;
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            WorldRotatePoint(c[0], c[1], c[2]);
            c += 3;
            if (row < ROWS - 1 && col < COLS - 1)
                *t++ = (U32)(U16)(CameraZr - Z0) << 16 | (U32)(p - s_proj);
            LongProjectPoint(X0, Y0, Z0);
            *p++ = (U16)Xp;
            *p++ = (U16)Yp;
        }
    }
    qsort(s_tri, (COLS - 1) * (ROWS - 1), sizeof s_tri[0], ByDepthFarFirst);

    // DrawHolomap from HOLOGLOB.CPP, with the plain texture filler.
    U8 *const memoMap = PtrMap;
    const S32 memoMask = RepMask;
    PtrMap = s_texture;
    RepMask = 0xFFFF;
    SetClip(0, 0, (S32)w - 1, (S32)h - 1);
    Struc_Point pts[3];
    memset(pts, 0, sizeof pts);
    for (U32 n = 0; n < (COLS - 1) * (ROWS - 1); n++) {
        const U32 off = s_tri[n] & 0xFFFF;
        const U16 *q = s_proj + off;
        const U16 *m = s_mapping + off;
        pts[0].Pt_XE = q[0], pts[0].Pt_YE = q[1];
        pts[1].Pt_XE = q[SLIG], pts[1].Pt_YE = q[1 + SLIG];
        pts[2].Pt_XE = q[2], pts[2].Pt_YE = q[3];
        if (TestVuePoly(pts)) {
            pts[0].Pt_MapU = m[0], pts[0].Pt_MapV = m[1];
            pts[1].Pt_MapU = m[SLIG], pts[1].Pt_MapV = m[1 + SLIG];
            pts[2].Pt_MapU = m[2], pts[2].Pt_MapV = m[3];
            Fill_Poly(POLY_TEXTURE, 0, 3, pts);
        }
        pts[0].Pt_XE = q[SLIG], pts[0].Pt_YE = q[1 + SLIG];
        pts[1].Pt_XE = q[2 + SLIG], pts[1].Pt_YE = q[3 + SLIG];
        pts[2].Pt_XE = q[2], pts[2].Pt_YE = q[3];
        if (TestVuePoly(pts)) {
            pts[0].Pt_MapU = m[SLIG], pts[0].Pt_MapV = m[1 + SLIG];
            pts[1].Pt_MapU = m[2 + SLIG], pts[1].Pt_MapV = m[3 + SLIG];
            pts[2].Pt_MapU = m[2], pts[2].Pt_MapV = m[3];
            Fill_Poly(POLY_TEXTURE, 0, 3, pts);
        }
    }
    UnsetClip();
    PtrMap = memoMap;
    RepMask = memoMask;
}
