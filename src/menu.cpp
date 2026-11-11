#include "menu.h"

#include "game_pieces.h"
#include "bindings.h"
#include "host.h"
#include "more_options.h"
#include "trace.h"
#include "menu_parts.h"
#include "lang.h"
#include "ui.h"
#include "twinsun.h"
#include "screen_font.h"

#include <SVGA/BOX.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SVGA/FIL_LINE.H>
#include <SVGA/FONT.H>

#include <assert.h>
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
    S_MASTER, S_SFX, S_VOICES, S_MUSIC, S_REVERSE,
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
    // the screen's resolution (src/screen_font.cpp), the face lba2cc's parked HD UI overlay chose.
    // Bold over Medium and the size, decided after play (2026-11-11).
    {"font", "Font", K_CHOICE, 1, {"Game", "Zilla Slab"}, 0, 0, false,
     "The game's letters, or sharp Zilla Slab."},
    // Not a stored value: the row shows the screen in use and steps it through
    // Host_SizeAt's list, as F2 does. g_val keeps its fresh value; the screen is the
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
    // ReverseStereo, back from More options' From 1997 group (EPmager, 2026-11-11): speakers
    // or headphones the wrong way round are not a 1997 machine's limit. The 1997 game's label.
    {"reverse_stereo", "Reverse stereo", K_TOGGLE, 1, {"Off", "On"}, 0, 0, false, "Swaps the left and right speakers."},
    {"cam_out", "Outdoors camera", K_CHOICE, 1, {"Classic", "Auto"}, 0, 0, true,
     "Classic holds still and cuts; Auto turns to follow you."},
    {"cam_in", "Indoors camera", K_CHOICE, 1, {"Classic", "Follow"}, 0, 0, true,
     "Follow keeps Twinsen in view inside houses."},
    // AllCameras, the 1997 "scenario cameras" (lba2cc's Movie camera, on Advanced options there):
    // off skips the outdoor shots that cut once as the hero walks in; held shots and interiors
    // stay. Camera behavior, not a machine's limit, so it sits with the cameras (2026-11-11).
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

static ScreenSize ScreenNow() { return ScreenSize{Host_PhysW(), Host_PhysH()}; }
static bool SameScreen(ScreenSize a, ScreenSize b) { return a.w == b.w && a.h == b.h; }
static ScreenSize PresetScreen(int preset) { return preset == 1 ? ScreenSize{640, 480} : g_modernScreen; }

static void SyncView();

// A preset, Reset settings or an undo can cross between the one column and the wide layout
// in the middle of a frame; the view starts its scroll again at once, as at any crossing.
static void SetScreenTo(ScreenSize to) {
    if (SameScreen(to, ScreenNow()))
        return;
    if (Host_SetScreen(to.w, to.h, true))
        Menu_NoteScreen();
    SyncView();
}

// "Changed": the player's values differ from the chosen preset in a setting presets set,
// or a tuning value differs from its default (a preset puts every one back). The player's
// settings, on Options or on More options, and the screen do not mark the preset changed.
static bool PresetChanged() {
    for (int s = 0; s < S_COUNT; s++)
        if (PresetSets(s) && g_val[s] != PresetValue(g_preset, s))
            return true;
    return MoreOptions_PresetChanged();
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
    int exp[MORE_OPTIONS_MAX];
    int binds[BINDINGS_MAX];
    int preset;
    ScreenSize screen;
};
static Backup g_presetBackup;  // the preset page's Undo preset
static Backup g_resetBackup;   // the Advanced page's Undo reset

static bool CoversSetting(const Backup &b, int s) { return b.reset ? !IsLanguage(s) : PresetSets(s); }
static bool CoversExp(const Backup &b, int i) { return b.reset || MoreOptions_PresetsSet(i); }

static void TakeBackup(Backup &b, bool reset) {
    b.have = true;
    b.reset = reset;
    memcpy(b.val, g_val, sizeof g_val);
    MoreOptions_Get(b.exp);
    Bindings_Get(b.binds);
    b.preset = g_preset;
    b.screen = ScreenNow();
}

static const char *const kPresetBackupNames[2] = {"Modern preset", "Classic preset"};

// What the undo would change, in the order its help names it: the preset, the settings
// (Resolution for the screen), the bindings, the More options rows. Settings in the Text
// language, More options' rows by their console names. None: the undo row is not offered.
enum { BACKUP_DIFF_MAX = 1 + S_COUNT + 1 + MORE_OPTIONS_MAX };
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
        int now[BINDINGS_MAX];
        Bindings_Get(now);
        if (memcmp(b.binds, now, sizeof(int) * Bindings_Count()) != 0)
            names[n++] = Ui_Tr("Keys and buttons");
    }
    int now[MORE_OPTIONS_MAX];
    MoreOptions_Get(now);
    for (int i = 0; i < MoreOptions_Count(); i++)
        if (CoversExp(b, i) && b.exp[i] != now[i])
            names[n++] = MoreOptions_Name(i);
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
    int exp[MORE_OPTIONS_MAX];
    MoreOptions_Get(exp);
    for (int i = 0; i < MoreOptions_Count(); i++)
        if (CoversExp(b, i))
            exp[i] = b.exp[i];
    MoreOptions_Put(exp);
    if (b.reset)
        Bindings_Put(b.binds);
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
    MoreOptions_PresetDefaults();  // the tuning values; the player's rows on More options stay
    SetScreenTo(PresetScreen(p));
    Trace("apply preset=%s", kPresetNames[p]);
}

