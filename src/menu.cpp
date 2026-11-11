#include "menu.h"

#include "cadre.h"
#include "controls.h"
#include "engine.h"
#include "experimental.h"
#include "trace.h"
#include "menu_parts.h"
#include "lang.h"
#include "ui.h"
#include "twinsun.h"
#include "vfont.h"

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
    const char *names[10];   // value names for choices, toggles and steps
    int fresh;               // what a fresh install has (= Modern)
    int classic;             // what Classic sets, when presetSets
    bool presetSets;
    const char *help;
};

enum {
    S_TEXT_LANG, S_VOICE_LANG, S_SUBTITLES, S_BACKGROUND, S_FONT,
    S_RESOLUTION, S_FULLSCREEN, S_VSYNC, S_FPS, S_IMAGE,
    S_MASTER, S_SFX, S_VOICES, S_MUSIC,
    S_CAM_OUT, S_CAM_IN, S_MOVIE_CAM, S_INVERT, S_SENS,
    S_FIXBUGS,
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
    // What the menu is drawn over (asked for 2026-11-11). Blue is menu palette 181, the
    // picture's second most common colour (picked over 196); rows and the band darken it
    // as they darken the picture. Twinsun is the holomap's planet turning once in three minutes on
    // stars (src/twinsun.cpp; asked for 2026-11-11). Off is black.
    {"background", "Background", K_CHOICE, 3, {"Picture", "Blue", "Twinsun", "Off"}, 0, 0, false,
     "The picture behind the menu."},
    // The menu's letters (asked for 2026-11-11): the game font, or Zilla Slab Bold drawn at
    // the screen's resolution (src/vfont.cpp), the face lba2cc's parked HD UI overlay chose.
    // Bold over Medium and the size, decided after play (2026-11-11).
    {"font", "Font", K_CHOICE, 1, {"Game", "Zilla Slab"}, 0, 0, false,
     "The game's letters, or sharp Zilla Slab."},
    // Not a stored value: the row shows the screen in use and steps it through
    // Engine_SizeAt's list, as F2 does. g_val keeps its fresh value; the screen is the
    // engine's, which presets (prototype only, kLook), Reset settings and their undo set.
    {"resolution", "Resolution", K_CHOICE, 9,
     {"640 x 480", "768 x 480", "1024 x 480", "1280 x 720", "1280 x 800", "1920 x 1080", "2400 x 1080", "2560 x 1440", "3440 x 1440", "3840 x 2160"},
     5, 5, false,
     "The size of the picture the game draws."},
    // Not a stored value either: the row shows the window as it is and turns it.
    {"fullscreen", "Fullscreen", K_TOGGLE, 1, {"Off", "On"}, 1, 1, false,
     "Fills the screen, or plays in a window."},
    {"vsync", "Vsync", K_TOGGLE, 1, {"Off", "On"}, 1, 1, false,
     "Stops the picture tearing. Off can raise the frame rate on a slow display."},
    // Not kept in lba2.cfg: off at every start, as the console's fps and the SPEED code have it.
    {"fps", "Frame rate", K_TOGGLE, 1, {"Off", "On"}, 0, 0, false,
     "Shows the frame rate while you play."},
    // Detail level and Videos (the films' size) were here: settings for 1997's machines, they
    // are on More options now, in its From 1997 group (decided 2026-11-11).
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
    // AllCameras, the 1997 "scenario cameras" (lba2cc's Movie camera, on Advanced options there):
    // off skips the outdoor shots that cut once as the hero walks in; held shots and interiors
    // stay. Camera behaviour, not a machine's limit, so it sits with the cameras (2026-11-11).
    {"movie_cam", "Movie camera", K_TOGGLE, 1, {"Off", "On"}, 1, 1, false,
     "Cuts to the designers' shots outdoors."},
    {"invert", "Invert camera", K_TOGGLE, 1, {"Off", "On"}, 0, 0, false,
     "Turns the camera the other way. Only with the Auto camera."},
    {"sensitivity", "Camera speed", K_SLIDER, 10, {0}, 5, 5, false,
     "How fast the camera turns. Only with the Auto camera."},
    {"fix_bugs", "Fix retail bugs", K_TOGGLE, 1, {"Off", "On"}, 1, 0, true,
     "Fixes the 1997 release's known bugs. Off plays them as they were."},
};

static int g_val[S_COUNT];
static int g_preset;  // 0 Modern, 1 Classic

static const char *kPresetNames[2] = {"Modern", "Classic"};

// The languages: Reset settings leaves them as they are (the menu plan), so a player who
// reads French still reads French after a reset.
static bool IsLanguage(int s) { return s == S_TEXT_LANG || s == S_VOICE_LANG; }

// A preset also sets the look (asked for 2026-11-12, prototype only), so choosing one shows
// the difference. Classic: the 1997 picture, the game font, Classic cameras, 640 x 480.
// Modern: Twinsun, Zilla Slab, Auto and Follow cameras, the screen the prototype started at
// (the display's own size on a console). The plan has presets set the rules only; a real
// port may leave the screen alone.

struct LookDef {
    int s;
    int modern, classic;
};
static const LookDef kLook[] = {
    {S_BACKGROUND, 2, 0},  // Twinsun / Picture
    {S_FONT, 1, 0},        // Zilla Slab / Game
    {S_CAM_OUT, 1, 0},     // Auto / Classic
    {S_CAM_IN, 1, 0},      // Follow / Classic
};
static const int kLookCount = (int)(sizeof kLook / sizeof kLook[0]);

static const LookDef *Look(int s) {
    for (int i = 0; i < kLookCount; i++)
        if (kLook[i].s == s)
            return &kLook[i];
    return NULL;
}

static bool PresetSets(int s) { return kSettings[s].presetSets || Look(s); }

// What a fresh install has (= Modern).
static int FreshValue(int s) {
    const LookDef *l = Look(s);
    return l ? l->modern : kSettings[s].fresh;
}

static int PresetValue(int preset, int s) {
    if (!PresetSets(s))
        return FreshValue(s);
    if (preset != 1)
        return FreshValue(s);
    const LookDef *l = Look(s);
    return l ? l->classic : kSettings[s].classic;
}

// The screen: Classic's is 640 x 480, Modern's the one the prototype
// started at.
struct ScreenSize {
    U32 w, h;
};
static ScreenSize g_modernScreen = {1920, 1080};

static ScreenSize ScreenNow() { return ScreenSize{Engine_PhysW(), Engine_PhysH()}; }
static bool SameScreen(ScreenSize a, ScreenSize b) { return a.w == b.w && a.h == b.h; }
static ScreenSize PresetScreen(int preset) { return preset == 1 ? ScreenSize{640, 480} : g_modernScreen; }

static void SyncLayout();

// A preset, Reset settings or an undo can cross between the one column and the wide layout
// in the middle of a frame: the place is made over for the new layout at once, so what the
// same frame does next (an undo row going, the cursor kept in range) is done in it. Left to
// the next draw, the wide page drew a row past its end for a frame after Undo preset.
static void SetScreenTo(ScreenSize to) {
    if (SameScreen(to, ScreenNow()))
        return;
    if (Engine_SetScreen(to.w, to.h, true))
        Menu_NoteScreen();
    SyncLayout();
}

// "Changed": the player's values differ from the chosen preset in a setting presets set,
// or a tuning value differs from its default (a preset puts every one back). The player's
// settings, on Options or on More options, and the screen do not mark the preset changed.
static bool PresetChanged() {
    for (int s = 0; s < S_COUNT; s++)
        if (PresetSets(s) && g_val[s] != PresetValue(g_preset, s))
            return true;
    return Exp_PresetChanged();
}

