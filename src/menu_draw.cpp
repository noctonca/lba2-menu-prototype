// The view: draws the page the loop says is up, in the one column or wide, and records
// what it drew (DrawnRow) and where the pointer can go (MenuPoint).
#include "menu.h"

#include "menu_internal.h"

#include "game_pieces.h"
#include "bindings.h"
#include "lang.h"
#include "word_fire.h"
#include "video_logo.h"
#include "screen_picture.h"
#include "screen_font.h"
#include "prop_scale.h"
#include "whats_new.h"

#include <SVGA/BOX.H>
#include <SVGA/SCREEN.H>
#include <SVGA/FIL_LINE.H>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// What SCREEN_SHADE_LVL (DEFINES.H, 8) keeps of the light through the menu palette's fog table,
// measured on the game's scene outside the boxes (2026-11-18: r 0.71, g 0.67, b 0.68).
static const float GAME_DIM_KEEP = 0.69f;

// What was drawn this frame, for `state`.
DrawnRow g_drawn[DRAWN_MAX];
int g_drawnCount;
HelpBox g_helpBox;
char g_legend[128];
// The box the legend's chips and words took last frame, and the 8x8 font's scale.
LegendBox g_legendBox;

static void LegendGrow(S32 x0, S32 y0, S32 x1, S32 y1, S32 scale) {
    if (!g_legendBox.shown) {
        g_legendBox.shown = true;
        g_legendBox.x0 = x0, g_legendBox.y0 = y0, g_legendBox.x1 = x1, g_legendBox.y1 = y1;
    }
    g_legendBox.x0 = x0 < g_legendBox.x0 ? x0 : g_legendBox.x0;
    g_legendBox.y0 = y0 < g_legendBox.y0 ? y0 : g_legendBox.y0;
    g_legendBox.x1 = x1 > g_legendBox.x1 ? x1 : g_legendBox.x1;
    g_legendBox.y1 = y1 > g_legendBox.y1 ? y1 : g_legendBox.y1;
    g_legendBox.scale = scale;
}
int g_voiceLines, g_voiceNeeded, g_voiceMax, g_voiceWidth;

DrawnRow &Menu_Record(const char *pane, const char *label, const char *value, RowFill fill, int x0, int y0, int x1,
                        int y1) {
    assert(g_drawnCount < DRAWN_MAX && "a frame records more rows than DRAWN_MAX: the tests would miss the rest");
    DrawnRow &d = g_drawn[g_drawnCount < DRAWN_MAX - 1 ? g_drawnCount++ : DRAWN_MAX - 1];
    snprintf(d.label, sizeof d.label, "%s", label);
    snprintf(d.value, sizeof d.value, "%s", value ? value : "");
    d.pane = pane;
    d.fill = fill == FILL_CURSOR ? "cursor" : fill == FILL_CURRENT ? "current" : "shade";
    d.x0 = x0;
    d.y0 = y0;
    d.x1 = x1;
    d.y1 = y1;
    d.dim = d.changed = d.marks = d.setting = d.isNew = false;
    d.textLeft = d.textRight = x0;
    d.valueRight = 0;
    return d;
}

// Centred text clipped to the row, the original's way (DrawOneChoice).
void Menu_RowLabelCentred(S32 x0, S32 x1, S32 yc, const char *s, U8 colour, bool asIs, bool cursor) {
    const ClipSave clip;
    SetClip(x0 + 1, yc - ROW_H / 2, x1 - 1, yc + ROW_H / 2);
    if (!asIs)
        s = Ui_Tr(s);
    if (Ui_TextWidthAsIs(s) > x1 - x0 - 16)
        Ui_TextInRoom(x0 + 8, x1 - 8, yc - ROW_TEXT_RISE, s, colour, cursor);  // 1997: x0 + 8, swinging
    else
        Ui_TextAsIs((x0 + x1) / 2 - Ui_TextWidthAsIs(s) / 2, yc - ROW_TEXT_RISE, s, colour);
    clip.Restore();
}

// What's new's dot (whats_new.h): after the row's name, a dot that draws the eye without
// alarm, in the menu palette's gold (247,199,111). Chosen in play over pale cyan, which went
// into the cursor's plasma, and soft green, too near Enter's (the maintainer, 2026-11-18); not
// red or orange (alarm, and Esc's), not the teal that says a value was changed.
enum { NEW_DOT_R = 4, NEW_DOT_GAP = 10 };  // the dot's radius; from the name's last letter
void Menu_NewDot(S32 cx, S32 yc, S32 r) {
    const U8 ink = 108;
    for (int pass = 0; pass < 2; pass++)  // its shadow, then the dot: it reads on the cursor's plasma too
        for (S32 dy = -r; dy <= r; dy++) {
            S32 w = 0;
            while ((w + 1) * (w + 1) + dy * dy <= r * r + r)
                w++;
            const S32 d = pass ? 0 : 1;
            Box(cx - w + d, yc + dy + d, cx + w + d, yc + dy + d, pass ? ink : INK_BLACK);
        }
}
// The dot after text ending at textEnd. Returns its right edge, for the fit checks.
static S32 NewDotAfter(S32 textEnd, S32 yc) {
    const S32 cx = textEnd + NEW_DOT_GAP + NEW_DOT_R;
    Menu_NewDot(cx, yc, NEW_DOT_R);
    return cx + NEW_DOT_R + 1;
}

// A group's head (What's new): its name where a row's label goes, on no shade, never lit.
enum { HEAD_INK = 191 };  // the wordmark's pale blue, (187,223,227)
static void DrawHeading(const char *pane, const Row &r, S32 x0, S32 y0, S32 x1, S32 y1, bool centred) {
    const S32 yc = (y0 + y1) / 2, w = Ui_TextWidth(r.label);
    const S32 x = centred ? (x0 + x1) / 2 - w / 2 : x0 + ROW_TEXT_INSET;
    Ui_TextShadow(x, yc - ROW_TEXT_RISE, r.label, HEAD_INK);
    DrawnRow &d = Menu_Record(pane, r.label, "", FILL_SHADE, x0, y0, x1, y1);
    d.fill = "heading";
    d.textLeft = x;
    d.textRight = x + w;
}

// One column: the 1997 row, "Label: Value" in one string, sliders as fills.
static void DrawColumnRow(const Row &r, int row, S32 x0, S32 y0, S32 x1, S32 y1, bool cursor) {
    if (r.type == R_HEADING) {
        DrawHeading("column", r, x0, y0, x1, y1, true);
        return;
    }
    S32 yc = (y0 + y1) / 2;
    char text[160], val[48];
    RowFill fill = cursor ? FILL_CURSOR : FILL_SHADE;
    bool active = RowActive(r);

    if (sw_greencurrent && r.type == R_PRESET_CHOICE && r.arg == Presets_Chosen() && !cursor)
        fill = FILL_CURRENT;  // the chosen preset wears the current-value plasma

    bool slider = r.type == R_SETTING && (Settings_Def(r.arg).kind == K_SLIDER || Settings_Def(r.arg).kind == K_STEPS);
    if (slider && cursor && active) {
        // GAMEMENU.CPP: the slider plasma up to the value, fill-selected after it.
        const Setting &st = Settings_Def(r.arg);
        S32 x2 = x0 + (x1 - x0) * Settings_Value(r.arg) / st.max;
        GamePieces_BackupCorners(x0, y0, x1, y1);
        GamePieces_FireBar(x0, y0, x1, y1, PLASMA_SLIDER);
        if (x2 < x1)
            Box(x2, y0, x1, y1, FILL_SELECTED);
        GamePieces_Frame(x0, y0, x1, y1);
    } else {
        Ui_RowBackground(x0, y0, x1, y1, fill);
    }

    if (r.type == R_PRESET_ROW)
        PresetRowLabel(text, sizeof text);
    else if (r.type == R_SETTING && Settings_Def(r.arg).kind != K_SLIDER)
        snprintf(text, sizeof text, "%s: %s", RowLabel(r), Settings_ValueText(r.arg, val, sizeof val));
    else if (r.type == R_ACTION && r.arg == A_CONSOLE)
        snprintf(text, sizeof text, "%s: %s", RowLabel(r), kVersion);
    else
        snprintf(text, sizeof text, "%s", RowLabel(r));
    // A save's name is the player's, not a text to translate.
    const bool asIs = r.type == R_SAVE || r.type == R_AUTOSAVE;
    const S32 textW = asIs ? Ui_TextWidthAsIs(text) : Ui_TextWidth(text);
    Menu_RowLabelCentred(x0, x1, yc, text, active ? INK_WHITE : INK_DISABLED, asIs, cursor);

    DrawnRow &d = Menu_Record("column", text,
                              r.type == R_SETTING                         ? Settings_ValueText(r.arg, val, sizeof val)
                              : r.type == R_ACTION && r.arg == A_CONSOLE ? kVersion
                                                                          : "",
                              fill, x0, y0, x1, y1);
    d.dim = !active;
    d.setting = r.type == R_SETTING;
    d.textLeft = (x0 + x1) / 2 - textW / 2;
    d.textRight = (x0 + x1) / 2 + textW / 2 + 8;  // 8: the original's side padding
    if (WhatsNew_RowNew(r)) {  // after the text; its right edge is the text's, for the fit checks
        const S32 right = NewDotAfter((x0 + x1) / 2 + textW / 2, yc);
        d.textRight = right + 8;  // the original's side padding, as above
        d.isNew = true;
    }
    Menu_Point(POINT_ROW, x0, y0, x1, y1, TopPage(), row);
    if (slider && cursor && active && Settings_Def(r.arg).kind == K_SLIDER)
        Menu_Point(POINT_SLIDER, x0, y0, x1, y1, r.arg, row);
}

