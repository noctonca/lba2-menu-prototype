#include "menu.h"

#include "cadre.h"
#include "controls.h"
#include "engine.h"
#include "trace.h"
#include "menu_parts.h"
#include "ui.h"

#include <SVGA/BOX.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SVGA/FIL_LINE.H>
#include <SVGA/FONT.H>

#include <stdio.h>
#include <string.h>

// ───────────────────────────── The fake settings ─────────────────────────────

enum Kind { K_CHOICE, K_TOGGLE, K_STEPS, K_SLIDER };

struct Setting {
    const char *id;
    const char *label;
    Kind kind;
    int max;                 // last value (choices: count - 1)
    const char *names[8];    // value names for choices, toggles and steps
    int fresh;               // what a fresh install has (= Modern)
    int classic;             // what Classic sets, when presetSets
    bool presetSets;
    const char *help;
};

enum {
    S_TEXT_LANG, S_VOICE_LANG, S_SUBTITLES,
    S_RESOLUTION, S_FULLSCREEN, S_VSYNC, S_DETAIL, S_VIDEOS, S_IMAGE,
    S_MASTER, S_SFX, S_VOICES, S_MUSIC,
    S_CAM_OUT, S_CAM_IN, S_INVERT, S_SENS,
    S_FIXBUGS, S_SHOWEXP,
    S_DODGE, S_HINTS,
    S_COUNT
};

// Example values only: the prototype's settings live in memory.
static const Setting kSettings[S_COUNT] = {
    {"text_lang", "Text", K_CHOICE, 5, {"English", "Francais", "Deutsch", "Espanol", "Italiano", "Portugues"}, 0, 0, false,
     "The language the game writes in."},
    {"voice_lang", "Voices", K_CHOICE, 2, {"English", "Francais", "Deutsch"}, 0, 0, false,
     "The language the characters speak."},
    {"subtitles", "Subtitles", K_TOGGLE, 1, {"Off", "On"}, 1, 1, false,
     "Shows what is said while the voices play."},
    {"resolution", "Resolution", K_CHOICE, 3, {"640 x 480", "1280 x 720", "1920 x 1080", "2560 x 1440"}, 2, 2, false,
     "The size of the picture the game draws."},
    {"fullscreen", "Fullscreen", K_TOGGLE, 1, {"Off", "On"}, 1, 1, false,
     "Fills the screen, or plays in a window."},
    {"vsync", "Vsync", K_TOGGLE, 1, {"Off", "On"}, 1, 1, false,
     "Stops the picture tearing. Off can raise the frame rate on a slow display."},
    {"detail", "Detail level", K_STEPS, 3, {"Low", "Medium", "High", "Full"}, 3, 3, false,
     "Rain, shadows and the far islands. Lower it on a slow machine."},
    {"videos", "Videos", K_CHOICE, 1, {"Full screen", "Original size"}, 0, 0, false,
     "How the films are shown."},
    {"image", "Image", K_CHOICE, 1, {"Classic", "Smooth"}, 0, 0, false,
     "Sharp pixels as in 1997, or smoothed when scaled."},
    {"master", "Master volume", K_SLIDER, 127, {0}, 100, 100, false, "Everything at once."},
    {"sfx", "Sound effects", K_SLIDER, 127, {0}, 100, 100, false, "Footsteps, doors, magic."},
    {"voices", "Voices volume", K_SLIDER, 127, {0}, 110, 110, false, "The characters' voices."},
    {"music", "Music", K_SLIDER, 127, {0}, 90, 90, false, "The score."},
    {"cam_out", "Outdoors camera", K_CHOICE, 1, {"Classic", "Auto"}, 0, 0, true,
     "Classic holds still and cuts; Auto turns to follow you."},
    {"cam_in", "Indoors camera", K_CHOICE, 1, {"Classic", "Follow"}, 0, 0, true,
     "Follow keeps Twinsen in view inside houses."},
    {"invert", "Invert camera", K_TOGGLE, 1, {"Off", "On"}, 0, 0, false,
     "Turns the camera the other way. Only with the Auto camera."},
    {"sensitivity", "Camera speed", K_SLIDER, 10, {0}, 5, 5, false,
     "How fast the camera turns. Only with the Auto camera."},
    {"fix_bugs", "Fix retail bugs", K_TOGGLE, 1, {"Off", "On"}, 1, 0, true,
     "Fixes the 1997 release's known bugs. Off plays them as they were."},
    {"show_exp", "Show experimental settings", K_TOGGLE, 1, {"Off", "On"}, 0, 0, false,
     "Settings still being tried. They may change or go."},
    {"dodge", "Dodge", K_CHOICE, 1, {"Classic", "Double tap"}, 0, 0, false,
     "How Twinsen jumps aside in Athletic mode."},
    {"hints", "Behaviour hints", K_TOGGLE, 1, {"Off", "On"}, 0, 0, false,
     "Names the behaviour when you change it."},
};

static int g_val[S_COUNT];
static int g_preset;  // 0 Modern, 1 Classic
static int g_before[S_COUNT];  // before a preset was chosen: the preset page's Back to my settings
static int g_beforePreset;
static bool g_haveBefore;
static int g_beforeReset[S_COUNT];  // before Reset settings: the Advanced page's Back to my settings
static int g_beforeResetPreset;
static bool g_haveBeforeReset;

static const char *kPresetNames[2] = {"Modern", "Classic"};

static int PresetValue(int preset, int s) {
    const Setting &st = kSettings[s];
    if (!st.presetSets)
        return st.fresh;
    return preset == 1 ? st.classic : st.fresh;
}

// "Changed": the player's values differ from the chosen preset in a setting presets set.
static bool PresetChanged() {
    for (int s = 0; s < S_COUNT; s++)
        if (kSettings[s].presetSets && g_val[s] != PresetValue(g_preset, s))
            return true;
    return false;
}

static void ApplyPreset(int p) {
    if (!g_haveBefore) {
        memcpy(g_before, g_val, sizeof g_val);
        g_beforePreset = g_preset;
        g_haveBefore = true;
    }
    g_preset = p;
    for (int s = 0; s < S_COUNT; s++)
        if (kSettings[s].presetSets)
            g_val[s] = PresetValue(p, s);
    Trace("apply preset=%s", kPresetNames[p]);
}

static bool BackDiffers() {
    if (!g_haveBefore)
        return false;
    for (int s = 0; s < S_COUNT; s++)
        if (kSettings[s].presetSets && g_before[s] != g_val[s])
            return true;
    return false;
}

// Reset settings puts every setting and the preset back; its undo covers them all.
static bool ResetBackDiffers() {
    if (!g_haveBeforeReset)
        return false;
    if (g_beforeResetPreset != g_preset)
        return true;
    return memcmp(g_beforeReset, g_val, sizeof g_val) != 0;
}

static void ResetSettings() {
    bool fresh = g_preset == 0;
    for (int s = 0; s < S_COUNT; s++)
        fresh = fresh && g_val[s] == kSettings[s].fresh;
    if (!fresh) {  // a second reset with nothing changed keeps the first one's undo
        memcpy(g_beforeReset, g_val, sizeof g_val);
        g_beforeResetPreset = g_preset;
        g_haveBeforeReset = true;
    }
    for (int s = 0; s < S_COUNT; s++)
        g_val[s] = kSettings[s].fresh;
    g_preset = 0;
    g_haveBefore = false;
    Trace("reset settings");
}

