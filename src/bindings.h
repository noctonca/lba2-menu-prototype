// The 36 actions with their keys and buttons, and the page that edits them (Keys
// and buttons, the Controls page): ControlsWideReview in the design system, and a
// one-column form of it for 640x480.
#pragma once

#include <stdio.h>

// Switches the menu lists with its own:
//   devices   1 = one device per view, Tab switches (the review); 0 = Key 1, Key 2 and Gamepad side by side
//   padnames  0 = the connected pad's own names (Xbox when none), 1 Xbox, 2 PlayStation, 3 Nintendo
//   heads     wide: 1 = the column names inside the first row, above its chips; 0 = a line of heads above the rows
extern int sw_padnames;

// The page's place is on the menu's stack (menu_parts.h): the groups page, then a group's page
// of binding rows, which the menu's loop walks, opens and leaves as any other page. What a
// binding row has besides is here: the chip along it, the device shown, the wait for a key
// and the question about a key another action has.
void Bindings_Open();   // from the "Keys and buttons" row, before its page opens: the device used last
void Bindings_Close();  // left by other means (`open`): nothing waits or asks
bool Bindings_Active(); // a Keys and buttons page is on top
void Bindings_GroupOpened();       // a group's page opened: its first chip
void Bindings_SwitchDevice();      // Tab
bool Bindings_StepColumn(int dir); // Left, Right along the chips; false at either end
void Bindings_Enter();             // Enter on a binding row: wait for a key
void Bindings_Clear();             // Del on a binding row
bool Bindings_Busy();              // waiting for a key, or asking
void Bindings_UpdateBusy();        // while busy: takes every key
void Bindings_Say();               // the help, or what the wait or the question says
void Bindings_Draw();
void Bindings_WriteState(FILE *f);

// Bindings back to the defaults and the page closed, as a fresh start has it.
void Bindings_Reset();
// The bindings as values, for Reset settings and Undo reset (the page stays as it is).
enum { BINDINGS_MAX = 256 };
int Bindings_Count();             // values Bindings_Get writes
void Bindings_Get(int *out);
void Bindings_Put(const int *in);
void Bindings_PutDefaults();
bool Bindings_AreDefault();
// The chips the last Bindings_Draw drew, for the self test.
struct ChipView {
    const char *action;
    const char *column;
    const char *name;
    const char *fill;  // shade, cursor or waiting
    bool fits, conflict;
    int x0, y0, x1, y1;
};
int Bindings_Chips(ChipView *out, int max);
// The column heads, above the actions or inside the first row, as the last Bindings_Draw drew them.
struct HeadView {
    const char *text;
    int x0, y0, x1, y1;
};
int Bindings_Heads(HeadView *out, int max);
// Every clash in the bindings: action's key in column is also other's.
struct ConflictView {
    const char *action, *column, *other;
};
int Bindings_Conflicts(ConflictView *out, int max);
// Where the cursor is ("Keyboard | Movement | Forward | Key 1 | Up | waiting"),
// and how deep within the page: 1 the groups, 2 the actions, 3 waiting for a key
// or asking whether to swap one another action has ("| asking").
int Bindings_Where(char *buf, size_t n);
// What an action's chip in a column says (0 Key 1, 1 Key 2, 2 the pad).
bool Bindings_Text(const char *action, int column, char *out, size_t n);
// The pad's Start as the voice says it (Menu, Options, Plus).
void Bindings_StartSaid(char *out, size_t n);
// A key or button's name as a sentence says it: "Esc" -> "<ESC>", "D-pad \x18" -> "<D-PAD UP>".
void Bindings_SaidKey(const char *name, char *out, size_t n);
// A legend key ("Enter", "Esc", "Del", "Tab") as the pad names it after a pad press.
const char *Bindings_LegendKey(const char *key);