// Every setting but the languages back as a fresh install has it, every More options row,
// the bindings and the preset (the menu plan's Reset settings).
static void ResetSettings() {
    bool fresh = g_preset == 0 && !MoreOptions_AnyChanged() && Bindings_AreDefault();
    for (int s = 0; s < S_COUNT; s++)
        fresh = fresh && (IsLanguage(s) || g_val[s] == FreshValue(s));
    fresh = fresh && SameScreen(ScreenNow(), g_modernScreen);
    if (!fresh)  // a second reset with nothing changed keeps the first one's undo
        TakeBackup(g_resetBackup, true);
    for (int s = 0; s < S_COUNT; s++)
        if (!IsLanguage(s))
            g_val[s] = FreshValue(s);
    MoreOptions_Defaults();
    Bindings_PutDefaults();
    g_preset = 0;
    g_presetBackup.have = false;
    SetScreenTo(g_modernScreen);
    Trace("reset settings");
}

// ───────────────────────────── Pages as data ─────────────────────────────

enum { A_KEYS, A_RESET, A_EXP, A_CONSOLE };  // R_ACTION: opens Keys and buttons, asks to reset, opens More
                                              // options, opens the console
enum { BACK_PRESET, BACK_RESET };  // R_BACK_TO_MINE: undoes a preset, or a reset

// Switches (prototype aids; every review switch was pruned 2026-11-12, keeping what was decided).
static int sw_overlay = 0;
// Help typed on every cursor move (decided 2026-11-03); 1 shows it whole at once, for the self test.
static int sw_whole = 0;
// The chosen preset: "(current)" after its name always; 1 also gives it the green
// current-value plasma, as before (EPmager, 2026-11-11: green and the blue cursor read alike,
// so he asked for the words alone). F3; off at start, to show his proposal.
static int sw_greencurrent = 0;

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

static bool UndoPresetOffered() { return UndoOffered(g_presetBackup); }
static bool UndoResetOffered() { return UndoOffered(g_resetBackup); }
static bool AutoCamera() { return g_val[S_CAM_OUT] == 1; }  // Invert and speed only mean something with it

#define SETTING_ROW(s) {R_SETTING, NULL, s, NULL, NULL, NULL, NULL}
#define AUTO_CAMERA_ROW(s) {R_SETTING, NULL, s, NULL, NULL, AutoCamera, "Only with the Auto camera."}
#define RETURN_ROW {R_RETURN, "Return to previous menu", 0, "", NULL, NULL, NULL}

static const Row kHubRows[] = {
    {R_RETURN, "Return to the game", 0, "Back to where you were.", NULL, NULL, NULL},
    {R_PRESET_ROW, "", P_PRESET, "As in 1997, or today's look with bugs fixed.", NULL, NULL, NULL},
    {R_CATEGORY, "Language and interface", P_LANG, "Text, voices, the font and the background.", NULL, NULL, NULL},
    {R_CATEGORY, "Display and graphics", P_DISPLAY, "Resolution, window and detail.", NULL, NULL, NULL},
    {R_CATEGORY, "Sound", P_SOUND, "Volumes.", NULL, NULL, NULL},
    {R_CATEGORY, "Controls and camera", P_CONTROLS, "Keys, pad and the camera.", NULL, NULL, NULL},
    {R_CATEGORY, "Advanced and reset", P_ADVANCED, "The rules switch, experiments and a fresh start.", NULL, NULL,
     NULL},
};

static const Row kPresetRows[] = {
    RETURN_ROW,
    {R_PRESET_CHOICE, "Modern", 0, "Today's look, cameras that follow, bugs fixed.", NULL, NULL, NULL},
    {R_PRESET_CHOICE, "Classic", 1, "The 1997 look and cameras, bugs and all.", NULL, NULL, NULL},
    {R_BACK_TO_MINE, "Undo preset", BACK_PRESET, "Puts back the settings you had before you chose a preset.",
     UndoPresetOffered, NULL, NULL},
};

static const Row kLangRows[] = {
    RETURN_ROW, SETTING_ROW(S_TEXT_LANG), SETTING_ROW(S_VOICE_LANG), SETTING_ROW(S_SUBTITLES),
    SETTING_ROW(S_BACKGROUND), SETTING_ROW(S_FONT),
};

static const Row kDisplayRows[] = {
    RETURN_ROW, SETTING_ROW(S_RESOLUTION), SETTING_ROW(S_FULLSCREEN), SETTING_ROW(S_VSYNC),
    SETTING_ROW(S_FPS), SETTING_ROW(S_IMAGE),
};

static const Row kSoundRows[] = {
    RETURN_ROW, SETTING_ROW(S_MASTER), SETTING_ROW(S_SFX), SETTING_ROW(S_VOICES), SETTING_ROW(S_MUSIC),
    SETTING_ROW(S_REVERSE),
};

static const Row kControlsRows[] = {
    RETURN_ROW,
    {R_ACTION, "Keys and buttons", A_KEYS, "Change which key or button does what.", NULL, NULL, NULL},
    SETTING_ROW(S_CAM_OUT),
    SETTING_ROW(S_CAM_IN),
    SETTING_ROW(S_MOVIE_CAM),
    AUTO_CAMERA_ROW(S_INVERT),
    AUTO_CAMERA_ROW(S_SENS),
};

static const Row kAdvancedRows[] = {
    RETURN_ROW,
    SETTING_ROW(S_FIXBUGS),
    // Always offered (decided 2026-11-03): its help carries the warning a switch once did.
    // The page holds every console setting without a row of its own: tuning for finished
    // features and the few unfinished ones, which it marks experimental.
    // Help within 750 px (2026-11-12); an experimental row's own help says it is experimental.
    {R_ACTION, "More options", A_EXP, "Tuning and 1997 extras, in English.", NULL, NULL, NULL},
    // The row's value is the version, so a bug report can name the build from the menu.
    {R_ACTION, "Console", A_CONSOLE, "Messages, and commands from a keyboard.", NULL, NULL, NULL},
    {R_ACTION, "Reset settings", A_RESET, "Puts every setting back as a fresh install has it.", NULL, NULL, NULL},
    {R_BACK_TO_MINE, "Undo reset", BACK_RESET, "Puts back what you had before the reset.", UndoResetOffered, NULL,
     NULL},
};