// Wide: label at the pane's one edge, value in a column on the right.
static void DrawWideRow(const char *pane, const Row &r, int row, S32 x0, S32 y0, S32 x1, S32 y1, RowFill fill) {
    if (r.type == R_HEADING) {
        DrawHeading(pane, r, x0, y0, x1, y1, false);
        return;
    }
    S32 yc = (y0 + y1) / 2;
    char label[160], val[48];
    bool active = RowActive(r);
    // The 1997 slider (GAMEMENU.CPP DrawOneChoice): under the cursor the whole row is the
    // slider plasma up to the value, fill-selected after it, the label on it.
    const bool barRow = r.type == R_SETTING && Settings_Def(r.arg).kind == K_SLIDER;
    if (barRow && fill == FILL_CURSOR && active) {
        S32 x2 = x0 + (x1 - x0) * Settings_Value(r.arg) / Settings_Def(r.arg).max;
        GamePieces_BackupCorners(x0, y0, x1, y1);
        GamePieces_FireBar(x0, y0, x1, y1, PLASMA_SLIDER);
        if (x2 < x1)
            Box(x2, y0, x1, y1, FILL_SELECTED);
        GamePieces_Frame(x0, y0, x1, y1);
    } else {
        Ui_RowBackground(x0, y0, x1, y1, fill);
    }

    // The preset row: "Preset" at the label's edge, the preset where a value goes.
    const char *presetValue = NULL;
    char presetText[160];
    if (r.type == R_PRESET_ROW) {
        PresetRowLabel(presetText, sizeof presetText);
        char *colon = strstr(presetText, ": ");
        if (colon) {
            *colon = 0;
            presetValue = colon + 2;
        }
        snprintf(label, sizeof label, "%s", presetText);
    } else {
        snprintf(label, sizeof label, "%s", RowLabel(r));
    }

    DrawnRow &d = Menu_Record(pane, label, "", fill, x0, y0, x1, y1);
    d.dim = !active;
    d.setting = r.type == R_SETTING;
    const bool asIs = r.type == R_SAVE || r.type == R_AUTOSAVE;  // a save's name is the player's, not a text to translate
    d.textLeft = x0 + ROW_TEXT_INSET;
    d.textRight = x0 + ROW_TEXT_INSET + (asIs ? Ui_TextWidthAsIs(label) : Ui_TextWidth(label));
    Menu_Point(POINT_ROW, x0, y0, x1, y1, TopPage(), row);
    if (barRow && fill == FILL_CURSOR && active)
        Menu_Point(POINT_SLIDER, x0, y0, x1, y1, r.arg, row);

    const S32 valueRight = x1 - VALUE_INSET;
    S32 valueLeft = 0;  // where a value starts: the label's room ends 8 before its marks
    if (r.type == R_SETTING) {
        const Setting &st = Settings_Def(r.arg);
        bool changed = Settings_Changed(r.arg);
        S32 left = valueRight;
        if (st.kind == K_STEPS) {
            Ui_Steps(valueRight, yc, st.max, Settings_Value(r.arg), &left);
            snprintf(d.value, sizeof d.value, "%d/%d", Settings_Value(r.arg), st.max);
            valueLeft = left;
        } else if (st.kind == K_SLIDER) {
            snprintf(d.value, sizeof d.value, "%d", Settings_Value(r.arg));  // the bar is the value
        } else {
            Settings_ValueText(r.arg, val, sizeof val);
            left = valueRight + 1 - Ui_TextInkWidth(val);  // the ink ends on the value edge
            if (d.textRight + 8 > left - MARK_ROOM)
                d.textRight = x1 + (d.textRight + 8 - (left - MARK_ROOM));  // label runs into the value
            Ui_Text(left, yc - ROW_TEXT_RISE, val, !active ? INK_DISABLED : changed ? INK_COUNT : INK_WHITE);
            snprintf(d.value, sizeof d.value, "%s", val);
            valueLeft = left;
        }
        d.changed = changed;
        d.valueRight = valueRight;
        if (fill == FILL_CURSOR && active && st.kind != K_SLIDER) {
            Ui_ChangeMarks(left - MARK_ROOM, valueRight + MARK_ROOM, yc);  // in the padding: the value never moves
            d.marks = true;
            // Each arrow and the padding around it (Triangle: 10 wide from its tip).
            Menu_Point(POINT_STEP, left - MARK_ROOM - 6, y0, left - 3, y1, -1, row);
            Menu_Point(POINT_STEP, valueRight + 3, y0, valueRight + MARK_ROOM + 6, y1, +1, row);
        }
    } else if (r.type == R_ACTION && r.arg == A_CONSOLE) {
        // The version where a value goes, in the dimmed grey: it reads, it does not change.
        S32 left = valueRight + 1 - Ui_TextInkWidth(kVersion);
        if (d.textRight + 8 > left - MARK_ROOM)
            d.textRight = x1 + (d.textRight + 8 - (left - MARK_ROOM));
        Ui_Text(left, yc - ROW_TEXT_RISE, kVersion, INK_DISABLED);
        snprintf(d.value, sizeof d.value, "%s", kVersion);
        d.valueRight = valueRight;
        valueLeft = left;
    } else if (presetValue) {
        S32 left = valueRight + 1 - Ui_TextInkWidth(presetValue);
        if (d.textRight + 8 > left - MARK_ROOM)
            d.textRight = x1 + (d.textRight + 8 - (left - MARK_ROOM));
        Ui_Text(left, yc - ROW_TEXT_RISE, presetValue, Presets_Changed() ? INK_COUNT : INK_WHITE);
        snprintf(d.value, sizeof d.value, "%s", presetValue);
        d.valueRight = valueRight;
        valueLeft = left;
    } else if (r.type == R_PRESET_CHOICE && r.arg == Presets_Chosen() && Presets_Changed()) {
        valueLeft = valueRight + 1 - Ui_TextInkWidth("Changed");
        Ui_Text(valueLeft, yc - ROW_TEXT_RISE, "Changed", INK_COUNT);
        snprintf(d.value, sizeof d.value, "Changed");
        d.valueRight = valueRight;
    }
    if (WhatsNew_RowNew(r)) {
        // After the name, in the room the value leaves; past it, the record says it spills.
        const S32 room = valueLeft ? valueLeft - MARK_ROOM - 8 - 1 : x1 - ROW_TEXT_INSET;
        const S32 right = NewDotAfter(x0 + ROW_TEXT_INSET + (asIs ? Ui_TextWidthAsIs(label) : Ui_TextWidth(label)), yc);
        if (right > room)
            d.textRight = x1 + (right - room);
        else if (right > d.textRight)
            d.textRight = right;
        d.isNew = true;
    }
    // The label last, in the room the value leaves (the 1997 swing when it is wider).
    const ClipSave clip;
    SetClip(x0 + 1, y0, x1 - 1, y1);
    Ui_TextInRoom(x0 + ROW_TEXT_INSET, valueLeft ? valueLeft - MARK_ROOM - 8 - 1 : x1 - ROW_TEXT_INSET,
                  yc - ROW_TEXT_RISE, asIs ? label : Ui_Tr(label), active ? INK_WHITE : INK_DISABLED,
                  fill == FILL_CURSOR);
    clip.Restore();
}

void Menu_DrawScrollBar(S32 x0, S32 y0, S32 y1, int first, int visible, int total) {
    // ScrollBarReview: 12 wide, thumb inset 2, in the margin.
    S32 x1 = x0 + 11;
    GamePieces_Shade(x0 + 1, y0 + 1, x1 - 1, y1 - 1, MENU_SHADE_LVL);
    Line(x0, y0, x1, y0, COUL_DOWN_BLUE);
    Line(x0, y0, x0, y1, COUL_DOWN_BLUE);
    Line(x1, y0, x1, y1, COUL_UP_BLUE);
    Line(x0, y1, x1, y1, COUL_UP_BLUE);
    S32 h = y1 - y0 - 4;
    S32 ty0 = y0 + 2 + h * first / total, ty1 = y0 + 2 + h * (first + visible) / total - 1;
    Box(x0 + 2, ty0, x1 - 2, ty1, FILL_SELECTED);
    Line(x0 + 2, ty0, x1 - 2, ty0, COUL_UP_BLUE);
    Line(x0 + 2, ty0, x0 + 2, ty1, COUL_UP_BLUE);
    Line(x1 - 2, ty0, x1 - 2, ty1, COUL_DOWN_BLUE);
    Line(x0 + 2, ty1, x1 - 2, ty1, COUL_DOWN_BLUE);
    g_bar = ScrollBarInfo{true, x0, y0, x1, y1, ty0, ty1, first, visible, total};
    // The target reaches past the groove into the margin, 12 px being thin for a finger;
    // not left of it, where the rows end 9 px away.
    const S32 reach = x1 + 8 < (S32)Host_LogicalW() - 1 ? x1 + 8 : (S32)Host_LogicalW() - 1;
    Menu_Point(POINT_SCROLL, x0 - 2, y0, reach, y1, 0, 0);
}

// Help with another window: Space (the pad's Y) turns it, "More" in the legend before its
// last entry (Back stays last). Every page's legend comes through the band or the dialogue
// window, so it is added there. Measured without it: a voice too long beside the legend as
// it was is longer still beside a wider one.
static int WithMore(const char **in, const char **inWords, int n, S32 voiceW, S32 lines, const char **keys,
                    const char **words) {
    for (int i = 0; i < n && i < 7; i++)
        keys[i] = in[i], words[i] = inWords[i];
    if (!g_voice.text[0] || n < 1 || n > 7 || Voice_Pages(g_voice.text, voiceW, lines) < 2)
        return n;
    keys[n] = keys[n - 1];
    words[n] = words[n - 1];
    keys[n - 1] = "Space";
    words[n - 1] = "More";
    return n + 1;
}

