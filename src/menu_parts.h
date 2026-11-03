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
    int textRight;  // where the label (or value) ends; > x1 means it spills
};
DrawnRow &Menu_Record(const char *pane, const char *label, const char *value, RowFill fill, int x0, int y0, int x1,
                      int y1);

void Menu_RowLabelCentred(S32 x0, S32 x1, S32 yc, const char *s, U8 colour);
void Menu_DrawScrollBar(S32 x0, S32 y0, S32 y1, int first, int visible, int total);

// The foot of a wide page: the band (voice + legend) or the help line and the
// prompt bar, as the `band` switch says. Returns the y the rows must stay above.
S32 Menu_WideRowsLimit();
void Menu_DrawFoot(const char **keys, const char **words, int n);
// The one-column dialogue window with the voice in it. Returns its top.
S32 Menu_DialogueTop();
void Menu_DrawDialogue();

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
    const char *text;
};
void Menu_Help(MenuHelp *out);
const char *Menu_PageName();  // as `state` names it
