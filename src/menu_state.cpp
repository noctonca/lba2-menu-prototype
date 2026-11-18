// What the tests and scripts read and set: the switches, `set`, `open`, and `state`.
#include "menu.h"

#include "menu_internal.h"

#include "bindings.h"
#include "trace.h"
#include "video_logo.h"
#include "pad_icons.h"
#include "whats_new.h"

#include <stdio.h>
#include <string.h>

// Switches (prototype aids; every review switch was pruned 2026-11-12, keeping what was decided).
int sw_overlay = 0;
// Help typed on every cursor move (decided 2026-11-03); 1 shows it whole at once, for the self test.
int sw_whole = 0;
int sw_propscale = 5;  // proportional scale (prop_scale.h), mode 5 decided 2026-11-16
int sw_zsub = 0;       // Zilla Slab letters at quarter pixels
// The intro's logo in the product name's place (a trial, 2026-11-17; video_logo.h): 0 the
// wordmark; 1 the logo, its ripple every 30 seconds at half the video's height; 2 that
// ripple at the video's height; 3 at the video's height with the blue ring.
// The intro's logo for the name: 0 the wordmark, 1 to 3 the logo with one of its ripples
// every 30 seconds. The build-in is the same for all three: the intro's, at its full
// height with the blue ring. 1, the gentle ripple after it, was chosen in play: the full
// one with the ring as the logo fades in, then the gentle one every 30 seconds (the
// maintainer, 2026-11-17, after trying 3 as the start).
int sw_videologo = 1;
int sw_logosub = 0;    // 1 = CLASSIC COMMUNITY kept under the logo in the picture's place
int sw_logocc = 1;     // the cc under the logo's 2; 0 = the logo as the intro has it
int sw_legendfit = 1;  // the help window's legend at the size that fits (EPmager, 2026-11-16)
// The chosen preset: "(current)" after its name always; 1 also gives it the green
// current-value plasma, as before (EPmager, 2026-11-11: green and the blue cursor read alike,
// so he asked for the words alone). F3; off at start, to show his proposal.
int sw_greencurrent = 0;
// Long help (asked for 2026-11-12, after the short help read as too short to be useful): a row's
// help is its short line, then a second window saying when or why to change it, turned with
// Space or the pad's Y ("More" in the legend); no 750 px cap. 0: the short help alone, as the
// 2026-11-03 cap left it. F4.
int sw_longhelp = 1;
// The pointer used last, as the host has it, or fixed for a test: switch pointer, 1 the mouse,
// 2 a finger (the self test's combos lay every screen out as each shows it).
static int sw_pointer = 0;
int LastPointer() { return sw_pointer ? sw_pointer : Host_LastPointer(); }

struct SwitchDef {
    const char *name;
    int *value;
    int min, max;
    int start;  // the value a fresh start has
};
static const SwitchDef kSwitches[] = {
    {"overlay", &sw_overlay, 0, 1, 0},
    {"padnames", Settings_Slot(S_PADNAMES), 0, 3, 0},  // the Button names row
    {"whole", &sw_whole, 0, 1, 0},
    {"greencurrent", &sw_greencurrent, 0, 1, 0},
    {"padicons", &g_padIcons, 0, 1, 1},  // shapes for the pad's buttons on chips
    {"roleink", &g_roleInk, 0, 1, 1},    // Enter green, Esc red in the legend
    {"longhelp", &sw_longhelp, 0, 1, 1},  // a second window of help, turned with Space or Y
    {"pointer", &sw_pointer, 0, 2, 0},   // for tests: as if the mouse (1) or a finger (2) was used last
    {"propscale", &sw_propscale, 0, 6, 5},  // the menu at a proportional scale (5, decided 2026-11-16), F5
    {"zsub", &sw_zsub, 0, 1, 0},  // Zilla Slab letters at quarter pixels, F6
    {"videologo", &sw_videologo, 0, 3, 1},  // the intro's logo for the wordmark, F9
    {"logosub", &sw_logosub, 0, 1, 0},
    {"logocc", &sw_logocc, 0, 1, 1},  // Shift+F9
    {"legendfit", &sw_legendfit, 0, 1, 1},  // the help window's legend fitted to its width, F8
    // What a new build tells a player, a trial (whats_new.h): 1 the first run after an update
    // from a pretend 0.13.2, 2 a later run of it, 3 a fresh install. Shift+F1.
    {"whatsnew", &sw_whatsnew, 0, 3, 0},
};