// A legend word in the Text language. "legend|Back" first: the legend's Back (the menu
// before) is not the Back an action names (walking backwards).
static const char *LegendWord(const char *w) {
    if (Ui_Lang() <= 0)
        return w;
    char key[48];
    snprintf(key, sizeof key, "legend|%s", w);
    if (const char *t = Lang_Label(key, Ui_Lang()))
        return t;
    return Ui_Tr(w);
}

// Stacked, the band holds three lines: four entries go in two columns, read down, so Back
// stays last at the bottom right as on every other page.
static int LegendPerColumn(int n) { return n > 3 ? (n + 1) / 2 : n; }

// One column of the stack, entries i0..i1-1: the widest chip and the widest word.
static void LegendColumn(const char **keys, const char **words, int i0, int i1, S32 scale, S32 *chipW, S32 *wordW) {
    *chipW = *wordW = 0;
    for (int i = i0; i < i1; i++) {
        S32 c = Ui_ChipWidth(keys[i], scale, Bindings_LegendPadFamily()), t = Ui_WordWidth(LegendWord(words[i]), scale);
        *chipW = c > *chipW ? c : *chipW;
        *wordW = t > *wordW ? t : *wordW;
    }
}

static const S32 LEGEND_COLUMN_GAP = 8;  // between two columns, at scale 1

// Stacked: each column the widest chip, the gap, the widest word.
static S32 LegendWidth(const char **keys, const char **words, int n, S32 scale) {
    const int per = LegendPerColumn(n);
    S32 w = 0;
    for (int i0 = 0; i0 < n; i0 += per) {
        S32 chipW, wordW;
        LegendColumn(keys, words, i0, i0 + per < n ? i0 + per : n, scale, &chipW, &wordW);
        w += (i0 ? LEGEND_COLUMN_GAP * scale : 0) + chipW + 4 * scale + wordW;
    }
    return w;
}

// A key as the record writes it: the arrows as <> and ^v, two keys joined by "/".
static void KeyText(const char *key, char *out, size_t n) {
    out[0] = 0;
    for (const char *k = key; k;) {
        const char *and_ = strchr(k, CHIP_AND[0]);
        std::string one = and_ ? std::string(k, and_ - k) : std::string(k);
        const char *shown = one[0] == '\x11' ? "<>" : one[0] == '\x18' ? "^v" : one.c_str();
        if (out[0])
            strncat(out, "/", n - strlen(out) - 1);
        strncat(out, shown, n - strlen(out) - 1);
        k = and_ ? and_ + 1 : NULL;
    }
}

// A legend entry the pointer can press: the key its word stands for. The arrows' entries
// (Adjust, and Choose on a question) take no click: the rows and arrows are pointed at.
static int LegendKeyOf(const char *key, const char *word) {
    static const struct {
        const char *word;
        UiKey key;
    } kWordKeys[] = {
        {"Back", KEY_ESC},  {"Cancel", KEY_ESC},  {"Modern", KEY_ESC},     {"Close", KEY_ESC},  {"Resume", KEY_ESC},
        {"Open", KEY_ENTER}, {"Change", KEY_ENTER}, {"Answer", KEY_ENTER}, {"Choose", KEY_ENTER}, {"Restore", KEY_ENTER},
        {"Clear", KEY_DEL},  {"Defaults", KEY_DEL}, {"Keyboard", KEY_TAB}, {"Gamepad", KEY_TAB},
        {"More", KEY_MORE},
    };
    if ((key[0] == '\x11' || key[0] == '\x18') && !strchr(key, CHIP_AND[0]))
        return -1;
    for (const auto &w : kWordKeys)
        if (!strcmp(word, w.word))
            return w.key;
    return -1;
}

// The entry's target, and whether it is lit: the mouse over it, or a finger pressing it.
static bool LegendPoint(S32 x0, S32 y0, S32 x1, S32 y1, const char *key, const char *word) {
    const int k = LegendKeyOf(key, word);
    if (k < 0)
        return false;
    Menu_Point(POINT_KEY, x0, y0, x1, y1, k);
    HostPointer in;
    Host_Pointer(&in);
    return LastPointer() != POINTER_NONE && (LastPointer() == POINTER_MOUSE || in.held) && in.x >= x0 && in.x <= x1 &&
           in.y >= y0 && in.y <= y1;
}

static void LegendRecord(int i, const char *key, const char *word) {
    char item[64], k[32];
    KeyText(key, k, sizeof k);
    snprintf(item, sizeof item, "%s[%s] %s", i ? "  " : "", k, word);
    strncat(g_legend, item, sizeof g_legend - strlen(g_legend) - 1);
}

// What a legend's key does, from its keyboard name (before a pad's names replace it):
// Enter confirms, Esc goes back. The wait for a pad's button names its Start, which
// cancels. Ui_Chip's roles: a key of two keys gives each its own.
static int LegendRoles(const char *raw, const char *word) {
    int roles = 0, part = 0;
    for (const char *p = raw; part < 2; part++) {
        const char *end = strchr(p, CHIP_AND[0]);
        const size_t n = end ? (size_t)(end - p) : strlen(p);
        const int role = n == 5 && !strncmp(p, "Enter", 5) ? CHIP_ROLE_CONFIRM
                         : n == 3 && !strncmp(p, "Esc", 3) ? CHIP_ROLE_BACK
                                                           : CHIP_ROLE_NONE;
        roles |= role << (2 * part);
        if (!end)
            break;
        p = end + 1;
    }
    if (!roles && !strcmp(word, "Cancel"))
        roles = CHIP_ROLE_BACK;
    return roles;
}

// In a row: chip, word, chip, word, centred on yc.
// Stacked (the review card's legend): one key a line from y0, the chips ending on
// one column and the words starting on the next, so the words read as a list.
static void DrawLegendStacked(S32 x0, S32 y0, const char **keys, const char **raws, const char **words, int n,
                              S32 scale) {
    g_legend[0] = 0;
    const int per = LegendPerColumn(n);
    S32 chipW = 0, wordW = 0, x = x0 - LEGEND_COLUMN_GAP * scale;
    S32 chipH = 12 * scale, pitch = chipH + 2;
    for (int i = 0; i < n; i++) {
        if (i % per == 0) {  // a new column, after the last one's widest word
            x += (i ? chipW + 4 * scale + wordW : 0) + LEGEND_COLUMN_GAP * scale;
            LegendColumn(keys, words, i, i + per < n ? i + per : n, scale, &chipW, &wordW);
        }
        S32 y = y0 + (i % per) * pitch;
        S32 c = Ui_ChipWidth(keys[i], scale, Bindings_LegendPadFamily());
        const bool lit = LegendPoint(x + chipW - c, y - 1, x + chipW + 4 * scale + Ui_WordWidth(LegendWord(words[i]), scale),
                                     y + chipH, keys[i], words[i]);
        Ui_Chip(x + chipW - c, y, keys[i], scale, lit, Bindings_LegendPadFamily(), LegendRoles(raws[i], words[i]));
        Ui_Word(x + chipW + 4 * scale, y + chipH / 2 - 4 * scale, LegendWord(words[i]), lit ? INK_WHITE : INK_CONFIG_MENU,
                scale);
        LegendRecord(i, keys[i], words[i]);
        LegendGrow(x + chipW - c, y, x + chipW + 4 * scale + Ui_WordWidth(LegendWord(words[i]), scale) - 1,
                   y + chipH - 1, scale);
    }
}

// The question's answers, the safe one first; the question itself is in the voice.
// One column: two rows in the middle, as wide as a page's rows, labels centred. Wide: in
// place of the page's rows, labels on their label edge.
void Menu_DrawAnswers(const char *pane, S32 x0, S32 x1, S32 top, bool centred) {
    assert(g_asking && "answers are drawn while a question is up");
    for (int i = 0; i < 2; i++) {
        // The answer as recorded (English) and as drawn (the Text language).
        char label[96], built[128];
        const char *shown;
        if (g_asking->answer) {
            g_asking->answer(i, label, sizeof label, false);
            g_asking->answer(i, built, sizeof built, true);
            shown = built;
        } else {
            snprintf(label, sizeof label, "%s", g_asking->answers[i]);
            shown = Ui_Tr(g_asking->answers[i]);
        }
        S32 y0 = top + i * ROW_PITCH, yc = y0 + ROW_H / 2;
        RowFill fill = i == g_answer && Menu_CursorLit() ? FILL_CURSOR : FILL_SHADE;
        Ui_RowBackground(x0, y0, x1, y0 + ROW_H, fill);
        DrawnRow &d = Menu_Record(pane, label, "", fill, x0, y0, x1, y0 + ROW_H);
        Menu_Point(POINT_ANSWER, x0, y0, x1, y0 + ROW_H, i);
        if (centred) {
            Menu_RowLabelCentred(x0, x1, yc, shown, INK_WHITE, true, i == g_answer);
            d.textLeft = (x0 + x1) / 2 - Ui_TextWidthAsIs(shown) / 2;
            d.textRight = d.textLeft + Ui_TextWidthAsIs(shown);
        } else {
            const ClipSave clip;
            SetClip(x0 + 1, y0, x1 - 1, y0 + ROW_H);
            Ui_TextInRoom(x0 + ROW_TEXT_INSET, x1 - ROW_TEXT_INSET, yc - ROW_TEXT_RISE, shown, INK_WHITE, i == g_answer);
            clip.Restore();
            d.textLeft = x0 + ROW_TEXT_INSET;
            d.textRight = x0 + ROW_TEXT_INSET + Ui_TextWidthAsIs(shown);
        }
    }
}