static const Row kFirstRows[] = {
    {R_PRESET_CHOICE, "Modern", 0, "Today's look, bugs fixed. You can change this later in the Options Menu.", NULL,
     NULL, NULL},
    {R_PRESET_CHOICE, "Classic", 1, "As in 1997, bugs and all. You can change this later in the Options Menu.", NULL,
     NULL, NULL},
};

// A fixture: today's Options page from the boot menu, to diff against lba2cc's
// `ui menu-main-options` capture.
static const Row kOriginalRows[] = {
    RETURN_ROW,
    {R_ACTION, "Display", 0, "", NULL, NULL, NULL},
    {R_ACTION, "Sound volume", 0, "", NULL, NULL, NULL},
    {R_ACTION, "Choose language", 0, "", NULL, NULL, NULL},
    {R_ACTION, "Advanced options", 0, "", NULL, NULL, NULL},
    {R_ACTION, "Controls", 0, "", NULL, NULL, NULL},
};

// The Options pages; Keys and buttons and its groups are filled in from bindings.cpp's tables.
#define PAGE(name, rows) {name, rows, (int)(sizeof rows / sizeof rows[0])}
static Page g_pages[P_COUNT] = {
    PAGE("Options", kHubRows),
    PAGE("Preset", kPresetRows),
    PAGE("Language and interface", kLangRows),
    PAGE("Display and graphics", kDisplayRows),
    PAGE("Sound", kSoundRows),
    PAGE("Controls and camera", kControlsRows),
    PAGE("Advanced and reset", kAdvancedRows),
    PAGE("First launch", kFirstRows),
    PAGE("original", kOriginalRows),
};
#undef PAGE

static bool g_first;       // first-launch screen

static const char *RowLabel(const Row &r) {
    if (r.type == R_SETTING)
        return kSettings[r.arg].label;
    if (r.type == R_PRESET_CHOICE && !g_first && r.arg == g_preset) {
        static char marked[2][64];  // two rows can be drawn before the first is used
        static int next;
        char *out = marked[next++ & 1];
        snprintf(out, sizeof marked[0], "%s (current)", r.label);  // Ui_Tr takes the parts apart
        return out;
    }
    return r.label;
}
static const char *RowHelp(const Row &r) { return r.type == R_SETTING ? kSettings[r.arg].help : r.help; }
static bool RowActive(const Row &r) { return !r.active || r.active(); }

static const char *PresetRowLabel(char *buf, size_t n) {
    return FormatPresetRow(buf, n, kPresetNames[g_preset], PresetChanged());
}

static const char *ValueText(int s, char *buf, size_t n) {
    const Setting &st = kSettings[s];
    if (s == S_RESOLUTION)
        snprintf(buf, n, "%u x %u", (unsigned)Host_PhysW(), (unsigned)Host_PhysH());
    else if (s == S_FULLSCREEN)
        snprintf(buf, n, "%s", st.names[Host_Fullscreen() ? 1 : 0]);
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

// Where the player is: one stack of pages, each with the row its cursor is on, the same in
// every layout (the menu plan's page model). A row is its index in the page's table, never a
// position on the screen, so a layout change or a row that comes or goes leaves it in place.
struct Place {
    int page;
    int row;
};
static Place g_stack[8];
static int g_depth;
static int g_scroll;       // the first shown row drawn: the view's, counted as its layout shows rows
static bool g_confirm;     // a question is up: Classic, or Reset settings
static int g_confirmCursor;
enum Question { Q_CLASSIC, Q_RESET };
static Question g_question;

// A question's title (in place of the page's, so the screen says it is a question:
// EPmager, 2026-11-11), what it says in the voice, and its answers: the safe one first.
struct QuestionText {
    const char *title;
    const char *voice;
    const char *answers[2];
};
static const QuestionText kQuestions[2] = {
    {"Confirm Classic preset", "Play as in 1997? Sound and keys stay as they are.", {"No, keep Modern", "Yes, play Classic"}},
    {"Confirm settings reset", "Put every setting back as a fresh install has it?", {"No, keep my settings", "Yes, reset settings"}},
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
    return Host_RuleChoseWide();
}

WideFrame Menu_WideFrame() {
    S32 W = (S32)Host_LogicalW(), H = (S32)Host_LogicalH();
    WideFrame f = {(W - 960) / 2, (H - 540) / 2, 960, 540};
    return f;
}

static bool Wide() { return Menu_Wide(); }

static void Toast(const char *fmt, const char *a, int v) {
    snprintf(g_toast, sizeof g_toast, fmt, a, v);
    g_toastUntil = Host_TimeMs() + 2000;
}

void Menu_Toast(const char *text) { Toast("%s", text, 0); }

void Menu_NoteScreen() {
    char note[64];
    snprintf(note, sizeof note, "%ux%u (logical %ux%u)%s", (unsigned)Host_PhysW(), (unsigned)Host_PhysH(),
             (unsigned)Host_LogicalW(), (unsigned)Host_LogicalH(), Host_Fullscreen() ? ", fullscreen" : "");
    Toast("%s", note, 0);
    Trace("screen %s", note);
}

static int TopPage() { return g_stack[g_depth - 1].page; }

// A row as the layout shows it: a wide page has no Return row (Esc goes back), and a row
// with a "shown when" check is there only while it holds.
static bool RowShown(const Page &p, int i) {
    const Row &r = p.rows[i];
    if (r.type == R_RETURN && Wide() && !g_first)
        return false;
    return !r.shown || r.shown();
}

enum { PAGE_ROWS_MAX = MORE_OPTIONS_MAX };  // More options is the longest page

// The rows a page shows, in order: their indices in its table.
static int ShownRows(int page, int *out) {
    const Page &p = g_pages[page];
    assert(p.count <= PAGE_ROWS_MAX && "PAGE_ROWS_MAX holds every page's rows");
    int n = 0;
    for (int i = 0; i < p.count; i++)
        if (RowShown(p, i))
            out[n++] = i;
    return n;
}

// Where a row is among the rows its page shows: 0 the first; -1 when it is not shown.
static int Position(int page, int row) {
    int shown[PAGE_ROWS_MAX];
    const int n = ShownRows(page, shown);
    for (int i = 0; i < n; i++)
        if (shown[i] == row)
            return i;
    return -1;
}

// A row the cursor can be on: the row itself when shown, else the next one shown, else the
// one before (an undo row that went, after the undo).
static int ResolveRow(int page, int row) {
    const Page &p = g_pages[page];
    if (row >= 0 && row < p.count && RowShown(p, row))
        return row;
    for (int i = row + 1; i < p.count; i++)
        if (RowShown(p, i))
            return i;
    for (int i = (row < p.count ? row : p.count) - 1; i >= 0; i--)
        if (RowShown(p, i))
            return i;
    return 0;
}

// The top of the stack, its row checked as it is read: nothing has to remember to move the
// cursor off a row that went.
static Place &Top() {
    Place &t = g_stack[g_depth - 1];
    t.row = ResolveRow(t.page, t.row);
    assert(RowShown(g_pages[t.page], t.row) && "the cursor is on a row its page shows");
    return t;
}

static const Row *CursorRow() {
    if (g_confirm)
        return NULL;
    const Place &t = Top();
    return &g_pages[t.page].rows[t.row];
}

int Menu_TopPage() { return g_depth ? TopPage() : -1; }
int Menu_TopRow() { return Top().row; }
int &Menu_Scroll() { return g_scroll; }

static void LegendFor(const Row *r, const char **keys, const char **words, int *n);
static S32 LegendWidth(const char **keys, const char **words, int n, S32 scale);

// Whether a line for the row under the cursor fits the voice it will be said in:
// the band beside the legend, the help line, or the 640x480 dialogue window.
static S32 BandVoiceWidth(const char **keys, const char **words, int nk);

static bool VoiceFits(const char *text, const Row *r) {
    S32 W = (S32)Host_LogicalW();
    if (!Wide())
        return Voice_Lines(text, W - 49) <= 2;  // the legend has the dialogue window's third line
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
    if (!RowActive(*r))
        return r->needs;
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
        return RowHelp(*r);
    }
    return RowHelp(*r) ? RowHelp(*r) : "";
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
    {"Does what the behavior does: jump, hit or hide.", "Jumps, hits or hides, by behavior."},
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
    if (Bindings_Active()) {
        Bindings_Say();
        return;
    }
    if (MoreOptions_Active()) {
        MoreOptions_Say();
        return;
    }
    const Row *r = CursorRow();
    const char *text = g_confirm ? kQuestions[g_question].voice : HelpFor(r);
    if (text == g_helpBuf)
        Menu_SayAsIs(text);
    else
        Menu_Say(text);
}

