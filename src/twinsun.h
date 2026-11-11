#pragma once

// The Background row's Twinsun value: the holomap's planet turning slowly behind
// the menu, on a still field of stars (asked for 2026-11-11).
//
// The globe is lba2cc's holomap globe (SOURCES/HOLOGLOB.CPP: ComputeCoorGlobe,
// ComputeGlobeProj, DrawHolomap) without the arrows, Twinsen or the dialogue,
// drawn on the logical canvas in the menu palette. HOLOMAP.HQR entries 0-2 are
// the same in the demo, the US CD, GOG and Steam.

#include <SYSTEM/ADELINE_TYPES.H>

// Loads the globe from holomap.hqr. False when the file or an entry is missing.
bool Twinsun_Init(const char *holomapHqr);
bool Twinsun_Ready();

// Tuning (switches tstilt, tssize): the view's tilt in 1/4096 turns, the size in percent.
void Twinsun_SetTilt(int alpha);
void Twinsun_SetSizePercent(int percent);

// Draws the stars and the globe into Log (w x h), turned for `timeMs`.
void Twinsun_Draw(U32 w, U32 h, U32 timeMs);