// ───────────────────────────── Pages as data ─────────────────────────────

enum RowType { R_RETURN, R_CATEGORY, R_PRESET_ROW, R_SETTING, R_PRESET_CHOICE, R_BACK_TO_MINE, R_ACTION };

struct Row {
    RowType type;
    const char *label;
    int arg;  // setting, page or preset
    const char *help;
};

enum { A_KEYS, A_RESET };  // R_ACTION: opens Keys and buttons, or asks to reset
enum { BACK_PRESET, BACK_RESET };  // R_BACK_TO_MINE: undoes a preset, or a reset

enum { P_HUB, P_PRESET, P_LANG, P_DISPLAY, P_SOUND, P_CONTROLS, P_ADVANCED, P_GAMEPLAY, P_FIRST, P_ORIGINAL, P_COUNT };

struct Page {
    const char *name;
    Row rows[12];
    int count;
};

static Page g_pages[P_COUNT];

// Switches.
static int sw_band = 1, sw_typing = 0, sw_chip = 2, sw_word = 0, sw_island = 0, sw_help640 = 0,
           sw_gameplay = 0, sw_overlay = 0;

static const char *kWords[3] = {"Settings", "Preset", "Rules"};

static void AddRow(Page &p, RowType t, const char *label, int arg, const char *help) {
    Row &r = p.rows[p.count++];
    r.type = t;
    r.label = label;
    r.arg = arg;
    r.help = help;
}

static void AddSetting(Page &p, int s) {
    AddRow(p, R_SETTING, kSettings[s].label, s, kSettings[s].help);
}

static void BuildPages() {
    memset(g_pages, 0, sizeof g_pages);

    Page &hub = g_pages[P_HUB];
    hub.name = "Options";
    AddRow(hub, R_RETURN, "Return to the game", 0, "Back to where you were.");
    AddRow(hub, R_PRESET_ROW, "", P_PRESET, "Choose how to play: the 1997 rules, or with the known bugs fixed.");
    AddRow(hub, R_CATEGORY, "Language and interface", P_LANG, "Text, voices and subtitles.");
    AddRow(hub, R_CATEGORY, "Display and graphics", P_DISPLAY, "Resolution, window and detail.");
    AddRow(hub, R_CATEGORY, "Sound", P_SOUND, "Volumes.");
    AddRow(hub, R_CATEGORY, "Controls and camera", P_CONTROLS, "Keys, pad and the camera.");
    if (sw_gameplay)
        AddRow(hub, R_CATEGORY, "Gameplay", P_GAMEPLAY, "How Twinsen moves and what the game tells you.");
    AddRow(hub, R_CATEGORY, "Advanced and reset", P_ADVANCED, "The rules switch, experiments and a fresh start.");

    Page &pr = g_pages[P_PRESET];
    pr.name = "Preset";
    AddRow(pr, R_RETURN, "Return to previous menu", 0, "");
    AddRow(pr, R_PRESET_CHOICE, "Modern", 0, "The original, with its known bugs fixed. What a fresh install plays.");
    AddRow(pr, R_PRESET_CHOICE, "Classic", 1, "The rules of the 1997 CD release, bugs and all. Nothing else changes.");
    if (BackDiffers())
        AddRow(pr, R_BACK_TO_MINE, "Back to my settings", BACK_PRESET, "Puts back the settings you had before you chose a preset.");

    Page &lang = g_pages[P_LANG];
    lang.name = "Language and interface";
    AddRow(lang, R_RETURN, "Return to previous menu", 0, "");
    AddSetting(lang, S_TEXT_LANG);
    AddSetting(lang, S_VOICE_LANG);
    AddSetting(lang, S_SUBTITLES);

    Page &disp = g_pages[P_DISPLAY];
    disp.name = "Display and graphics";
    AddRow(disp, R_RETURN, "Return to previous menu", 0, "");
    AddSetting(disp, S_RESOLUTION);
    AddSetting(disp, S_FULLSCREEN);
    AddSetting(disp, S_VSYNC);
    AddSetting(disp, S_DETAIL);
    AddSetting(disp, S_VIDEOS);
    AddSetting(disp, S_IMAGE);

    Page &snd = g_pages[P_SOUND];
    snd.name = "Sound";
    AddRow(snd, R_RETURN, "Return to previous menu", 0, "");
    AddSetting(snd, S_MASTER);
    AddSetting(snd, S_SFX);
    AddSetting(snd, S_VOICES);
    AddSetting(snd, S_MUSIC);

    Page &ctl = g_pages[P_CONTROLS];
    ctl.name = "Controls and camera";
    AddRow(ctl, R_RETURN, "Return to previous menu", 0, "");
    AddRow(ctl, R_ACTION, "Keys and buttons", A_KEYS, "Change which key or button does what.");
    AddSetting(ctl, S_CAM_OUT);
    AddSetting(ctl, S_CAM_IN);
    AddSetting(ctl, S_INVERT);
    AddSetting(ctl, S_SENS);

    Page &adv = g_pages[P_ADVANCED];
    adv.name = "Advanced and reset";
    AddRow(adv, R_RETURN, "Return to previous menu", 0, "");
    AddSetting(adv, S_FIXBUGS);
    AddSetting(adv, S_SHOWEXP);
    AddRow(adv, R_ACTION, "Reset settings", A_RESET, "Puts every setting back as a fresh install has it.");
    if (ResetBackDiffers())
        AddRow(adv, R_BACK_TO_MINE, "Back to my settings", BACK_RESET, "Puts back what you had before the reset.");

    Page &gp = g_pages[P_GAMEPLAY];
    gp.name = "Gameplay";
    AddRow(gp, R_RETURN, "Return to previous menu", 0, "");
    AddSetting(gp, S_DODGE);
    AddSetting(gp, S_HINTS);

    // A fixture: today's Options page from the boot menu, to diff against lba2cc's
    // `ui menu-main-options` capture.
    Page &og = g_pages[P_ORIGINAL];
    og.name = "original";
    AddRow(og, R_RETURN, "Return to previous menu", 0, "");
    AddRow(og, R_ACTION, "Display", 0, "");
    AddRow(og, R_ACTION, "Sound volume", 0, "");
    AddRow(og, R_ACTION, "Choose language", 0, "");
    AddRow(og, R_ACTION, "Advanced options", 0, "");
    AddRow(og, R_ACTION, "Controls", 0, "");

    Page &fl = g_pages[P_FIRST];
    fl.name = "First launch";
    AddRow(fl, R_PRESET_CHOICE, "Modern", 0, "The original, with its known bugs fixed. You can change this later in the Options Menu.");
    AddRow(fl, R_PRESET_CHOICE, "Classic", 1, "The rules of the 1997 CD release, bugs and all. You can change this later in the Options Menu.");
}

// Invert and speed only mean something with the Auto camera: dimmed, not hidden.
static bool RowActive(const Row &r) {
    if (r.type == R_SETTING && (r.arg == S_INVERT || r.arg == S_SENS))
        return g_val[S_CAM_OUT] == 1;
    return true;
}

static const char *PresetRowLabel(char *buf, size_t n) {
    snprintf(buf, n, "%s: %s%s", kWords[sw_word], kPresetNames[g_preset], PresetChanged() ? ", changed" : "");
    return buf;
}