static void Push(int page) {
    g_stack[g_depth].page = page;
    g_stack[g_depth].row = ResolveRow(page, 0);  // the first row shown: 1997's sub-menus open on their Return row
    g_depth++;
    g_scroll = 0;
    Trace("open page=%s", g_pages[page].name);
}

// The layout the view was last made in. The place is the same in every layout; only the
// scroll, which counts rows as a layout shows them, starts again when the layout changes.
static int g_layoutWide = -1;

static void SyncView() {
    const int wide = Wide() ? 1 : 0;
    if (g_layoutWide == wide)
        return;
    const int was = g_layoutWide;
    g_layoutWide = wide;
    if (was < 0 || g_first)
        return;
    g_scroll = 0;
    const Place &t = Top();
    Trace("layout %s page=%s cursor=%d", wide ? "wide" : "column", g_pages[t.page].name, Position(t.page, t.row));
}

static void Pop() {
    if (g_depth > 1) {
        Trace("close page=%s", g_pages[TopPage()].name);
        g_depth--;
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
    Bindings_Pages(g_pages);
    MoreOptions_Pages(g_pages);
    g_first = firstLaunch;
    g_depth = 1;
    g_stack[0].page = g_first ? P_FIRST : P_HUB;
    g_stack[0].row = g_first ? 0 : 1;  // the preset row, as the mock draws it
    g_voice.text[0] = 0;
    g_layoutWide = Wide() ? 1 : 0;
    Trace("init layout=%s logical=%ux%u first=%d", Wide() ? "wide" : "column",
          (unsigned)Host_LogicalW(), (unsigned)Host_LogicalH(), (int)g_first);
}

static void ChangeSetting(int s, int dir) {
    if (s == S_RESOLUTION) {
        if (Host_StepScreen(dir))
            Menu_NoteScreen();
        return;
    }
    if (s == S_FULLSCREEN) {
        if (Host_SetFullscreen(!Host_Fullscreen()))
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
        Push(r.arg);
        if (r.arg >= P_KEY_GROUP && r.arg < P_KEY_GROUP + KEY_GROUPS)
            Bindings_GroupOpened();
        break;
    case R_BINDING:
        Bindings_Enter();  // waits for a key, or says the chip is fixed
        break;
    case R_TUNING:
        MoreOptions_Enter(r.arg);
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
            g_depth = 1;
            g_stack[0].page = P_HUB;
            g_stack[0].row = 1;  // on the preset row
            Trace("first launch chose=%s", kPresetNames[r.arg]);
            break;
        }
        if (r.arg == 1 && g_preset != 1)
            Ask(Q_CLASSIC);
        else
            ApplyPreset(r.arg);
        break;
    case R_BACK_TO_MINE:
        RestoreBackup(r.arg == BACK_RESET ? g_resetBackup : g_presetBackup);
        Trace("back to my settings");
        break;
    case R_ACTION:
        if (r.arg == A_RESET)
            Ask(Q_RESET);
        else if (r.arg == A_EXP)
            Push(P_MORE);
        else if (r.arg == A_CONSOLE) {
            g_console = Host_LastInputWasPad() ? CON_READONLY : CON_FULL;
            Trace("open console mode=%s", g_console == CON_READONLY ? "read-only" : "full");
        }
        else {
            Bindings_Open();
            Push(P_KEYS);
            Top().row = 1;  // on its first group, not its Return row
        }
        break;
    }
}

