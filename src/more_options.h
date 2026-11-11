// The More options page: every console setting with no row of its own in
// the Options, from lba2cc's settings tables (the menu plan's step 8). Most tune
// finished features; the few lba2cc calls experimental carry a tag. English only,
// labelled with the console name.
#pragma once

#include <stdio.h>

// One dense table in the 8x8 font, as the 1997 key binding screen (decided 2026-11-03;
// design card ExperimentalTable).

void MoreOptions_Open();   // from the "More options" row
void MoreOptions_Close();  // back to Advanced and reset, as it was
bool MoreOptions_Active();
void MoreOptions_Update();
void MoreOptions_Draw();
void MoreOptions_WriteState(FILE *f);
// The table as the last MoreOptions_Draw drew it, for the self test: its panel, its lines
// (group heads and rows) and its status line. false when no table was drawn.
struct MoreOptionsLine {
    char text[40], value[24];
    bool head, cursor, changed;
    bool first;          // the first row of its group
    bool experimental;   // tagged experimental after its name
    int x0, y0, x1, y1;  // the line's strip (the cursor's plasma when it is the cursor)
    int textLeft, textRight;  // the name
    int valueLeft, valueRight;  // the value, and on the cursor line its marks with it
};
struct MoreOptionsTable {
    int x0, y0, x1, y1;  // the panel
    int scale, statusTop;
    int valueEdge;       // where every value ends
    int marksRight;      // where the cursor line's right mark ends
    const char *status;
    int statusLeft, statusRight;
    int count;           // lines drawn
    const MoreOptionsLine *lines;
    int total, scroll;   // lines in the table, the first one drawn
    const char *rowHelp; // the row's help in the status lines
    bool rowHelpCut;     // it needed more than its two lines
};
bool MoreOptions_Table(MoreOptionsTable *out);
const char *MoreOptions_CursorName();  // the console name under the cursor
const char *MoreOptions_CursorValue();
const char *MoreOptions_CursorGroupHelp();  // the help of the group the cursor's row is in
bool MoreOptions_CursorPresetsSet();  // the cursor's row is a tuning value, which presets set

// Where the cursor is ("Auto camera | cam_ease_ms"), and how deep within the page.
int MoreOptions_Where(char *buf, size_t n);
// The page closed and every value at its default, as a fresh start has it.
void MoreOptions_Reset();

// The values, for the presets, Reset settings and their undo. Two kinds of row: tuning
// values (the Auto camera's), which a preset puts back to their defaults and which mark
// the preset changed; and the player's settings (mouse, pad, filters, From 1997), which
// no preset touches. Reset settings puts both back.
enum { MORE_OPTIONS_MAX = 48 };
int MoreOptions_Count();
const char *MoreOptions_Name(int i);  // the console name
bool MoreOptions_PresetsSet(int i);   // a tuning value: presets put it back to its default
void MoreOptions_Get(int *out);
void MoreOptions_Put(const int *in);
void MoreOptions_Defaults();          // every row, as Reset settings does
void MoreOptions_PresetDefaults();    // the tuning values only, as choosing a preset does
bool MoreOptions_AnyChanged();        // a value differs from its default
bool MoreOptions_PresetChanged();     // a tuning value differs from its default
bool MoreOptions_Set(const char *name, int value);  // `set cam_ease_ms 450`