static const char *ValueText(int s, char *buf, size_t n) {
    const Setting &st = kSettings[s];
    if (st.kind == K_SLIDER)
        snprintf(buf, n, "%d", g_val[s]);
    else
        snprintf(buf, n, "%s", st.names[g_val[s]]);
    return buf;
}

// "Changed is a colour": differs from a fresh install, or for a setting presets
// set, from the chosen preset.
static bool ValueChanged(int s) {
    return g_val[s] != PresetValue(g_preset, s);
}

// ───────────────────────────── Navigation state ─────────────────────────────

enum Focus { F_LEFT, F_RIGHT };

static int g_stack[8];
static int g_cursorStack[8];
static int g_depth;
static int g_cursor;       // one column: cursor on the top page; wide: cursor in the focused pane
static int g_leftCursor;   // wide: cursor in the left pane
static Focus g_focus;
static int g_scroll;
static bool g_first;       // first-launch screen
static bool g_confirm;     // a question is up: Classic, or Reset settings
static int g_confirmCursor;
enum Question { Q_CLASSIC, Q_RESET };
static Question g_question;

// What a question says in the voice, and its answers: the safe one first.
struct QuestionText {
    const char *voice;
    const char *answers[2];
};
static const QuestionText kQuestions[2] = {
    {"Play by the 1997 rules? The screen, sound and keys stay as they are.", {"No, keep Modern", "Yes, play Classic"}},
    {"Put every setting back as a fresh install has it?", {"No, keep my settings", "Yes, reset settings"}},
};

static void Ask(Question q) {
    g_confirm = true;
    g_question = q;
    g_confirmCursor = 0;  // the safe answer first
    Trace("ask %s", q == Q_RESET ? "reset" : "classic");
}
static bool g_closed;      // left the Options (shown as a note)
static Voice g_voice;
static bool g_pageEntered; // typing mode 1: type once on entering a page
static char g_toast[128];
static U32 g_toastUntil;

static bool Wide() {
    return Engine_LogicalW() >= 840;
}

static void Toast(const char *fmt, const char *a, int v) {
    snprintf(g_toast, sizeof g_toast, fmt, a, v);
    g_toastUntil = Engine_TimeMs() + 2000;
}

// The page in the right pane (wide) or on screen (one column).
static int TopPage() {
    return g_stack[g_depth - 1];
}

// Wide: the left pane is the hub without its Return row.
static int LeftRowToHub(int i) { return i + 1; }
static int LeftCount() { return g_pages[P_HUB].count - 1; }

static int RightPage() {
    if (g_depth > 1)
        return TopPage();
    const Row &r = g_pages[P_HUB].rows[LeftRowToHub(g_leftCursor)];
    return r.arg;
}

// Wide right pane skips the Return row.
static int RightFirst() { return 1; }

static const Row *CursorRow() {
    if (g_confirm)
        return NULL;
    if (g_first)
        return &g_pages[P_FIRST].rows[g_cursor];
    if (!Wide())
        return &g_pages[TopPage()].rows[g_cursor];
    if (g_focus == F_LEFT)
        return &g_pages[P_HUB].rows[LeftRowToHub(g_leftCursor)];
    return &g_pages[RightPage()].rows[RightFirst() + g_cursor];
}

static void LegendFor(const Row *r, const char **keys, const char **words, int *n);
static S32 LegendWidth(const char **keys, const char **words, int n, S32 scale);

// Whether a line for the row under the cursor fits the voice it will be said in:
// the band beside the legend, the help line, or the 640x480 dialogue window.
static bool VoiceFits(const char *text, const Row *r) {
    S32 W = (S32)Engine_LogicalW();
    if (!Wide())
        return Voice_Lines(text, W - 49) <= 3;
    if (!sw_band)
        return Voice_Lines(text, W - 64) <= 1;
    const char *keys[4], *words[4];
    int nk;
    LegendFor(r, keys, words, &nk);
    S32 voiceW = W - 64 - LegendWidth(keys, words, nk, sw_chip) - 48;  // as Menu_DrawFoot
    return Voice_Lines(text, voiceW < 620 ? voiceW : 620) <= 2;
}

static const char *HelpFor(const Row *r) {
    static char buf[256];
    if (!r)
        return "";
    if (r->type == R_BACK_TO_MINE && r->arg == BACK_PRESET && BackDiffers()) {
        char list[160] = "";
        for (int s = 0; s < S_COUNT; s++) {
            if (kSettings[s].presetSets && g_before[s] != g_val[s]) {
                if (list[0])
                    strncat(list, ", ", sizeof list - strlen(list) - 1);
                strncat(list, kSettings[s].label, sizeof list - strlen(list) - 1);
            }
        }
        snprintf(buf, sizeof buf, "Puts back what you had: %s.", list);
        return buf;
    }
    if (r->type == R_BACK_TO_MINE && r->arg == BACK_RESET && ResetBackDiffers()) {
        // Every setting that differs, and the preset; as many named as the voice
        // holds, the rest counted.
        const char *names[S_COUNT + 1];
        char preset[48];
        int n = 0;
        if (g_beforeResetPreset != g_preset) {
            snprintf(preset, sizeof preset, "%s %c%s", kPresetNames[g_beforeResetPreset], kWords[sw_word][0] + 32,
                     kWords[sw_word] + 1);  // "Classic settings"
            names[n++] = preset;
        }
        for (int s = 0; s < S_COUNT; s++)
            if (g_beforeReset[s] != g_val[s])
                names[n++] = kSettings[s].label;
        for (int shown = n; shown > 0; shown--) {
            char list[200] = "";
            for (int i = 0; i < shown; i++) {
                if (i)
                    strncat(list, i + 1 == n ? " and " : ", ", sizeof list - strlen(list) - 1);
                strncat(list, names[i], sizeof list - strlen(list) - 1);
            }
            if (shown < n) {
                char more[24];
                snprintf(more, sizeof more, " and %d more", n - shown);
                strncat(list, more, sizeof list - strlen(list) - 1);
            }
            snprintf(buf, sizeof buf, "Puts back what you had: %s.", list);
            if (VoiceFits(buf, r))
                return buf;
        }
        return r->help;
    }
    return r->help ? r->help : "";
}

void Menu_Say(const char *text) {
    bool typed = sw_typing == 0 || (sw_typing == 1 && g_pageEntered);
    if (strcmp(g_voice.text, text) != 0) {
        Voice_Say(g_voice, text, typed);
        g_pageEntered = false;
    }
}

void Menu_PageEntered() { g_pageEntered = true; }

static void UpdateVoice() {
    const Row *r = CursorRow();
    const char *text = g_confirm ? kQuestions[g_question].voice : HelpFor(r);
    Menu_Say(text);
}

static void Push(int page) {
    g_cursorStack[g_depth - 1] = g_cursor;
    g_stack[g_depth++] = page;
    g_cursor = 0;  // the 1997 sub-menus open on their Return row
    g_scroll = 0;
    g_pageEntered = true;
    Trace("open page=%s", g_pages[page].name);
}

static void Pop() {
    if (g_depth > 1) {
        Trace("close page=%s", g_pages[TopPage()].name);
        g_depth--;
        g_cursor = g_cursorStack[g_depth - 1];
        g_scroll = 0;
        g_pageEntered = true;
    } else {
        g_closed = true;
        Trace("close options");
    }
}

