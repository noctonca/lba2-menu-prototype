// The Controls page (Keys and buttons): ControlsWideReview in the design system,
// and a one-column form of it for 640x480.
#pragma once

#include <stdio.h>

// Switches the menu lists with its own:
//   devices   1 = one device per view, Tab switches (the review); 0 = Key 1, Key 2 and Gamepad side by side
//   padnames  0 = the connected pad's own names (Xbox when none), 1 Xbox, 2 PlayStation, 3 Nintendo
//   heads     wide: 1 = the column names inside the first row, above its chips; 0 = a line of heads above the rows
extern int sw_devices, sw_padnames, sw_heads;

void Controls_Open();   // from the "Keys and buttons" row
void Controls_Close();  // back to the Options, as they were
bool Controls_Active();
void Controls_Update();
void Controls_Draw();
void Controls_WriteState(FILE *f);

// Bindings back to the defaults and the page closed, as a fresh start has it.
void Controls_Reset();
// The chips the last Controls_Draw drew, for the self test.
struct ChipView {
    const char *action;
    const char *column;
    const char *name;
    const char *fill;  // shade, cursor or waiting
    bool fits, conflict;
    int x0, y0, x1, y1;
};
int Controls_Chips(ChipView *out, int max);
// The column heads, above the actions or inside the first row, as the last Controls_Draw drew them.
struct HeadView {
    const char *text;
    int x0, y0, x1, y1;
};
int Controls_Heads(HeadView *out, int max);
// Every clash in the bindings: action's key in column is also other's.
struct ConflictView {
    const char *action, *column, *other;
};
int Controls_Conflicts(ConflictView *out, int max);
// Where the cursor is ("Keyboard | Movement | Forward | Key 1 | Up | waiting"),
// and how deep within the page: 1 the groups, 2 the actions, 3 waiting for a key
// or asking whether to swap one another action has ("| asking").
int Controls_Where(char *buf, size_t n);
// What an action's chip in a column says (0 Key 1, 1 Key 2, 2 the pad).
bool Controls_Binding(const char *action, int column, char *out, size_t n);
// The pad's Start as the voice says it (Menu, Options, Plus).
void Controls_StartSaid(char *out, size_t n);
// A key or button's name as a sentence says it: "Esc" -> "<ESC>", "D-pad \x18" -> "<D-PAD UP>".
void Controls_SaidKey(const char *name, char *out, size_t n);
// A legend key ("Enter", "Esc", "Del", "Tab") as the pad names it after a pad press.
const char *Controls_LegendKey(const char *key);