static void Move(int dir) {
    Place &t = Top();
    int shown[PAGE_ROWS_MAX];
    const int n = ShownRows(t.page, shown);
    if (n <= 0)
        return;
    t.row = shown[(Position(t.page, t.row) + dir + n) % n];
    const Row *r = CursorRow();
    Trace("cursor %s", r ? (r->type == R_PRESET_ROW ? "preset row" : RowLabel(*r)) : "-");
}

// The cursor on the top page's row of that kind, as Enter on it leaves it.
static void PutCursorOn(RowType type, int arg) {
    Place &t = g_stack[g_depth - 1];
    const Page &p = g_pages[t.page];
    for (int i = 0; i < p.count; i++) {
        if (p.rows[i].type == type && p.rows[i].arg == arg) {
            t.row = i;
            return;
        }
    }
}

void Menu_Update() {
    SyncView();
    if (Bindings_Busy()) {  // waiting for a key, or asking about one another action has
        Bindings_UpdateBusy();
        return;
    }
    if (g_console) {
        // Full: Esc closes (F12 in the game; F12 is a prototype switch here). Read-only: anything does.
        bool any = Host_RawPressed() != 0;
        for (int k = 0; k < KEY_COUNT && !any; k++)
            any = Host_Pressed((UiKey)k);
        if (g_console == CON_READONLY ? any : Host_Pressed(KEY_ESC)) {
            g_console = CON_CLOSED;
            Trace("close console");
        }
        return;
    }
    if (g_closed) {
        if (Host_Pressed(KEY_ENTER)) {
            g_closed = false;
            Trace("reopen options");
        } else if (Host_Pressed(KEY_ESC)) {
            g_quit = true;
            Trace("quit");
        }
        return;
    }
    if (g_confirm) {
        if (Host_Pressed(KEY_UP) || Host_Pressed(KEY_DOWN)) {
            g_confirmCursor ^= 1;
            Trace("cursor %s", g_confirmCursor ? "Yes" : "No");
        }
        if (Host_Pressed(KEY_ESC)) {
            g_confirm = false;
            Trace("answer no (esc)");
        }
        if (Host_Pressed(KEY_ENTER)) {
            g_confirm = false;
            Trace("answer %s", g_confirmCursor ? "yes" : "no");
            if (g_confirmCursor) {
                if (g_question == Q_RESET)
                    ResetSettings();
                else
                    ApplyPreset(1);
            }
        }
        UpdateVoice();
        return;
    }

    if (Bindings_Active() && Host_Pressed(KEY_TAB))
        Bindings_SwitchDevice();
    if (Host_Pressed(KEY_UP))
        Move(-1);
    if (Host_Pressed(KEY_DOWN))
        Move(+1);
    if (TopPage() == P_MORE) {  // a long table: Page Up and Page Down, stopping at its ends
        const int step = MoreOptions_PageStep(), last = g_pages[P_MORE].count - 1;
        Place &t = Top();
        if (Host_Pressed(KEY_PGUP))
            t.row = t.row > step ? t.row - step : 0;
        if (Host_Pressed(KEY_PGDN))
            t.row = t.row + step < last ? t.row + step : last;
    }

    const Row *r = CursorRow();
    if (r && r->type == R_SETTING && RowActive(*r)) {
        if (Host_Pressed(KEY_LEFT))
            ChangeSetting(r->arg, -1);
        if (Host_Pressed(KEY_RIGHT))
            ChangeSetting(r->arg, +1);
    } else if (r && r->type == R_BINDING) {
        // Left and Right go along the row's chips; wide, Left from the first goes back.
        if (Host_Pressed(KEY_RIGHT))
            Bindings_StepColumn(+1);
        if (Host_Pressed(KEY_LEFT) && !Bindings_StepColumn(-1) && Wide())
            Pop();
    } else if (r && r->type == R_TUNING) {
        MoreOptions_Keys(r->arg);  // Left and Right change the value, Del puts its default back
    } else if (Wide() && !g_first && r && (r->type == R_CATEGORY || r->type == R_PRESET_ROW) &&
               Host_Pressed(KEY_RIGHT)) {
        Activate(*r);  // wide, Right opens what Enter opens: a category, the preset page, a group
    }
    r = CursorRow();
    if (r && Host_Pressed(KEY_ENTER) && RowActive(*r))
        Activate(*r);
    r = CursorRow();
    if (r && r->type == R_BINDING && Host_Pressed(KEY_DEL))
        Bindings_Clear();

    if (Host_Pressed(KEY_ESC)) {
        if (g_first)
            Activate(g_pages[P_FIRST].rows[0]);  // Escape answers Modern
        else
            Pop();
    }

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
    const ClipSave clip;
    SetClip(x0 + 1, yc - 25, x1 - 1, yc + 25);
    if (!asIs)
        s = Ui_Tr(s);
    if (Ui_TextWidthAsIs(s) > x1 - x0 - 16)
        Ui_TextInRoom(x0 + 8, x1 - 8, yc - 18, s, colour, cursor);  // 1997: x0 + 8, swinging
    else
        Ui_TextAsIs((x0 + x1) / 2 - Ui_TextWidthAsIs(s) / 2, yc - 18, s, colour);
    clip.Restore();
}