void Menu_Init(bool firstLaunch) {
    for (int s = 0; s < S_COUNT; s++)
        g_val[s] = kSettings[s].fresh;
    g_preset = 0;
    BuildPages();
    g_depth = 1;
    g_stack[0] = P_HUB;
    g_cursor = 1;  // the preset row, as the mock draws it
    g_leftCursor = 0;
    g_focus = F_LEFT;
    g_first = firstLaunch;
    if (g_first)
        g_cursor = 0;
    g_pageEntered = true;
    g_voice.text[0] = 0;
    Trace("init layout=%s logical=%ux%u first=%d", Wide() ? "wide" : "column",
          (unsigned)Engine_LogicalW(), (unsigned)Engine_LogicalH(), (int)g_first);
}

static void ChangeSetting(int s, int dir) {
    const Setting &st = kSettings[s];
    int v = g_val[s];
    if (st.kind == K_SLIDER) {
        int step = st.max >= 100 ? 8 : 1;
        v += dir * step;
        if (v < 0)
            v = 0;
        if (v > st.max)
            v = st.max;
    } else if (st.kind == K_STEPS) {
        v += dir;
        if (v < 0)
            v = 0;
        if (v > st.max)
            v = st.max;
    } else {
        v = (v + dir + st.max + 1) % (st.max + 1);
    }
    if (v != g_val[s]) {
        g_val[s] = v;
        char buf[32];
        Trace("set %s=%s", st.id, ValueText(s, buf, sizeof buf));
        BuildPages();
    }
}

// Enter (or a click) on a row.
static void Activate(const Row &r) {
    switch (r.type) {
    case R_RETURN:
        Pop();
        break;
    case R_CATEGORY:
    case R_PRESET_ROW:
        if (Wide()) {
            g_focus = F_RIGHT;
            g_cursor = 0;
            g_pageEntered = true;
            Trace("focus right page=%s", g_pages[r.arg].name);
        } else {
            Push(r.arg);
        }
        break;
    case R_SETTING:
        if (kSettings[r.arg].kind == K_CHOICE || kSettings[r.arg].kind == K_TOGGLE)
            ChangeSetting(r.arg, +1);
        break;
    case R_PRESET_CHOICE:
        if (g_first) {
            ApplyPreset(r.arg);
            g_first = false;
            g_haveBefore = false;
            g_cursor = 1;
            Trace("first launch chose=%s", kPresetNames[r.arg]);
            break;
        }
        if (r.arg == 1 && g_preset != 1) {
            Ask(Q_CLASSIC);
        } else {
            ApplyPreset(r.arg);
            BuildPages();
        }
        break;
    case R_BACK_TO_MINE:
        if (r.arg == BACK_RESET) {
            memcpy(g_val, g_beforeReset, sizeof g_val);
            g_preset = g_beforeResetPreset;
            g_haveBeforeReset = false;
        } else {
            memcpy(g_val, g_before, sizeof g_val);
            g_preset = g_beforePreset;
            g_haveBefore = false;
        }
        Trace("back to my settings");
        BuildPages();
        if (!Wide() && g_cursor >= g_pages[TopPage()].count)
            g_cursor = g_pages[TopPage()].count - 1;
        break;
    case R_ACTION:
        if (r.arg == A_RESET)
            Ask(Q_RESET);
        else
            Controls_Open();
        break;
    }
}

static int CountOnFocus() {
    if (g_first)
        return g_pages[P_FIRST].count;
    if (!Wide())
        return g_pages[TopPage()].count;
    if (g_focus == F_LEFT)
        return LeftCount();
    return g_pages[RightPage()].count - RightFirst();
}

static void Move(int dir) {
    int n = CountOnFocus();
    if (n <= 0)
        return;
    if (Wide() && !g_first && g_focus == F_LEFT) {
        g_leftCursor = (g_leftCursor + dir + n) % n;
        g_pageEntered = true;
    } else {
        g_cursor = (g_cursor + dir + n) % n;
    }
    const Row *r = CursorRow();
    Trace("cursor %s", r ? (r->type == R_PRESET_ROW ? "preset row" : r->label) : "-");
}

void Menu_Update() {
    if (Controls_Active()) {
        Controls_Update();
        return;
    }
    if (g_closed) {
        if (Engine_Pressed(KEY_ENTER) || Engine_Pressed(KEY_ESC)) {
            g_closed = false;
            Trace("reopen options");
        }
        return;
    }
    if (g_confirm) {
        if (Engine_Pressed(KEY_UP) || Engine_Pressed(KEY_DOWN)) {
            g_confirmCursor ^= 1;
            Trace("cursor %s", g_confirmCursor ? "Yes" : "No");
        }
        if (Engine_Pressed(KEY_ESC)) {
            g_confirm = false;
            Trace("answer no (esc)");
        }
        if (Engine_Pressed(KEY_ENTER)) {
            g_confirm = false;
            Trace("answer %s", g_confirmCursor ? "yes" : "no");
            if (g_confirmCursor) {
                if (g_question == Q_RESET)
                    ResetSettings();
                else
                    ApplyPreset(1);
                BuildPages();
            }
        }
        UpdateVoice();
        return;
    }

    if (Engine_Pressed(KEY_UP))
        Move(-1);
    if (Engine_Pressed(KEY_DOWN))
        Move(+1);

    const Row *r = CursorRow();
    if (r && r->type == R_SETTING && RowActive(*r)) {
        if (Engine_Pressed(KEY_LEFT))
            ChangeSetting(r->arg, -1);
        if (Engine_Pressed(KEY_RIGHT))
            ChangeSetting(r->arg, +1);
    } else if (Wide() && !g_first && g_focus == F_LEFT && Engine_Pressed(KEY_RIGHT) && r) {
        Activate(*r);
    }
    r = CursorRow();
    if (r && Engine_Pressed(KEY_ENTER) && RowActive(*r))
        Activate(*r);

    if (Engine_Pressed(KEY_ESC)) {
        if (g_first) {
            Activate(g_pages[P_FIRST].rows[0]);  // Escape answers Modern
        } else if (Wide() && g_focus == F_RIGHT) {
            g_focus = F_LEFT;
            g_pageEntered = true;
            Trace("focus left");
        } else {
            Pop();
        }
    }

    // Keep cursors in range after pages change shape.
    int n = CountOnFocus();
    if (Wide() && !g_first && g_focus == F_LEFT) {
        if (g_leftCursor >= n)
            g_leftCursor = n - 1;
    } else if (g_cursor >= n) {
        g_cursor = n - 1;
    }
    if (!Controls_Active())  // Keys and buttons just opened: its help, not this row's
        UpdateVoice();
}

// ───────────────────────────── Drawing ─────────────────────────────

// What was drawn this frame, for `state`.
static DrawnRow g_drawn[40];
static int g_drawnCount;
static struct {
    bool shown;
    int x0, y0, x1, y1;
    const char *kind;  // band, help-line, dialogue
} g_helpBox;
static char g_legend[128];
static int g_voiceLines, g_voiceNeeded, g_voiceMax, g_voiceWidth;

DrawnRow &Menu_Record(const char *pane, const char *label, const char *value, RowFill fill, int x0, int y0, int x1,
                        int y1) {
    DrawnRow &d = g_drawn[g_drawnCount < 39 ? g_drawnCount++ : 39];
    snprintf(d.label, sizeof d.label, "%s", label);
    snprintf(d.value, sizeof d.value, "%s", value ? value : "");
    d.pane = pane;
    d.fill = fill == FILL_CURSOR ? "cursor" : fill == FILL_CURRENT ? "current" : "shade";
    d.x0 = x0;
    d.y0 = y0;
    d.x1 = x1;
    d.y1 = y1;
    d.dim = d.changed = d.marks = d.setting = false;
    d.textLeft = d.textRight = x0;
    d.valueRight = 0;
    return d;
}