void Menu_QuestionLegend(const char **keys, const char **words, int *n) {
    keys[*n] = "\x18\x19"; words[(*n)++] = "Choose";
    keys[*n] = "Enter"; words[(*n)++] = "Answer";
    keys[*n] = "Esc"; words[(*n)++] = "Cancel";  // the safe answer, without moving to it
}

// Paged help says where it is with ellipses in the text (Voice_Draw), not a "1/2" mark: the
// mark sat under the legend's Back at 640x480 (2026-11-12). `state` still names the window.
char g_pageMark[8];
static void NotePageMark() {
    S32 page = 0, pages = 1;
    Voice_LastPage(&page, &pages);
    g_pageMark[0] = 0;
    if (pages > 1)
        snprintf(g_pageMark, sizeof g_pageMark, "%d/%d", (int)page + 1, (int)pages);
}
const char *Menu_PageMark() { return g_pageMark; }
const char *Menu_Legend() { return g_legend; }

bool Menu_CursorEnterSteps(bool *isValue) {
    *isValue = false;
    if (!g_depth || g_asking || g_console)
        return false;
    const Row *r = CursorRow();
    if (!r || !KindOf(*r).value || !RowActive(*r))
        return false;
    bool enterSteps = false;
    *isValue = KindOf(*r).value(*r, &enterSteps);
    return *isValue && enterSteps;
}

// MESSAGE.CPP NormalWinDial: the window's top this far above the screen's last line.
enum { DIALOGUE_RISE = 137 };

S32 Menu_DialogueTop() { return (S32)Host_LogicalH() - 1 - DIALOGUE_RISE; }

void Menu_DrawDialogue(const char **keys, const char **words, int nk) {
    // MESSAGE.CPP NormalWinDial: 8 in from the sides, 137 tall, 8 off the bottom.
    S32 W = (S32)Host_LogicalW(), H = (S32)Host_LogicalH();
    Menu_DrawDialogueIn(8, H - 1 - DIALOGUE_RISE, W - 1 - 8, H - 1 - 8, keys, words, nk);
}

// In a row: chip, word, then the next, from x; returns the width.
static S32 DrawLegendRow(S32 x, S32 y, const char **keys, const char **raws, const char **words, int n, S32 scale,
                         bool draw) {
    if (draw)
        g_legend[0] = 0;
    S32 w = 0;
    for (int i = 0; i < n; i++) {
        if (i)
            w += 12 * scale;
        S32 c = Ui_ChipWidth(keys[i], scale, Bindings_LegendPadFamily());
        if (draw) {
            const bool lit = LegendPoint(x + w, y - 1, x + w + c + 4 * scale + Ui_WordWidth(LegendWord(words[i]), scale),
                                         y + 12 * scale, keys[i], words[i]);
            Ui_Chip(x + w, y, keys[i], scale, lit, Bindings_LegendPadFamily(), LegendRoles(raws[i], words[i]));
            Ui_Word(x + w + c + 4 * scale, y + 2 * scale, LegendWord(words[i]), lit ? INK_WHITE : INK_CONFIG_MENU, scale);
            LegendRecord(i, keys[i], words[i]);
            LegendGrow(x + w, y, x + w + c + 4 * scale + Ui_WordWidth(LegendWord(words[i]), scale) - 1,
                       y + 12 * scale - 1, scale);
        }
        w += c + 4 * scale + Ui_WordWidth(LegendWord(words[i]), scale);
    }
    return w;
}

// The help window's legend at the size that fits (switch legendfit, F8; EPmager, 2026-11-16):
// the row laid out at f times its 1x size, its chips' boxes on the canvas at that size, their
// letters and the words drawn at 1x and painted at f by the text layer. Returns the width.
static float DrawLegendRowFit(float x, float y, const char **keys, const char **raws, const char **words, int n,
                              float f, bool draw) {
    if (draw)
        g_legend[0] = 0;
    float w = 0;
    const int pad = Bindings_LegendPadFamily();
    for (int i = 0; i < n; i++) {
        if (i)
            w += 12 * f;
        const float c = (float)Ui_ChipWidth(keys[i], 1, pad) * f;
        const char *word = LegendWord(words[i]);
        const float ww = (float)Ui_WordWidth(word, 1) * f;
        if (draw) {
            const S32 x0 = (S32)lrintf(x + w), y0 = (S32)lrintf(y);
            const S32 x1 = (S32)lrintf(x + w + c + 4 * f + ww) - 1, y1 = (S32)lrintf(y + 12 * f) - 1;
            const bool lit = LegendPoint(x0, y0 - 1, x1 + 1, y1 + 1, keys[i], words[i]);
            Ui_ChipFit(x + w, y, keys[i], f, lit, pad, LegendRoles(raws[i], words[i]));
            Ui_WordFit((S32)lrintf(x + w + c + 4 * f), (S32)lrintf(y + 2 * f), word,
                       lit ? INK_WHITE : INK_CONFIG_MENU, f);
            LegendRecord(i, keys[i], words[i]);
            LegendGrow(x0, y0, x1, y1, f >= 1.5f ? 2 : 1);
        }
        w += c + 4 * f + ww;
    }
    return w;
}

// The legend's keys as the device used last names them (a pad's buttons after a pad press).
static void LegendKeys(const char **in, const char **out, int nk) {
    for (int i = 0; i < nk && i < 8; i++)
        out[i] = Bindings_LegendKey(in[i]);
}

void Menu_DrawDialogueIn(S32 x0, S32 y0, S32 x1, S32 y1, const char **keysGiven, const char **wordsGiven, int nk) {
    const char *keysIn[8], *words[8];
    nk = WithMore(keysGiven, wordsGiven, nk, x1 - x0 - 2 * PANEL_PAD_X, nk ? 2 : 3, keysIn, words);
    Ui_Panel(x0, y0, x1, y1, PANEL_SHADE_LVL);
    // The legend takes the window's third line, so the help has two, as in the wide band;
    // no help at 640x480 needs a third (measured 2026-11-11, six languages).
    const S32 maxLines = nk ? 2 : 3;
    g_voiceLines = Voice_Draw(g_voice, x0 + PANEL_PAD_X, y0 + PANEL_PAD_Y, x1 - x0 - 2 * PANEL_PAD_X, maxLines,
                              &g_voiceNeeded);
    g_voiceMax = maxLines;
    g_voiceWidth = x1 - x0 - 2 * PANEL_PAD_X;
    if (nk) {
        const char *keys[8];
        LegendKeys(keysIn, keys, nk);
        // The 8x8 font at 2x when the line fits, as the wide band has it; 1x, the 1997 key
        // screen's size, when it does not. With legendfit, the size between that fits.
        const S32 room = x1 - x0 - 2 * PANEL_PAD_X;
        S32 scale = 2;
        if (DrawLegendRow(0, 0, keys, keysIn, words, nk, scale, false) > room)
            scale = 1;
        if (scale == 1 && sw_legendfit && PropScale_CanFit()) {
            float f = (float)room / DrawLegendRowFit(0, 0, keys, keysIn, words, nk, 1.0f, false);
            f = f > 2.0f ? 2.0f : f < 1.0f ? 1.0f : f;
            const float w = DrawLegendRowFit(0, 0, keys, keysIn, words, nk, f, false);
            DrawLegendRowFit((float)(x1 - PANEL_PAD_X + 1) - w, (float)(y1 - 8 + 1) - 12 * f, keys, keysIn, words, nk,
                             f, true);
        } else {
            S32 w = DrawLegendRow(0, 0, keys, keysIn, words, nk, scale, false);
            DrawLegendRow(x1 - PANEL_PAD_X - w + 1, y1 - 8 - 12 * scale + 1, keys, keysIn, words, nk, scale, true);
        }
    } else {
        g_legend[0] = 0;
    }
    NotePageMark();
    g_helpBox.shown = true;
    g_helpBox.kind = "dialogue";
    g_helpBox.x0 = x0; g_helpBox.y0 = y0; g_helpBox.x1 = x1; g_helpBox.y1 = y1;
}

// ── The corner Back (decided 2026-11-12) ──
// For a mouse or a finger only: shown once the pointer was used last, gone at a key or a
// pad's button, which have Esc and B. It sits on the title's line: in the one column at the
// rows' left edge (the title moves right of it when it must), wide in the frame's margin left
// of the rows. A row's height, as big a target as a row; it presses Esc.
// Not on the game menu itself: Resume game is its first row (the maintainer, 2026-11-16).
// Always Back, one look and one width on every screen (decided 2026-11-12; on a question it
// does what the legend's Esc Cancel does). In the Text language as the legend has it
// ("legend|Back": the plain "Back" is the walk-back key).
static const char *BackWord() { return LegendWord("Back"); }
static S32 BackWidth() { return 44 + Ui_TextWidthAsIs(BackWord()) + 20; }
// In the one column at the rows' left edge (1997's LargeurMenu 550, centred); wide on the
// frame's side margin, left of the rows.
static S32 BackLeft() {
    return Wide() ? Menu_WideFrame().x0 + WIDE_INSET_X : (S32)Host_LogicalW() / 2 - COLUMN_HALF_W;
}
BackBox g_backBox;