// A settings backup, taken before a preset or Reset settings changes anything, which its
// undo row puts back. What the undo puts back and what its help names both come from the
// Covers functions, so the help names exactly what the undo changes (2026-11-11: the help
// named the five settings presets set while the undo put back every setting).
//   A preset's backup covers what presets set: their settings, the tuning values, the
//   preset and (prototype only) the screen. A player's setting changed since stays, as the
//   menu plan's Undo preset has it.
//   Reset's covers what Reset changes: every setting but the languages, every More options
//   row, the bindings, the preset and the screen.
struct Backup {
    bool have;
    bool reset;
    int val[S_COUNT];
    int exp[EXP_MAX];
    int binds[CONTROLS_BIND_MAX];
    int preset;
    ScreenSize screen;
};
static Backup g_presetBackup;  // the preset page's Undo preset
static Backup g_resetBackup;   // the Advanced page's Undo reset

static bool CoversSetting(const Backup &b, int s) { return b.reset ? !IsLanguage(s) : PresetSets(s); }
static bool CoversExp(const Backup &b, int i) { return b.reset || Exp_PresetsSet(i); }

static void TakeBackup(Backup &b, bool reset) {
    b.have = true;
    b.reset = reset;
    memcpy(b.val, g_val, sizeof g_val);
    Exp_Get(b.exp);
    Controls_GetBindings(b.binds);
    b.preset = g_preset;
    b.screen = ScreenNow();
}

static const char *const kPresetBackupNames[2] = {"Modern preset", "Classic preset"};

// What the undo would change, in the order its help names it: the preset, the settings
// (Resolution for the screen), the bindings, the More options rows. Settings in the Text
// language, More options' rows by their console names. None: the undo row is not offered.
enum { BACKUP_DIFF_MAX = 1 + S_COUNT + 1 + EXP_MAX };
static int BackupDiffers(const Backup &b, const char **names) {
    if (!b.have)
        return 0;
    int n = 0;
    if (b.preset != g_preset)
        names[n++] = Ui_Tr(kPresetBackupNames[b.preset]);
    for (int s = 0; s < S_COUNT; s++) {
        const bool differs = s == S_RESOLUTION ? !SameScreen(b.screen, ScreenNow())
                                               : CoversSetting(b, s) && b.val[s] != g_val[s];
        if (differs)
            names[n++] = Ui_Tr(kSettings[s].label);
    }
    if (b.reset) {
        int now[CONTROLS_BIND_MAX];
        Controls_GetBindings(now);
        if (memcmp(b.binds, now, sizeof(int) * Controls_BindingCount()) != 0)
            names[n++] = Ui_Tr("Keys and buttons");
    }
    int now[EXP_MAX];
    Exp_Get(now);
    for (int i = 0; i < Exp_Count(); i++)
        if (CoversExp(b, i) && b.exp[i] != now[i])
            names[n++] = Exp_Name(i);
    return n;
}

static bool UndoOffered(const Backup &b) {
    const char *names[BACKUP_DIFF_MAX];
    return BackupDiffers(b, names) > 0;
}

static void RestoreBackup(Backup &b) {
    for (int s = 0; s < S_COUNT; s++)
        if (CoversSetting(b, s))
            g_val[s] = b.val[s];
    int exp[EXP_MAX];
    Exp_Get(exp);
    for (int i = 0; i < Exp_Count(); i++)
        if (CoversExp(b, i))
            exp[i] = b.exp[i];
    Exp_Put(exp);
    if (b.reset)
        Controls_PutBindings(b.binds);
    g_preset = b.preset;
    b.have = false;
    SetScreenTo(b.screen);
}

static void ApplyPreset(int p) {
    if (!g_presetBackup.have)
        TakeBackup(g_presetBackup, false);
    g_preset = p;
    for (int s = 0; s < S_COUNT; s++)
        if (PresetSets(s))
            g_val[s] = PresetValue(p, s);
    Exp_PresetDefaults();  // the tuning values; the player's rows on More options stay
    SetScreenTo(PresetScreen(p));
    Trace("apply preset=%s", kPresetNames[p]);
}

// Every setting but the languages back as a fresh install has it, every More options row,
// the bindings and the preset (the menu plan's Reset settings).
static void ResetSettings() {
    bool fresh = g_preset == 0 && !Exp_AnyChanged() && Controls_BindingsAreDefault();
    for (int s = 0; s < S_COUNT; s++)
        fresh = fresh && (IsLanguage(s) || g_val[s] == FreshValue(s));
    fresh = fresh && SameScreen(ScreenNow(), g_modernScreen);
    if (!fresh)  // a second reset with nothing changed keeps the first one's undo
        TakeBackup(g_resetBackup, true);
    for (int s = 0; s < S_COUNT; s++)
        if (!IsLanguage(s))
            g_val[s] = FreshValue(s);
    Exp_Defaults();
    Controls_DefaultBindings();
    g_preset = 0;
    g_presetBackup.have = false;
    SetScreenTo(g_modernScreen);
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

enum { A_KEYS, A_RESET, A_EXP, A_CONSOLE };  // R_ACTION: opens Keys and buttons, asks to reset, opens Experimental
                                              // settings, opens the console
enum { BACK_PRESET, BACK_RESET };  // R_BACK_TO_MINE: undoes a preset, or a reset

enum { P_HUB, P_PRESET, P_LANG, P_DISPLAY, P_SOUND, P_CONTROLS, P_ADVANCED, P_FIRST, P_ORIGINAL, P_COUNT };

struct Page {
    const char *name;
    Row rows[12];
    int count;
};

static Page g_pages[P_COUNT];

// Switches (prototype aids; every review switch was pruned 2026-11-12, keeping what was decided).
static int sw_overlay = 0;
// Help typed on every cursor move (decided 2026-11-03); 1 shows it whole at once, for the self test.
static int sw_whole = 0;

// What a build off main says: lba2cc's LBA2_VERSION_STRING and the commit's first 7. A release
// says its number alone. An example; the prototype has no build of its own to name.
static const char *kVersion = "0.13.3-dev 011394f";
static const char *kVersionString = "0.13.3-dev";  // LBA2_VERSION_STRING alone, as the log's first line has it
static const char *kCommit = "011394fbc247";

// The preset row: "Preset: Modern", the page's own name (2026-11-11, after playing on the
// Xbox: "Modern settings" read as a page of settings, not the preset).
static const char *FormatPresetRow(char *buf, size_t n, const char *name, bool changed) {
    snprintf(buf, n, "Preset: %s%s", name, changed ? ", changed" : "");
    return buf;
}

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
    AddRow(hub, R_PRESET_ROW, "", P_PRESET, "As in 1997, or today's look with bugs fixed.");
    AddRow(hub, R_CATEGORY, "Language and interface", P_LANG, "Text, voices, the font and the background.");
    AddRow(hub, R_CATEGORY, "Display and graphics", P_DISPLAY, "Resolution, window and detail.");
    AddRow(hub, R_CATEGORY, "Sound", P_SOUND, "Volumes.");
    AddRow(hub, R_CATEGORY, "Controls and camera", P_CONTROLS, "Keys, pad and the camera.");
    AddRow(hub, R_CATEGORY, "Advanced and reset", P_ADVANCED, "The rules switch, experiments and a fresh start.");

    Page &pr = g_pages[P_PRESET];
    pr.name = "Preset";
    AddRow(pr, R_RETURN, "Return to previous menu", 0, "");
    AddRow(pr, R_PRESET_CHOICE, "Modern", 0, "Today's look, cameras that follow, bugs fixed.");
    AddRow(pr, R_PRESET_CHOICE, "Classic", 1, "The 1997 look and cameras, bugs and all.");
    if (UndoOffered(g_presetBackup))
        AddRow(pr, R_BACK_TO_MINE, "Undo preset", BACK_PRESET, "Puts back the settings you had before you chose a preset.");

    Page &lang = g_pages[P_LANG];
    lang.name = "Language and interface";
    AddRow(lang, R_RETURN, "Return to previous menu", 0, "");
    AddSetting(lang, S_TEXT_LANG);
    AddSetting(lang, S_VOICE_LANG);
    AddSetting(lang, S_SUBTITLES);
    AddSetting(lang, S_BACKGROUND);
    AddSetting(lang, S_FONT);

    Page &disp = g_pages[P_DISPLAY];
    disp.name = "Display and graphics";
    AddRow(disp, R_RETURN, "Return to previous menu", 0, "");
    AddSetting(disp, S_RESOLUTION);
    AddSetting(disp, S_FULLSCREEN);
    AddSetting(disp, S_VSYNC);
    AddSetting(disp, S_FPS);
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
    AddSetting(ctl, S_MOVIE_CAM);
    AddSetting(ctl, S_INVERT);
    AddSetting(ctl, S_SENS);

    Page &adv = g_pages[P_ADVANCED];
    adv.name = "Advanced and reset";
    AddRow(adv, R_RETURN, "Return to previous menu", 0, "");
    AddSetting(adv, S_FIXBUGS);
    // Always offered (decided 2026-11-03): its help carries the warning a switch once did.
    // The page holds every console setting without a row of its own: tuning for finished
    // features and the few unfinished ones, which it marks experimental.
    // Help within 750 px (2026-11-12); an experimental row's own help says it is experimental.
    AddRow(adv, R_ACTION, "More options", A_EXP, "Tuning and 1997 extras, in English.");
    // The row's value is the version, so a bug report can name the build from the menu.
    AddRow(adv, R_ACTION, "Console", A_CONSOLE, "Messages, and commands from a keyboard.");
    AddRow(adv, R_ACTION, "Reset settings", A_RESET, "Puts every setting back as a fresh install has it.");
    if (UndoOffered(g_resetBackup))
        AddRow(adv, R_BACK_TO_MINE, "Undo reset", BACK_RESET, "Puts back what you had before the reset.");

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
    AddRow(fl, R_PRESET_CHOICE, "Modern", 0, "Today's look, bugs fixed. You can change this later in the Options Menu.");
    AddRow(fl, R_PRESET_CHOICE, "Classic", 1, "As in 1997, bugs and all. You can change this later in the Options Menu.");
}