bool Menu_SetSwitch(const char *name, int v) {
    if (!strcmp(name, "videologo")) {
        // Switched on: the build-in from its start. From one ripple to another: that ripple
        // now, since the build-in is the same for all three and would only hide the change.
        if (sw_videologo >= 1 && v >= 1 && v <= 3 && v != sw_videologo)
            VideoLogo_RippleNow();
        else
            VideoLogo_Restart();
    }
    if (!strcmp(name, "keyboard")) {  // 0: as a console with no keyboard yet
        Host_SetKeyboardSeen(v != 0);
        return true;
    }
    for (size_t i = 0; i < sizeof kSwitches / sizeof kSwitches[0]; i++) {
        const SwitchDef &s = kSwitches[i];
        if (strcmp(s.name, name) != 0)
            continue;
        if (v < s.min || v > s.max)
            v = s.min + ((v - s.min) % (s.max - s.min + 1) + (s.max - s.min + 1)) % (s.max - s.min + 1);
        *s.value = v;
        if (s.value == &sw_whatsnew)
            WhatsNew_Reset();  // as the player starts the build: nothing seen yet
        g_voice.text[0] = 0;  // say it again under the new rules
        Toast("%s = %d", name, v);
        Trace("switch %s=%d", name, v);
        return true;
    }
    return false;
}

int Menu_GetSwitch(const char *name) {
    for (size_t i = 0; i < sizeof kSwitches / sizeof kSwitches[0]; i++)
        if (!strcmp(kSwitches[i].name, name))
            return *kSwitches[i].value;
    return -1;
}

bool Menu_Open(const char *name) {
    Menu_SyncView();
    if (!strcmp(name, "Keys and buttons")) {
        Menu_Open("Controls and camera");
        PutCursorOn(R_ACTION, A_KEYS);  // as Enter on it leaves it
        Activate(*CursorRow());
        UpdateVoice();
        return true;
    }
    if (!strcmp(name, "More options")) {
        Menu_Open("Advanced and reset");
        PutCursorOn(R_ACTION, A_EXP);  // as Enter on it leaves it
        Activate(*CursorRow());
        return true;
    }
    if (!strcmp(name, "What's new")) {  // from the hub's row, as Enter on it leaves it
        if (!WhatsNew_Offered())
            return false;  // no row to open it from: everything on it was seen
        Menu_Open("Options");
        PutCursorOn(R_CATEGORY, P_WHATSNEW);
        Activate(*CursorRow());
        return true;
    }
    if (!strcmp(name, "Console")) {
        Menu_Open("Advanced and reset");
        PutCursorOn(R_ACTION, A_CONSOLE);
        g_console = Host_LastInputWasPad() ? CON_READONLY : CON_FULL;
        return true;
    }
    g_console = CON_CLOSED;
    if (const PageHooks *h = TopHooks(); h && h->left)
        h->left();
    for (int p = P_MAIN; p <= P_SAVE; p++) {  // the game menu, and its pages from their rows
        if (strcmp(g_pages[p].name, name) != 0)
            continue;
        g_asking = NULL;
        Game_UnderMenu(p == P_SAVE);  // Save game is there only in a game
        OpenGameMenu();
        if (p != P_MAIN) {
            PutCursorOn(R_GAME, p == P_LOAD ? M_LOAD : M_SAVE);
            Activate(*CursorRow());
        }
        Trace("open page=%s (control)", name);
        return true;
    }
    for (int p = 0; p < P_KEYS; p++) {  // Keys and buttons opens from its row, above
        if (strcmp(g_pages[p].name, name) != 0)
            continue;
        // Esc comes back to the page's own row on the hub, as after Enter on it.
        int hubRow = OpeningRow(P_HUB);
        for (int i = 0; i < g_pages[P_HUB].count; i++)
            if (g_pages[P_HUB].rows[i].type != R_RETURN && g_pages[P_HUB].rows[i].arg == p)
                hubRow = i;
        // As the Options key opens them in play: Esc from the hub goes back to the game.
        g_asking = NULL;
        Game_UnderOptions();
        g_depth = 1;
        g_stack[0].page = P_HUB;
        g_stack[0].row = hubRow;
        if (p != P_HUB)
            Push(p);
        g_scroll = 0;
        Trace("open page=%s (control)", name);
        return true;
    }
    return false;
}

bool Menu_Set(const char *id, int value) {
    for (int s = 0; s < S_COUNT; s++) {
        if (strcmp(Settings_Def(s).id, id) != 0)
            continue;
        if (!Settings_Put(s, value))
            return false;
        Trace("set %s=%d (control)", id, value);
        return true;
    }
    return MoreOptions_Set(id, value);
}