static void DrawBack() {
    S32 tx, ty;
    const bool titled = Ui_FirstTitle(&tx, &ty);
    (void)tx;
    const S32 x0 = BackLeft();
    // The title's 38 in a row's 50, so the word sits on the title's line (wide, 6 into the
    // top margin: lined up with the title reads better than kept out of it).
    const S32 y0 = (titled ? ty : Wide() ? Menu_WideFrame().y0 + WIDE_INSET_Y : ROWS_MARGIN) - 6;
    const S32 x1 = x0 + BackWidth(), y1 = y0 + ROW_H, yc = (y0 + y1) / 2;
    HostPointer in;
    Host_Pointer(&in);
    const bool lit = !Menu_CursorLit();  // it has the cursor
    Ui_RowBackground(x0, y0, x1, y1, lit ? FILL_CURSOR : FILL_SHADE);
    Ui_Arrow(x0 + 18, yc, -1);
    Ui_TextAsIs(x0 + 44, yc - ROW_TEXT_RISE, BackWord(), INK_WHITE);
    Menu_Point(POINT_KEY, x0, y0, x1, y1, KEY_ESC, POINT_KEY_BACK);
    g_backBox = {true, x0, y0, x1, y1};
}

// The game menu and its save lists: no help, so in the one column a strip one voice line tall
// (a note such as "Game saved.", and the legend at its right) in place of the dialogue window.
// A question there still asks in the dialogue window.
// The save picture, where 1997 shows one: on the first game menu, Resume game's (the
// current save's), in place of the title; on Load game, the save under the cursor's, under
// the title. Not in play: six rows and a picture don't fit (387 px at 640x480; decided
// 2026-11-14), and the game it would show is behind the menu.
// Framed, then the gap rows have between them: a row is drawn ROW_H + 1 tall, so 5 px.
enum { PICTURE_H = SAVE_PICTURE_H + 2 + ROW_GAP - 1 };

// Before a game the game menu keeps the picture's place even with no save to show, the
// product's name centred in it: the name and the first row are where a returning player has
// them, laid out for the five rows a save brings (Resume game, New game, Load game, Options,
// Quit). With the name in a title's place it sat 8 px over New game (EPmager, 2026-11-16).
enum { MAIN_ROWS_WITH_SAVE = 5 };
static bool MainBeforeGame() { return !g_asking && TopPage() == P_MAIN && !Game_InProgress(); }

// A save's picture, drawn from where it left the game.
static const U8 *Picture(const GameSpot &spot) {
    static U8 picture[SAVE_PICTURE_W * SAVE_PICTURE_H];
    GameScene_Picture(picture, spot);
    return picture;
}

// The picture this page shows, NULL for none.
static const U8 *PagePicture() {
    if (g_asking)
        return NULL;
    if (TopPage() == P_MAIN)
        return !Game_InProgress() && Game_HaveCurrent() ? Picture(Game_CurrentSpot()) : NULL;
    if (TopPage() == P_LOAD) {
        const Row *r = CursorRow();
        if (r && r->type == R_SAVE)
            return Picture(Game_SaveSpot(r->arg));
        if (r && r->type == R_AUTOSAVE)
            return Picture(Game_SaveSpot(-1));
    }
    return NULL;
}

// The game menu is titled with the product's name, never translated (the maintainer,
// 2026-11-16; the game's logo was tried and kept for later).
#define PRODUCT_NAME "LBA2 Classic Community"

// The name as a wordmark (chosen from a mock-up, 2026-11-16): LBA large in gold, the 2 raised
// as the game's logo has it (LBA and a raised 2), and CLASSIC COMMUNITY in small spaced capitals under it,
// its capitals 8 above the first row, in the 56 px a title takes. The game font has one size:
// there, the name on one line.
struct MarkSize {
    S32 cap, supCap, supGap, subCap, subSpacing, gap, shadow, subShadow;
    float rim, supRim;  // the gold rim round LBA and round the 2
};
static const MarkSize kMarkTitle = {32, 18, 2, 10, 3, 6, 4, 2, 1.5f, 0.75f};
// In the save picture's place, 1.8 times as large, so the name fills it as the picture does
// (chosen in play over 1 and 1.5, the maintainer, 2026-11-16: at a title's size it was 1.5
// times the rows' capitals, under 40% of the place's height). The 2 keeps its 2 px gap: scaled,
// the gap after the A was 14 screen px at 1080p against 10 between L and B.
static const MarkSize kMarkPlace = {58, 32, 2, 18, 5, 11, 7, 4, 2.7f, 1.35f};
enum { MARK_ROW_GAP = 8 };
enum { INK_GOLD = 107, INK_PALE = 191 };  // the menu palette's (247,187,87) and (187,223,227)
static const char kMarkSub[] = "CLASSIC COMMUNITY";

static S32 MarkSubWidth(const MarkSize &m) {
    S32 w = 0;
    for (const char *c = kMarkSub; *c; c++) {
        const char one[2] = {*c, 0};
        w += ScreenFont_SmallWidth(one, m.subCap) + (c[1] ? m.subSpacing : 0);
    }
    return w;
}

// CLASSIC COMMUNITY, its shadow then its letters, the top of its capitals at subTop.
static void DrawMarkSub(S32 xSub, S32 subTop, const MarkSize &m) {
    for (int pass = 0; pass < 2; pass++) {
        const S32 ds = pass ? 0 : m.subShadow;
        S32 x = xSub;
        for (const char *c = kMarkSub; *c; c++) {
            const char one[2] = {*c, 0};
            ScreenFont_SmallText(x + ds, subTop + ds, one, pass ? INK_PALE : INK_BLACK, m.subCap);
            x += ScreenFont_SmallWidth(one, m.subCap) + m.subSpacing;
        }
    }
}

static bool LogoOn() { return sw_videologo && ScreenFont_On() && VideoLogo_Ready(); }

// The intro's logo for the name (switch videologo), centred on cx between top and bottom
// (both included), as wide as its height makes it. In the picture's place with logosub,
// CLASSIC COMMUNITY keeps the bottom of that space and the logo takes what is above it.
// still: the logo at rest, no build-in and no ripple, as the game menu in play has it.
static void DrawLogoTitle(S32 cx, S32 top, S32 bottom, const MarkSize &m, bool still) {
    S32 logoBottom = bottom, l = cx, r = cx;
    if (sw_logosub && !still) {
        // The line's cell reaches half a capital under its baseline, and its shadow further:
        // all of it stays above `bottom`, or the row drawn under it would take it off the screen.
        const S32 subTop = bottom + 1 - m.subShadow - (m.subCap + m.subCap / 2), wSub = MarkSubWidth(m);
        DrawMarkSub(cx - wSub / 2, subTop, m);
        logoBottom = subTop - m.gap - 1;
        l = cx - wSub / 2, r = cx - wSub / 2 + wSub - 1;
    }
    const S32 h = logoBottom - top + 1, w = (S32)(h * VideoLogo_Aspect() + 0.5f);
    const S32 x0 = cx - w / 2, x1 = x0 + w - 1;
    VideoLogo_Queue(x0, top, x1, logoBottom, still);
    Menu_Record("title", PRODUCT_NAME, "", FILL_SHADE, x0 < l ? x0 : l, top, x1 > r ? x1 : r, bottom).textLeft = x0 < l ? x0 : l;
}

// Over x0..x1, its last line MARK_ROW_GAP above firstRow.
static void DrawProductTitle(S32 x0, S32 x1, S32 firstRow, const MarkSize &m = kMarkTitle) {
    const S32 CAP = m.cap, SUP_CAP = m.supCap, SUP_GAP = m.supGap, SUB_CAP = m.subCap, GAP = m.gap, SHADOW = m.shadow,
              SUB_SHADOW = m.subShadow;
    const S32 cx = (x0 + x1 + 1) / 2;
    if (!ScreenFont_On()) {
        Ui_Title(x0, x1, firstRow - ROW_PITCH, PRODUCT_NAME, true);
        Menu_Record("title", PRODUCT_NAME, "", FILL_SHADE, x0, firstRow - ROW_PITCH, x1, firstRow - ROW_PITCH + TEXT_LINE_H)
            .textLeft = cx - Ui_TextWidthAsIs(PRODUCT_NAME) / 2;
        return;
    }
    if (LogoOn() && &m == &kMarkTitle) {
        // In play: the logo smaller, in a title's pitch less the gap to the first row, and
        // at rest (the maintainer, 2026-11-17).
        DrawLogoTitle(cx, firstRow - ROW_PITCH, firstRow - MARK_ROW_GAP - 1, m, true);
        return;
    }
    const S32 subTop = firstRow - MARK_ROW_GAP - SUB_CAP;
    const S32 top = subTop - GAP - CAP;
    const S32 wLba = ScreenFont_SmallWidth("LBA", CAP), wSup = ScreenFont_SmallWidth("2", SUP_CAP);
    // LBA centred on the line under it, the 2 hanging right of it as a superscript does.
    const S32 wMark = wLba + SUP_GAP + wSup, xMark = cx - wLba / 2;
    const S32 wSub = MarkSubWidth(m), xSub = cx - wSub / 2;
    // Fire in LBA and the 2 (word_fire.h), over their box; the shadows stay black.
    WordFire_Frame(xMark, top, xMark + wMark - 1, top + CAP - 1);
    for (int pass = 0; pass < 2; pass++) {  // the shadows, then the letters over them
        const S32 d = pass ? 0 : SHADOW, ds = pass ? 0 : SUB_SHADOW;
        if (pass) {
            // The 2's rim half LBA's: at its size a full one left little fire inside (the
            // maintainer, 2026-11-16).
            ScreenFont_SmallTextFilled(xMark, top, "LBA", INK_GOLD, CAP, m.rim);
            ScreenFont_SmallTextFilled(xMark + wLba + SUP_GAP, top, "2", INK_GOLD, SUP_CAP, m.supRim);
        } else {
            ScreenFont_SmallText(xMark + d, top + d, "LBA", INK_BLACK, CAP);
            ScreenFont_SmallText(xMark + wLba + SUP_GAP + d, top + d, "2", INK_BLACK, SUP_CAP);
        }
        S32 x = xSub;
        for (const char *c = kMarkSub; *c; c++) {
            const char one[2] = {*c, 0};
            ScreenFont_SmallText(x + ds, subTop + ds, one, pass ? INK_PALE : INK_BLACK, SUB_CAP);
            x += ScreenFont_SmallWidth(one, SUB_CAP) + m.subSpacing;
        }
    }
    const S32 l = xMark < xSub ? xMark : xSub, r = xMark + wMark > xSub + wSub ? xMark + wMark : xSub + wSub;
    Menu_Record("title", PRODUCT_NAME, "", FILL_SHADE, l, top, r - 1, subTop + SUB_CAP - 1).textLeft = l;
}