// Invert and speed only mean something with the Auto camera: dimmed, not hidden.
static bool RowActive(const Row &r) {
    if (r.type == R_SETTING && (r.arg == S_INVERT || r.arg == S_SENS))
        return g_val[S_CAM_OUT] == 1;
    return true;
}

static const char *PresetRowLabel(char *buf, size_t n) {
    return FormatPresetRow(buf, n, kPresetNames[g_preset], PresetChanged());
}

static const char *ValueText(int s, char *buf, size_t n) {
    const Setting &st = kSettings[s];
    if (s == S_RESOLUTION)
        snprintf(buf, n, "%u x %u", (unsigned)Engine_PhysW(), (unsigned)Engine_PhysH());
    else if (s == S_FULLSCREEN)
        snprintf(buf, n, "%s", st.names[Engine_Fullscreen() ? 1 : 0]);
    else if (st.kind == K_SLIDER)
        snprintf(buf, n, "%d", g_val[s]);
    else
        snprintf(buf, n, "%s", st.names[g_val[s]]);
    return buf;
}

// "Changed is a colour": differs from a fresh install, or for a setting presets
// set, from the chosen preset.
static bool ValueChanged(int s) {
    if (s == S_RESOLUTION || s == S_FULLSCREEN)
        return false;  // the player's screen, not a choice a preset makes
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
    {"Play as in 1997? Sound and keys stay as they are.", {"No, keep Modern", "Yes, play Classic"}},
    {"Put every setting back as a fresh install has it?", {"No, keep my settings", "Yes, reset settings"}},
};

static void Ask(Question q) {
    g_confirm = true;
    g_question = q;
    g_confirmCursor = 0;  // the safe answer first
    Trace("ask %s", q == Q_RESET ? "reset" : "classic");
}
static bool g_closed;      // left the Options (shown as a note)
static bool g_quit;        // Esc on that note: the prototype ends (a fullscreen window has no close box)
// The console (#679), opened from its row: by the device used last, the full console
// (keyboard, touch) or read-only (pad: the messages, and any button closes it).
enum { CON_CLOSED, CON_FULL, CON_READONLY };
static int g_console;
static Voice g_voice;
static char g_toast[128];
static U32 g_toastUntil;

bool Menu_Wide() {
    return Engine_RuleChoseWide();
}

WideFrame Menu_WideFrame() {
    S32 W = (S32)Engine_LogicalW(), H = (S32)Engine_LogicalH();
    WideFrame f = {(W - 960) / 2, (H - 540) / 2, 960, 540};
    return f;
}

static bool Wide() { return Menu_Wide(); }

static void Toast(const char *fmt, const char *a, int v) {
    snprintf(g_toast, sizeof g_toast, fmt, a, v);
    g_toastUntil = Engine_TimeMs() + 2000;
}

void Menu_Toast(const char *text) { Toast("%s", text, 0); }

