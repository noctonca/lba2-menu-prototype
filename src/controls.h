// The Controls page (Keys and buttons): ControlsWideReview in the design system,
// and a one-column form of it for 640x480.
#pragma once

#include <stdio.h>

// Switches the menu lists with its own:
//   devices   1 = one device per view, Tab switches (the review); 0 = Key 1, Key 2 and Gamepad side by side
//   padnames  0 = the connected pad's own names (Xbox when none), 1 Xbox, 2 PlayStation, 3 Nintendo
extern int sw_devices, sw_padnames;

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
    bool fits;
};
int Controls_Chips(ChipView *out, int max);