// The picture with 1997's frame round it (DrawScreenSave: DrawCadre one pixel outside),
// its top left at x, y.
static void DrawPicture(const U8 *pic, S32 x, S32 y) {
    const S32 x1 = x + SAVE_PICTURE_W - 1, y1 = y + SAVE_PICTURE_H - 1;
    GamePieces_BackupCorners(x - 1, y - 1, x1 + 1, y1 + 1);
    U8 *dst = (U8 *)Log;
    for (S32 j = 0; j < SAVE_PICTURE_H; j++)
        memcpy(dst + TabOffLine[y + j] + x, pic + j * SAVE_PICTURE_W, SAVE_PICTURE_W);
    GamePieces_Frame(x - 1, y - 1, x1 + 1, y1 + 1);
    Menu_Record("picture", "", "", FILL_SHADE, x - 1, y - 1, x1 + 1, y1 + 1);
}

// The picture centred in its own place: between the last line taken above it (the title's,
// or the edge's) and the first row, the same gap above and below (the odd pixel below);
// the maintainer, 2026-11-16: it sat on the first row and far from Load game's title.
// On the first game menu the picture is Resume game's: on the other rows the product's name
// takes its place, centred in it, so the rows stay where they are (the maintainer, 2026-11-16).
static void DrawPictureBetween(const U8 *pic, S32 x, S32 above, S32 firstRow) {
    const S32 frameH = SAVE_PICTURE_H + 2, free = firstRow - above - 1 - frameH;
    const S32 y = above + 1 + free / 2 + 1;
    const Row *r = CursorRow();
    if (!pic || (TopPage() == P_MAIN && !(r && r->type == R_GAME && r->arg == M_RESUME))) {
        const MarkSize &m = kMarkPlace;
        if (LogoOn()) {  // the logo fills the picture's place, frame and all
            DrawLogoTitle(x - 1 + (SAVE_PICTURE_W + 2) / 2, y - 1, y - 1 + frameH - 1, m, false);
            return;
        }
        const S32 markH = m.cap + m.gap + m.subCap;
        DrawProductTitle(x - 1 + SAVE_PICTURE_W / 2 - COLUMN_HALF_W, x - 1 + SAVE_PICTURE_W / 2 + COLUMN_HALF_W,
                         y - 1 + (frameH - markH) / 2 + markH + MARK_ROW_GAP, m);
        return;
    }
    DrawPicture(pic, x, y);
}

enum { STRIP_H = 60 };  // the voice line at 12 in, 10 under it: six rows and a title fit 640x480
enum { BAND_H = 86 };   // the wide band: two lines of help and the legend
static bool StripPage() {
    const int p = TopPage();
    return !g_asking && (p == P_MAIN || p == P_LOAD || p == P_SAVE);
}

// The strip with its bottom right corner at x1, y1: in the one column 8 in from the screen's,
// in the wide frame where the band's is (the band is two lines tall for help these pages
// don't have; decided 2026-11-16).
// The strip's legend: its width and the 8x8 font's scale, 2 unless that leaves no room.
static S32 StripLegend(S32 x0, S32 x1, const char **keys, const char **keysGiven, const char **words, int nk,
                       S32 *scale) {
    *scale = 2;
    if (DrawLegendRow(0, 0, keys, keysGiven, words, nk, *scale, false) > x1 - x0 - 2 * PANEL_PAD_X)
        *scale = 1;
    return nk ? DrawLegendRow(0, 0, keys, keysGiven, words, nk, *scale, false) : 0;
}
static S32 StripVoiceWidth(S32 x0, S32 x1, S32 lw) { return x1 - x0 - 2 * PANEL_PAD_X - (lw ? lw + PANEL_PAD_X : 0); }

static void DrawStripAt(S32 x0, S32 x1, S32 y1, const char **keysGiven, const char **words, int nk) {
    const S32 y0 = y1 - STRIP_H + 1;
    Ui_Panel(x0, y0, x1, y1, PANEL_SHADE_LVL);
    const char *keys[8];
    LegendKeys(keysGiven, keys, nk);
    S32 scale;
    const S32 lw = StripLegend(x0, x1, keys, keysGiven, words, nk, &scale);
    const S32 voiceW = StripVoiceWidth(x0, x1, lw);
    g_voiceLines = Voice_Draw(g_voice, x0 + PANEL_PAD_X, y0 + PANEL_PAD_Y, voiceW, 1, &g_voiceNeeded);
    g_voiceMax = 1;
    g_voiceWidth = voiceW;
    if (nk)
        DrawLegendRow(x1 - PANEL_PAD_X - lw + 1, (y0 + y1) / 2 - 6 * scale + 1, keys, keysGiven, words, nk, scale,
                      true);
    else
        g_legend[0] = 0;
    NotePageMark();
    g_helpBox.shown = true;
    g_helpBox.kind = "strip";
    g_helpBox.x0 = x0; g_helpBox.y0 = y0; g_helpBox.x1 = x1; g_helpBox.y1 = y1;
}

static void DrawStrip(const char **keysGiven, const char **words, int nk) {
    const S32 W = (S32)Host_LogicalW(), H = (S32)Host_LogicalH();
    DrawStripAt(8, W - 1 - 8, H - 1 - 8, keysGiven, words, nk);
}

static void DrawColumn() {
    S32 W = (S32)Host_LogicalW(), H = (S32)Host_LogicalH();
    const Place &t = Top();
    const Page &p = g_pages[t.page];
    int shown[PAGE_ROWS_MAX];
    const int count = ShownRows(t.page, shown);
    int cursor = Position(t.page, t.row);

    if (g_asking) {
        // As wide as the page's rows, so a translated answer fits ("Nein, meine Einstellungen
        // behalten"): the 360 px panel this was cut the safe answer off in five languages.
        Menu_DrawAnswers("confirm", W / 2 - COLUMN_HALF_W, W / 2 + COLUMN_HALF_W, 120, true);
    } else {
        // Help in the dialogue window on every one-column page (decided 2026-11-03); the game
        // menu's pages, which have none, take a strip one line tall, so its six rows fit at
        // 640x480 as 1997's did.
        const U8 *pic = PagePicture();
        const bool place = MainBeforeGame();  // the picture's place, the name in it with no save
        S32 top = ROWS_MARGIN, bottom = H - 1 - (StripPage() ? STRIP_H : DIALOGUE_RISE) - ROWS_MARGIN;
        if (place) {
            // The first game menu's picture and five rows take 402 px (127 + 5 x 56 - 5): at
            // 640x480 they fit with the frame 4 from the top and the last row 6 above the strip,
            // one more than between rows (the proximity rule). 1997's frame was 9 from the top.
            top = 4;
            bottom = H - 1 - STRIP_H - 14;
        }
        // A page names itself in a title above its rows, as the wide pages do (EPmager,
        // 2026-11-12). The hub too, so the corner Back has its line and nothing moves when
        // it comes (decided 2026-11-12).
        const bool titled = !place;  // the picture in its place
        const S32 titleH = (titled ? ROW_PITCH : 0) + (pic || place ? PICTURE_H : 0);
        int visible = (bottom - top - titleH + ROW_GAP) / ROW_PITCH;
        if (visible > count)
            visible = count;
        if (g_scroll > count - visible)
            g_scroll = count - visible;  // a row went (Undo reset): no rows past the end
        if (const int want = Menu_WheelView(g_scroll, visible, count, cursor); want != cursor) {
            CursorTo(t.page, shown[want]);
            cursor = want;
        }
        if (cursor < g_scroll)
            g_scroll = cursor;
        if (cursor >= g_scroll + visible)
            g_scroll = cursor - visible + 1;
        S32 stackH = visible * ROW_PITCH - ROW_GAP;
        const int laid = place && visible < MAIN_ROWS_WITH_SAVE ? MAIN_ROWS_WITH_SAVE : visible;
        S32 y = top + (bottom - top - titleH - (laid * ROW_PITCH - ROW_GAP)) / 2 + titleH;  // centred above the dialogue window
        if (titled && t.page == P_MAIN) {
            DrawProductTitle(W / 2 - COLUMN_HALF_W, W / 2 + COLUMN_HALF_W, y);
        } else if (titled) {
            Ui_Title(W / 2 - COLUMN_HALF_W, W / 2 + COLUMN_HALF_W, y - titleH, p.name);
            Menu_Record("title", p.name, "", FILL_SHADE, W / 2 - COLUMN_HALF_W, y - titleH, W / 2 + COLUMN_HALF_W, y - titleH + TEXT_LINE_H).textLeft =
                W / 2 - Ui_TextWidth(p.name) / 2;
        }
        if (pic || place)  // under the title, or on the first game menu from the screen's edge (640x480 has no more)
            DrawPictureBetween(pic, W / 2 - SAVE_PICTURE_W / 2, titled ? y - titleH + TEXT_LINE_H : -1, y);
        // The 1997 width: LargeurMenu 550, centred.
        S32 x0 = W / 2 - COLUMN_HALF_W, x1 = W / 2 + COLUMN_HALF_W;
        for (int i = g_scroll; i < g_scroll + visible; i++) {
            DrawColumnRow(p.rows[shown[i]], shown[i], x0, y, x1, y + ROW_H, i == cursor && Menu_CursorLit());
            y += ROW_PITCH;
        }
        if (visible < count)
            Menu_DrawScrollBar(x1 + SCROLL_BAR_GAP, y - visible * ROW_PITCH, y - visible * ROW_PITCH + stackH - 1,
                               g_scroll, visible, count);
    }
    const char *keys[4], *words[4];
    int nk;
    LegendFor(g_asking ? NULL : CursorRow(), keys, words, &nk);
    if (StripPage())
        DrawStrip(keys, words, nk);
    else
        Menu_DrawDialogue(keys, words, nk);
}

