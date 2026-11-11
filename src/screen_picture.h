#pragma once

// The menu picture at the screen's own resolution (switch bgres).
//
// The menu is still drawn on the logical canvas, picture included. When the
// frame is presented, the canvas is enlarged by the whole step, and every
// canvas pixel that is still the picture (as drawn, or darkened by
// GamePieces_Shade) is taken instead from the picture scaled straight to the screen:
//   0  off: the picture is part of the canvas, as before;
//   1  whole step: the smallest whole factor that covers the screen, cropped,
//      nearest neighbour, so every source pixel is the same size;
//   2  smooth: the exact cover factor, cropped, Catmull-Rom.
// The darkening goes through the same fog rows as on the canvas, applied to
// the picture's palette indices before it is scaled.

#include <SYSTEM/ADELINE_TYPES.H>

void ScreenPicture_SetMode(int mode);
int ScreenPicture_Mode();

// After the picture is drawn on the canvas: remember it, forget the darkening.
void ScreenPicture_BeginFrame(U32 lw, U32 lh);

// The canvas has no picture this frame, only `fill`: the screen outside the canvas takes it.
void ScreenPicture_NoPicture(U8 fill);

// GamePieces_Shade's rectangle, clipped, and the fog row it darkened through.
void ScreenPicture_NoteShade(S32 x0, S32 y0, S32 x1, S32 y1, U32 row);

// The screen's indexed frame (pw x ph, `pitch` bytes a row) from Log (lw x lh)
// at whole step k, centred. `picture` is the 640x480 menu picture.
void ScreenPicture_Compose(U8 *out, U32 pitch, U32 pw, U32 ph, U32 lw, U32 lh, U32 k, const U8 *picture);

// Mode 2: paints the picture's pixels of the frame Compose built, in RGB.
void ScreenPicture_PaintSmooth(U32 *argb, U32 pitchBytes, U32 pw, U32 ph, const U8 *palette, const U8 *picture);