// Centred text clipped to the row, the original's way (DrawOneChoice).
void Menu_RowLabelCentred(S32 x0, S32 x1, S32 yc, const char *s, U8 colour) {
    MemoClip();
    SetClip(x0 + 1, yc - 25, x1 - 1, yc + 25);
    Ui_Text((x0 + x1) / 2 - Ui_TextWidth(s) / 2, yc - 18, s, colour);
    RestoreClip();
}

// One column: the 1997 row, "Label: Value" in one string, sliders as fills.
static void DrawColumnRow(const Row &r, S32 x0, S32 y0, S32 x1, S32 y1, bool cursor) {
    S32 yc = (y0 + y1) / 2;
    char text[160], val[48];
    RowFill fill = cursor ? FILL_CURSOR : FILL_SHADE;
    bool active = RowActive(r);

    if (r.type == R_PRESET_CHOICE && !g_first && r.arg == g_preset && !cursor)
        fill = FILL_CURRENT;  // the chosen preset wears the current-value plasma

    bool slider = r.type == R_SETTING && (kSettings[r.arg].kind == K_SLIDER || kSettings[r.arg].kind == K_STEPS);
    if (slider && cursor && active) {
        // GAMEMENU.CPP: the slider plasma up to the value, fill-selected after it.
        const Setting &st = kSettings[r.arg];
        S32 x2 = x0 + (x1 - x0) * g_val[r.arg] / st.max;
        Cadre_BackupCorners(x0, y0, x1, y1);
        Cadre_FireBar(x0, y0, x1, y1, PLASMA_SLIDER);
        if (x2 < x1)
            Box(x2, y0, x1, y1, FILL_SELECTED);
        Cadre_Frame(x0, y0, x1, y1);
    } else {
        Ui_RowBackground(x0, y0, x1, y1, fill);
    }

    if (r.type == R_PRESET_ROW)
        PresetRowLabel(text, sizeof text);
    else if (r.type == R_SETTING && kSettings[r.arg].kind != K_SLIDER)
        snprintf(text, sizeof text, "%s: %s", r.label, ValueText(r.arg, val, sizeof val));
    else
        snprintf(text, sizeof text, "%s", r.label);
    Menu_RowLabelCentred(x0, x1, yc, text, active ? INK_WHITE : INK_DISABLED);

    DrawnRow &d = Menu_Record("column", text, r.type == R_SETTING ? ValueText(r.arg, val, sizeof val) : "", fill, x0, y0,
                         x1, y1);
    d.dim = !active;
    d.setting = r.type == R_SETTING;
    d.textLeft = (x0 + x1) / 2 - Ui_TextWidth(text) / 2;
    d.textRight = (x0 + x1) / 2 + Ui_TextWidth(text) / 2 + 8;  // 8: the original's side padding
}

// Wide: label at the pane's one edge, value in a column on the right.
static void DrawWideRow(const char *pane, const Row &r, S32 x0, S32 y0, S32 x1, S32 y1, RowFill fill) {
    S32 yc = (y0 + y1) / 2;
    char label[160], val[48];
    bool active = RowActive(r);
    Ui_RowBackground(x0, y0, x1, y1, fill);

    if (r.type == R_PRESET_ROW)
        PresetRowLabel(label, sizeof label);
    else
        snprintf(label, sizeof label, "%s", r.label);

    MemoClip();
    SetClip(x0 + 1, y0, x1 - 1, y1);
    Ui_Text(x0 + 16, yc - 18, label, active ? INK_WHITE : INK_DISABLED);
    RestoreClip();

    DrawnRow &d = Menu_Record(pane, label, "", fill, x0, y0, x1, y1);
    d.dim = !active;
    d.setting = r.type == R_SETTING;
    d.textLeft = x0 + 16;
    d.textRight = x0 + 16 + Ui_TextWidth(label);

    const S32 valueRight = x1 - 24;
    if (r.type == R_SETTING) {
        const Setting &st = kSettings[r.arg];
        bool changed = ValueChanged(r.arg);
        S32 left = valueRight;
        if (st.kind == K_STEPS) {
            Ui_Steps(valueRight, yc, st.max, g_val[r.arg], &left);
            snprintf(d.value, sizeof d.value, "%d/%d", g_val[r.arg], st.max);
        } else if (st.kind == K_SLIDER) {
            left = valueRight - 128;
            if (d.textRight + 8 > left - 17)
                d.textRight = x1 + (d.textRight + 8 - (left - 17));
            Ui_MiniSlider(left, valueRight, yc, g_val[r.arg], st.max, !active);
            snprintf(d.value, sizeof d.value, "%d", g_val[r.arg]);
        } else {
            ValueText(r.arg, val, sizeof val);
            left = valueRight + 1 - Ui_TextInkWidth(val);  // the ink ends on the value edge
            if (d.textRight + 8 > left - 17)
                d.textRight = x1 + (d.textRight + 8 - (left - 17));  // label runs into the value
            Ui_Text(left, yc - 18, val, !active ? INK_DISABLED : changed ? INK_COUNT : INK_WHITE);
            snprintf(d.value, sizeof d.value, "%s", val);
        }
        d.changed = changed;
        d.valueRight = valueRight;
        if (fill == FILL_CURSOR && active) {
            Ui_ChangeMarks(left - 17, valueRight + 17, yc);  // in the padding: the value never moves
            d.marks = true;
        }
    } else if (r.type == R_PRESET_CHOICE && r.arg == g_preset && PresetChanged()) {
        Ui_Text(valueRight + 1 - Ui_TextInkWidth("Changed"), yc - 18, "Changed", INK_COUNT);
        snprintf(d.value, sizeof d.value, "Changed");
        d.valueRight = valueRight;
    }
}

void Menu_DrawScrollBar(S32 x0, S32 y0, S32 y1, int first, int visible, int total) {
    // ScrollBarReview: 12 wide, thumb inset 2, in the margin.
    S32 x1 = x0 + 11;
    Cadre_Shade(x0 + 1, y0 + 1, x1 - 1, y1 - 1, MENU_SHADE_LVL);
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
}

// The legend: only the keys the selected row takes.
static void LegendFor(const Row *r, const char **keys, const char **words, int *n) {
    *n = 0;
    if (g_confirm) {
        keys[*n] = "\x18\x19"; words[(*n)++] = "Choose";
        keys[*n] = "Enter"; words[(*n)++] = "Answer";
        return;
    }
    if (r) {
        if (r->type == R_SETTING && RowActive(*r)) {
            Kind k = kSettings[r->arg].kind;
            keys[*n] = "\x11\x10";
            words[(*n)++] = (k == K_SLIDER || k == K_STEPS) ? "Adjust" : "Change";
        } else if (r->type == R_CATEGORY || r->type == R_PRESET_ROW || (r->type == R_ACTION && r->arg == A_KEYS)) {
            keys[*n] = "Enter"; words[(*n)++] = "Open";
        } else if (r->type != R_SETTING) {
            keys[*n] = "Enter"; words[(*n)++] = "Choose";
        }
    }
    keys[*n] = "Esc";
    words[(*n)++] = g_first ? "Modern" : "Back";
}