// Wide: one page at a time, centred and as wide as the 1997 canvas: the hub, then a page,
// as the 1997 menu goes. Every page names itself in a title, the hub too (1997's had none).
static void DrawWide() {
    WideFrame f = Menu_WideFrame();
    const S32 top = f.y0 + WIDE_INSET_Y;
    const S32 x0 = f.x0 + (f.w - DRILL_W) / 2, x1 = x0 + DRILL_W - 1;
    const Place &t = Top();
    const Page &page = g_pages[t.page];

    if (g_asking) {
        // The question's answers in place of the rows.
        Menu_DrawAnswers("wide", x0, x1, Ui_Title(x0, x1, top, g_asking->title), false);
    } else {
        int shown[PAGE_ROWS_MAX];
        const int n = ShownRows(t.page, shown);
        int cursor = Position(t.page, t.row);
        const U8 *pic = PagePicture();
        // The first game menu: the picture in the title's place and the rows under it, what
        // height is left shared out in three like gaps, above the picture, under it and above
        // the strip, that last at least 6, more than between rows (the proximity rule;
        // decided 2026-11-16).
        const bool inPlace = MainBeforeGame();  // with no save, the name in the picture's place
        const S32 stripTop = Menu_WideRowsLimit() + ROWS_MARGIN + 1;
        S32 rowsTop = top, above = top - 1;
        if (!inPlace && t.page == P_MAIN) {
            rowsTop = top + ROW_PITCH;  // the title's pitch, as every page has it
            DrawProductTitle(x0, x1, rowsTop);
        } else if (!inPlace) {  // the hub too, as the column has it: rows keep their height between pages (decided 2026-11-13)
            rowsTop = Ui_Title(x0, x1, top, page.name);
            Menu_Record("title", page.name, "", FILL_SHADE, x0, top, x1, top + TEXT_LINE_H).textLeft =
                (x0 + x1 + 1) / 2 - Ui_TextWidth(page.name) / 2;
        }
        if (pic || inPlace) {
            if (!inPlace) {
                above = top + TEXT_LINE_H;  // the title's last line
                rowsTop += PICTURE_H;
            } else {
                const int laid = n < MAIN_ROWS_WITH_SAVE ? MAIN_ROWS_WITH_SAVE : n;
                const S32 space = stripTop - top - (SAVE_PICTURE_H + 2) - (laid * ROW_PITCH - ROW_GAP + 1);
                S32 a = (space + 1) / 3, c = space - 2 * a;
                if (c < 6) {
                    c = 6;
                    a = (space - c) / 2;
                }
                const S32 b = space - a - c;  // under the picture
                rowsTop = space < 5 + 6 ? top + PICTURE_H : top + a + SAVE_PICTURE_H + 2 + b;
            }
            DrawPictureBetween(pic, (x0 + x1 + 1) / 2 - SAVE_PICTURE_W / 2, above, rowsTop);
        }
        const S32 limit = inPlace ? stripTop - 7 : Menu_WideRowsLimit();
        int visible = (limit - rowsTop + ROW_GAP) / ROW_PITCH;
        if (visible > n)
            visible = n;
        if (g_scroll > n - visible)
            g_scroll = n - visible;  // a row went (Undo reset): no rows past the end
        if (const int want = Menu_WheelView(g_scroll, visible, n, cursor); want != cursor) {
            CursorTo(t.page, shown[want]);
            cursor = want;
        }
        if (cursor < g_scroll)
            g_scroll = cursor;
        if (cursor >= g_scroll + visible)
            g_scroll = cursor - visible + 1;
        S32 y = rowsTop;
        for (int i = g_scroll; i < g_scroll + visible; i++) {
            const Row &row = page.rows[shown[i]];
            RowFill fill = FILL_SHADE;
            if (i == cursor && Menu_CursorLit())
                fill = FILL_CURSOR;
            else if (sw_greencurrent && row.type == R_PRESET_CHOICE && row.arg == Presets_Chosen())
                fill = FILL_CURRENT;  // the chosen preset wears the current-value plasma, as in the column
            DrawWideRow("wide", row, shown[i], x0, y, x1, y + ROW_H, fill);
            y += ROW_PITCH;
        }
        if (visible < n)
            Menu_DrawScrollBar(x1 + SCROLL_BAR_GAP, rowsTop, rowsTop + visible * ROW_PITCH - ROW_GAP - 1, g_scroll,
                               visible, n);
    }

    // Help and legend.
    const char *keys[4], *words[4];
    int nk;
    LegendFor(CursorRow(), keys, words, &nk);
    Menu_DrawFoot(keys, words, nk);
}

// The last line a row may take: 16 above the band, or above the game pages' strip.
S32 Menu_WideRowsLimit() {
    WideFrame f = Menu_WideFrame();
    S32 H = f.y0 + f.h;
    return H - WIDE_INSET_Y - (StripPage() ? STRIP_H : BAND_H) - ROWS_MARGIN;
}

// The voice's width in the band: what the legend leaves, at most the review's 620.
static S32 BandVoiceWidth(const char **keys, const char **words, int nk) {
    S32 W = Menu_WideFrame().w;
    S32 legendW = LegendWidth(keys, words, nk, 2);
    S32 voiceW = (W - 2 * WIDE_INSET_X) - legendW - 48;
    return voiceW > 620 ? 620 : voiceW;
}

// Whether a line for the row under the cursor fits the voice it will be said in: the band
// beside the legend, the help line, or the 640x480 dialogue window. The loop asks, to say a
// row's short help where its help would not fit.
bool VoiceFits(const char *text, const Row *r) {
    S32 W = (S32)Host_LogicalW();
    if (StripPage()) {  // the game menu's pages: one line beside the legend, as DrawStripAt
        const char *keysGiven[4], *words[4], *keys[8];
        int nk;
        LegendFor(r, keysGiven, words, &nk);
        LegendKeys(keysGiven, keys, nk);
        const WideFrame fr = Menu_WideFrame();
        const S32 x0 = Wide() ? fr.x0 + WIDE_INSET_X : 8, x1 = Wide() ? fr.x0 + fr.w - 1 - WIDE_INSET_X : W - 1 - 8;
        S32 scale;
        return Voice_Lines(text, StripVoiceWidth(x0, x1, StripLegend(x0, x1, keys, keysGiven, words, nk, &scale))) <= 1;
    }
    if (!Wide())
        return Voice_Lines(text, W - 49) <= 2;  // the legend has the dialogue window's third line
    const char *keys[4], *words[4];
    int nk;
    LegendFor(r, keys, words, &nk);
    return Voice_Lines(text, BandVoiceWidth(keys, words, nk)) <= 2;  // as Menu_DrawFoot
}

void Menu_DrawFoot(const char **keysGiven, const char **wordsGiven, int nk) {
    if (StripPage()) {
        const WideFrame fr = Menu_WideFrame();
        DrawStripAt(fr.x0 + WIDE_INSET_X, fr.x0 + fr.w - 1 - WIDE_INSET_X, fr.y0 + fr.h - 1 - WIDE_INSET_Y, keysGiven,
                    wordsGiven, nk);
        return;
    }
    const char *keysIn[8], *words[8];
    nk = WithMore(keysGiven, wordsGiven, nk, BandVoiceWidth(keysGiven, wordsGiven, nk), 2, keysIn, words);
    // After a pad press the legend names the pad's buttons, not the keyboard's.
    const char *keys[8];
    for (int i = 0; i < nk && i < 8; i++)
        keys[i] = Bindings_LegendKey(keysIn[i]);
    WideFrame f = Menu_WideFrame();
    S32 W = f.w, H = f.h;
    const S32 side = WIDE_INSET_X;
    const S32 chip = 2;  // the 8x8 key font at 2x (decided 2026-11-03)
    S32 bx0 = f.x0 + side, bx1 = f.x0 + W - side - 1;
    S32 bandY0 = f.y0 + H - WIDE_INSET_Y - BAND_H, bandY1 = f.y0 + H - 1 - WIDE_INSET_Y;
    S32 legendW = LegendWidth(keys, words, nk, chip);
    Ui_Panel(bx0, bandY0, bx1, bandY1, 15);  // darkened to level 15 (decided 2026-11-03)
    S32 voiceW = BandVoiceWidth(keys, words, nk);
    g_voiceLines = Voice_Draw(g_voice, bx0 + PANEL_PAD_X, bandY0 + 5, voiceW, 2, &g_voiceNeeded);
    g_voiceMax = 2;
    g_voiceWidth = voiceW;
    NotePageMark();
    DrawLegendStacked(bx1 - 24 - legendW + 1, bandY0 + 7, keys, keysIn, words, nk, chip);
    g_helpBox.shown = true;
    g_helpBox.kind = "band";
    g_helpBox.x0 = bx0; g_helpBox.y0 = bandY0; g_helpBox.x1 = bx1; g_helpBox.y1 = bandY1;
}