// One column: the 1997 row, "Label: Value" in one string, sliders as fills.
static void DrawColumnRow(const Row &r, S32 x0, S32 y0, S32 x1, S32 y1, bool cursor) {
    S32 yc = (y0 + y1) / 2;
    char text[160], val[48];
    RowFill fill = cursor ? FILL_CURSOR : FILL_SHADE;
    bool active = RowActive(r);

    if (sw_greencurrent && r.type == R_PRESET_CHOICE && !g_first && r.arg == g_preset && !cursor)
        fill = FILL_CURRENT;  // the chosen preset wears the current-value plasma

    bool slider = r.type == R_SETTING && (kSettings[r.arg].kind == K_SLIDER || kSettings[r.arg].kind == K_STEPS);
    if (slider && cursor && active) {
        // GAMEMENU.CPP: the slider plasma up to the value, fill-selected after it.
        const Setting &st = kSettings[r.arg];
        S32 x2 = x0 + (x1 - x0) * g_val[r.arg] / st.max;
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
    else if (r.type == R_SETTING && kSettings[r.arg].kind != K_SLIDER)
        snprintf(text, sizeof text, "%s: %s", RowLabel(r), ValueText(r.arg, val, sizeof val));
    else if (r.type == R_ACTION && r.arg == A_CONSOLE)
        snprintf(text, sizeof text, "%s: %s", RowLabel(r), kVersion);
    else
        snprintf(text, sizeof text, "%s", RowLabel(r));
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
    const ClipSave clip;
    SetClip(x0 + 1, y0, x1 - 1, y1);
    Ui_TextInRoom(x0 + 16, valueLeft ? valueLeft - 17 - 8 - 1 : x1 - 16, yc - 18, Ui_Tr(label),
                  active ? INK_WHITE : INK_DISABLED, fill == FILL_CURSOR);
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
        keys[*n] = "Esc"; words[(*n)++] = "Cancel";  // the safe answer, without moving to it
        return;
    }
    if (r) {
        if (r->type == R_SETTING && RowActive(*r)) {
            Kind k = kSettings[r->arg].kind;
            // A choice or a switch also steps on Enter, so the legend names both (EPmager,
            // 2026-11-11); a slider or steps take Left and Right only.
            const bool steps = k == K_SLIDER || k == K_STEPS;
            keys[*n] = steps ? "\x11\x10" : "\x11\x10" CHIP_AND "Enter";
            words[(*n)++] = steps ? "Adjust" : "Change";
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
        S32 c = Ui_ChipWidth(keys[i], scale), t = Ui_WordWidth(LegendWord(words[i]), scale);
        chipW = c > chipW ? c : chipW;
        wordW = t > wordW ? t : wordW;
    }
    return n ? chipW + 4 * scale + wordW : 0;
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

static void LegendRecord(int i, const char *key, const char *word) {
    char item[64], k[32];
    KeyText(key, k, sizeof k);
    snprintf(item, sizeof item, "%s[%s] %s", i ? "  " : "", k, word);
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
        Ui_Word(x + chipW + 4 * scale, y + chipH / 2 - 4 * scale, LegendWord(words[i]), INK_CONFIG_MENU, scale);
        LegendRecord(i, keys[i], words[i]);
    }
}

// The question's answers, the safe one first; the question itself is in the voice.
// One column: a small panel in the middle, labels centred, as 1997. Wide: in place
// of the page's rows, labels on their label edge.
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
            const ClipSave clip;
            SetClip(x0 + 1, ry0, x1 - 1, ry0 + ROW_H);
            Ui_TextInRoom(x0 + 16, x1 - 16, yc - 18, Ui_Tr(answers[i]), INK_WHITE, i == g_confirmCursor);
            clip.Restore();
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

bool Menu_CursorEnterSteps(bool *isValue) {
    *isValue = false;
    if (!g_depth || g_confirm || g_console || Bindings_Active())
        return false;
    if (MoreOptions_Active()) {
        *isValue = true;
        return !MoreOptions_CursorIsNumber();
    }
    const Row *r = CursorRow();
    if (!r || r->type != R_SETTING || !RowActive(*r))
        return false;
    *isValue = true;
    const Kind k = kSettings[r->arg].kind;
    return k == K_CHOICE || k == K_TOGGLE;
}

S32 Menu_DialogueTop() { return (S32)Host_LogicalH() - 1 - 137; }

void Menu_DrawDialogue(const char **keys, const char **words, int nk) {
    // MESSAGE.CPP NormalWinDial: 8 in from the sides, 137 tall, 8 off the bottom.
    S32 W = (S32)Host_LogicalW(), H = (S32)Host_LogicalH();
    Menu_DrawDialogueIn(8, H - 1 - 137, W - 1 - 8, H - 1 - 8, keys, words, nk);
}

// In a row: chip, word, then the next, from x; returns the width.
static S32 DrawLegendRow(S32 x, S32 y, const char **keys, const char **words, int n, S32 scale, bool draw) {
    if (draw)
        g_legend[0] = 0;
    S32 w = 0;
    for (int i = 0; i < n; i++) {
        if (i)
            w += 12 * scale;
        S32 c = Ui_ChipWidth(keys[i], scale);
        if (draw) {
            Ui_Chip(x + w, y, keys[i], scale);
            Ui_Word(x + w + c + 4 * scale, y + 2 * scale, LegendWord(words[i]), INK_CONFIG_MENU, scale);
            LegendRecord(i, keys[i], words[i]);
        }
        w += c + 4 * scale + Ui_WordWidth(LegendWord(words[i]), scale);
    }
    return w;
}

// The legend's keys as the device used last names them (a pad's buttons after a pad press).
static void LegendKeys(const char **in, const char **out, int nk) {
    for (int i = 0; i < nk && i < 8; i++)
        out[i] = Bindings_LegendKey(in[i]);
}

void Menu_DrawDialogueIn(S32 x0, S32 y0, S32 x1, S32 y1, const char **keysIn, const char **words, int nk) {
    Ui_Panel(x0, y0, x1, y1, PANEL_SHADE_LVL);
    // The legend takes the window's third line, so the help has two, as in the wide band;
    // no help at 640x480 needs a third (measured 2026-11-11, six languages).
    const S32 maxLines = nk ? 2 : 3;
    g_voiceLines = Voice_Draw(g_voice, x0 + 16, y0 + 12, x1 - x0 - 32, maxLines, &g_voiceNeeded);
    g_voiceMax = maxLines;
    if (nk) {
        const char *keys[8];
        LegendKeys(keysIn, keys, nk);
        // The 8x8 font at 2x when the line fits, as the wide band has it; 1x, the 1997 key
        // screen's size, when it does not.
        S32 scale = 2;
        if (DrawLegendRow(0, 0, keys, words, nk, scale, false) > x1 - x0 - 32)
            scale = 1;
        S32 w = DrawLegendRow(0, 0, keys, words, nk, scale, false);
        DrawLegendRow(x1 - 16 - w + 1, y1 - 8 - 12 * scale + 1, keys, words, nk, scale, true);
    } else {
        g_legend[0] = 0;
    }
    DrawPageMark(x1, y1);  // the window's own corner, where MESSAGE.CPP AffFleche puts its arrow
    g_helpBox.shown = true;
    g_helpBox.kind = "dialogue";
    g_helpBox.x0 = x0; g_helpBox.y0 = y0; g_helpBox.x1 = x1; g_helpBox.y1 = y1;
}

static void DrawColumn() {
    S32 W = (S32)Host_LogicalW(), H = (S32)Host_LogicalH();
    const Place &t = Top();
    const Page &p = g_pages[t.page];
    int shown[PAGE_ROWS_MAX];
    const int count = ShownRows(t.page, shown);
    const int cursor = Position(t.page, t.row);

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
        if (visible > count)
            visible = count;
        if (g_scroll > count - visible)
            g_scroll = count - visible;  // a row went (Undo reset): no rows past the end
        if (cursor < g_scroll)
            g_scroll = cursor;
        if (cursor >= g_scroll + visible)
            g_scroll = cursor - visible + 1;
        S32 stackH = visible * ROW_PITCH - ROW_GAP;
        S32 y = top + (bottom - top - stackH) / 2;  // centred above the dialogue window
        if (g_first)
            y = oy + 168;
        // The 1997 width: LargeurMenu 550, centred.
        S32 x0 = W / 2 - 275, x1 = W / 2 + 275;
        for (int i = g_scroll; i < g_scroll + visible; i++) {
            DrawColumnRow(p.rows[shown[i]], x0, y, x1, y + ROW_H, i == cursor);
            y += ROW_PITCH;
        }
        if (visible < count)
            Menu_DrawScrollBar(x1 + 9, y - visible * ROW_PITCH, y - visible * ROW_PITCH + stackH - 1, g_scroll, visible,
                          count);
    }
    const char *keys[4], *words[4];
    int nk;
    LegendFor(g_confirm ? NULL : CursorRow(), keys, words, &nk);
    if (framed)
        Menu_DrawDialogueIn(fr.x0 + 32, fr.y0 + fr.h - 25 - 137, fr.x0 + fr.w - 33, fr.y0 + fr.h - 25, keys, words, nk);
    else
        Menu_DrawDialogue(keys, words, nk);
}