void Menu_NoteScreen() {
    char note[64];
    snprintf(note, sizeof note, "%ux%u (logical %ux%u)%s", (unsigned)Engine_PhysW(), (unsigned)Engine_PhysH(),
             (unsigned)Engine_LogicalW(), (unsigned)Engine_LogicalH(), Engine_Fullscreen() ? ", fullscreen" : "");
    Toast("%s", note, 0);
    Trace("screen %s", note);
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
static S32 BandVoiceWidth(const char **keys, const char **words, int nk);

static bool VoiceFits(const char *text, const Row *r) {
    S32 W = (S32)Engine_LogicalW();
    if (!Wide())
        return Voice_Lines(text, W - 49) <= 3;
    const char *keys[4], *words[4];
    int nk;
    LegendFor(r, keys, words, &nk);
    return Voice_Lines(text, BandVoiceWidth(keys, words, nk)) <= 2;  // as Menu_DrawFoot
}

static char g_helpBuf[256];  // HelpFor's built texts, already in the Text language

static const char *HelpFor(const Row *r) {
    char *buf = g_helpBuf;
    const size_t bufSize = sizeof g_helpBuf;
    if (!r)
        return "";
    // One message at a time: a dimmed row says why it is dimmed, not what it does.
    if (r->type == R_SETTING && !RowActive(*r))
        return "Only with the Auto camera.";
    if (r->type == R_BACK_TO_MINE) {
        // What the undo puts back (BackupDiffers, as RestoreBackup does it); as many named
        // as the voice holds, the rest counted, so the help never pages.
        const char *names[BACKUP_DIFF_MAX];
        const int n = BackupDiffers(r->arg == BACK_RESET ? g_resetBackup : g_presetBackup, names);
        for (int shown = n; shown > 0; shown--) {
            char list[200] = "";
            for (int i = 0; i < shown; i++) {
                if (i)
                    strncat(list, i + 1 == n ? Menu_Tr(" and ") : ", ", sizeof list - strlen(list) - 1);
                strncat(list, names[i], sizeof list - strlen(list) - 1);
            }
            if (shown < n) {
                char more[24];
                snprintf(more, sizeof more, Menu_Tr(" and %d more"), n - shown);
                strncat(list, more, sizeof list - strlen(list) - 1);
            }
            snprintf(buf, bufSize, Menu_Tr("Puts back what you had: %s."), list);
            if (VoiceFits(buf, r))
                return buf;
        }
        return r->help;
    }
    return r->help ? r->help : "";
}

// Shorter help for translation (decided 2026-11-03): English at most about 750 px,
// so a translation half as long again still fits the voice's two lines.
static const char *const kShortHelp[][2] = {
    {"Stops the picture tearing. Off can raise the frame rate on a slow display.", "Stops the picture tearing."},
    {"Rain, shadows and the far islands. Lower it on a slow machine.", "Rain, shadows and the far islands."},
    {"Sharp pixels as in 1997, or smoothed when scaled.", "Sharp pixels, or smoothed when scaled."},
    {"Fixes the 1997 release's known bugs. Off plays them as they were.", "Off plays the 1997 bugs as they were."},
    {"Settings still being tried. They may change or go.", "Settings still being tried."},
    {"Classic holds still and cuts; Auto turns to follow you.", "Classic cuts; Auto follows you."},
    {"Turns the camera the other way. Only with the Auto camera.", "Turns the camera the other way."},
    {"How fast the camera turns. Only with the Auto camera.", "How fast the camera turns."},
    {"Puts back the settings you had before you chose a preset.", "Puts back what you had before."},
    {"Talking, fighting, and the screens you open in play.", "Talking, fighting and the game's screens."},
    {"The game menu, pause, help, saving and loading.", "Game menu, pause, help, save and load."},
    {"Does what the behaviour does: jump, hit or hide.", "Jumps, hits or hides, by behaviour."},
};

const char *Menu_Tr(const char *english) { return Lang_Tr(english, g_val[S_TEXT_LANG]); }

static std::set<std::string> s_voiceMissed;
const std::set<std::string> &Menu_VoiceMissed() { return s_voiceMissed; }

static void Say(const char *text, bool translate) {
    if (translate) {
        for (size_t i = 0; i < sizeof kShortHelp / sizeof kShortHelp[0]; i++)
            if (!strcmp(text, kShortHelp[i][0]))
                text = kShortHelp[i][1];
        if (g_val[S_TEXT_LANG] > 0 && text[0] && !Lang_Find(text, g_val[S_TEXT_LANG]))
            s_voiceMissed.insert(text);
        text = Lang_Tr(text, g_val[S_TEXT_LANG]);  // the voice speaks the Text language
    }
    // The game font has no =, ~ or + (each draws as a stray mark); the console help has all three.
    static char said[512];
    size_t o = 0;
    for (const char *p = text; *p && o + 8 < sizeof said; p++) {
        if (!strncmp(p, " + ", 3)) {
            memcpy(said + o, " and ", 5), o += 5, p += 2;
        } else if (!strncmp(p, " ~= ", 4)) {
            memcpy(said + o, " about ", 7), o += 7, p += 3;
        } else if (!strncmp(p, " = ", 3)) {
            memcpy(said + o, ": ", 2), o += 2, p += 2;
        } else if (*p == '=') {
            memcpy(said + o, ": ", 2), o += 2;
        } else {
            said[o++] = *p;
        }
    }
    said[o] = 0;
    text = said;
    if (strcmp(g_voice.text, text) != 0)
        Voice_Say(g_voice, text, !sw_whole);
}

void Menu_Say(const char *text) { Say(text, true); }
void Menu_SayAsIs(const char *text) { Say(text, false); }

static void UpdateVoice() {
    const Row *r = CursorRow();
    const char *text = g_confirm ? kQuestions[g_question].voice : HelpFor(r);
    if (text == g_helpBuf)
        Menu_SayAsIs(text);
    else
        Menu_Say(text);
}

static void Push(int page) {
    g_cursorStack[g_depth - 1] = g_cursor;
    g_stack[g_depth++] = page;
    g_cursor = 0;  // the 1997 sub-menus open on their Return row
    g_scroll = 0;
    Trace("open page=%s", g_pages[page].name);
}

// The layout the navigation state was made in. The screen can change under it (a
// new size, the scale rule, the sheet switch), and the two layouts keep the place
// differently: one column stacks hub, category, sub-page, each cursor counting
// the Return row; wide keeps the category as the left pane's cursor and the right
// pane's stack under the hub, its cursors past the Return row.
static int g_layoutWide = -1;

static int HubRowFor(int page) {
    for (int i = 0; i < LeftCount(); i++)
        if (g_pages[P_HUB].rows[LeftRowToHub(i)].arg == page)
            return LeftRowToHub(i);
    return -1;
}

static bool PageLeadsTo(int from, int to) {
    for (int i = 0; i < g_pages[from].count; i++)
        if (g_pages[from].rows[i].arg == to && g_pages[from].rows[i].type != R_SETTING)
            return true;
    return false;
}

static void SyncLayout() {
    const int wide = Wide() ? 1 : 0;
    if (g_layoutWide == wide)
        return;
    const int was = g_layoutWide;
    g_layoutWide = wide;
    if (was < 0 || g_first)
        return;
    g_scroll = 0;
    if (wide) {
        int hubRow = g_depth >= 2 ? HubRowFor(g_stack[1]) : -1;
        if (g_depth == 1) {
            g_leftCursor = g_cursor > 0 ? g_cursor - 1 : 0;
            g_focus = F_LEFT;
            g_cursor = 0;
        } else if (hubRow >= 0) {
            // Drop the category from the stack: the left cursor holds it.
            g_leftCursor = hubRow - 1;
            for (int i = 1; i + 1 < g_depth; i++) {
                g_stack[i] = g_stack[i + 1];
                g_cursorStack[i - 1] = g_cursorStack[i] > 0 ? g_cursorStack[i] - 1 : 0;
            }
            g_depth--;
            g_cursor = g_cursor > 0 ? g_cursor - 1 : 0;
            g_focus = F_RIGHT;
        } else {
            for (int i = 1; i + 1 < g_depth; i++)
                g_cursorStack[i] = g_cursorStack[i] > 0 ? g_cursorStack[i] - 1 : 0;
            g_leftCursor = 0;
            g_cursor = g_cursor > 0 ? g_cursor - 1 : 0;
            g_focus = F_RIGHT;
        }
    } else {
        const int hubRow = LeftRowToHub(g_leftCursor);
        const int cat = g_pages[P_HUB].rows[hubRow].arg;
        g_leftCursor = 0;  // one column has no left pane
        if (g_focus == F_LEFT) {
            g_depth = 1;
            g_cursor = hubRow;
        } else if (g_depth == 1 || (g_depth < 8 && PageLeadsTo(cat, g_stack[1]))) {
            // Put the category back between the hub and the right pane's pages.
            for (int i = g_depth; i >= 2; i--) {
                g_stack[i] = g_stack[i - 1];
                g_cursorStack[i - 1] = g_cursorStack[i - 2] + 1;
            }
            g_stack[1] = cat;
            g_cursorStack[0] = hubRow;
            g_depth++;
            g_cursor++;
        } else {
            for (int i = 1; i + 1 < g_depth; i++)
                g_cursorStack[i]++;
            g_cursor++;
        }
    }
    Trace("layout %s page=%s cursor=%d", wide ? "wide" : "column",
          g_pages[wide && g_focus == F_RIGHT ? RightPage() : TopPage()].name, g_cursor);
}

static void Pop() {
    if (g_depth > 1) {
        Trace("close page=%s", g_pages[TopPage()].name);
        g_depth--;
        g_cursor = g_cursorStack[g_depth - 1];
        g_scroll = 0;
    } else {
        g_closed = true;
        Trace("close options");
    }
}

void Menu_Init(bool firstLaunch) {
    g_modernScreen = ScreenNow();  // Modern's screen
    for (int s = 0; s < S_COUNT; s++)
        g_val[s] = FreshValue(s);
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
    g_voice.text[0] = 0;
    g_layoutWide = Wide() ? 1 : 0;
    Trace("init layout=%s logical=%ux%u first=%d", Wide() ? "wide" : "column",
          (unsigned)Engine_LogicalW(), (unsigned)Engine_LogicalH(), (int)g_first);
}

static void ChangeSetting(int s, int dir) {
    if (s == S_RESOLUTION) {
        if (Engine_StepScreen(dir))
            Menu_NoteScreen();
        return;
    }
    if (s == S_FULLSCREEN) {
        if (Engine_SetFullscreen(!Engine_Fullscreen()))
            Menu_NoteScreen();
        return;
    }
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
            g_presetBackup.have = false;
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
        RestoreBackup(r.arg == BACK_RESET ? g_resetBackup : g_presetBackup);
        Trace("back to my settings");
        BuildPages();
        if (!Wide() && g_cursor >= g_pages[TopPage()].count)
            g_cursor = g_pages[TopPage()].count - 1;
        break;
    case R_ACTION:
        if (r.arg == A_RESET)
            Ask(Q_RESET);
        else if (r.arg == A_EXP)
            Exp_Open();
        else if (r.arg == A_CONSOLE) {
            g_console = Engine_LastInputWasPad() ? CON_READONLY : CON_FULL;
            Trace("open console mode=%s", g_console == CON_READONLY ? "read-only" : "full");
        }
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
    } else {
        g_cursor = (g_cursor + dir + n) % n;
    }
    const Row *r = CursorRow();
    Trace("cursor %s", r ? (r->type == R_PRESET_ROW ? "preset row" : r->label) : "-");
}