// The prototype's own lines, the F1 keys and a switch's note, on one line in the top margin:
// the one place free at every size (EPmager, 2026-11-12: a list at the top right hid the
// rows under it, the note at the bottom right the legend's words). Each switch's toggle
// names its value in the note.
static void DrawTopLine(const char *s, U8 ink) {
    const S32 w = Ui_SmallWidth(s, 1), x = (S32)Host_LogicalW() / 2 - w / 2, y = 4;
    Box(x - 4, y - 3, x + w + 3, y + 10, 0);
    Ui_Small(x, y, s, ink, 1);
}

static void DrawOverlay() {
    DrawTopLine("F1 hide  F2 size  F3 green  F4 long help  F10 key colours  F11 pad names  F12 shapes", INK_CONFIG_MENU);
}

// The console as lba2cc draws it (CONSOLE.CPP Console_Draw): the top half of the screen,
// darkened, the 8x8 font at 9 px a line from x 8, a line under it, the prompt "]> " last.
// Here on the logical canvas (lba2cc draws it on the render buffer). The messages are
// examples of what a launch logs.
static const char *const kConsoleLines[][2] = {
    {"h", "LBA2 Classic Community %s \xFA macOS arm64 \xFA 10 cores \xFA 16 GB RAM"},
    {"t", "Built Nov  3 2026 12:00:00 from commit %s"},
    {"t", "Game data: ~/Games/lba2 (found by: the install folder)"},
    {"t", "Profile: ~/Library/Application Support/lba2cc"},
    {"d", "Video: 1920 x 1080 fullscreen, vsync on"},
    {"t", "Island 0, cube 0: Twinsen's house"},
    {"t", "Type help for the commands."},
};
S32 g_consoleBottom;  // the line under it, for `state`

static void DrawConsole() {
    S32 W = (S32)Host_LogicalW(), H = (S32)Host_LogicalH();
    S32 h = H / 2;
    if (h < 9 * 4)
        h = 9 * 4;
    GamePieces_Shade(0, 0, W - 1, h - 1, 15);
    Line(0, h - 1, W - 1, h - 1, INK_WHITE);
    g_consoleBottom = h - 1;
    S32 n = (S32)(sizeof kConsoleLines / sizeof kConsoleLines[0]);
    S32 lines = h / 9 - 1;  // one is the prompt's
    S32 y = 0;
    for (S32 i = n > lines ? n - lines : 0; i < n; i++, y += 9) {
        char line[160];
        snprintf(line, sizeof line, kConsoleLines[i][1], kConsoleLines[i][0][0] == 'h' ? kVersionString : kCommit);
        U8 ink = kConsoleLines[i][0][0] == 'h' ? INK_CONFIG_HEAD : kConsoleLines[i][0][0] == 'd' ? INK_DISABLED : INK_WHITE;
        Ui_Small(8, y, line, ink, 1);
    }
    if (g_console == CON_READONLY)
        Ui_Small(8, y, "Read only. Any button closes it.", INK_CONFIG_HEAD, 1);
    else
        Ui_Small(8, y, (Host_TimeMs() / 500) % 2 ? "]> " : "]> _", INK_WHITE, 1);
}

// Frame rate on: the count in the bottom left corner, white, the 8x8 font (OBJECT.CPP
// AffNbFps). lba2cc draws it only in play; here it shows over the menu so the row can be judged.
static void DrawFps() {
    static U32 windowStart, frames, shown;
    U32 now = Host_TimeMs();
    frames++;
    if (now - windowStart >= 1000) {
        shown = frames * 1000 / (now - windowStart ? now - windowStart : 1);
        windowStart = now;
        frames = 0;
    }
    char buf[16];
    snprintf(buf, sizeof buf, "%u ", (unsigned)shown);
    Ui_Small(0, (S32)Host_LogicalH() - 8, buf, INK_WHITE, 1);
}

// The picture at the screen's own resolution, as the Image row says (decided 2026-11-11):
// Classic the smallest whole step that covers the screen, Smooth the exact cover factor.
static void SyncBackgroundRes() {
    const int want = 1 + Settings_Value(S_IMAGE);
    if (want != Host_BackgroundRes())
        Host_SetBackgroundRes(want);
}

// The pretend game, or Quit's screen. The game is a landscape drawn in code (game_scene.h),
// its saves' pictures taken from it, with three lines in the voice at its pitch near the
// bottom (on one, 784 wide, they would not fit 640). Quit's screen is black, two lines.
static void DrawClosed() {
    const S32 W = (S32)Host_LogicalW(), H = (S32)Host_LogicalH();
    char what[128];
    const char *lines[3];
    int n;
    S32 y0;
    if (Game_QuitShown()) {
        Box(0, 0, W - 1, H - 1, 0);
        lines[0] = Ui_Tr("The game would close here.");
        lines[1] = Ui_Tr("<ENTER> starts it again. <ESC> quits.");
        n = 2;
        y0 = H / 2 - TEXT_LINE_H;
    } else {
        GameScene_Draw((U8 *)Log + TabOffLine[0], TabOffLine[1] - TabOffLine[0], (U32)W, (U32)H, Game_Spot());
        ScreenPicture_NoPicture(0);  // the menu picture is gone: the canvas is all game
        if (Game_JustSaved())
            snprintf(what, sizeof what, "%s", Lang_Tr("Game saved.", Settings_Value(S_TEXT_LANG)));
        else if (Game_PlayingWhat()[0])
            snprintf(what, sizeof what, "%s", Game_PlayingWhat());
        else
            snprintf(what, sizeof what, "%s", Ui_Tr("A new game."));
        lines[0] = what;
        lines[1] = Ui_Tr("<ENTER> walks on to the next place.");
        lines[2] = Ui_Tr("<ESC> opens the game menu.");
        n = 3;
        y0 = H - 3 * TEXT_LINE_H - 8;
        GamePieces_Shade(0, y0 - 6, W - 1, H - 1, MENU_SHADE_LVL);  // the menu's darkening, so the lines read on any scene
    }
    for (int i = 0; i < n; i++) {
        const S32 w = Ui_TextWidthAsIs(lines[i]), y = y0 + i * TEXT_LINE_H;
        // the dialogue shadow
        Ui_TextAsIs(W / 2 - w / 2 + 2, y + 4, lines[i], INK_BLACK);
        Ui_TextAsIs(W / 2 - w / 2, y, lines[i], DIALOGUE_HERO * 16 + 12);
        Menu_Record(Game_QuitShown() ? "quit" : "game", lines[i], "", FILL_SHADE, W / 2 - w / 2, y, W / 2 + w / 2,
                    y + TEXT_LINE_H)
            .textLeft = W / 2 - w / 2;
    }
}

void Menu_Draw() {
    SyncBackgroundRes();
    Host_SetPropScale(sw_propscale);
    ScreenFont_SetSubpixel(sw_zsub != 0);
    PropScale_BeginFrame();
    ScreenFont_Set(Settings_Value(S_FONT));  // before the layout: widths follow the font
    Ui_SetLang(Settings_Value(S_TEXT_LANG));  // and the language
    ScreenFont_BeginFrame();
    VideoLogo_BeginFrame();
    VideoLogo_SetIdle(sw_videologo);
    VideoLogo_SetCc(sw_logocc != 0);
    Ui_BeginFrame();
    Menu_SyncView();
    g_drawnCount = 0;
    g_pointCount = 0;
    g_bar.shown = false;
    g_helpBox.shown = false;
    g_helpBox.x0 = g_helpBox.y0 = g_helpBox.x1 = g_helpBox.y1 = 0;  // where it was is no help on a new screen
    g_voiceLines = g_voiceNeeded = g_voiceMax = g_voiceWidth = 0;
    g_legend[0] = 0;
    g_legendBox.shown = false;

    Host_SetBackdrop(Settings_Value(S_BACKGROUND));
    Host_DrawBackground();
    GamePieces_AnimatePlasma();
    if (Game_InProgress() && !MenuClosed()) {  // in a game the menu is over the game as it was left
        const U32 W = Host_LogicalW(), H = Host_LogicalH();
        // Esc's dimming in RGB as the scene is drawn: the fog table on a dithered scene speckles.
        GameScene_Draw((U8 *)Log + TabOffLine[0], TabOffLine[1] - TabOffLine[0], W, H, Game_Spot(),
                       Game_Dimmed() ? GAME_DIM_KEEP : 1.0f);
        ScreenPicture_NoPicture(0);
    }

    Ui_SetBackRight(BackShown() ? BackLeft() + BackWidth() : 0);
    g_backBox.shown = false;
    if (MenuClosed()) {
        DrawClosed();
    } else if (const PageHooks *h = TopHooks(); h && h->draw) {
        h->draw();  // a page with its own view
    } else if (Wide()) {
        DrawWide();
    } else {
        DrawColumn();
    }
    if (BackShown())
        DrawBack();

    if (g_console)
        DrawConsole();
    if (Settings_Value(S_FPS))
        DrawFps();
    VideoLogo_NoteBegin();  // the logo stays above the top line
    if (g_toast[0] && Host_TimeMs() < g_toastUntil)
        DrawTopLine(g_toast, INK_CONFIG_HEAD);
    else if (sw_overlay)
        DrawOverlay();
    VideoLogo_NoteEnd();
    g_wheelView = 0;  // turned where nothing scrolls: dropped
    g_scrollTo = -1;
}