// Wide: one page at a time, centred and as wide as the 1997 canvas: the hub, then a page,
// as the 1997 menu goes. A page names itself in a title; the hub has none.
static void DrawWide() {
    WideFrame f = Menu_WideFrame();
    const S32 top = f.y0 + 24;
    const S32 x0 = f.x0 + (f.w - DRILL_W) / 2, x1 = x0 + DRILL_W - 1;
    const Place &t = Top();
    const Page &page = g_pages[t.page];

    if (g_confirm) {
        // The question's answers in place of the rows.
        DrawConfirm("wide", x0, x1, Ui_Title(x0, x1, top, kQuestions[g_question].title));
    } else {
        int shown[PAGE_ROWS_MAX];
        const int n = ShownRows(t.page, shown);
        const int cursor = Position(t.page, t.row);
        S32 rowsTop = top;
        if (g_depth > 1) {
            rowsTop = Ui_Title(x0, x1, top, page.name);
            Menu_Record("title", page.name, "", FILL_SHADE, x0, top, x1, top + 38).textLeft =
                (x0 + x1 + 1) / 2 - Ui_TextWidth(page.name) / 2;
        }
        int visible = (Menu_WideRowsLimit() - rowsTop + ROW_GAP) / ROW_PITCH;
        if (visible > n)
            visible = n;
        if (g_scroll > n - visible)
            g_scroll = n - visible;  // a row went (Undo reset): no rows past the end
        if (cursor < g_scroll)
            g_scroll = cursor;
        if (cursor >= g_scroll + visible)
            g_scroll = cursor - visible + 1;
        S32 y = rowsTop;
        for (int i = g_scroll; i < g_scroll + visible; i++) {
            const Row &row = page.rows[shown[i]];
            RowFill fill = FILL_SHADE;
            if (i == cursor)
                fill = FILL_CURSOR;
            else if (sw_greencurrent && row.type == R_PRESET_CHOICE && !g_first && row.arg == g_preset)
                fill = FILL_CURRENT;  // the chosen preset wears the current-value plasma, as in the column
            DrawWideRow("wide", row, x0, y, x1, y + ROW_H, fill);
            y += ROW_PITCH;
        }
        if (visible < n)
            Menu_DrawScrollBar(x1 + 9, rowsTop, rowsTop + visible * ROW_PITCH - ROW_GAP - 1, g_scroll, visible, n);
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
        keys[i] = Bindings_LegendKey(keysIn[i]);
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
        "F3 green plasma on the chosen preset: off / on",
        "F11 pad names: connected / Xbox / PlayStation / Nintendo",
    };
    S32 n = (S32)(sizeof lines / sizeof lines[0]);
    S32 w = 8 * 50 + 16, h = n * 12 + 12;
    S32 x0 = (S32)Host_LogicalW() - w - 8, y0 = 8;
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
    const int want = 1 + g_val[S_IMAGE];
    if (want != Host_BackgroundRes())
        Host_SetBackgroundRes(want);
}