static S32 LegendWidth(const char **keys, const char **words, int n, S32 scale) {
    S32 w = 0;
    for (int i = 0; i < n; i++)
        w += Ui_ChipWidth(keys[i], scale) + 4 * scale + Ui_SmallWidth(words[i], scale) + (i + 1 < n ? 12 * scale : 0);
    return w;
}

static void DrawLegend(S32 x, S32 yc, const char **keys, const char **words, int n, S32 scale) {
    g_legend[0] = 0;
    for (int i = 0; i < n; i++) {
        S32 chipH = 12 * scale;
        x += Ui_Chip(x, yc - chipH / 2, keys[i], scale) + 4 * scale;
        Ui_Small(x, yc - 4 * scale, words[i], INK_CONFIG_MENU, scale);
        x += Ui_SmallWidth(words[i], scale) + 12 * scale;
        char item[48];
        snprintf(item, sizeof item, "%s[%s] %s", i ? "  " : "", keys[i][0] == '\x11' ? "<>" : keys[i][0] == '\x18' ? "^v" : keys[i], words[i]);
        strncat(g_legend, item, sizeof g_legend - strlen(g_legend) - 1);
    }
}

static void DrawConfirm() {
    // A small panel in the middle: the question is in the voice below, the rows here.
    S32 w = 360, x0 = (S32)Engine_LogicalW() / 2 - w / 2, x1 = x0 + w;
    S32 y0 = Wide() ? 150 : 120;
    const char *const *answers = kQuestions[g_question].answers;
    for (int i = 0; i < 2; i++) {
        S32 ry0 = y0 + i * ROW_PITCH;
        RowFill fill = i == g_confirmCursor ? FILL_CURSOR : FILL_SHADE;
        Ui_RowBackground(x0, ry0, x1, ry0 + ROW_H, fill);
        Menu_RowLabelCentred(x0, x1, ry0 + ROW_H / 2, answers[i], INK_WHITE);
        Menu_Record("confirm", answers[i], "", fill, x0, ry0, x1, ry0 + ROW_H).textLeft =
            (x0 + x1) / 2 - Ui_TextWidth(answers[i]) / 2;
    }
}

S32 Menu_DialogueTop() { return (S32)Engine_LogicalH() - 1 - 137; }

void Menu_DrawDialogue() {
    // MESSAGE.CPP NormalWinDial: 8 in from the sides, 137 tall, 8 off the bottom.
    S32 W = (S32)Engine_LogicalW(), H = (S32)Engine_LogicalH();
    S32 x0 = 8, y0 = H - 1 - 137, x1 = W - 1 - 8, y1 = H - 1 - 8;
    Ui_Panel(x0, y0, x1, y1, PANEL_SHADE_LVL);
    g_voiceLines = Voice_Draw(g_voice, x0 + 16, y0 + 12, x1 - x0 - 32, 3, &g_voiceNeeded);
    g_voiceMax = 3;
    g_helpBox.shown = true;
    g_helpBox.kind = "dialogue";
    g_helpBox.x0 = x0; g_helpBox.y0 = y0; g_helpBox.x1 = x1; g_helpBox.y1 = y1;
}

static void DrawColumn() {
    S32 W = (S32)Engine_LogicalW(), H = (S32)Engine_LogicalH();
    const Page &p = g_first ? g_pages[P_FIRST] : g_pages[TopPage()];
    bool help = g_first || g_confirm || TopPage() == P_PRESET || sw_help640;

    if (g_first) {
        Box(0, 0, W - 1, H - 1, 0);
        const char *title = "Choose how to play";
        S32 tx = W / 2 - Ui_TextWidth(title) / 2;
        ColorFont(INK_BLACK);
        Font(tx + 4, 64 + 4, title);  // the title shadow, +4,+4
        Ui_Text(tx, 64, title, INK_WHITE);
        Menu_Record("title", title, "", FILL_SHADE, tx, 64, tx + Ui_TextWidth(title), 64 + 38).textLeft = tx;
    }

    if (g_confirm) {
        DrawConfirm();
    } else {
        S32 top = 16, bottom = help ? H - 1 - 137 - 16 : H - 16;
        if (g_first)
            top = 168 - 8;
        int visible = (bottom - top + ROW_GAP) / ROW_PITCH;
        if (visible > p.count)
            visible = p.count;
        if (g_cursor < g_scroll)
            g_scroll = g_cursor;
        if (g_cursor >= g_scroll + visible)
            g_scroll = g_cursor - visible + 1;
        S32 stackH = visible * ROW_PITCH - ROW_GAP;
        // GAMEMENU.CPP: tables are centred on y 260 (DrawGameMenu takes it as the
        // first row's centre after subtracting half the stack), so the stack sits
        // 5 px above the true middle; taller canvases float it (UI_VCENTER_OFS).
        S32 y = 260 + (H - 480) / 2 - stackH / 2 - ROW_H / 2;
        if (help || y < top)
            y = top + (bottom - top - stackH) / 2;
        if (g_first)
            y = 168;
        // The 1997 width: LargeurMenu 550, centred.
        S32 x0 = W / 2 - 275, x1 = W / 2 + 275;
        for (int i = g_scroll; i < g_scroll + visible; i++) {
            DrawColumnRow(p.rows[i], x0, y, x1, y + ROW_H, i == g_cursor);
            y += ROW_PITCH;
        }
        if (visible < p.count)
            Menu_DrawScrollBar(x1 + 9, y - visible * ROW_PITCH, y - visible * ROW_PITCH + stackH - 1, g_scroll, visible,
                          p.count);
    }
    if (help)
        Menu_DrawDialogue();
}

static void DrawWide() {
    S32 W = (S32)Engine_LogicalW(), H = (S32)Engine_LogicalH();
    const S32 side = 32, top = 24, gap = 16;
    S32 inner = W - 2 * side - gap;
    S32 leftW = inner * 464 / 880;
    S32 lx0 = side, lx1 = side + leftW - 1;
    S32 rx0 = lx1 + 1 + gap, rx1 = W - side - 1;

    // Left pane: the hub's categories.
    const Page &hub = g_pages[P_HUB];
    S32 y = top;
    for (int i = 0; i < LeftCount(); i++) {
        RowFill fill = FILL_SHADE;
        if (i == g_leftCursor)
            fill = g_focus == F_LEFT ? FILL_CURSOR : FILL_CURRENT;
        DrawWideRow("left", hub.rows[LeftRowToHub(i)], lx0, y, lx1, y + ROW_H, fill);
        y += ROW_PITCH;
    }

    // Right pane: the page of the category under the left cursor.
    int rp = RightPage();
    const Page &page = g_pages[rp];
    int n = page.count - RightFirst();
    S32 limit = Menu_WideRowsLimit();
    int visible = (limit - top + ROW_GAP) / ROW_PITCH;
    if (visible > n)
        visible = n;
    if (g_focus == F_RIGHT) {
        if (g_cursor < g_scroll)
            g_scroll = g_cursor;
        if (g_cursor >= g_scroll + visible)
            g_scroll = g_cursor - visible + 1;
    } else {
        g_scroll = 0;
    }
    y = top;
    for (int i = g_scroll; i < g_scroll + visible; i++) {
        RowFill fill = FILL_SHADE;
        if (g_focus == F_RIGHT && i == g_cursor)
            fill = g_confirm ? FILL_CURRENT : FILL_CURSOR;  // the question has the cursor
        DrawWideRow("right", page.rows[RightFirst() + i], rx0, y, rx1, y + ROW_H, fill);
        y += ROW_PITCH;
    }
    if (visible < n)
        Menu_DrawScrollBar(rx1 + 9, top, top + visible * ROW_PITCH - ROW_GAP - 1, g_scroll, visible, n);

    if (g_confirm)
        DrawConfirm();

    // Help and legend.
    const char *keys[4], *words[4];
    int nk;
    LegendFor(CursorRow(), keys, words, &nk);
    Menu_DrawFoot(keys, words, nk);
}

