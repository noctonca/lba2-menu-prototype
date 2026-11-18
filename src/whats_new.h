// What a new build tells a player (docs/plan/MENU_PLAN.md open question 16, "not designed"),
// a trial behind switch `whatsnew`, Shift+F1. The notes are rows, not prose, computed per
// player: each thing carries the build it arrived in and the build Modern turned it on, and
// the config remembers the last build the player ran. Three quiet layers:
//   1. a dot after a new row's name, and on the path to it and on the path to it (the game menu's Options, the category, the
//      row), gone once the cursor has been on the row and left it;
//   2. one line in the game menu's voice after an update, gone once What's new was opened;
//   3. the What's new page under Options: now on in Modern, new settings, to try; its row is
//      there for a whole run that starts with something unseen, and gone from the next run
//      that starts with nothing unseen.
// The line is for the first run of the build alone. A fresh install sees none of it; bug fixes stay in README.txt and the changelog.
//
// Switch whatsnew: 0 off, as before the trial; 1 the first run of this build by a player who
// last ran 0.13.2 (a pretend previous build, so some rows are new); 2 a later run of it: no
// line, the marks still on what was not seen; 3 a fresh install, which remembers no build.
// Quit and starting again in the prototype is a later run (WhatsNew_NextRun).
#pragma once

#include "menu_parts.h"

#include <stdio.h>

enum { WN_ON_IN_MODERN, WN_NEW, WN_TRY, WN_GROUPS };

// The page's rows, made from the table of things (whats_new.cpp), into pages[P_WHATSNEW].
void WhatsNew_Pages(Page *pages);
// The switch is on and this player ran an older build: something to tell.
bool WhatsNew_Any();
bool WhatsNew_Offered();  // Options' What's new row is there this run
int WhatsNew_Count();  // the things new to this player
// The game menu's line: the first run of the build, something new, What's new not opened yet.
bool WhatsNew_LineShown();
// The row wears the mark: new itself and not seen yet, or on the way to such a row.
bool WhatsNew_RowNew(const Row &r);
bool WhatsNew_TuningNew(int i);  // More options' row i, the same
// Each frame the menu is up: the row under the cursor (NULL for none). A new row is seen once
// the cursor has been on it and left it, so its mark is there while the cursor is on it.
void WhatsNew_Frame(const Row *cursor);
void WhatsNew_Reset();    // as a start has it: nothing seen, the page not opened
void WhatsNew_NextRun();  // the game started again: a later run of the same build
void WhatsNew_WriteState(FILE *f);
