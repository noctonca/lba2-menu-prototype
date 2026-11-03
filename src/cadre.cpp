#include "cadre.h"

#include "engine.h"

#include <POLYGON/POLY.H>
#include <SVGA/CLIP.H>
#include <SVGA/GRAPH.H>
#include <SVGA/FIL_LINE.H>
#include <SVGA/PLOT.H>
#include <SVGA/SCREEN.H>

#include <stdlib.h>
#include <string.h>

// From lba2cc SOURCES/ANIMTEX.CPP, compiled from the submodule.
extern "C++" {
void InitPlasmaMenu(void);
void SelectPlasmaBank(U8 bank);
void DoTextureAnimation(void);
void FixePlasmaPtrMap(void);
}

// ANIMTEX builds the menu plasma inside the sky/sea texture; the prototype
// has no scene, so it owns the texture.
U8 *SkySeaTexture;

void Cadre_InitPlasma() {
    SkySeaTexture = (U8 *)calloc(256 * 256, 1);
    InitPlasmaMenu();
}

void Cadre_AnimatePlasma() {
    DoTextureAnimation();
}

// GAMEMENU.CPP DrawFireBar, with the bank passed in.
void Cadre_FireBar(S32 xmin, S32 ymin, S32 xmax, S32 ymax, S32 bank) {
    Struc_Point p[4];
    p[0].Pt_XE = (S16)xmin;
    p[0].Pt_YE = (S16)ymin;
    p[0].Pt_MapU = 0;
    p[0].Pt_MapV = 0;
    p[1].Pt_XE = (S16)xmin;
    p[1].Pt_YE = (S16)ymax;
    p[1].Pt_MapU = 0;
    p[1].Pt_MapV = (U16)((63 << 8) + 250);
    p[2].Pt_XE = (S16)xmax;
    p[2].Pt_YE = (S16)ymax;
    p[2].Pt_MapU = (U16)(2 * ((63 << 8) + 250));
    p[2].Pt_MapV = (U16)((63 << 8) + 250);
    p[3].Pt_XE = (S16)xmax;
    p[3].Pt_YE = (S16)ymin;
    p[3].Pt_MapU = (U16)(2 * ((63 << 8) + 250));
    p[3].Pt_MapV = 0;

    RepMask = 0x3f3f;
    SelectPlasmaBank((U8)bank);
    FixePlasmaPtrMap();
    Fill_Poly(POLY_TEXTURE, 0, 4, p);
}

// INVENT.CPP BackupAngles / RestoreAngles: the four chamfered corners keep
// whatever was behind the frame.
static U8 s_angles[48];

static bool AnglesFit(S32 x0, S32 y0, S32 x1, S32 y1) {
    if (x0 < 0 || y0 < 0)
        return false;
    if (x1 >= (S32)ModeDesiredX || y1 >= (S32)ModeDesiredY)
        return false;
    return x1 >= 4 && y1 >= 4;
}

static void BackupAngles(S32 x0, S32 y0, S32 x1, S32 y1) {
    U8 *dest = s_angles;
    U8 *src;
    if (!AnglesFit(x0, y0, x1, y1))
        return;
    const S32 stride = TabOffLine[1];

    src = (U8 *)Log + TabOffLine[y0] + x0;
    memcpy(dest, src, 4);
    dest[4] = src[4];
    dest[5] = src[stride];
    memcpy(dest + 6, src + stride + 1, 2);
    memcpy(dest + 8, src + stride * 2, 2);
    dest[10] = src[stride * 3];
    dest[11] = src[stride * 4];
    dest += 12;

    src = (U8 *)Log + TabOffLine[y1 - 4] + x0;
    dest[0] = src[0];
    dest[1] = src[stride];
    memcpy(dest + 2, src + stride * 2, 2);
    memcpy(dest + 4, src + stride * 3, 2);
    dest[6] = src[stride * 3 + 2];
    dest[7] = src[stride * 4];
    memcpy(dest + 8, src + stride * 4 + 1, 4);
    dest += 12;

    src = (U8 *)Log + TabOffLine[y0] + x1;
    memcpy(dest, src - 4, 4);
    dest[4] = src[0];
    dest[5] = src[stride - 2];
    memcpy(dest + 6, src + stride - 1, 2);
    memcpy(dest + 8, src + stride * 2 - 1, 2);
    dest[10] = src[stride * 3];
    dest[11] = src[stride * 4];
    dest += 12;

    src = (U8 *)Log + TabOffLine[y1 - 4] + x1;
    dest[0] = src[0];
    dest[1] = src[stride];
    memcpy(dest + 2, src + stride * 2 - 1, 2);
    memcpy(dest + 4, src + stride * 3 - 1, 2);
    dest[6] = src[stride * 3 - 2];
    dest[7] = src[stride * 4 - 4];
    memcpy(dest + 8, src + stride * 4 - 3, 4);
}