void Menu_Reset() {
    for (size_t i = 0; i < sizeof kSwitches / sizeof kSwitches[0]; i++)
        *kSwitches[i].value = kSwitches[i].start;
    Host_ResetInput();
    VideoLogo_Restart();
    Bindings_Reset();
    MoreOptions_Defaults();
    g_asking = NULL;
    g_answer = 0;
    g_console = CON_CLOSED;
    Presets_ForgetBackups();
    WhatsNew_Reset();
    g_scroll = 0;
    g_toast[0] = 0;
    g_toastUntil = 0;
    Menu_Init();
}

int Menu_DrawnRows(const DrawnRow **rows) {
    *rows = g_drawn;
    return g_drawnCount;
}

void Menu_Help(MenuHelp *h) {
    h->where = g_helpBox.shown ? g_helpBox.kind : "none";
    h->x0 = g_helpBox.x0;
    h->y0 = g_helpBox.y0;
    h->x1 = g_helpBox.x1;
    h->y1 = g_helpBox.y1;
    h->needed = g_voiceNeeded;
    S32 page, pages;
    Voice_LastPage(&page, &pages);
    if (!g_helpBox.shown)
        page = pages = 0;  // no help this frame: the count is the last one drawn, elsewhere
    h->page = page;
    h->pages = pages;
    h->max = g_voiceMax;
    h->width = g_voiceWidth;
    h->text = g_voice.text;
    h->legend = g_legendBox.shown;
    h->lx0 = g_legendBox.x0, h->ly0 = g_legendBox.y0, h->lx1 = g_legendBox.x1, h->ly1 = g_legendBox.y1;
    h->legendScale = g_legendBox.scale;
}

bool Menu_WantsQuit() { return g_quit; }

const char *Menu_Layout() {
    return Game_Playing() ? "game" : Game_QuitShown() ? "quit" : Wide() ? "wide" : "column";
}

const char *Menu_PageName() {
    return g_pages[Top().page].name;
}

// One for the hub; each page opened and a question add one.
static int Depth() {
    return g_depth + (g_asking ? 1 : 0);
}

const char *Menu_CursorLabel() {
    static char buf[160];
    const Row *r = MenuClosed() ? NULL : CursorRow();
    if (!r || r->type == R_TUNING)  // More options' rows are named by its own getters
        return "";
    return r->type == R_PRESET_ROW ? PresetRowLabel(buf, sizeof buf) : RowLabel(*r);
}

int Menu_Where(char *buf, size_t n) {
    if (MenuClosed()) {
        snprintf(buf, n, "%s", Game_Playing() ? "game" : "quit");
        return 0;
    }
    if (const PageHooks *h = TopHooks(); h && h->where)
        return Depth() + h->where(buf, n);  // a page with its own view says where its cursor is
    if (g_console) {
        snprintf(buf, n, "%s | %s | console", Menu_PageName(), Menu_CursorLabel());
        return Depth() + 1;
    }
    if (g_asking)
        snprintf(buf, n, "%s | %s | confirm", Menu_PageName(), g_answer ? "Yes" : "No");
    else
        snprintf(buf, n, "%s | %s", Menu_PageName(), Menu_CursorLabel());
    return Depth();
}

bool Menu_CursorSetting(bool *active) {
    const Row *r = MenuClosed() ? NULL : CursorRow();
    if (!r || r->type != R_SETTING)
        return false;
    *active = RowActive(*r);
    return true;
}

const char *Menu_CursorValue() {
    for (int i = 0; i < g_drawnCount; i++)
        if (!strcmp(g_drawn[i].fill, "cursor"))
            return g_drawn[i].value;
    return "";
}

const char *Menu_ConsoleMode() {
    return g_console == CON_FULL ? "full" : g_console == CON_READONLY ? "read-only" : "closed";
}

const char *Menu_PresetLabel() {
    static char buf[160];
    return PresetRowLabel(buf, sizeof buf);
}

void Menu_JsonString(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\')
            fprintf(f, "\\%c", c);
        else if (c < 0x20)
            fprintf(f, "\\u%04x", c);
        else
            fputc(c, f);
    }
    fputc('"', f);
}

