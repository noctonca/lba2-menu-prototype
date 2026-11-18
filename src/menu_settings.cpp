// The prototype's settings and presets: example values held in memory, nothing written.
// What the menu asks of them is in menu_settings.h; lba2cc answers the same calls from its
// settings tables.
#include "menu_settings.h"

#include "bindings.h"
#include "host.h"
#include "menu.h"
#include "menu_parts.h"
#include "trace.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>





// Example values only: the prototype's settings live in memory.
static const Setting kSettings[S_COUNT] = {
    {"text_lang", "Text", K_CHOICE, 5, {"English", "Fran\x87""ais", "Deutsch", "Espa\xa4""ol", "Italiano", "Portugu\x88""s"}, 0, 0, false,
     "The language the game writes in."},
    {"voice_lang", "Voices", K_CHOICE, 2, {"English", "Fran\x87""ais", "Deutsch"}, 0, 0, false,
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
    // The names the prompts give the pad's buttons (EPmager, 2026-11-11: a Switch pad in a
    // browser showed Xbox names). Auto takes the connected pad's family, Xbox when none or
    // unknown; the others hold one. Was the prototype's F11 switch, which now steps it.
    {"button_names", "Button names", K_CHOICE, 3, {"Auto", "Xbox", "PlayStation", "Nintendo"}, 0, 0, false,
     "The pad's button names in the prompts. Auto reads the pad."},
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

// The screen: Classic's is 640 x 480, Modern's the native size as it is when Modern is
// chosen (EPmager, 2026-11-11: Modern took the launch size, which can be smaller).
struct ScreenSize {
    U32 w, h;
};

static ScreenSize ScreenNow() { return ScreenSize{Host_PhysW(), Host_PhysH()}; }
static ScreenSize NativeScreen() {
    ScreenSize n;
    Host_NativeSize(&n.w, &n.h);
    return n;
}
static bool SameScreen(ScreenSize a, ScreenSize b) { return a.w == b.w && a.h == b.h; }
static ScreenSize PresetScreen(int preset) { return preset == 1 ? ScreenSize{640, 480} : NativeScreen(); }

// A preset, Reset settings or an undo can cross between the one column and the wide layout
// in the middle of a frame; the view starts its scroll again at once, as at any crossing.
static void SetScreenTo(ScreenSize to) {
    if (SameScreen(to, ScreenNow()))
        return;
    if (Host_SetScreen(to.w, to.h, true))
        Menu_NoteScreen();
    Menu_SyncView();
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
    fresh = fresh && SameScreen(ScreenNow(), NativeScreen());
    if (!fresh)  // a second reset with nothing changed keeps the first one's undo
        TakeBackup(g_resetBackup, true);
    for (int s = 0; s < S_COUNT; s++)
        if (!IsLanguage(s))
            g_val[s] = FreshValue(s);
    MoreOptions_Defaults();
    Bindings_PutDefaults();
    g_preset = 0;
    g_presetBackup.have = false;
    SetScreenTo(NativeScreen());
    Trace("reset settings");
}

static const char *ValueText(int s, char *buf, size_t n) {
    const Setting &st = kSettings[s];
    if (s == S_RESOLUTION)
        snprintf(buf, n, "%u x %u%s", (unsigned)Host_PhysW(), (unsigned)Host_PhysH(),
                 SameScreen(ScreenNow(), NativeScreen()) ? " (native)" : "");
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

// ───────────────────────────── What the menu asks ─────────────────────────────

const Setting &Settings_Def(int s) { return kSettings[s]; }
int Settings_Value(int s) { return g_val[s]; }
bool Settings_Put(int s, int value) {
    if (value < 0 || value > kSettings[s].max)
        return false;
    g_val[s] = value;
    return true;
}
void Settings_Step(int s, int dir) { ChangeSetting(s, dir); }
const char *Settings_ValueText(int s, char *buf, size_t n) { return ValueText(s, buf, n); }
bool Settings_Changed(int s) { return ValueChanged(s); }
void Settings_Fresh() {
    for (int s = 0; s < S_COUNT; s++)
        g_val[s] = FreshValue(s);
    g_preset = 0;
}
int *Settings_Slot(int s) { return &g_val[s]; }

int Presets_Chosen() { return g_preset; }
const char *Presets_Name(int preset) { return kPresetNames[preset]; }
bool Presets_Changed() { return PresetChanged(); }
void Presets_Apply(int preset) { ApplyPreset(preset); }
void Settings_ResetAll() { ResetSettings(); }

static Backup &BackupOf(int which) { return which == BACK_RESET ? g_resetBackup : g_presetBackup; }
int Presets_UndoNames(int which, const char **names) { return BackupDiffers(BackupOf(which), names); }
bool Presets_UndoOffered(int which) { return UndoOffered(BackupOf(which)); }
void Presets_Undo(int which) { RestoreBackup(BackupOf(which)); }
void Presets_ForgetBackups() { g_presetBackup.have = g_resetBackup.have = false; }
