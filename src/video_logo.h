#pragma once

// The LBA 2 logo as the 1997 intro builds it in, drawn in code at the screen's resolution
// (switch videologo, F9; the maintainer, 2026-11-17).
//
// The shape is assets/logo/lba2-logo.svg, traced from the intro video's settled frames and
// baked into the binary. At start it becomes a distance field in the logo's own units (one
// unit is one pixel of the 320x200 video), with the fill's colour beside it: the video's
// left-to-right ramp and its lit bevel. Each screen pixel looks the field up, so the edge is
// one screen pixel soft at any size, and what is behind the logo shows through.
//
// The build-in is the video's, measured from frames 144 to 203 of video.hqr entry 16 (see
// video_logo.cpp for each number): the logo fades in dark orange, a ring of water crosses it
// from the upper right with blue highlights and bends the letters as it passes, the logo
// brightens, the 2 lands in a swirl and sends a second ripple through the letters. After
// that the logo rests, and every 30 seconds the ripples cross it again (asked 2026-11-17).
//
// Like the screen font it is queued while the canvas is drawn and painted over the
// presented frame; a logo that something later in the frame drew over is left out.

#include <SYSTEM/ADELINE_TYPES.H>

void VideoLogo_Init();
bool VideoLogo_Ready();

// Width over height of the settled logo's ink.
float VideoLogo_Aspect();

// Start of a frame: forget last frame's logo.
void VideoLogo_BeginFrame();
// The logo fitted into the logical box x0..x1, y0..y1 (both ends included), centred.
// still: the settled logo whatever the clock says, as the game menu in play has it (no
// build-in and no ripple there, the maintainer, 2026-11-17); it does not start the build-in.
void VideoLogo_Queue(S32 x0, S32 y0, S32 x1, S32 y1, bool still = false);
// Round what the canvas gets over the logo that the logo stays above (the top line's note:
// at 640x480 the logo's place reaches it, and the note took the logo off for its 2 seconds).
// Anything drawn over the logo before Begin still takes it off.
void VideoLogo_NoteBegin();
void VideoLogo_NoteEnd();
// Play the build-in again from its first frame, on the next frame queued.
void VideoLogo_Restart();
// Play the idle ripple now, the build-in taken as over (for comparing the three ripples
// without waiting 30 seconds for each).
void VideoLogo_RippleNow();
// Milliseconds since the build-in ended (negative while it plays; very negative before the
// logo has been queued at all).
S32 VideoLogo_SinceBuiltMs();
// Resume game's opening (switch opening, the maintainer, 2026-11-18): the logo builds in and
// rests where the save's picture goes, then cross-fades into the picture. Called after this
// frame's Queue, with the picture's frame x0..x1, y0..y1 (both ends included). t < 0: the
// logo is drawn as usual, and the screen under it and the frame is kept. 0 to 1: the canvas
// has the picture, and the settled logo over what was kept fades into it (eased).
void VideoLogo_Opening(S32 x0, S32 y0, S32 x1, S32 y1, float t);
// The idle ripple: 1 at half the video's height, 2 at the video's, 3 at the video's with the blue ring.
void VideoLogo_SetIdle(int mode);
// The cc of LBA2 Classic Community under the 2 (assets/logo/cc.svg, ours and not the
// video's), in the logo's own colours. Off, the logo is as the intro has it. It arrives
// after the intro's build-in, as its own short beat.
void VideoLogo_SetCc(bool on);

// Paint the queued logo over a presented frame (w x h, pitch in bytes). The canvas sits
// where PropScale_Map puts it.
void VideoLogo_Paint(U32 *argb, U32 pitch, U32 w, U32 h);

// For comparing with the video: the moment `videoFrame` (144 is the first of the build-in,
// fractions allowed) drawn as the video frames it, `scale` screen pixels to a video pixel,
// over what `argb` (320 * scale by 200 * scale) already holds.
void VideoLogo_RenderVideoFrame(U32 *argb, U32 scale, float videoFrame, bool withRing);
