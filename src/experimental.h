// The Experimental settings page: every tuning value of the Experimental ring,
// generated from a settings table (the menu plan's step 8), behind Show
// experimental settings. English only, labelled with the console name.
#pragma once

#include <stdio.h>

// Switch exptable (key 0; decided 2026-11-03): 1 = one dense table in the 8x8 font, as the 1997 key
// binding screen (design card ExperimentalTable); 0 = the game-font list, a page
// per group (ExperimentalList).
extern int sw_exptable;

void Exp_Open();   // from the "Experimental settings" row
void Exp_Close();  // back to Advanced and reset, as it was
bool Exp_Active();
void Exp_Update();
void Exp_Draw();
void Exp_WriteState(FILE *f);
// Where the cursor is ("Auto camera | cam_ease_ms"), and how deep within the page.
int Exp_Where(char *buf, size_t n);
// The page closed and every value at its default, as a fresh start has it.
void Exp_Reset();

// The values, for the presets, Reset settings and their undo: a preset or a reset
// puts every tuning value back to its default; changing one marks the preset changed.
enum { EXP_MAX = 48 };
int Exp_Count();
const char *Exp_Name(int i);  // the console name
void Exp_Get(int *out);
void Exp_Put(const int *in);
void Exp_Defaults();
bool Exp_AnyChanged();  // a value differs from its default
bool Exp_Set(const char *name, int value);  // `set cam_ease_ms 450`
