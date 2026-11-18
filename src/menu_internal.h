// What the menu's own files share: menu.cpp (the loop), menu_pages.cpp (the row tables),
// menu_draw.cpp (the view) and menu_state.cpp (what scripts and tests read and set); and
// whats_new.cpp, a trial that marks rows across every page.
//
// Not for a page's own file: bindings.cpp and more_options.cpp use menu_parts.h, as a page
// added later does. Not for the settings or the game either: those are asked through
// menu_settings.h and game.h.
//
// This list is the coupling between the four, so it is the thing to keep short: a name
// added here is one more thing a change to one file can break in another. Each is under
// the file that owns it. The others read it; where one writes it, the line says so.
#pragma once

#include "game.h"
#include "host.h"
#include "menu.h"
#include "menu_parts.h"
#include "menu_settings.h"
#include "more_options.h"
#include "ui.h"

#include <stddef.h>

// ───────────────────────────── menu_pages.cpp: the rows ─────────────────────────────

// What a row's `arg` names, for the kinds that have a table of things to do.
enum { A_KEYS, A_RESET, A_EXP, A_CONSOLE, A_TRY, A_COUNT };  // R_ACTION: opens Keys and buttons, asks to reset,
    // opens More options, opens the console, opens More options on What's new's To try row (kActionDo)
enum { M_RESUME, M_NEW, M_LOAD, M_SAVE, M_OPTIONS, M_QUIT, M_COUNT };  // R_GAME (kGameDo)
enum { PAGE_ROWS_MAX = MORE_OPTIONS_MAX };  // More options is the longest page

extern Page g_pages[P_COUNT];  // bindings.cpp and more_options.cpp fill theirs in as the menu starts
extern const Page kGamePages[3];
const char *FormatPresetRow(char *buf, size_t n, const char *name, bool changed);
extern const char *kVersion;
extern const char *kVersionString;
extern const char *kCommit;

// ───────────────────────────── menu.cpp: the loop ─────────────────────────────

// Where the player is: one stack of pages, each with the row its cursor is on.
struct Place {
    int page;
    int row;
};
extern Place g_stack[8];
extern int g_depth;
Place &Top();
int TopPage();
const PageHooks *TopHooks();
const Row *CursorRow();
void Push(int page);
int OpeningRow(int page);  // the row Push puts the cursor on
void CursorTo(int page, int row);
void PutCursorOn(RowType type, int arg);
void Activate(const Row &r);
void OpenGameMenu();
bool BackShown();  // the corner Back is offered: a pointer is in use and Esc goes somewhere
// The legend: only the keys the row under the cursor takes, as its kind and its page say.
void LegendFor(const Row *r, const char **keys, const char **words, int *n);
bool MenuClosed();  // the game, or the screen after Quit, is up in the menu's place

// A row as its kind and its page have it.
const RowKind &KindOf(const Row &r);
const char *RowLabel(const Row &r);
bool RowActive(const Row &r);
const char *PresetRowLabel(char *buf, size_t n);
int ShownRows(int page, int *out);
int Position(int page, int row);

// The question that is up, and its cursor (menu_parts.h Question).
extern const Question *g_asking;
extern int g_answer;  // 0 No, the safe answer, where it starts; 1 Yes

bool Wide();
// The first shown row drawn. The loop starts it again on a new page; the view moves it to
// keep the cursor in sight and for the wheel and the scroll bar (menu_draw.cpp writes it).
extern int g_scroll;
extern int g_wheelView;  // notches turned this frame, for a page that scrolls (Menu_WheelView)
extern int g_scrollTo;   // the first line the scroll bar asked for this frame; -1 none

// The pointer's targets, as the view lays them out each frame (Menu_Point).
typedef MenuPoint PointTarget;
enum { POINT_MAX = 96 };
extern PointTarget g_points[POINT_MAX];
extern int g_pointCount;  // the view starts it again each frame (menu_draw.cpp writes it)
extern ScrollBarInfo g_bar;  // the last draw's scroll bar (menu_draw.cpp writes it)

enum { CON_CLOSED, CON_FULL, CON_READONLY };
extern int g_console;
extern Voice g_voice;
void UpdateVoice();
extern char g_toast[128];
extern U32 g_toastUntil;
void Toast(const char *fmt, const char *a, int v);
extern bool g_quit;  // Esc on the Quit screen: the prototype ends (a fullscreen window has no close box)

// ───────────────────────────── menu_draw.cpp: the view ─────────────────────────────

// What was drawn last frame, for `state` and the tests.
enum { DRAWN_MAX = 40 };  // the most rows a frame draws: a full wide screen is under 30
extern DrawnRow g_drawn[DRAWN_MAX];
extern int g_drawnCount;
struct HelpBox {
    bool shown;
    int x0, y0, x1, y1;
    const char *kind;  // band, help-line, dialogue
};
extern HelpBox g_helpBox;
// The box the legend's chips and words took, and the 8x8 font's scale.
struct LegendBox {
    bool shown;
    int x0, y0, x1, y1, scale;
};
extern LegendBox g_legendBox;
struct BackBox {
    bool shown;
    S32 x0, y0, x1, y1;
};
extern BackBox g_backBox;
extern char g_legend[128];
extern char g_pageMark[8];
extern int g_voiceLines, g_voiceNeeded, g_voiceMax, g_voiceWidth;
extern S32 g_consoleBottom;  // the line under the console, for `state`
// The loop's one question about room: does this line fit the voice it would be said in?
// (It says a row's short help where the help would not.)
bool VoiceFits(const char *text, const Row *r);

// ───────────────────────────── menu_state.cpp: the switches ─────────────────────────────

extern int sw_overlay;
extern int sw_whole;
extern int sw_propscale;
extern int sw_zsub;
extern int sw_videologo;
extern int sw_logosub;
extern int sw_logocc;
extern int sw_legendfit;
extern int sw_greencurrent;
extern int sw_longhelp;
extern int sw_whatsnew;  // whats_new.cpp owns it
int LastPointer();  // the pointer used last, or the one a test fixed
