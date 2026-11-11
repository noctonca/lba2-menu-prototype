// What menu.cpp shares with the pages drawn in their own files (bindings.cpp).
#pragma once

#include "ui.h"

#include <stdio.h>
#include <set>
#include <string>

// ── The page model (docs/plan/MENU_PLAN.md, "The page model") ──

enum RowType { R_RETURN, R_CATEGORY, R_PRESET_ROW, R_SETTING, R_PRESET_CHOICE, R_BACK_TO_MINE, R_ACTION, R_BINDING, R_TUNING };

// A check on the menu's state, for the menu plan's "shown when" and "active when".
typedef bool (*RowCheck)();

// A row of a page. A page's rows never change: a row that comes and goes (Undo preset) has a
// "shown when" check, asked each time the page is walked or drawn, so the cursor stays on the
// row it was on.
struct Row {
    RowType type;
    const char *label;  // a setting row's is its setting's (RowLabel)
    int arg;            // setting, page, preset or action
    const char *help;   // a setting row's is its setting's (RowHelp)
    RowCheck shown;     // NULL: always there
    RowCheck active;    // NULL: always active; else dimmed while false, the help saying `needs`
    const char *needs;
};

struct Page {
    const char *name;
    const Row *rows;
    int count;
};

// The pages: the Options pages, then Keys and buttons (its groups) and a page of each group's
// actions, which bindings.cpp makes from its tables (Bindings_Pages), then More options, a row
// for each of its values, which more_options.cpp makes from its table (MoreOptions_Pages).
enum { KEY_GROUPS = 7 };  // bindings.cpp kGroups
enum {
    P_HUB, P_PRESET, P_LANG, P_DISPLAY, P_SOUND, P_CONTROLS, P_ADVANCED, P_FIRST, P_ORIGINAL,
    P_KEYS, P_KEY_GROUP, P_MORE = P_KEY_GROUP + KEY_GROUPS, P_COUNT
};
void Bindings_Pages(Page *pages);     // fills pages[P_KEYS] and pages[P_KEY_GROUP + group]
void MoreOptions_Pages(Page *pages);  // fills pages[P_MORE]

// Where the player is: the page on top of the stack (-1 before Menu_Init) and the row its
// cursor is on, checked as it is read; and the view's scroll (the first row drawn; on More
// options the first line, its group heads counted), which starts again when a page opens or
// closes or the layout changes.
int Menu_TopPage();
int Menu_TopRow();
int &Menu_Scroll();

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

// Centred in the row; wider than the row, from its left (8 in) and, with cursor, swinging.
void Menu_RowLabelCentred(S32 x0, S32 x1, S32 yc, const char *s, U8 colour, bool asIs = false, bool cursor = false);
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
// Wide, one page at a time (decided 2026-11-03), centred and DRILL_W wide: the hub,
// then a page, as the 1997 menu goes.
enum { DRILL_W = 640 };

// The foot of a wide page: the band (voice + legend) or the help line and the
// prompt bar, as the `band` switch says. Returns the y the rows must stay above.
S32 Menu_WideRowsLimit();
void Menu_DrawFoot(const char **keys, const char **words, int n);
// The one-column dialogue window with the voice in it. Returns its top.
S32 Menu_DialogueTop();
void Menu_DrawDialogue();
void Menu_DrawDialogueIn(S32 x0, S32 y0, S32 x1, S32 y1);

// Say a line in the voice, typed or not as the `typing` switch says, in the Text
// language; a text lang/help.tsv lacks is said in English and noted (Menu_VoiceMissed).
void Menu_Say(const char *text);
// Say a line as it is: a format already filled in the Text language, or the console's
// help, English by design.
void Menu_SayAsIs(const char *text);
const std::set<std::string> &Menu_VoiceMissed();
// A voice text or format in the Text language (lang/help.tsv); English when it has none.
const char *Menu_Tr(const char *english);

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
const char *Menu_PageMark();  // "1/2" while paged help shows, else empty
const char *Menu_ConsoleMode();  // closed, full or read-only  // the preset row as it reads ("Modern settings, changed")