// Widths in the game font of every string a page can show, for sizing panes:
// labels, every value a setting can take (ink), help, category names.
void Menu_WriteMeasure(FILE *f) {
    fprintf(f, "{\"settings\":[");
    for (int s = 0; s < S_COUNT; s++) {
        const Setting &st = Settings_Def(s);
        S32 vmax = 0;
        if (st.kind == K_CHOICE || st.kind == K_TOGGLE) {
            for (int v = 0; v <= st.max; v++) {
                S32 w = Ui_TextInkWidth(st.names[v]);
                if (w > vmax)
                    vmax = w;
            }
        } else if (st.kind == K_SLIDER) {
            vmax = 128;
        } else {
            S32 left = 0;
            vmax = -1;  // steps: measured where drawn
            (void)left;
        }
        fprintf(f, "%s{\"id\":\"%s\",\"label\":\"%s\",\"label_w\":%d,\"value_w\":%d,\"help_w\":%d}", s ? "," : "",
                st.id, st.label, Ui_TextWidth(st.label), vmax, Ui_TextWidth(st.help));
    }
    fprintf(f, "],\"hub\":[");
    for (int i = 0; i < g_pages[P_HUB].count; i++) {
        const Row &r = g_pages[P_HUB].rows[i];
        char label[160];
        if (r.type == R_PRESET_ROW) {
            FormatPresetRow(label, sizeof label, "Classic", true);
        } else {
            snprintf(label, sizeof label, "%s", RowLabel(r));
        }
        fprintf(f, "%s{\"label\":\"%s\",\"w\":%d,\"help_w\":%d}", i ? "," : "", label, Ui_TextWidth(label),
                Ui_TextWidth(r.help ? r.help : ""));
    }
    fprintf(f, "]}\n");
}