void Menu_Draw() {
    SyncBackgroundRes();
    ScreenFont_Set(g_val[S_FONT]);  // before the layout: widths follow the font
    Ui_SetLang(g_val[S_TEXT_LANG]);  // and the language
    ScreenFont_BeginFrame();
    SyncView();
    g_drawnCount = 0;
    g_helpBox.shown = false;
    g_helpBox.x0 = g_helpBox.y0 = g_helpBox.x1 = g_helpBox.y1 = 0;  // where it was is no help on a new screen
    g_voiceLines = g_voiceNeeded = g_voiceMax = g_voiceWidth = 0;
    g_legend[0] = 0;

    Host_SetBackdrop(g_val[S_BACKGROUND]);
    Host_DrawBackground();
    GamePieces_AnimatePlasma();

    if (g_closed) {
        // Two lines at the voice's pitch: on one, 784 wide, it would not fit 640.
        const char *lines[2] = {"Options closed.", "<ENTER> opens them again. <ESC> quits."};
        for (int i = 0; i < 2; i++)
            Ui_TextShadow((S32)Host_LogicalW() / 2 - Ui_TextWidth(lines[i]) / 2,
                          (S32)Host_LogicalH() / 2 - 38 + i * 38, lines[i], DIALOGUE_HERO * 16 + 12);
    } else if (Bindings_Active()) {
        Bindings_Draw();
    } else if (MoreOptions_Active()) {
        MoreOptions_Draw();
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
    if (g_toast[0] && Host_TimeMs() < g_toastUntil) {
        S32 w = Ui_SmallWidth(g_toast, 1);
        S32 x = (S32)Host_LogicalW() - w - 12, yy = (S32)Host_LogicalH() - 14;
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
    {"greencurrent", &sw_greencurrent, 0, 1, 0},
};

bool Menu_SetSwitch(const char *name, int v) {
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
    SyncView();
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
    if (!strcmp(name, "Console")) {
        Menu_Open("Advanced and reset");
        PutCursorOn(R_ACTION, A_CONSOLE);
        g_console = Host_LastInputWasPad() ? CON_READONLY : CON_FULL;
        return true;
    }
    g_console = CON_CLOSED;
    Bindings_Close();
    for (int p = 0; p < P_KEYS; p++) {  // Keys and buttons opens from its row, above
        if (strcmp(g_pages[p].name, name) != 0)
            continue;
        // Esc comes back to the page's own row on the hub, as after Enter on it.
        int hubRow = 0;
        for (int i = 0; i < g_pages[P_HUB].count; i++)
            if (g_pages[P_HUB].rows[i].type != R_RETURN && g_pages[P_HUB].rows[i].arg == p)
                hubRow = i;
        g_first = false;
        g_confirm = false;
        g_closed = false;
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
        if (strcmp(kSettings[s].id, id) != 0)
            continue;
        if (value < 0 || value > kSettings[s].max)
            return false;
        g_val[s] = value;
        Trace("set %s=%d (control)", id, value);
        return true;
    }
    return MoreOptions_Set(id, value);
}

void Menu_Reset() {
    for (size_t i = 0; i < sizeof kSwitches / sizeof kSwitches[0]; i++)
        *kSwitches[i].value = kSwitches[i].start;
    Host_ResetInput();
    Bindings_Reset();
    MoreOptions_Defaults();
    g_confirm = false;
    g_confirmCursor = 0;
    g_closed = false;
    g_console = CON_CLOSED;
    g_presetBackup.have = false;
    g_resetBackup.have = false;
    g_scroll = 0;
    g_toast[0] = 0;
    g_toastUntil = 0;
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
    return g_pages[Top().page].name;
}

// One for the hub; each page opened and a question add one.
static int Depth() {
    if (g_first)
        return 1;
    return g_depth + (g_confirm ? 1 : 0);
}

const char *Menu_CursorLabel() {
    static char buf[160];
    const Row *r = MoreOptions_Active() || g_closed ? NULL : CursorRow();
    if (!r)
        return "";
    return r->type == R_PRESET_ROW ? PresetRowLabel(buf, sizeof buf) : RowLabel(*r);
}

int Menu_Where(char *buf, size_t n) {
    if (g_closed) {
        snprintf(buf, n, "closed");
        return 0;
    }
    if (Bindings_Active()) {
        // One name for the page and its groups' pages, which the inner part names.
        char inner[200];
        int d = Bindings_Where(inner, sizeof inner);
        snprintf(buf, n, "%s | %s", g_pages[P_KEYS].name, inner);
        return Depth() + (d == 3 ? 1 : 0);  // waiting for a key or asking: one more than its page
    }
    if (MoreOptions_Active()) {
        char inner[200];
        MoreOptions_Where(inner, sizeof inner);
        snprintf(buf, n, "%s | %s", Menu_PageName(), inner);
        return Depth();
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
    const Row *r = MoreOptions_Active() || g_closed ? NULL : CursorRow();
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
            snprintf(label, sizeof label, "%s", RowLabel(r));
        }
        fprintf(f, "%s{\"label\":\"%s\",\"w\":%d,\"help_w\":%d}", i ? "," : "", label, Ui_TextWidth(label),
                Ui_TextWidth(RowHelp(r) ? RowHelp(r) : ""));
    }
    fprintf(f, "]}\n");
}

void Menu_WriteState(FILE *f) {
    SyncView();
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
            g_confirm ? "true" : "false");
    const Row *r = CursorRow();
    fprintf(f, ",\"cursor_row\":");
    Menu_JsonString(f, r ? (r->type == R_PRESET_ROW ? PresetRowLabel(buf, sizeof buf) : RowLabel(*r)) : "");
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
    fprintf(f, ",\"console\":\"%s\",\"console_bottom\":%d,\"version\":", g_console == CON_FULL ? "full" : g_console == CON_READONLY ? "read-only" : "closed",
            g_console ? (int)g_consoleBottom : -1);
    Menu_JsonString(f, kVersion);

    if (Bindings_Active()) {
        fprintf(f, ",\"bindings\":");
        Bindings_WriteState(f);
    }
    if (MoreOptions_Active()) {
        fprintf(f, ",\"more_options\":");
        MoreOptions_WriteState(f);
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
