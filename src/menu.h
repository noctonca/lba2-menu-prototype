// The Options menu: pages as data, two layouts.
#pragma once

#include <stdio.h>

void Menu_Init(bool firstLaunch);
// Everything back as a fresh start has it: switches, settings, bindings, pages.
void Menu_Reset();
void Menu_Update();  // reads this frame's keys
void Menu_Draw();

// The prototype's switches (every review switch was pruned 2026-11-12). Names are what
// the control channel and the trace use.
//   overlay  1 = show the prototype's key list (F1)
//   padnames the pad's names: 0 the connected pad's, 1 Xbox, 2 PlayStation, 3 Nintendo (F11)
//   whole    1 = help shown whole, not typed (the self test)
//   keyboard 0 = as a console before a key is typed (no Keyboard view or Tab)
bool Menu_SetSwitch(const char *name, int value);
// A short note at the bottom right for two seconds, as switches show theirs.
void Menu_Toast(const char *text);
// Esc on the "Options closed" note: end the prototype.
bool Menu_WantsQuit();
// After the screen changed: the note with its size and canvas, and a trace line.
void Menu_NoteScreen();
int Menu_GetSwitch(const char *name);
bool Menu_Set(const char *setting, int value);  // a fake setting by name
bool Menu_Open(const char *page);  // jump to a page by name (cursor on its first row)

// Everything on screen and in the model, as one JSON object on one line.
void Menu_WriteState(FILE *out);
// Widths of every label, value and help string (game font, logical pixels).
void Menu_WriteMeasure(FILE *out);
