// The More options page: every console setting with no row of its own in
// the Options, from lba2cc's settings tables (the menu plan's step 8). Most tune
// finished features; the few lba2cc calls experimental carry a tag. English only,
// labelled with the console name.
#pragma once

#include <stdio.h>

// Switch exptable (key 0; decided 2026-11-03): 1 = one dense table in the 8x8 font, as the 1997 key
// binding screen (design card ExperimentalTable); 0 = the game-font list, a page
// per group (ExperimentalList).
extern int sw_exptable;

void Exp_Open();   // from the "More options" row
void Exp_Close();  // back to Advanced and reset, as it was
bool Exp_Active();
void Exp_Update();
void Exp_Draw();
void Exp_WriteState(FILE *f);
// The table as the last Exp_Draw drew it, for the self test: its panel, its lines
// (group heads and rows) and its status line. false when no table was drawn.
struct ExpLine {
    char text[40], value[24];
    bool head, cursor, changed;
    bool first;          // the first row of its group
    bool experimental;   // tagged experimental after its name
    int x0, y0, x1, y1;  // the line's strip (the cursor's plasma when it is the cursor)
    int textLeft, textRight;  // the name
    int valueLeft, valueRight;  // the value, and on the cursor line its marks with it
};
struct ExpTable {
    int x0, y0, x1, y1;  // the panel
    int scale, statusTop;
    int valueEdge;       // where every value ends
    int marksRight;      // where the cursor line's right mark ends
    const char *status;
    int statusLeft, statusRight;
    int count;           // lines drawn
    const ExpLine *lines;
    int total, scroll;   // lines in the table, the first one drawn
};
bool Exp_Table(ExpTable *out);
const char *Exp_CursorName();  // the console name under the cursor, "" on the list's groups
const char *Exp_CursorValue();

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