S32 Menu_WideRowsLimit() {
    S32 H = (S32)Engine_LogicalH();
    return sw_band ? H - 24 - 86 - 16 : H - 168 - 8;
}

void Menu_DrawFoot(const char **keys, const char **words, int nk) {
    S32 W = (S32)Engine_LogicalW(), H = (S32)Engine_LogicalH();
    const S32 side = 32;
    S32 chip = sw_chip;
    S32 bandY0 = H - 24 - 86, bandY1 = H - 25;
    S32 helpLineY = H - 168, promptY = H - 68;
    S32 legendW = LegendWidth(keys, words, nk, chip);
    if (sw_band) {
        Ui_Panel(side, bandY0, W - side - 1, bandY1, BAND_SHADE_LVL);
        S32 voiceW = (W - 2 * side) - legendW - 48;
        if (voiceW > 620)
            voiceW = 620;
        g_voiceLines = Voice_Draw(g_voice, side + 16, bandY0 + 5, voiceW, 2, &g_voiceNeeded);
        g_voiceMax = 2;
        g_voiceWidth = voiceW;
        DrawLegend(W - side - 16 - legendW, (bandY0 + bandY1) / 2, keys, words, nk, chip);
        g_helpBox.shown = true;
        g_helpBox.kind = "band";
        g_helpBox.x0 = side; g_helpBox.y0 = bandY0; g_helpBox.x1 = W - side - 1; g_helpBox.y1 = bandY1;
    } else {
        // The proposal: a help line, and a prompt bar at the foot.
        g_voiceLines = Voice_Draw(g_voice, side, helpLineY, W - 2 * side, 1, &g_voiceNeeded);
        g_voiceMax = 1;
        g_voiceWidth = W - 2 * side;
        DrawLegend(side, promptY + 6 * chip, keys, words, nk, chip);
        g_helpBox.shown = true;
        g_helpBox.kind = "help-line";
        g_helpBox.x0 = side; g_helpBox.y0 = helpLineY; g_helpBox.x1 = W - side - 1; g_helpBox.y1 = helpLineY + 38;
    }
}

static void DrawOverlay() {
    static const char *lines[] = {
        "Prototype keys",
        "F1 this list      F2 next screen size",
        "F3 band / help line + prompt bar",
        "F4 help typing: every move / page entry / none",
        "F5 key font 1x / 2x    F6 preset word",
        "F7 island palette      F8 help on 640 pages",
        "F9 Gameplay category",
        "F10 Controls: one device per view / all columns",
        "F11 pad names: connected / Xbox / PlayStation / Nintendo",
    };
    S32 n = (S32)(sizeof lines / sizeof lines[0]);
    S32 w = 8 * 50 + 16, h = n * 12 + 12;
    S32 x0 = (S32)Engine_LogicalW() - w - 8, y0 = 8;
    Ui_Panel(x0, y0, x0 + w, y0 + h, PANEL_SHADE_LVL);
    for (S32 i = 0; i < n; i++)
        Ui_Small(x0 + 8, y0 + 8 + i * 12, lines[i], i ? INK_CONFIG_MENU : INK_CONFIG_HEAD, 1);
}

void Menu_Draw() {
    g_drawnCount = 0;
    g_helpBox.shown = false;
    g_voiceLines = g_voiceNeeded = g_voiceMax = g_voiceWidth = 0;
    g_legend[0] = 0;

    Engine_DrawBackground();
    Cadre_AnimatePlasma();

    if (g_closed) {
        const char *s = "Options closed. Enter opens them again.";
        Ui_TextShadow((S32)Engine_LogicalW() / 2 - Ui_TextWidth(s) / 2, (S32)Engine_LogicalH() / 2 - 19, s,
                      DIALOGUE_HERO * 16 + 12);
    } else if (Controls_Active()) {
        Controls_Draw();
    } else if (Wide() && !g_first) {
        DrawWide();
    } else {
        DrawColumn();
    }

    if (sw_overlay)
        DrawOverlay();
    if (g_toast[0] && Engine_TimeMs() < g_toastUntil) {
        S32 w = Ui_SmallWidth(g_toast, 1);
        S32 x = (S32)Engine_LogicalW() - w - 12, yy = (S32)Engine_LogicalH() - 14;
        Box(x - 4, yy - 3, x + w + 3, yy + 10, 0);
        Ui_Small(x, yy, g_toast, INK_CONFIG_HEAD, 1);
    }
}

// ───────────────────────────── Switches and state ─────────────────────────────

struct SwitchDef {
    const char *name;
    int *value;
    int min, max;
    int start;  // the value a fresh start has
};
static const SwitchDef kSwitches[] = {
    {"band", &sw_band, 0, 1, 1},           {"typing", &sw_typing, 0, 2, 0},   {"chip", &sw_chip, 1, 2, 2},
    {"word", &sw_word, 0, 2, 0},           {"island", &sw_island, 0, 1, 0},   {"help640", &sw_help640, 0, 1, 0},
    {"gameplay", &sw_gameplay, 0, 1, 0},   {"overlay", &sw_overlay, 0, 1, 0},
    {"devices", &sw_devices, 0, 1, 1},     {"padnames", &sw_padnames, 0, 3, 0},
};