static void RestoreAngles(S32 x0, S32 y0, S32 x1, S32 y1) {
    U8 *src = s_angles;
    U8 *dest;
    if (!AnglesFit(x0, y0, x1, y1))
        return;
    const S32 stride = TabOffLine[1];

    dest = (U8 *)Log + TabOffLine[y0] + x0;
    memcpy(dest, src, 4);
    dest[4] = src[4];
    dest[stride] = src[5];
    memcpy(dest + stride + 1, src + 6, 2);
    memcpy(dest + stride * 2, src + 8, 2);
    dest[stride * 3] = src[10];
    dest[stride * 4] = src[11];
    src += 12;

    dest = (U8 *)Log + TabOffLine[y1 - 4] + x0;
    dest[0] = src[0];
    dest[stride] = src[1];
    memcpy(dest + stride * 2, src + 2, 2);
    memcpy(dest + stride * 3, src + 4, 2);
    dest[stride * 3 + 2] = src[6];
    dest[stride * 4] = src[7];
    memcpy(dest + stride * 4 + 1, src + 8, 4);
    src += 12;

    dest = (U8 *)Log + TabOffLine[y0] + x1;
    memcpy(dest - 4, src, 4);
    dest[0] = src[4];
    dest[stride - 2] = src[5];
    memcpy(dest + stride - 1, src + 6, 2);
    memcpy(dest + stride * 2 - 1, src + 8, 2);
    dest[stride * 3] = src[10];
    dest[stride * 4] = src[11];
    src += 12;

    dest = (U8 *)Log + TabOffLine[y1 - 4] + x1;
    dest[0] = src[0];
    dest[stride] = src[1];
    memcpy(dest + stride * 2 - 1, src + 2, 2);
    memcpy(dest + stride * 3 - 1, src + 4, 2);
    dest[stride * 3 - 2] = src[6];
    dest[stride * 4 - 4] = src[7];
    memcpy(dest + stride * 4 - 3, src + 8, 4);
}

static void DrawAngles(S32 x0, S32 y0, S32 x1, S32 y1, U8 up, U8 down) {
    Plot(x0 + 5, y0, up);
    Plot(x0 + 3, y0 + 1, up);
    Plot(x0 + 4, y0 + 1, up);
    Plot(x0 + 2, y0 + 2, up);
    Plot(x0 + 1, y0 + 3, up);
    Plot(x0 + 1, y0 + 4, up);
    Plot(x0, y0 + 5, up);

    Plot(x0, y1 - 5, up);
    Plot(x0 + 1, y1 - 4, up);
    Plot(x0 + 1, y1 - 3, up);
    Plot(x0 + 2, y1 - 2, up);
    Plot(x0 + 3, y1 - 1, down);
    Plot(x0 + 4, y1 - 1, down);
    Plot(x0 + 5, y1, down);

    Plot(x1, y1 - 5, down);
    Plot(x1 - 1, y1 - 4, down);
    Plot(x1 - 1, y1 - 3, down);
    Plot(x1 - 2, y1 - 2, down);
    Plot(x1 - 3, y1 - 1, down);
    Plot(x1 - 4, y1 - 1, down);
    Plot(x1 - 5, y1, down);

    Plot(x1 - 5, y0, up);
    Plot(x1 - 4, y0 + 1, up);
    Plot(x1 - 3, y0 + 1, up);
    Plot(x1 - 2, y0 + 2, up);
    Plot(x1 - 1, y0 + 3, down);
    Plot(x1 - 1, y0 + 4, down);
    Plot(x1, y0 + 5, down);
}

// The menu row draws its fill, then the frame, then puts the corners back
// (DrawOneChoice: BackupAngles before the fill, RestoreAngles after the frame).
// Cadre_Frame does the corner work itself when given the fill as a callback-free
// sequence: callers back up, fill, then call Cadre_Frame.
void Cadre_Frame(S32 x0, S32 y0, S32 x1, S32 y1, bool red) {
    U8 up = red ? COUL_UP_RED : COUL_UP_BLUE;
    U8 down = red ? COUL_DOWN_RED : COUL_DOWN_BLUE;
    Line(x0 + 5, y0, x1 - 5, y0, up);
    Line(x0, y0 + 5, x0, y1 - 5, up);
    Line(x1, y0 + 5, x1, y1 - 5, down);
    Line(x0 + 5, y1, x1 - 5, y1, down);
    RestoreAngles(x0, y0, x1, y1);
    DrawAngles(x0, y0, x1, y1, up, down);
}

void Cadre_BackupCorners(S32 x0, S32 y0, S32 x1, S32 y1) {
    BackupAngles(x0, y0, x1, y1);
}

// FLOW_A.CPP ShadeBoxBlk.
void Cadre_Shade(S32 x0, S32 y0, S32 x1, S32 y1, S32 deccoul) {
    if (!Engine_Pixels())
        return;
    if (x0 > ClipXMax || x1 < ClipXMin || y0 > ClipYMax || y1 < ClipYMin)
        return;
    if (x0 < ClipXMin)
        x0 = ClipXMin;
    if (x1 > ClipXMax)
        x1 = ClipXMax;
    if (y0 < ClipYMin)
        y0 = ClipYMin;
    if (y1 > ClipYMax)
        y1 = ClipYMax;

    U8 *dst = (U8 *)Log + TabOffLine[y0] + x0;
    U32 stride = TabOffLine[1];
    U32 clutOffset = 15 * 256;
    U8 hi = (U8)((clutOffset >> 8) & 0xFF);
    hi -= (U8)(deccoul & 0xFF);
    clutOffset = (clutOffset & 0xFFFF00FF) | ((U32)hi << 8);
    U8 *clut = PtrCLUTGouraud + clutOffset;

    for (S32 y = y0; y <= y1; y++) {
        for (S32 x = 0; x <= x1 - x0; x++)
            dst[x] = clut[dst[x]];
        dst += stride;
    }
}
