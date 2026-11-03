// The Options menu: pages as data, two layouts, the review's open calls as switches.
#pragma once

#include <stdio.h>

void Menu_Init(bool firstLaunch);
// Everything back as a fresh start has it: switches, settings, bindings, pages.
void Menu_Reset();
void Menu_Update();  // reads this frame's keys
void Menu_Draw();

// Switches for the open calls (and a few prototype aids). Names are what the
// control channel and the trace use.
//   band     1 = one band (help + legend), 0 = help line + prompt bar   (open call 1)
//   typing   0 = type on every cursor move, 1 = type on page entry only, 2 = no typing (2)
//   chip     8x8 font scale on the logical canvas, 1 or 2 (2 decided)     (3)
//   tablefont  the Experimental table's 8x8 font scale, 1 or 2 (1 decided)
//   word     preset row word: 0 Settings, 1 Preset, 2 Rules              (6)
//   island   1 = Citadel's palette (Options opened in play), 0 = menu palette
//   help640  1 = help in the dialogue window on every one-column page (decided);
//            0 = only first launch, questions, the preset page and Keys and buttons
//   gameplay 1 = show the Gameplay category
//   overlay  1 = show the prototype's key list
//   sheet    1 = the wide layout is one 960x540 composition, centred; 0 = stretched to the margins
//   legend   1 = the band's legend stacked at its right; 0 = in a row beside the voice
//   bg       1 = the background scaled evenly and cropped; 0 = stretched, as lba2cc does
//   scale    1 = the largest whole step the 960x540 composition fits, else the one column; 0 = the
//            plan's rule. Set at launch (--switch scale=N), since the canvas depends on it.
//   panes    1 = one pane at a time, the page's name as a title (decided 2026-11-03); 0 = two panes
//   bandshade the band's darkening level; 15 as decided 2026-11-03; 0 = BAND_SHADE_LVL (11), as before
//   short    1 = the shorter help, for translation (kShortHelp; decided 2026-11-03); 0 = as first written
//   narrow   (decided 2026-11-03) 1 = one column: key chips 58 wide at 1x (116 at 2x), not 60, so the longest
//            action name keeps 8 clear of them; 0 = 60
//   clash    (decided 2026-11-03) 1 = a key clash's note replaces the action's help in the voice; 0 = added after it
//   exptable (decided 2026-11-03) 1 = the Experimental settings page as one dense table in the 8x8 font; 0 = the game-font list
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