bool Menu_SetSwitch(const char *name, int v) {
    for (size_t i = 0; i < sizeof kSwitches / sizeof kSwitches[0]; i++) {
        const SwitchDef &s = kSwitches[i];
        if (strcmp(s.name, name) != 0)
            continue;
        if (v < s.min || v > s.max)
            v = s.min + ((v - s.min) % (s.max - s.min + 1) + (s.max - s.min + 1)) % (s.max - s.min + 1);
        *s.value = v;
        if (!strcmp(name, "island")) {
            if (v)
                Engine_UseIslandPalette(0);
            else
                Engine_UseMenuPalette();
        }
        BuildPages();
        g_voice.text[0] = 0;  // say it again under the new rules
        g_pageEntered = true;
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
    if (!strcmp(name, "Keys and buttons")) {
        Menu_Open("Controls and camera");
        Controls_Open();
        return true;
    }
    Controls_Close();
    for (int p = 0; p < P_COUNT; p++) {
        if (g_pages[p].name && !strcmp(g_pages[p].name, name)) {
            g_first = false;
            g_confirm = false;
            g_closed = false;
            g_depth = 1;
            g_focus = F_LEFT;
            g_leftCursor = 0;  // the same place whatever was open before
            if (Wide() && p != P_HUB) {
                // Put the left cursor on the page's category; focus the right pane.
                bool found = false;
                for (int i = 0; i < LeftCount(); i++) {
                    if (g_pages[P_HUB].rows[LeftRowToHub(i)].arg == p) {
                        g_leftCursor = i;
                        found = true;
                    }
                }
                if (!found)
                    Push(p);
                g_focus = F_RIGHT;
            } else if (p != P_HUB) {
                Push(p);
            }
            g_cursor = 0;
            g_scroll = 0;
            Trace("open page=%s (control)", name);
            return true;
        }
    }
    return false;
}

bool Menu_Set(const char *id, int value) {
    for (int s = 0; s < S_COUNT; s++) {
        if (strcmp(kSettings[s].id, id) != 0)
            continue;
        if (value < 0 || value > kSettings[s].max)
            return false;
        g_val[s] = value;
        BuildPages();
        Trace("set %s=%d (control)", id, value);
        return true;
    }
    return false;
}

void Menu_Reset() {
    for (size_t i = 0; i < sizeof kSwitches / sizeof kSwitches[0]; i++)
        *kSwitches[i].value = kSwitches[i].start;
    Engine_UseMenuPalette();
    Engine_ResetInput();
    Controls_Reset();
    g_confirm = false;
    g_confirmCursor = 0;
    g_closed = false;
    g_haveBefore = false;
    g_haveBeforeReset = false;
    g_scroll = 0;
    g_toast[0] = 0;
    g_toastUntil = 0;
    memset(g_cursorStack, 0, sizeof g_cursorStack);
    Menu_Init(false);
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
    h->max = g_voiceMax;
    h->width = g_voiceWidth;
    h->text = g_voice.text;
}

const char *Menu_Layout() { return g_closed ? "closed" : g_first ? "first-launch" : Wide() ? "wide" : "column"; }

const char *Menu_PageName() {
    return Controls_Active() ? "Keys and buttons" : g_first ? "First launch" : g_pages[Wide() ? RightPage() : TopPage()].name;
}

// One for the hub; each page pushed, the wide right pane and a question add one.
static int Depth() {
    if (g_first)
        return 1;
    return g_depth + (Wide() && g_focus == F_RIGHT ? 1 : 0) + (g_confirm ? 1 : 0);
}

const char *Menu_CursorLabel() {
    static char buf[160];
    const Row *r = Controls_Active() || g_closed ? NULL : CursorRow();
    if (!r)
        return "";
    return r->type == R_PRESET_ROW ? PresetRowLabel(buf, sizeof buf) : r->label;
}

int Menu_Where(char *buf, size_t n) {
    if (g_closed) {
        snprintf(buf, n, "closed");
        return 0;
    }
    if (Controls_Active()) {
        char inner[200];
        int d = Controls_Where(inner, sizeof inner);
        snprintf(buf, n, "%s | %s", Menu_PageName(), inner);
        return Depth() + d;
    }
    if (g_confirm)
        snprintf(buf, n, "%s | %s | confirm", Menu_PageName(), g_confirmCursor ? "Yes" : "No");
    else
        snprintf(buf, n, "%s | %s", Menu_PageName(), Menu_CursorLabel());
    return Depth();
}

bool Menu_CursorSetting(bool *active) {
    const Row *r = Controls_Active() || g_closed ? NULL : CursorRow();
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

void Menu_WriteState(FILE *f) {
    char buf[160];
    fprintf(f, "{\"t\":%u,\"physical\":[%u,%u],\"logical\":[%u,%u],\"scale\":%u,\"layout\":",
            (unsigned)Engine_TimeMs(), (unsigned)Engine_PhysW(), (unsigned)Engine_PhysH(),
            (unsigned)Engine_LogicalW(), (unsigned)Engine_LogicalH(),
            (unsigned)Engine_Scale(Engine_PhysW(), Engine_PhysH()));
    Menu_JsonString(f, Menu_Layout());
    fprintf(f, ",\"page\":");
    Menu_JsonString(f, Menu_PageName());
    fprintf(f, ",\"focus\":\"%s\",\"cursor\":%d,\"left_cursor\":%d,\"scroll\":%d,\"confirm\":%s",
            Wide() ? (g_focus == F_LEFT ? "left" : "right") : "column", g_cursor, g_leftCursor, g_scroll,
            g_confirm ? "true" : "false");
    const Row *r = Controls_Active() ? NULL : CursorRow();
    fprintf(f, ",\"cursor_row\":");
    Menu_JsonString(f, r ? (r->type == R_PRESET_ROW ? PresetRowLabel(buf, sizeof buf) : r->label) : "");
    fprintf(f, ",\"preset\":\"%s\",\"preset_changed\":%s,\"preset_row\":", kPresetNames[g_preset],
            PresetChanged() ? "true" : "false");
    Menu_JsonString(f, PresetRowLabel(buf, sizeof buf));

    fprintf(f, ",\"rows\":[");
    for (int i = 0; i < g_drawnCount; i++) {
        const DrawnRow &d = g_drawn[i];
        fprintf(f, "%s{\"pane\":\"%s\",\"label\":", i ? "," : "", d.pane);
        Menu_JsonString(f, d.label);
        fprintf(f, ",\"value\":");
        Menu_JsonString(f, d.value);
        fprintf(f, ",\"fill\":\"%s\",\"rect\":[%d,%d,%d,%d],\"dim\":%s,\"changed\":%s,\"marks\":%s,\"setting\":%s,\"text_left\":%d,\"text_right\":%d,\"value_right\":%d}",
                d.fill, d.x0, d.y0, d.x1, d.y1, d.dim ? "true" : "false", d.changed ? "true" : "false",
                d.marks ? "true" : "false", d.setting ? "true" : "false", d.textLeft, d.textRight, d.valueRight);
    }
    fprintf(f, "]");

    int total = (int)strlen(g_voice.text);
    int shown = g_voice.typed ? (int)((Engine_TimeMs() - g_voice.start) / 20) : total;
    if (shown > total)
        shown = total;
    fprintf(f, ",\"voice\":{\"text\":");
    Menu_JsonString(f, g_voice.text);
    fprintf(f, ",\"typed\":%s,\"letters_shown\":%d,\"letters\":%d,\"where\":", g_voice.typed ? "true" : "false",
            shown, total);
    Menu_JsonString(f, g_helpBox.shown ? g_helpBox.kind : "none");
    fprintf(f, ",\"lines\":%d,\"lines_needed\":%d,\"max_lines\":%d,\"width\":%d,\"cut_off\":%s", g_voiceLines,
            g_voiceNeeded, g_voiceMax, g_voiceWidth, g_voiceNeeded > g_voiceMax && g_voiceMax ? "true" : "false");
    fprintf(f, ",\"rect\":[%d,%d,%d,%d]}", g_helpBox.x0, g_helpBox.y0, g_helpBox.x1, g_helpBox.y1);
    fprintf(f, ",\"legend\":");
    Menu_JsonString(f, g_legend);

    if (Controls_Active()) {
        fprintf(f, ",\"controls\":");
        Controls_WriteState(f);
    }
    fprintf(f, ",\"switches\":{");
    for (size_t i = 0; i < sizeof kSwitches / sizeof kSwitches[0]; i++)
        fprintf(f, "%s\"%s\":%d", i ? "," : "", kSwitches[i].name, *kSwitches[i].value);
    fprintf(f, "},\"settings\":{");
    for (int s = 0; s < S_COUNT; s++)
        fprintf(f, "%s\"%s\":%d", s ? "," : "", kSettings[s].id, g_val[s]);
    fprintf(f, "}}\n");
    fflush(f);
}
