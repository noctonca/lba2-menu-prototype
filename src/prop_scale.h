#pragma once

// The menu at a scale proportional to the screen instead of a whole step (EPmager's
// second round, point 4; switch propscale, F5 steps it; mode 5 decided 2026-11-16).
//
// The canvas becomes the screen divided by the proportional scale s: the wide
// composition s = min(W/960, H/540), the one column min(W/640, H/480), never under 1.
// At 1440x900 that is 1.5, a 960x600 canvas. The picture stays at the screen's own
// resolution as the Image row says. What the menu drew (frames, plasma, shading
// edges, arrows) is scaled to the screen linearly. The text depends on the mode:
//   0  off: today's whole step;
//   1  everything linear, text included;
//   2  text taken off the canvas and drawn on top, nearest neighbour at s
//      (sharp, but its pixels are 1 or 2 screen pixels wide by turns);
//   3  text on top at the whole step below s, placed where the layout puts it
//      (sharp and even, smaller than the boxes around it);
//   4  text on top at the whole step above s (sharp and even, larger than its boxes:
//      it runs into what is beside it, as the layout only knows 1x widths);
//   5  text on top, each font pixel its exact s x s square, partly covered screen
//      pixels blended by the share covered (sharp bilinear): one stroke weight, soft edge;
//   6  as 5 after Scale2x, which rounds diagonals and curves by half a font pixel.
// With the Font row on Zilla Slab the TrueType text is drawn at s in every mode.

#include <SYSTEM/ADELINE_TYPES.H>

#include <functional>

void PropScale_Set(int mode);
int PropScale_Mode();

// The proportional scale for a screen, wide or one column as the whole-step rule picks.
float PropScale_For(U32 w, U32 h, bool wide);

// Where the canvas (lw x lh) sits on a frame of w x h: the scale and the top left.
// The whole step when propscale is 0.
void PropScale_Map(U32 w, U32 h, float *s, float *bx, float *by);

// Start of a frame: forget last frame's text.
void PropScale_BeginFrame();

// True when text goes to the layer above the canvas instead of into it (modes 2 to 4).
bool PropScale_TextOnTop();

// Draw text through `draw` into the layer: `draw` paints into Log as usual, and is run
// twice over two fills to find its ink. (x, y) is where the text starts, the anchor for
// modes 3 and 4; the box bounds what it may touch, clipped to the clip as it is now.
void PropScale_Text(S32 x, S32 y, S32 x0, S32 y0, S32 x1, S32 y1, const std::function<void()> &draw);

// After the picture's own paint: the menu's pixels of the frame, linearly scaled.
void PropScale_PaintLinear(U32 *argb, U32 pitchBytes, U32 w, U32 h, const U8 *palette);
// The text layer over the frame.
void PropScale_PaintText(U32 *argb, U32 pitchBytes, U32 w, U32 h, const U8 *palette);