// After the page changed shape: the cursor back on the row it was on, if it is still there.
static void KeepCursorOn(const char *label) {
    if (!label || g_first || (Wide() && g_focus == F_LEFT))
        return;
    const Page &p = g_pages[Wide() ? RightPage() : TopPage()];
    int first = Wide() ? RightFirst() : 0;
    for (int i = first; i < p.count; i++) {
        if (p.rows[i].label && !strcmp(p.rows[i].label, label)) {
            g_cursor = i - first;
            return;
        }
    }
}

void Menu_Update() {
    SyncLayout();
    if (Controls_Active()) {
        Controls_Update();
        return;
    }
    if (Exp_Active()) {
        Exp_Update();
        if (!Exp_Active())
            BuildPages();  // a changed tuning value can change the Undo rows
        return;
    }
    if (g_console) {
        // Full: Esc closes (F12 in the game; F12 is a prototype switch here). Read-only: anything does.
        bool any = Engine_RawPressed() != 0;
        for (int k = 0; k < KEY_COUNT && !any; k++)
            any = Engine_Pressed((UiKey)k);
        if (g_console == CON_READONLY ? any : Engine_Pressed(KEY_ESC)) {
            g_console = CON_CLOSED;
            Trace("close console");
        }
        return;
    }
    if (g_closed) {
        if (Engine_Pressed(KEY_ENTER)) {
            g_closed = false;
            Trace("reopen options");
        } else if (Engine_Pressed(KEY_ESC)) {
            g_quit = true;
            Trace("quit");
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
                const Row *was = CursorRow();
                const char *label = was ? was->label : NULL;
                if (g_question == Q_RESET)
                    ResetSettings();
                else
                    ApplyPreset(1);
                BuildPages();
                KeepCursorOn(label);  // the page was rebuilt; an Undo row comes and goes
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
    if (!Controls_Active() && !Exp_Active())  // a page just opened: its help, not this row's
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
void Menu_RowLabelCentred(S32 x0, S32 x1, S32 yc, const char *s, U8 colour, bool asIs, bool cursor) {
    MemoClip();
    SetClip(x0 + 1, yc - 25, x1 - 1, yc + 25);
    if (!asIs)
        s = Ui_Tr(s);
    if (Ui_TextWidthAsIs(s) > x1 - x0 - 16)
        Ui_TextInRoom(x0 + 8, x1 - 8, yc - 18, s, colour, cursor);  // 1997: x0 + 8, swinging
    else
        Ui_TextAsIs((x0 + x1) / 2 - Ui_TextWidthAsIs(s) / 2, yc - 18, s, colour);
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
    else if (r.type == R_ACTION && r.arg == A_CONSOLE)
        snprintf(text, sizeof text, "%s: %s", r.label, kVersion);
    else
        snprintf(text, sizeof text, "%s", r.label);
    Menu_RowLabelCentred(x0, x1, yc, text, active ? INK_WHITE : INK_DISABLED, false, cursor);

    DrawnRow &d = Menu_Record("column", text,
                              r.type == R_SETTING                         ? ValueText(r.arg, val, sizeof val)
                              : r.type == R_ACTION && r.arg == A_CONSOLE ? kVersion
                                                                          : "",
                              fill, x0, y0, x1, y1);
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
    // The 1997 slider (GAMEMENU.CPP DrawOneChoice): under the cursor the whole row is the
    // slider plasma up to the value, fill-selected after it, the label on it.
    const bool barRow = r.type == R_SETTING && kSettings[r.arg].kind == K_SLIDER;
    if (barRow && fill == FILL_CURSOR && active) {
        S32 x2 = x0 + (x1 - x0) * g_val[r.arg] / kSettings[r.arg].max;
        Cadre_BackupCorners(x0, y0, x1, y1);
        Cadre_FireBar(x0, y0, x1, y1, PLASMA_SLIDER);
        if (x2 < x1)
            Box(x2, y0, x1, y1, FILL_SELECTED);
        Cadre_Frame(x0, y0, x1, y1);
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
        snprintf(label, sizeof label, "%s", r.label);
    }

    DrawnRow &d = Menu_Record(pane, label, "", fill, x0, y0, x1, y1);
    d.dim = !active;
    d.setting = r.type == R_SETTING;
    d.textLeft = x0 + 16;
    d.textRight = x0 + 16 + Ui_TextWidth(label);

    const S32 valueRight = x1 - 24;
    S32 valueLeft = 0;  // where a value starts: the label's room ends 8 before its marks
    if (r.type == R_SETTING) {
        const Setting &st = kSettings[r.arg];
        bool changed = ValueChanged(r.arg);
        S32 left = valueRight;
        if (st.kind == K_STEPS) {
            Ui_Steps(valueRight, yc, st.max, g_val[r.arg], &left);
            snprintf(d.value, sizeof d.value, "%d/%d", g_val[r.arg], st.max);
            valueLeft = left;
        } else if (st.kind == K_SLIDER) {
            snprintf(d.value, sizeof d.value, "%d", g_val[r.arg]);  // the bar is the value
        } else {
            ValueText(r.arg, val, sizeof val);
            left = valueRight + 1 - Ui_TextInkWidth(val);  // the ink ends on the value edge
            if (d.textRight + 8 > left - 17)
                d.textRight = x1 + (d.textRight + 8 - (left - 17));  // label runs into the value
            Ui_Text(left, yc - 18, val, !active ? INK_DISABLED : changed ? INK_COUNT : INK_WHITE);
            snprintf(d.value, sizeof d.value, "%s", val);
            valueLeft = left;
        }
        d.changed = changed;
        d.valueRight = valueRight;
        if (fill == FILL_CURSOR && active && st.kind != K_SLIDER) {
            Ui_ChangeMarks(left - 17, valueRight + 17, yc);  // in the padding: the value never moves
            d.marks = true;
        }
    } else if (r.type == R_ACTION && r.arg == A_CONSOLE) {
        // The version where a value goes, in the dimmed grey: it reads, it does not change.
        S32 left = valueRight + 1 - Ui_TextInkWidth(kVersion);
        if (d.textRight + 8 > left - 17)
            d.textRight = x1 + (d.textRight + 8 - (left - 17));
        Ui_Text(left, yc - 18, kVersion, INK_DISABLED);
        snprintf(d.value, sizeof d.value, "%s", kVersion);
        d.valueRight = valueRight;
        valueLeft = left;
    } else if (presetValue) {
        S32 left = valueRight + 1 - Ui_TextInkWidth(presetValue);
        if (d.textRight + 8 > left - 17)
            d.textRight = x1 + (d.textRight + 8 - (left - 17));
        Ui_Text(left, yc - 18, presetValue, PresetChanged() ? INK_COUNT : INK_WHITE);
        snprintf(d.value, sizeof d.value, "%s", presetValue);
        d.valueRight = valueRight;
        valueLeft = left;
    } else if (r.type == R_PRESET_CHOICE && r.arg == g_preset && PresetChanged()) {
        valueLeft = valueRight + 1 - Ui_TextInkWidth("Changed");
        Ui_Text(valueLeft, yc - 18, "Changed", INK_COUNT);
        snprintf(d.value, sizeof d.value, "Changed");
        d.valueRight = valueRight;
    }
    // The label last, in the room the value leaves (the 1997 swing when it is wider).
    MemoClip();
    SetClip(x0 + 1, y0, x1 - 1, y1);
    Ui_TextInRoom(x0 + 16, valueLeft ? valueLeft - 17 - 8 - 1 : x1 - 16, yc - 18, Ui_Tr(label),
                  active ? INK_WHITE : INK_DISABLED, fill == FILL_CURSOR);
    RestoreClip();
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
    if (g_console) {
        keys[*n] = g_console == CON_READONLY ? "Any" : "Esc";
        words[(*n)++] = "Close";
        return;
    }
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
        } else if (r->type == R_CATEGORY || r->type == R_PRESET_ROW ||
                   (r->type == R_ACTION && (r->arg == A_KEYS || r->arg == A_EXP || r->arg == A_CONSOLE))) {
            keys[*n] = "Enter"; words[(*n)++] = "Open";
        } else if (r->type != R_SETTING) {
            keys[*n] = "Enter"; words[(*n)++] = "Choose";
        }
    }
    keys[*n] = "Esc";
    words[(*n)++] = g_first ? "Modern" : "Back";
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

// Stacked: the widest chip, the gap, the widest word.
static S32 LegendWidth(const char **keys, const char **words, int n, S32 scale) {
    S32 chipW = 0, wordW = 0;
    for (int i = 0; i < n; i++) {
        S32 c = Ui_ChipWidth(keys[i], scale), t = Ui_SmallWidth(LegendWord(words[i]), scale);
        chipW = c > chipW ? c : chipW;
        wordW = t > wordW ? t : wordW;
    }
    return n ? chipW + 4 * scale + wordW : 0;
}

static void LegendRecord(int i, const char *key, const char *word) {
    char item[48];
    snprintf(item, sizeof item, "%s[%s] %s", i ? "  " : "", key[0] == '\x11' ? "<>" : key[0] == '\x18' ? "^v" : key, word);
    strncat(g_legend, item, sizeof g_legend - strlen(g_legend) - 1);
}

// In a row: chip, word, chip, word, centred on yc.
// Stacked (the review card's legend): one key a line from y0, the chips ending on
// one column and the words starting on the next, so the words read as a list.
static void DrawLegendStacked(S32 x, S32 y0, const char **keys, const char **words, int n, S32 scale) {
    g_legend[0] = 0;
    S32 chipW = 0;
    for (int i = 0; i < n; i++) {
        S32 c = Ui_ChipWidth(keys[i], scale);
        chipW = c > chipW ? c : chipW;
    }
    S32 chipH = 12 * scale, pitch = chipH + 2;
    for (int i = 0; i < n; i++) {
        S32 y = y0 + i * pitch;
        S32 c = Ui_ChipWidth(keys[i], scale);
        Ui_Chip(x + chipW - c, y, keys[i], scale);
        Ui_Small(x + chipW + 4 * scale, y + chipH / 2 - 4 * scale, LegendWord(words[i]), INK_CONFIG_MENU, scale);
        LegendRecord(i, keys[i], words[i]);
    }
}

// The question's answers, the safe one first; the question itself is in the voice.
// One column: a small panel in the middle, labels centred, as 1997. Wide: in place
// of the right pane's rows, labels on the pane's label edge.
static void DrawConfirm(const char *pane, S32 x0, S32 x1, S32 y0) {
    bool centred = !strcmp(pane, "confirm");
    const char *const *answers = kQuestions[g_question].answers;
    for (int i = 0; i < 2; i++) {
        S32 ry0 = y0 + i * ROW_PITCH, yc = ry0 + ROW_H / 2;
        RowFill fill = i == g_confirmCursor ? FILL_CURSOR : FILL_SHADE;
        Ui_RowBackground(x0, ry0, x1, ry0 + ROW_H, fill);
        DrawnRow &d = Menu_Record(pane, answers[i], "", fill, x0, ry0, x1, ry0 + ROW_H);
        if (centred) {
            Menu_RowLabelCentred(x0, x1, yc, answers[i], INK_WHITE, false, i == g_confirmCursor);
            d.textLeft = (x0 + x1) / 2 - Ui_TextWidth(answers[i]) / 2;
        } else {
            MemoClip();
            SetClip(x0 + 1, ry0, x1 - 1, ry0 + ROW_H);
            Ui_TextInRoom(x0 + 16, x1 - 16, yc - 18, Ui_Tr(answers[i]), INK_WHITE, i == g_confirmCursor);
            RestoreClip();
            d.textLeft = x0 + 16;
            d.textRight = x0 + 16 + Ui_TextWidth(answers[i]);
        }
    }
}

// Paged help (help longer than its window) says where it is: "1/2" in the 8x8 font in the
// window's bottom-right corner, kept on the last window, so a player who looks late still sees
// there was more (decided 2026-11-12, over the dialogue's arrow tried on the Xbox 2026-11-11).
static char g_pageMark[8];
static void DrawPageMark(S32 x1, S32 y1) {
    S32 page = 0, pages = 1;
    Voice_LastPage(&page, &pages);
    g_pageMark[0] = 0;
    if (pages < 2)
        return;
    S32 sc = Wide() ? 2 : 1;
    snprintf(g_pageMark, sizeof g_pageMark, "%d/%d", (int)page + 1, (int)pages);
    Ui_Small(x1 - 12 - Ui_SmallWidth(g_pageMark, sc) + 1, y1 - 6 - 8 * sc, g_pageMark, INK_CONFIG_MENU, sc);
}
const char *Menu_PageMark() { return g_pageMark; }
const char *Menu_Legend() { return g_legend; }

S32 Menu_DialogueTop() { return (S32)Engine_LogicalH() - 1 - 137; }

void Menu_DrawDialogue() {
    // MESSAGE.CPP NormalWinDial: 8 in from the sides, 137 tall, 8 off the bottom.
    S32 W = (S32)Engine_LogicalW(), H = (S32)Engine_LogicalH();
    Menu_DrawDialogueIn(8, H - 1 - 137, W - 1 - 8, H - 1 - 8);
}

void Menu_DrawDialogueIn(S32 x0, S32 y0, S32 x1, S32 y1) {
    Ui_Panel(x0, y0, x1, y1, PANEL_SHADE_LVL);
    g_voiceLines = Voice_Draw(g_voice, x0 + 16, y0 + 12, x1 - x0 - 32, 3, &g_voiceNeeded);
    g_voiceMax = 3;
    DrawPageMark(x1, y1);  // the window's own corner, where MESSAGE.CPP AffFleche puts its arrow
    g_helpBox.shown = true;
    g_helpBox.kind = "dialogue";
    g_helpBox.x0 = x0; g_helpBox.y0 = y0; g_helpBox.x1 = x1; g_helpBox.y1 = y1;
}

static void DrawColumn() {
    S32 W = (S32)Engine_LogicalW(), H = (S32)Engine_LogicalH();
    const Page &p = g_first ? g_pages[P_FIRST] : g_pages[TopPage()];

    // First launch on a canvas that takes the wide composition sits in its frame,
    // the dialogue window where the band is on every other page.
    bool framed = g_first && Wide();
    WideFrame fr = Menu_WideFrame();
    S32 oy = framed ? fr.y0 : 0;
    if (g_first) {
        Box(0, 0, W - 1, H - 1, 0);
        const char *title = "Choose how to play";
        S32 tx = W / 2 - Ui_TextWidth(title) / 2;
        Ui_Text(tx + 4, oy + 64 + 4, title, INK_BLACK);  // the title shadow, +4,+4
        Ui_Text(tx, oy + 64, title, INK_WHITE);
        Menu_Record("title", title, "", FILL_SHADE, tx, oy + 64, tx + Ui_TextWidth(title), oy + 64 + 38).textLeft = tx;
    }

    if (g_confirm) {
        DrawConfirm("confirm", W / 2 - 180, W / 2 + 180, 120);
    } else {
        // Help in the dialogue window on every one-column page (decided 2026-11-03).
        S32 top = 16, bottom = H - 1 - 137 - 16;
        if (g_first)
            top = 168 - 8;
        int visible = (bottom - top + ROW_GAP) / ROW_PITCH;
        if (visible > p.count)
            visible = p.count;
        if (g_scroll > p.count - visible)
            g_scroll = p.count - visible;  // a row went (Undo reset): no rows past the end
        if (g_cursor < g_scroll)
            g_scroll = g_cursor;
        if (g_cursor >= g_scroll + visible)
            g_scroll = g_cursor - visible + 1;
        S32 stackH = visible * ROW_PITCH - ROW_GAP;
        S32 y = top + (bottom - top - stackH) / 2;  // centred above the dialogue window
        if (g_first)
            y = oy + 168;
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
    if (framed)
        Menu_DrawDialogueIn(fr.x0 + 32, fr.y0 + fr.h - 25 - 137, fr.x0 + fr.w - 33, fr.y0 + fr.h - 25);
    else
        Menu_DrawDialogue();
}

static void DrawWide() {
    WideFrame f = Menu_WideFrame();
    const S32 side = 32, gap = 16;
    const S32 top = f.y0 + 24;
    S32 inner = f.w - 2 * side - gap;
    S32 leftW = inner * 464 / 880;
    S32 lx0 = f.x0 + side, lx1 = lx0 + leftW - 1;
    S32 rx0 = lx1 + 1 + gap, rx1 = f.x0 + f.w - side - 1;
    // One pane at a time, the focused one, centred and as wide as the 1997 canvas.
    lx0 = rx0 = f.x0 + (f.w - DRILL_W) / 2;
    lx1 = rx1 = lx0 + DRILL_W - 1;
    bool drawLeft = g_focus == F_LEFT && !g_confirm;
    bool drawRight = !drawLeft;

    // Left pane: the hub's categories.
    const Page &hub = g_pages[P_HUB];
    S32 y = top;
    for (int i = 0; drawLeft && i < LeftCount(); i++) {
        RowFill fill = FILL_SHADE;
        if (i == g_leftCursor)
            fill = g_focus == F_LEFT ? FILL_CURSOR : FILL_CURRENT;
        DrawWideRow("left", hub.rows[LeftRowToHub(i)], lx0, y, lx1, y + ROW_H, fill);
        y += ROW_PITCH;
    }

    // Right pane: the page of the category under the left cursor, or, while a
    // question is up, its answers in place of the rows.
    if (!drawRight) {
    } else if (g_confirm) {
        S32 qtop = Ui_Title(rx0, rx1, top, g_pages[RightPage()].name);
        DrawConfirm("right", rx0, rx1, qtop);
    } else {
        int rp = RightPage();
        const Page &page = g_pages[rp];
        int n = page.count - RightFirst();
        S32 limit = Menu_WideRowsLimit();
        // Alone, the page names itself: a title in the seventh row's place.
        S32 rtop = Ui_Title(rx0, rx1, top, page.name);
        Menu_Record("title", page.name, "", FILL_SHADE, rx0, top, rx1, top + 38).textLeft =
            (rx0 + rx1 + 1) / 2 - Ui_TextWidth(page.name) / 2;
        int visible = (limit - rtop + ROW_GAP) / ROW_PITCH;
        if (visible > n)
            visible = n;
        if (g_focus == F_RIGHT) {
            if (g_scroll > n - visible)
                g_scroll = n - visible;  // a row went (Undo reset): no rows past the end
            if (g_cursor < g_scroll)
                g_scroll = g_cursor;
            if (g_cursor >= g_scroll + visible)
                g_scroll = g_cursor - visible + 1;
        } else {
            g_scroll = 0;
        }
        y = rtop;
        for (int i = g_scroll; i < g_scroll + visible; i++) {
            RowFill fill = FILL_SHADE;
            const Row &row = page.rows[RightFirst() + i];
            if (g_focus == F_RIGHT && i == g_cursor)
                fill = FILL_CURSOR;
            else if (row.type == R_PRESET_CHOICE && !g_first && row.arg == g_preset)
                fill = FILL_CURRENT;  // the chosen preset wears the current-value plasma, as in the column
            DrawWideRow("right", row, rx0, y, rx1, y + ROW_H, fill);
            y += ROW_PITCH;
        }
        if (visible < n)
            Menu_DrawScrollBar(rx1 + 9, rtop, rtop + visible * ROW_PITCH - ROW_GAP - 1, g_scroll, visible, n);
    }

    // Help and legend.
    const char *keys[4], *words[4];
    int nk;
    LegendFor(CursorRow(), keys, words, &nk);
    Menu_DrawFoot(keys, words, nk);
}

S32 Menu_WideRowsLimit() {
    WideFrame f = Menu_WideFrame();
    S32 H = f.y0 + f.h;
    return H - 24 - 86 - 16;
}

// The voice's width in the band: what the legend leaves, at most the review's 620.
static S32 BandVoiceWidth(const char **keys, const char **words, int nk) {
    S32 W = Menu_WideFrame().w;
    S32 legendW = LegendWidth(keys, words, nk, 2);
    S32 voiceW = (W - 2 * 32) - legendW - 48;
    return voiceW > 620 ? 620 : voiceW;
}

void Menu_DrawFoot(const char **keysIn, const char **words, int nk) {
    // After a pad press the legend names the pad's buttons, not the keyboard's.
    const char *keys[8];
    for (int i = 0; i < nk && i < 8; i++)
        keys[i] = Controls_LegendKey(keysIn[i]);
    WideFrame f = Menu_WideFrame();
    S32 W = f.w, H = f.h;
    const S32 side = 32;
    const S32 chip = 2;  // the 8x8 key font at 2x (decided 2026-11-03)
    S32 bx0 = f.x0 + side, bx1 = f.x0 + W - side - 1;
    S32 bandY0 = f.y0 + H - 24 - 86, bandY1 = f.y0 + H - 25;
    S32 legendW = LegendWidth(keys, words, nk, chip);
    Ui_Panel(bx0, bandY0, bx1, bandY1, 15);  // darkened to level 15 (decided 2026-11-03)
    S32 voiceW = BandVoiceWidth(keys, words, nk);
    g_voiceLines = Voice_Draw(g_voice, bx0 + 16, bandY0 + 5, voiceW, 2, &g_voiceNeeded);
    g_voiceMax = 2;
    g_voiceWidth = voiceW;
    // More to read: the page mark at the voice's bottom-right corner, short of the legend.
    S32 markX1 = bx0 + 16 + voiceW + 29, legendX = bx1 - 24 - legendW + 1;
    if (markX1 > legendX - 8)
        markX1 = legendX - 8;
    DrawPageMark(markX1, bandY1);
    DrawLegendStacked(bx1 - 24 - legendW + 1, bandY0 + 7, keys, words, nk, chip);
    g_helpBox.shown = true;
    g_helpBox.kind = "band";
    g_helpBox.x0 = bx0; g_helpBox.y0 = bandY0; g_helpBox.x1 = bx1; g_helpBox.y1 = bandY1;
}

static void DrawOverlay() {
    static const char *lines[] = {
        "Prototype keys",
        "F1 this list   F2 / Shift F2 screen size",
        "Drag the window's edge: any screen size",
        "F11 pad names: connected / Xbox / PlayStation / Nintendo",
    };
    S32 n = (S32)(sizeof lines / sizeof lines[0]);
    S32 w = 8 * 50 + 16, h = n * 12 + 12;
    S32 x0 = (S32)Engine_LogicalW() - w - 8, y0 = 8;
    Ui_Panel(x0, y0, x0 + w, y0 + h, PANEL_SHADE_LVL);
    for (S32 i = 0; i < n; i++)
        Ui_Small(x0 + 8, y0 + 8 + i * 12, lines[i], i ? INK_CONFIG_MENU : INK_CONFIG_HEAD, 1);
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
static S32 g_consoleBottom;  // the line under it, for `state`

static void DrawConsole() {
    S32 W = (S32)Engine_LogicalW(), H = (S32)Engine_LogicalH();
    S32 h = H / 2;
    if (h < 9 * 4)
        h = 9 * 4;
    Cadre_Shade(0, 0, W - 1, h - 1, 15);
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
        Ui_Small(8, y, (Engine_TimeMs() / 500) % 2 ? "]> " : "]> _", INK_WHITE, 1);
}

// Frame rate on: the count in the bottom left corner, white, the 8x8 font (OBJECT.CPP
// AffNbFps). lba2cc draws it only in play; here it shows over the menu so the row can be judged.
static void DrawFps() {
    static U32 windowStart, frames, shown;
    U32 now = Engine_TimeMs();
    frames++;
    if (now - windowStart >= 1000) {
        shown = frames * 1000 / (now - windowStart ? now - windowStart : 1);
        windowStart = now;
        frames = 0;
    }
    char buf[16];
    snprintf(buf, sizeof buf, "%u ", (unsigned)shown);
    Ui_Small(0, (S32)Engine_LogicalH() - 8, buf, INK_WHITE, 1);
}

// The picture at the screen's own resolution, as the Image row says (decided 2026-11-11):
// Classic the smallest whole step that covers the screen, Smooth the exact cover factor.
static void SyncBackgroundRes() {
    const int want = 1 + g_val[S_IMAGE];
    if (want != Engine_BackgroundRes())
        Engine_SetBackgroundRes(want);
}

void Menu_Draw() {
    SyncBackgroundRes();
    VFont_Set(g_val[S_FONT]);  // before the layout: widths follow the font
    Ui_SetLang(g_val[S_TEXT_LANG]);  // and the language
    VFont_BeginFrame();
    SyncLayout();
    g_drawnCount = 0;
    g_helpBox.shown = false;
    g_helpBox.x0 = g_helpBox.y0 = g_helpBox.x1 = g_helpBox.y1 = 0;  // where it was is no help on a new screen
    g_voiceLines = g_voiceNeeded = g_voiceMax = g_voiceWidth = 0;
    g_legend[0] = 0;

    Engine_SetBackdrop(g_val[S_BACKGROUND]);
    Engine_DrawBackground();
    Cadre_AnimatePlasma();

    if (g_closed) {
        // Two lines at the voice's pitch: on one, 784 wide, it would not fit 640.
        const char *lines[2] = {"Options closed.", "<ENTER> opens them again. <ESC> quits."};
        for (int i = 0; i < 2; i++)
            Ui_TextShadow((S32)Engine_LogicalW() / 2 - Ui_TextWidth(lines[i]) / 2,
                          (S32)Engine_LogicalH() / 2 - 38 + i * 38, lines[i], DIALOGUE_HERO * 16 + 12);
    } else if (Controls_Active()) {
        Controls_Draw();
    } else if (Exp_Active()) {
        Exp_Draw();
    } else if (Wide() && !g_first) {
        DrawWide();
    } else {
        DrawColumn();
    }

    if (g_console)
        DrawConsole();
    if (g_val[S_FPS])
        DrawFps();
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
    {"overlay", &sw_overlay, 0, 1, 0},
    {"padnames", &sw_padnames, 0, 3, 0},
    {"whole", &sw_whole, 0, 1, 0},
};

bool Menu_SetSwitch(const char *name, int v) {
    if (!strcmp(name, "keyboard")) {  // 0: as a console with no keyboard yet
        Engine_SetKeyboardSeen(v != 0);
        return true;
    }
    for (size_t i = 0; i < sizeof kSwitches / sizeof kSwitches[0]; i++) {
        const SwitchDef &s = kSwitches[i];
        if (strcmp(s.name, name) != 0)
            continue;
        if (v < s.min || v > s.max)
            v = s.min + ((v - s.min) % (s.max - s.min + 1) + (s.max - s.min + 1)) % (s.max - s.min + 1);
        *s.value = v;
        BuildPages();
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
    SyncLayout();  // what follows is made in the layout in use
    if (!strcmp(name, "Keys and buttons")) {
        Menu_Open("Controls and camera");
        KeepCursorOn("Keys and buttons");  // as Enter on it leaves it
        Controls_Open();
        return true;
    }
    if (!strcmp(name, "More options")) {
        Menu_Open("Advanced and reset");
        KeepCursorOn("More options");  // as Enter on it leaves it
        Exp_Open();
        return true;
    }
    if (!strcmp(name, "Console")) {
        Menu_Open("Advanced and reset");
        KeepCursorOn("Console");  // as Enter on it leaves it
        g_console = Engine_LastInputWasPad() ? CON_READONLY : CON_FULL;
        return true;
    }
    g_console = CON_CLOSED;
    Controls_Close();
    Exp_Close();
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
    if (Exp_Set(id, value)) {
        BuildPages();
        return true;
    }
    return false;
}

void Menu_Reset() {
    for (size_t i = 0; i < sizeof kSwitches / sizeof kSwitches[0]; i++)
        *kSwitches[i].value = kSwitches[i].start;
    Engine_ResetInput();
    Controls_Reset();
    Exp_Reset();
    g_confirm = false;
    g_confirmCursor = 0;
    g_closed = false;
    g_console = CON_CLOSED;
    g_presetBackup.have = false;
    g_resetBackup.have = false;
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
    S32 page, pages;
    Voice_LastPage(&page, &pages);
    if (!g_helpBox.shown)
        page = pages = 0;  // no help this frame: the count is the last one drawn, elsewhere
    h->page = page;
    h->pages = pages;
    h->max = g_voiceMax;
    h->width = g_voiceWidth;
    h->text = g_voice.text;
}

bool Menu_WantsQuit() { return g_quit; }

const char *Menu_Layout() { return g_closed ? "closed" : g_first ? "first-launch" : Wide() ? "wide" : "column"; }

const char *Menu_PageName() {
    return Controls_Active() ? "Keys and buttons" : Exp_Active() ? "More options" : g_first ? "First launch" : g_pages[Wide() ? RightPage() : TopPage()].name;
}

// One for the hub; each page pushed, the wide right pane and a question add one.
static int Depth() {
    if (g_first)
        return 1;
    return g_depth + (Wide() && g_focus == F_RIGHT ? 1 : 0) + (g_confirm ? 1 : 0);
}

const char *Menu_CursorLabel() {
    static char buf[160];
    const Row *r = Controls_Active() || Exp_Active() || g_closed ? NULL : CursorRow();
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
    if (Exp_Active()) {
        char inner[200];
        int d = Exp_Where(inner, sizeof inner);
        snprintf(buf, n, "%s | %s", Menu_PageName(), inner);
        return Depth() + d;
    }
    if (g_console) {
        snprintf(buf, n, "%s | %s | console", Menu_PageName(), Menu_CursorLabel());
        return Depth() + 1;
    }
    if (g_confirm)
        snprintf(buf, n, "%s | %s | confirm", Menu_PageName(), g_confirmCursor ? "Yes" : "No");
    else
        snprintf(buf, n, "%s | %s", Menu_PageName(), Menu_CursorLabel());
    return Depth();
}

bool Menu_CursorSetting(bool *active) {
    const Row *r = Controls_Active() || Exp_Active() || g_closed ? NULL : CursorRow();
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
        const Setting &st = kSettings[s];
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
            snprintf(label, sizeof label, "%s", r.label);
        }
        fprintf(f, "%s{\"label\":\"%s\",\"w\":%d,\"help_w\":%d}", i ? "," : "", label, Ui_TextWidth(label),
                Ui_TextWidth(r.help ? r.help : ""));
    }
    fprintf(f, "]}\n");
}

void Menu_WriteState(FILE *f) {
    SyncLayout();
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
    const Row *r = Controls_Active() || Exp_Active() ? NULL : CursorRow();
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
    fprintf(f, ",\"console\":\"%s\",\"console_bottom\":%d,\"version\":", g_console == CON_FULL ? "full" : g_console == CON_READONLY ? "read-only" : "closed",
            g_console ? (int)g_consoleBottom : -1);
    Menu_JsonString(f, kVersion);

    if (Controls_Active()) {
        fprintf(f, ",\"controls\":");
        Controls_WriteState(f);
    }
    if (Exp_Active()) {
        fprintf(f, ",\"experimental\":");
        Exp_WriteState(f);
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