void Menu_WriteState(FILE *f) {
    Menu_SyncView();
    char buf[160];
    fprintf(f, "{\"t\":%u,\"physical\":[%u,%u],\"logical\":[%u,%u],\"scale\":%u,\"layout\":",
            (unsigned)Host_TimeMs(), (unsigned)Host_PhysW(), (unsigned)Host_PhysH(),
            (unsigned)Host_LogicalW(), (unsigned)Host_LogicalH(),
            (unsigned)Host_Scale(Host_PhysW(), Host_PhysH()));
    Menu_JsonString(f, Menu_Layout());
    fprintf(f, ",\"page\":");
    Menu_JsonString(f, Menu_PageName());
    const Place &top = Top();
    fprintf(f, ",\"stack\":[");
    for (int i = 0; i < g_depth; i++) {
        const Row &r = g_pages[g_stack[i].page].rows[g_stack[i].row];
        fprintf(f, "%s{\"page\":", i ? "," : "");
        Menu_JsonString(f, g_pages[g_stack[i].page].name);
        fprintf(f, ",\"row\":");
        Menu_JsonString(f, r.type == R_PRESET_ROW ? PresetRowLabel(buf, sizeof buf) : RowLabel(r));
        fprintf(f, "}");
    }
    fprintf(f, "],\"cursor\":%d,\"scroll\":%d,\"confirm\":%s", Position(top.page, top.row), g_scroll,
            g_asking ? "true" : "false");
    const Row *r = CursorRow();
    fprintf(f, ",\"cursor_row\":");
    Menu_JsonString(f, r ? (r->type == R_PRESET_ROW ? PresetRowLabel(buf, sizeof buf) : RowLabel(*r)) : "");
    fprintf(f, ",\"preset\":\"%s\",\"preset_changed\":%s,\"preset_row\":", Presets_Name(Presets_Chosen()),
            Presets_Changed() ? "true" : "false");
    Menu_JsonString(f, PresetRowLabel(buf, sizeof buf));

    fprintf(f, ",\"rows\":[");
    for (int i = 0; i < g_drawnCount; i++) {
        const DrawnRow &d = g_drawn[i];
        fprintf(f, "%s{\"pane\":\"%s\",\"label\":", i ? "," : "", d.pane);
        Menu_JsonString(f, d.label);
        fprintf(f, ",\"value\":");
        Menu_JsonString(f, d.value);
        fprintf(f, ",\"fill\":\"%s\",\"rect\":[%d,%d,%d,%d],\"dim\":%s,\"changed\":%s,\"marks\":%s,\"setting\":%s,\"text_left\":%d,\"text_right\":%d,\"value_right\":%d",
                d.fill, d.x0, d.y0, d.x1, d.y1, d.dim ? "true" : "false", d.changed ? "true" : "false",
                d.marks ? "true" : "false", d.setting ? "true" : "false", d.textLeft, d.textRight, d.valueRight);
        // "new" only while the trial marks the row, so a record with the switch off is as before.
        fprintf(f, "%s}", d.isNew ? ",\"new\":true" : "");
    }
    fprintf(f, "]");

    int total = (int)strlen(g_voice.text);
    int shown = g_voice.typed ? (int)((Host_TimeMs() - g_voice.start) / 20) : total;
    if (shown > total)
        shown = total;
    fprintf(f, ",\"voice\":{\"text\":");
    Menu_JsonString(f, g_voice.text);
    fprintf(f, ",\"typed\":%s,\"letters_shown\":%d,\"letters\":%d,\"where\":", g_voice.typed ? "true" : "false",
            shown, total);
    Menu_JsonString(f, g_helpBox.shown ? g_helpBox.kind : "none");
    S32 page, pages;
    Voice_LastPage(&page, &pages);
    // Help longer than its window pages on; it is cut off only past what the voice wraps.
    fprintf(f, ",\"lines\":%d,\"lines_needed\":%d,\"max_lines\":%d,\"width\":%d,\"page\":%d,\"pages\":%d,\"cut_off\":%s",
            g_voiceLines, g_voiceNeeded, g_voiceMax, g_voiceWidth, (int)page, (int)pages,
            g_voiceNeeded >= VOICE_MAX_LINES ? "true" : "false");
    fprintf(f, ",\"page_mark\":");
    Menu_JsonString(f, g_pageMark);
    fprintf(f, ",\"rect\":[%d,%d,%d,%d]}", g_helpBox.x0, g_helpBox.y0, g_helpBox.x1, g_helpBox.y1);
    fprintf(f, ",\"legend\":");
    Menu_JsonString(f, g_legend);
    {
        bool isValue;
        const bool steps = Menu_CursorEnterSteps(&isValue);
        fprintf(f, ",\"cursor_value\":\"%s\"", !isValue ? "none" : steps ? "enter steps" : "enter leaves");
    }
    fprintf(f, ",\"clip\":[%d,%d,%d,%d]", (int)ClipXMin, (int)ClipYMin, (int)ClipXMax, (int)ClipYMax);
    // What the pointer can point at, as the last draw placed it: [kind, x0, y0, x1, y1, a, b].
    static const char *const kPointKinds[] = {"row", "chip", "answer", "step", "slider", "key", "scroll"};
    static_assert(sizeof kPointKinds / sizeof kPointKinds[0] == POINT_SCROLL + 1, "a name for every PointKind");
    fprintf(f, ",\"pointer\":\"%s\"", LastPointer() == POINTER_TOUCH ? "touch" : LastPointer() == POINTER_MOUSE ? "mouse" : "none");
    if (g_backBox.shown)
        fprintf(f, ",\"back\":[%d,%d,%d,%d]", (int)g_backBox.x0, (int)g_backBox.y0, (int)g_backBox.x1, (int)g_backBox.y1);
    fprintf(f, ",\"points\":[");
    for (int i = 0; i < g_pointCount; i++) {
        const PointTarget &p = g_points[i];
        fprintf(f, "%s[\"%s\",%d,%d,%d,%d,%d,%d]", i ? "," : "", kPointKinds[p.kind], (int)p.x0, (int)p.y0, (int)p.x1,
                (int)p.y1, p.a, p.b);
    }
    fprintf(f, "]");
    if (g_bar.shown)  // groove, thumb, and the view it shows
        fprintf(f, ",\"scrollbar\":{\"groove\":[%d,%d,%d,%d],\"thumb\":[%d,%d],\"first\":%d,\"visible\":%d,\"total\":%d}",
                (int)g_bar.x0, (int)g_bar.y0, (int)g_bar.x1, (int)g_bar.y1, (int)g_bar.ty0, (int)g_bar.ty1, g_bar.first,
                g_bar.visible, g_bar.total);
    fprintf(f, ",\"console\":\"%s\",\"console_bottom\":%d,\"version\":", g_console == CON_FULL ? "full" : g_console == CON_READONLY ? "read-only" : "closed",
            g_console ? (int)g_consoleBottom : -1);
    Menu_JsonString(f, kVersion);

    if (const PageHooks *h = TopHooks(); h && h->writeState) {
        fprintf(f, ",\"%s\":", h->stateName);
        h->writeState(f);
    }
    if (sw_whatsnew) {
        fprintf(f, ",\"whats_new\":");
        WhatsNew_WriteState(f);
    }
    fprintf(f, ",\"switches\":{");
    for (size_t i = 0; i < sizeof kSwitches / sizeof kSwitches[0]; i++)
        fprintf(f, "%s\"%s\":%d", i ? "," : "", kSwitches[i].name, *kSwitches[i].value);
    fprintf(f, "},\"settings\":{");
    for (int s = 0; s < S_COUNT; s++)
        fprintf(f, "%s\"%s\":%d", s ? "," : "", Settings_Def(s).id, Settings_Value(s));
    fprintf(f, "}}\n");
    fflush(f);
}
