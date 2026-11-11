// What menu.cpp shares with the pages drawn in their own files (controls.cpp).
#pragma once

#include "ui.h"

#include <stdio.h>

// One row as drawn this frame, for `state`.
struct DrawnRow {
    char label[96];
    char value[48];
    const char *pane;
    const char *fill;
    int x0, y0, x1, y1;
    bool dim, changed, marks;
    bool setting;   // a setting row: it shows a value
    int textLeft;   // where the label starts
    int textRight;  // where the label (or value) ends; > x1 means it spills
    int valueRight; // the last pixel column of what the row shows on the right (a value, a
                    // slider, steps, key chips); 0 when it shows nothing there
};
DrawnRow &Menu_Record(const char *pane, const char *label, const char *value, RowFill fill, int x0, int y0, int x1,
                      int y1);

void Menu_RowLabelCentred(S32 x0, S32 x1, S32 yc, const char *s, U8 colour);
void Menu_DrawScrollBar(S32 x0, S32 y0, S32 y1, int first, int visible, int total);

// Whether pages take the wide layout, and the frame it is composed in. With the
// `sheet` switch off, the frame is the whole canvas (the wide layout from 840
// logical pixels, stretched to the margins). With it on, the wide layout is one
// 960x540 composition, centred on any larger canvas as the 1997 640x480 one is,
// and a canvas it does not fit takes the one column.
bool Menu_Wide();
struct WideFrame {
    S32 x0, y0, w, h;
};
WideFrame Menu_WideFrame();
// The `panes` switch: 1 (decided 2026-11-03) one pane at a time, centred, DRILL_W
// wide: the hub, then a page, as the 1997 menu goes; 0 two panes side by side.
int Menu_Panes();
enum { DRILL_W = 640 };

// The foot of a wide page: the band (voice + legend) or the help line and the
// prompt bar, as the `band` switch says. Returns the y the rows must stay above.
S32 Menu_WideRowsLimit();
void Menu_DrawFoot(const char **keys, const char **words, int n);
// The one-column dialogue window with the voice in it. Returns its top.
S32 Menu_DialogueTop();
void Menu_DrawDialogue();
void Menu_DrawDialogueIn(S32 x0, S32 y0, S32 x1, S32 y1);

// Say a line in the voice, typed or not as the `typing` switch says.
void Menu_Say(const char *text);
void Menu_PageEntered();

void Menu_JsonString(FILE *f, const char *s);

// What the last Menu_Draw recorded, for the self test (selftest.cpp).
int Menu_DrawnRows(const DrawnRow **rows);
struct MenuHelp {
    const char *where;  // band, help-line, dialogue, or none
    int x0, y0, x1, y1;
    int needed, max, width;
    int page, pages;  // the window of help showing, of how many (longer help pages on)
    const char *text;
};
void Menu_Help(MenuHelp *out);
const char *Menu_PageName();  // as `state` names it
const char *Menu_Layout();    // as `state` names it: wide, column, first-launch, closed
// Where the player is, as one string ("page | cursor row | confirm"), and how deep:
// each screen or pane Enter opens adds one, so Esc should take it back by one.
int Menu_Where(char *buf, size_t n);
const char *Menu_CursorLabel();  // the cursor row of an Options page; "" elsewhere
bool Menu_CursorSetting(bool *active);  // the cursor row is a setting; active: not dimmed
const char *Menu_CursorValue();  // the value drawn on the cursor row last frame
const char *Menu_PresetLabel();
const char *Menu_Legend();  // the footer's legend as the last draw had it: "[A] Open  [B] Back"
bool Menu_MoreArrow();      // the last draw showed the dialogue's arrow: more help to come
const char *Menu_ConsoleMode();  // closed, full or read-only  // the preset row as it reads ("Modern settings, changed")
