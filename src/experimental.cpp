#include "experimental.h"

#include "cadre.h"
#include "engine.h"
#include "menu.h"
#include "menu_parts.h"
#include "trace.h"
#include "ui.h"

#include <SVGA/CLIP.H>
#include <SVGA/FIL_LINE.H>

#include <stdio.h>
#include <string.h>

int sw_exptable = 1;

// ───────────────────────────── The tuning rows ─────────────────────────────

// lba2cc's settings tables at e3a3f847: every row with a console name and no row
// in Options (cam_interior and Fix retail bugs have one), with its default, range
// and console help as written. The groups are where a row is declared; naming
// them needs a field those tables do not have yet.

struct Group {
    const char *name;
    const char *help;
};

static const Group kGroups[] = {
    {"Auto camera", "The Auto camera's timings, zones and HD framing."},
    {"Mouse", "How the mouse turns the camera."},
    {"Gamepad", "The right stick's camera and the stick deadzone."},
    {"Graphics", "Shading, and the filters on the picture."},
    {"From 1997", "Settings for the machines of 1997. Today's play best as they are."},
};
enum { GROUP_COUNT = sizeof kGroups / sizeof kGroups[0] };

enum TKind { T_TOGGLE, T_CHOICE, T_NUM };
enum { ANY = 99999 };  // no bound in the table (SETTING_MIN_NONE, SETTING_MAX_NONE)

struct Tuning {
    int group;
    const char *name;  // console name
    const char *key;   // lba2.cfg key
    TKind kind;
    int def, min, max, step;
    const char *names[4];
    const char *help;  // the console help
};

static const Tuning kTuning[] = {
    // FOLLOWCAM.CPP FollowCamSettings
    {0, "cam_recenter_delay", "FollowCamReengageFrames", T_NUM, 60, 0, 600, 5, {0},
     "Frames before auto-cam re-centers after a manual pan"},
    {0, "cam_recenter_drift", "FollowCamDriftDivisor", T_NUM, 24, 1, 200, 1, {0},
     "Auto-cam re-center drift divisor (higher = gentler)"},
    {0, "cam_scene_carry", "FollowCamSceneCarry", T_TOGGLE, 1, 0, 1, 1, {0},
     "Auto cam: start behind the hero in a new outdoor scene, pitch kept"},
    {0, "cam_zone_skip", "FollowCamZoneSkip", T_TOGGLE, 1, 0, 1, 1, {0},
     "Auto cam: leave scripted shots that only latch unfired while steering"},
    {0, "cam_obstruct", "FollowCamObstruct", T_TOGGLE, 1, 0, 1, 1, {0},
     "Auto cam: eased obstruction clearance in place of the classic search"},
    {0, "cam_obstruct_hurry", "FollowCamObstructHurryDivisor", T_NUM, 10, 1, 10, 1, {0},
     "cam_obstruct: ease divisor while the hero is hidden (1 = cut, 10 = no hurry)"},
    {0, "cam_hd", "FollowCamHDRecompose", T_TOGGLE, 1, 0, 1, 1, {0},
     "Auto cam HD recompose: pull in + steepen at tall render heights"},
    {0, "cam_hd_cap", "FollowCamHDExcessCap", T_NUM, 0, 0, ANY, 5, {0},
     "Cap on recompose strength so tall res converges (0 = pure linear; 70 ~= 816p)"},
    {0, "cam_hd_pitch", "FollowCamHDPitchGain", T_NUM, 180, -ANY, ANY, 5, {0},
     "HD recompose pitch gain (AlphaCam units per 1.0 of k-1)"},
    {0, "cam_hd_dist", "FollowCamHDDistGain", T_NUM, 27, -ANY, ANY, 1, {0},
     "HD recompose distance pull (% of boom per 1.0 of k-1)"},
    {0, "cam_hd_lean", "FollowCamHDLeanGain", T_NUM, 45, -ANY, ANY, 1, {0},
     "HD recompose forward-lean cut (% per 1.0 of k-1)"},
    {0, "cam_ground", "FollowCamGround", T_TOGGLE, 1, 0, 1, 1, {0}, "Auto cam: smooth terrain ground-clearance lift"},
    {0, "cam_ground_clear", "FollowCamGroundClearance", T_NUM, 600, 0, 20000, 50, {0},
     "Ground clearance: keep the eye this far above the ground under it"},
    {0, "cam_manual_hold", "FollowCamManualHoldFrames", T_NUM, 45, 0, 600, 5, {0},
     "Frames the pitch/distance auto-assist yields after a manual camera nudge"},
    {0, "cam_hold_angle", "FollowCamHoldAngle", T_TOGGLE, 1, 0, 1, 1, {0},
     "Hold the manual camera rotation (free-camera); off = classic auto-recenter drift"},
    {0, "cam_zone_return", "FollowCamZoneReturn", T_TOGGLE, 1, 0, 1, 1, {0},
     "Auto cam: give a scripted shot's yaw and pitch back when the hero leaves its box"},
    {0, "cam_zone_anchor", "FollowCamZoneAnchor", T_TOGGLE, 0, 0, 1, 1, {0},
     "Prototype: hold a latching shot's world yaw while the hero is inside its box"},
    {0, "cam_world_anchor", "FollowCamWorldAnchor", T_TOGGLE, 0, 0, 1, 1, {0},
     "Prototype: hold the camera's world direction while the hero turns (off = it turns with him)"},
    {0, "cam_world_return", "FollowCamWorldReturn", T_NUM, 700, 0, 20000, 50, {0},
     "cam_world_anchor with cam_hold_angle 0: units walked forward that pace the return behind the hero (0 = none)"},
    {0, "cam_ease_ms", "FollowCamEaseMs", T_NUM, 300, 0, 5000, 50, {0},
     "Auto cam: time constant of the turn behind the hero, ms (0 = per-frame lerp, 300 = retail, 600 = the lerp at "
     "60 fps)"},
    {0, "cam_target_ms", "FollowCamTargetMs", T_NUM, 300, 0, 5000, 50, {0},
     "Auto cam: time constant of the look-at point trailing the hero, ms (0 = pinned to him, 300 = retail)"},
    {0, "cam_shot_blend_ms", "FollowCamShotBlendMs", T_NUM, 600, 0, 5000, 50, {0},
     "Auto cam and interior follow: blend into and out of a scripted shot or camera zone over this many ms (0 = cut)"},
    // CONFIG_FILE.CPP MouseSettings
    {1, "mouse_confine", "MouseConfine", T_CHOICE, 1, 0, 2, 1, {"Never", "Fullscreen", "Always"},
     "Keep the cursor in the window: 0 never, 1 fullscreen, 2 always"},
    {1, "cam_mouse", "MouseCamera", T_TOGGLE, 1, 0, 1, 1, {0}, "Mouse camera enable"},
    {1, "cam_mouse_drag", "MouseCameraDrag", T_CHOICE, 1, 0, 1, 1, {"Free orbit", "Right-drag"},
     "Mouse camera: 1=right-drag, 0=free orbit"},
    {1, "cam_mouse_x", "MouseSensitivityX", T_NUM, 4, 1, 10, 1, {0}, "Mouse orbit sensitivity (1-10)"},
    {1, "cam_mouse_y", "MouseSensitivityY", T_NUM, 4, 1, 10, 1, {0}, "Mouse elevation sensitivity (1-10)"},
    {1, "cam_mouse_invert", "MouseInvertY", T_TOGGLE, 0, 0, 1, 1, {0}, "Invert mouse elevation"},
    {1, "cam_mouse_div", "MouseCameraDivisor", T_NUM, 4, 1, 16, 1, {0},
     "Mouse pixel-to-angle divisor (1-16, lower = faster)"},
    {1, "cam_smooth", "ManualCameraSmoothing", T_NUM, 2, 1, 16, 1, {0},
     "Manual orbit lerp divisor (1 = snap, higher = laggier)"},
    {1, "cam_glide", "FollowCamOrbitGlide", T_NUM, 75, 0, 95, 5, {0},
     "Orbit follow-through: % of orbit speed kept per frame after release (0 = stop dead)"},
    // CONFIG_FILE.CPP GamepadSettings
    {2, "pad_deadzone", "GamepadDeadzone", T_NUM, 8000, 0, 32767, 500, {0}, "Gamepad stick deadzone (SDL units 0-32767)"},
    {2, "cam_stick_analog", "GamepadCameraAnalog", T_TOGGLE, 1, 0, 1, 1, {0}, "Analog right-stick camera in Auto cam"},
    {2, "cam_stick_x", "GamepadCameraSensX", T_NUM, 5, 1, 10, 1, {0}, "Right-stick orbit sensitivity (1-10)"},
    {2, "cam_stick_y", "GamepadCameraSensY", T_NUM, 5, 1, 10, 1, {0}, "Right-stick elevation sensitivity (1-10)"},
    {2, "cam_stick_invert", "GamepadCameraInvertY", T_TOGGLE, 0, 0, 1, 1, {0}, "Invert right-stick elevation"},
    {2, "cam_stick_max_x", "GamepadCamMaxBeta", T_NUM, 36, 1, 200, 1, {0},
     "Right-stick orbit speed ceiling (units/frame at sens 10)"},
    {2, "cam_stick_max_y", "GamepadCamMaxAlpha", T_NUM, 10, 1, 100, 1, {0},
     "Right-stick elevation speed ceiling (units/frame at sens 10)"},
    // CONFIG_FILE.CPP BootSettings
    {3, "gfx_dither", "DitherShading", T_TOGGLE, 0, 0, 1, 1, {0},
     "Ordered dither on Gouraud shade rows (softens 16-step banding)"},
    {3, "gfx_texfilter", "TextureFilter", T_CHOICE, 0, 0, 2, 1, {"Off", "Horizontal", "Bilinear"},
     "Texture filtering: 0 off, 1 horizontal, 2 bilinear"},
    {3, "gfx_presentfilter", "PresentFilter", T_CHOICE, 0, 0, 2, 1, {"Nearest", "Linear", "Pixel art"},
     "Filter stretching the frame to the window: 0 nearest, 1 linear, 2 pixelart"},
    // Rows the 1997 Options had for the machines of the day, moved off the Modern pages
    // (decided 2026-11-11). They have no console name: they keep their game labels, and the
    // 1997 Options page (the plan's step 10) keeps them where they were.
    {4, "Detail level", "DetailLevel", T_CHOICE, 3, 0, 3, 1, {"Low", "Medium", "High", "Full"},
     "For slow machines: each step down takes away animated water, rain, the far scenery, then shadows."},
    // Named as lba2cc's glossary names it: "Movies at original size / Movies fit to screen".
    {4, "Movies", "FullScreen", T_CHOICE, 1, 0, 1, 1, {"At original size", "Fit to screen"},
     "For slow CD drives: the movies fit to the screen, or play at their original size."},
    {4, "Reverse stereo", "ReverseStereo", T_TOGGLE, 0, 0, 1, 1, {0}, "Swaps the left and right speakers."},
};
enum { TUNING_COUNT = sizeof kTuning / sizeof kTuning[0] };
static_assert(TUNING_COUNT <= EXP_MAX, "EXP_MAX holds every tuning row");

// The rows lba2cc calls experimental (docs/CAMERA.md marks them so, CHANGELOG "experimental
// and off by default"). The rest tune finished features. The settings tables have no field
// for it yet: the menu plan's ring field would give it.
static const char *const kExperimental[] = {"cam_zone_anchor", "cam_world_anchor"};

static bool IsExperimental(int i) {
    for (size_t k = 0; k < sizeof kExperimental / sizeof kExperimental[0]; k++)
        if (!strcmp(kTuning[i].name, kExperimental[k]))
            return true;
    return false;
}
static const char *kTag = "experimental";

static int g_val[TUNING_COUNT];
static bool g_valReady;

static void Ready() {
    if (!g_valReady) {
        Exp_Defaults();
        g_valReady = true;
    }
}

int Exp_Count() { return TUNING_COUNT; }
const char *Exp_Name(int i) { return kTuning[i].name; }
void Exp_Get(int *out) {
    Ready();
    memcpy(out, g_val, sizeof g_val);
}
void Exp_Put(const int *in) {
    memcpy(g_val, in, sizeof g_val);
    g_valReady = true;
}
void Exp_Defaults() {
    for (int i = 0; i < TUNING_COUNT; i++)
        g_val[i] = kTuning[i].def;
    g_valReady = true;
}
bool Exp_AnyChanged() {
    Ready();
    for (int i = 0; i < TUNING_COUNT; i++)
        if (g_val[i] != kTuning[i].def)
            return true;
    return false;
}

bool Exp_Set(const char *name, int value) {
    Ready();
    for (int i = 0; i < TUNING_COUNT; i++) {
        if (strcmp(kTuning[i].name, name) != 0)
            continue;
        if (value < kTuning[i].min || value > kTuning[i].max)
            return false;
        g_val[i] = value;
        Trace("set %s=%d (control)", name, value);
        return true;
    }
    return false;
}

static const char *ValueOf(int i, int v, char *buf, size_t n) {
    const Tuning &t = kTuning[i];
    if (t.kind == T_TOGGLE)
        snprintf(buf, n, "%s", v ? "On" : "Off");
    else if (t.kind == T_CHOICE)
        snprintf(buf, n, "%s", t.names[v]);
    else
        snprintf(buf, n, "%d", v);
    return buf;
}

static const char *ValueText(int i, char *buf, size_t n) { return ValueOf(i, g_val[i], buf, n); }

// Default and range, then the cfg key when the line holds it: the 1997 binding
// screen's status line.
static void StatusText(int i, char *buf, size_t n, S32 width, S32 s) {
    const Tuning &t = kTuning[i];
    char def[24], range[40];
    ValueOf(i, t.def, def, sizeof def);
    if (t.kind != T_NUM)
        range[0] = 0;
    else if (t.min == -ANY && t.max == ANY)
        snprintf(range, sizeof range, ", any value");
    else if (t.max == ANY)
        snprintf(range, sizeof range, ", %d or more", t.min);
    else
        snprintf(range, sizeof range, ", %d to %d", t.min, t.max);
    snprintf(buf, n, "Default %s%s.  lba2.cfg %s", def, range, t.key);
    if (Ui_SmallWidth(buf, s) > width)
        snprintf(buf, n, "Default %s%s.", def, range);
}

// ───────────────────────────── Page state ─────────────────────────────

static bool g_active;
static int g_cursor;      // the table: the row under the cursor, 0..TUNING_COUNT-1
static int g_scroll;      // the table: the first line shown; the list: the first row shown
static int g_visible;     // the table: lines in its window at the last draw
static bool g_inGroup;    // the list: a group's rows are open
static int g_group;       // the list: the group under the cursor (or open)
static int g_colCursor;   // the list, one column: the groups page cursor, 0 is the Return row
static int g_row;         // the list: the row within the open group

static bool Table() { return sw_exptable != 0; }
static bool Wide() { return Menu_Wide(); }

// The screen can change under the page. Wide keeps the group as g_group alone;
// the one column's groups page counts its Return row, so put its cursor back on
// the same group when the layout turns to one column.
static int g_layoutWide = -1;
static void SyncLayout() {
    const int wide = Wide() ? 1 : 0;
    if (g_layoutWide == 1 && !wide)
        g_colCursor = g_group + 1;
    g_layoutWide = wide;
}

static int GroupFirst(int g) {
    for (int i = 0; i < TUNING_COUNT; i++)
        if (kTuning[i].group == g)
            return i;
    return 0;
}
static int GroupCount(int g) {
    int n = 0;
    for (int i = 0; i < TUNING_COUNT; i++)
        n += kTuning[i].group == g;
    return n;
}

// The row the cursor is on, or -1 on the list's groups page.
static int CursorTuning() {
    if (Table())
        return g_cursor;
    return g_inGroup ? GroupFirst(g_group) + g_row : -1;
}

static void Say();

void Exp_Open() {
    Ready();
    g_active = true;
    g_cursor = g_scroll = g_row = 0;
    g_group = 0;
    g_colCursor = 1;
    g_inGroup = false;
    Menu_PageEntered();
    Say();
    Trace("experimental open form=%s", Table() ? "table" : "list");
}

void Exp_Close() {
    if (!g_active)
        return;
    g_active = false;
    Menu_PageEntered();
    Trace("experimental close");
}

bool Exp_Active() { return g_active; }

void Exp_Reset() {
    g_active = g_inGroup = false;
    g_cursor = g_scroll = g_group = g_row = 0;
    g_colCursor = 1;
    Exp_Defaults();
}

// ───────────────────────────── Input ─────────────────────────────

static void Change(int i, int dir) {
    const Tuning &t = kTuning[i];
    int v = g_val[i];
    if (t.kind == T_NUM) {
        v += dir * t.step;
        if (v < t.min)
            v = t.min;
        if (v > t.max)
            v = t.max;
    } else {
        v = (v + dir + t.max + 1) % (t.max + 1);
    }
    if (v != g_val[i]) {
        g_val[i] = v;
        char buf[24];
        Trace("set %s=%s", t.name, ValueText(i, buf, sizeof buf));
    }
}

static void Default(int i) {
    if (g_val[i] == kTuning[i].def)
        return;
    g_val[i] = kTuning[i].def;
    char buf[24];
    Trace("default %s=%s", kTuning[i].name, ValueText(i, buf, sizeof buf));
}

// Left, Right, Enter and Del on a row.
static void RowKeys(int i) {
    if (Engine_Pressed(KEY_LEFT))
        Change(i, -1);
    if (Engine_Pressed(KEY_RIGHT))
        Change(i, +1);
    if (Engine_Pressed(KEY_ENTER) && kTuning[i].kind != T_NUM)
        Change(i, +1);
    if (Engine_Pressed(KEY_DEL))
        Default(i);
}

static void UpdateTable() {
    int page = g_visible > 3 ? g_visible - 2 : 1;  // the group heads take lines too
    if (Engine_Pressed(KEY_UP))
        g_cursor = (g_cursor + TUNING_COUNT - 1) % TUNING_COUNT;
    if (Engine_Pressed(KEY_DOWN))
        g_cursor = (g_cursor + 1) % TUNING_COUNT;
    if (Engine_Pressed(KEY_PGUP))
        g_cursor = g_cursor > page ? g_cursor - page : 0;
    if (Engine_Pressed(KEY_PGDN))
        g_cursor = g_cursor + page < TUNING_COUNT ? g_cursor + page : TUNING_COUNT - 1;
    RowKeys(g_cursor);
    if (Engine_Pressed(KEY_ESC))
        Exp_Close();
}

static void UpdateList() {
    bool column = !Wide();
    if (!g_inGroup) {
        if (column) {
            int n = GROUP_COUNT + 1;
            if (Engine_Pressed(KEY_UP))
                g_colCursor = (g_colCursor + n - 1) % n;
            if (Engine_Pressed(KEY_DOWN))
                g_colCursor = (g_colCursor + 1) % n;
            if (g_colCursor > 0)
                g_group = g_colCursor - 1;
        } else {
            if (Engine_Pressed(KEY_UP))
                g_group = (g_group + GROUP_COUNT - 1) % GROUP_COUNT, Menu_PageEntered();
            if (Engine_Pressed(KEY_DOWN))
                g_group = (g_group + 1) % GROUP_COUNT, Menu_PageEntered();
        }
        bool open = Engine_Pressed(KEY_ENTER) || (!column && Engine_Pressed(KEY_RIGHT));
        if (open && column && g_colCursor == 0) {
            Exp_Close();
        } else if (open) {
            g_inGroup = true;
            g_row = g_scroll = 0;
            Menu_PageEntered();
            Trace("experimental group=%s", kGroups[g_group].name);
        } else if (Engine_Pressed(KEY_ESC)) {
            Exp_Close();
        }
        return;
    }
    int n = GroupCount(g_group);
    if (Engine_Pressed(KEY_UP))
        g_row = (g_row + n - 1) % n;
    if (Engine_Pressed(KEY_DOWN))
        g_row = (g_row + 1) % n;
    RowKeys(GroupFirst(g_group) + g_row);
    if (Engine_Pressed(KEY_ESC)) {
        g_inGroup = false;
        Menu_PageEntered();
        Trace("experimental back to groups");
    }
}

void Exp_Update() {
    SyncLayout();
    if (Table())
        UpdateTable();
    else
        UpdateList();
    if (g_active)
        Say();
}

static void Say() {
    int i = CursorTuning();
    if (i >= 0 && IsExperimental(i)) {
        char buf[320];
        const char *help = kTuning[i].help;
        if (!strncmp(help, "Prototype: ", 11))
            help += 11;  // the console's word for the same thing
        snprintf(buf, sizeof buf, "Experimental: %s", help);
        Menu_Say(buf);
    } else if (i >= 0)
        Menu_Say(kTuning[i].help);
    else if (Wide() || g_colCursor > 0)
        Menu_Say(kGroups[g_group].help);
    else
        Menu_Say("");
}

// ───────────────────────────── Drawing ─────────────────────────────

// What was drawn, for `state` and the self test.
static ExpLine g_lines[64];
static int g_lineCount;
static char g_status[96];
static int g_total;  // the table's lines, heads included
static ExpTable g_table;
static bool g_tableShown;

// The table is drawn at 1x on every canvas, as the 1997 key binding screen (decided
// 2026-11-03); switch tablefont 2 draws it at 2x to compare.
static S32 Scale() { return Menu_GetSwitch("tablefont"); }

static S32 PanelShade() {
    int b = Menu_GetSwitch("bandshade");
    return b ? b : BAND_SHADE_LVL;
}

// The table's lines in order: a head per group, then its rows. Returns the line
// count; line[k] is -1 - group for a head, else the row.
static int TableLines(int *line) {
    int n = 0;
    for (int g = 0; g < GROUP_COUNT; g++) {
        line[n++] = -1 - g;
        for (int i = 0; i < TUNING_COUNT; i++)
            if (kTuning[i].group == g)
                line[n++] = i;
    }
    return n;
}

// The 1997 binding screen's way (CONFIG.CPP DrawOneConfig): the 8x8 font on a
// 10-pixel pitch (at 1x), the cursor a strip across the line, here the cursor
// plasma in a frame as the rows have it. Inside one panel from y0 to y1, with a
// status line at its foot and the scroll bar beside it.
static void DrawTable(S32 px0, S32 px1, S32 py0, S32 py1) {
    S32 s = Scale(), pitch = 10 * s;
    Ui_Panel(px0, py0, px1, py1, PanelShade());

    S32 nameX = px0 + 16;
    S32 valueRight = px1 - 24 - 12 * s;  // the marks after it end 24 in
    S32 statusTop = py1 - 16 * s;
    S32 bodyTop = py0 + 5 * s;
    int visible = (statusTop - 1 - 9 * s - bodyTop) / pitch + 1;

    int line[64];
    int total = TableLines(line);
    g_total = total;
    int cl = 0;
    for (int k = 0; k < total; k++)
        if (line[k] == g_cursor)
            cl = k;
    // Keep the cursor in view, and its group's head when it is the group's first row.
    int want = line[cl - 1] < 0 ? cl - 1 : cl;
    if (want < g_scroll)
        g_scroll = want;
    if (cl >= g_scroll + visible)
        g_scroll = cl - visible + 1;
    if (g_scroll > total - visible)
        g_scroll = total - visible;
    if (g_scroll < 0)
        g_scroll = 0;
    if (visible > total)
        visible = total;
    g_visible = visible;

    g_tableShown = true;
    g_table.x0 = px0, g_table.y0 = py0, g_table.x1 = px1, g_table.y1 = py1;
    g_table.scale = s;
    g_table.statusTop = statusTop;
    g_table.valueEdge = valueRight;
    g_table.marksRight = valueRight + 12 * s;
    g_table.total = total;
    g_table.scroll = g_scroll;
    // The tags line up, two letters clear of the longest name.
    S32 tagX = 0;
    for (int i = 0; i < TUNING_COUNT; i++) {
        S32 w = Ui_SmallWidth(kTuning[i].name, s);
        if (w > tagX)
            tagX = w;
    }
    tagX += nameX + 16 * s;
    MemoClip();
    SetClip(px0 + 1, py0 + 1, px1 - 1, py1 - 1);
    for (int k = g_scroll; k < g_scroll + visible; k++) {
        S32 y = bodyTop + (k - g_scroll) * pitch;
        ExpLine &d = g_lines[g_lineCount < 63 ? g_lineCount++ : 63];
        memset(&d, 0, sizeof d);
        d.x0 = px0 + 8, d.x1 = px1 - 8, d.y0 = y - s, d.y1 = y + 9 * s - 1;
        d.textLeft = nameX;
        if (line[k] < 0) {
            const char *name = kGroups[-1 - line[k]].name;
            Ui_Small(nameX, y, name, INK_CONFIG_HEAD, s);
            snprintf(d.text, sizeof d.text, "%s", name);
            d.head = true;
            d.textRight = nameX + Ui_SmallWidth(name, s) - 1;
            continue;
        }
        int i = line[k];
        bool cursor = i == g_cursor;
        bool changed = g_val[i] != kTuning[i].def;
        if (cursor)
            Ui_RowBackground(d.x0, d.y0, d.x1, d.y1, FILL_CURSOR);
        Ui_Small(nameX, y, kTuning[i].name, cursor ? INK_WHITE : INK_CONFIG_MENU, s);
        char val[24];
        ValueText(i, val, sizeof val);
        S32 vx = valueRight + 1 - Ui_SmallWidth(val, s);
        Ui_Small(vx, y, val, changed ? INK_COUNT : cursor ? INK_WHITE : INK_CONFIG_MENU, s);
        S32 left = vx, right = vx + Ui_SmallWidth(val, s) - 1;  // what is drawn on the right, for the record
        if (cursor) {
            left = vx - 12 * s;
            Ui_Small(left, y, "\x11", INK_WHITE, s);
            Ui_Small(right + 1 + 4 * s, y, "\x10", INK_WHITE, s);
            right += 12 * s;
        }
        snprintf(d.text, sizeof d.text, "%s", kTuning[i].name);
        snprintf(d.value, sizeof d.value, "%s", val);
        d.cursor = cursor;
        d.changed = changed;
        d.first = line[k - 1] < 0;
        d.textRight = nameX + Ui_SmallWidth(kTuning[i].name, s) - 1;
        S32 tx = tagX;
        if (IsExperimental(i) && tx + Ui_SmallWidth(kTag, s) - 1 + 8 * s < left) {
            // The tag after the name, in the heads' colour, two letters clear of it; where it
            // does not fit (the 2x font in the one column), the voice still says it.
            Ui_Small(tx, y, kTag, INK_CONFIG_HEAD, s);
            d.textRight = tx + Ui_SmallWidth(kTag, s) - 1;
            d.experimental = true;
        }
        d.valueLeft = left;
        d.valueRight = right;
    }

    // The status line under a groove, as the 1997 screen's status box.
    Line(px0 + 8, statusTop, px1 - 8, statusTop, COUL_DOWN_BLUE);
    Line(px0 + 8, statusTop + 1, px1 - 8, statusTop + 1, COUL_UP_BLUE);
    StatusText(g_cursor, g_status, sizeof g_status, px1 - 24 - nameX, s);
    Ui_Small(nameX, statusTop + 5 * s, g_status, INK_CONFIG_HEAD, s);
    RestoreClip();
    g_table.status = g_status;
    g_table.statusLeft = nameX;
    g_table.statusRight = nameX + Ui_SmallWidth(g_status, s) - 1;
    g_table.count = g_lineCount;
    g_table.lines = g_lines;

    if (visible < total)
        Menu_DrawScrollBar(px1 + 9, py0, py1, g_scroll, visible, total);
}

// The list: rows of the game font, the label on the pane's label edge, the value
// on its value edge, as every other page.
static void DrawListRow(const char *pane, const char *label, int i, S32 x0, S32 y0, S32 x1, RowFill fill,
                        bool centred) {
    S32 y1 = y0 + ROW_H, yc = (y0 + y1) / 2;
    Ui_RowBackground(x0, y0, x1, y1, fill);
    char val[24] = "", text[96];
    if (i >= 0)
        ValueText(i, val, sizeof val);
    bool changed = i >= 0 && g_val[i] != kTuning[i].def;
    DrawnRow &d = Menu_Record(pane, label, val, fill, x0, y0, x1, y1);
    d.setting = i >= 0;
    d.changed = changed;
    if (centred) {
        // One column: "Label: Value" in one string, as 1997.
        if (i >= 0)
            snprintf(text, sizeof text, "%s: %s", label, val);
        else
            snprintf(text, sizeof text, "%s", label);
        Menu_RowLabelCentred(x0, x1, yc, text, INK_WHITE);
        d.textLeft = (x0 + x1) / 2 - Ui_TextWidth(text) / 2;
        d.textRight = (x0 + x1) / 2 + Ui_TextWidth(text) / 2 + 8;
        return;
    }
    MemoClip();
    SetClip(x0 + 1, y0, x1 - 1, y1);
    Ui_Text(x0 + 16, yc - 18, label, INK_WHITE);
    RestoreClip();
    d.textLeft = x0 + 16;
    d.textRight = x0 + 16 + Ui_TextWidth(label);
    if (i < 0)
        return;
    S32 valueRight = x1 - 24;
    S32 left = valueRight + 1 - Ui_TextInkWidth(val);
    if (d.textRight + 8 > left - 17)
        d.textRight = x1 + (d.textRight + 8 - (left - 17));  // the label runs into the value
    Ui_Text(left, yc - 18, val, changed ? INK_COUNT : INK_WHITE);
    d.valueRight = valueRight;
    if (fill == FILL_CURSOR) {
        Ui_ChangeMarks(left - 17, valueRight + 17, yc);
        d.marks = true;
    }
}

static void Legend(const char **keys, const char **words, int *n) {
    *n = 0;
    if (Table() || g_inGroup) {
        keys[*n] = "\x11\x10", words[(*n)++] = "Change";
        keys[*n] = "Del", words[(*n)++] = "Default";
    } else {
        keys[*n] = "Enter", words[(*n)++] = "Open";
    }
    keys[*n] = "Esc", words[(*n)++] = "Back";
}

// Rows of one group from top to limit, scrolled to keep the cursor in view.
static void DrawGroupRows(S32 x0, S32 x1, S32 top, S32 limit, const char *pane, bool centred) {
    int n = GroupCount(g_group), first = GroupFirst(g_group);
    int visible = (limit - top + ROW_GAP) / ROW_PITCH;
    if (visible > n)
        visible = n;
    if (g_row < g_scroll)
        g_scroll = g_row;
    if (g_row >= g_scroll + visible)
        g_scroll = g_row - visible + 1;
    S32 y = top;
    for (int r = g_scroll; r < g_scroll + visible; r++) {
        DrawListRow(pane, kTuning[first + r].name, first + r, x0, y, x1, r == g_row ? FILL_CURSOR : FILL_SHADE,
                    centred);
        y += ROW_PITCH;
    }
    if (visible < n)
        Menu_DrawScrollBar(x1 + 9, top, top + visible * ROW_PITCH - ROW_GAP - 1, g_scroll, visible, n);
}

static void DrawWideExp() {
    WideFrame f = Menu_WideFrame();
    S32 x0 = f.x0 + (f.w - DRILL_W) / 2, x1 = x0 + DRILL_W - 1;
    S32 top = f.y0 + 24, limit = Menu_WideRowsLimit();
    const char *title = Table() || !g_inGroup ? "More options" : kGroups[g_group].name;
    S32 rtop = Ui_Title(x0, x1, top, title);
    Menu_Record("title", title, "", FILL_SHADE, x0, top, x1, top + 38).textLeft =
        (x0 + x1 + 1) / 2 - Ui_TextWidth(title) / 2;
    if (Table()) {
        DrawTable(x0, x1, rtop, limit);
    } else if (!g_inGroup) {
        S32 y = rtop;
        for (int g = 0; g < GROUP_COUNT; g++) {
            DrawListRow("exp", kGroups[g].name, -1, x0, y, x1, g == g_group ? FILL_CURSOR : FILL_SHADE, false);
            y += ROW_PITCH;
        }
    } else {
        DrawGroupRows(x0, x1, rtop, limit, "exp", false);
    }
    const char *keys[4], *words[4];
    int nk;
    Legend(keys, words, &nk);
    Menu_DrawFoot(keys, words, nk);
}

static void DrawColumnExp() {
    S32 W = (S32)Engine_LogicalW(), H = (S32)Engine_LogicalH();
    S32 x0 = W / 2 - 275, x1 = W / 2 + 275;
    if (Table()) {
        DrawTable(x0, x1, 16, Menu_DialogueTop() - 16);
        Menu_DrawDialogue();
        return;
    }
    if (!g_inGroup) {
        // The groups, as a 1997 sub-menu: a Return row, then one row each.
        bool help = Menu_GetSwitch("help640") != 0;
        S32 top = 16, bottom = help ? Menu_DialogueTop() - 16 : H - 16;
        int n = GROUP_COUNT + 1;
        S32 stackH = n * ROW_PITCH - ROW_GAP;
        S32 y = top + (bottom - top - stackH) / 2;
        for (int r = 0; r < n; r++) {
            const char *label = r == 0 ? "Return to previous menu" : kGroups[r - 1].name;
            DrawListRow("column", label, -1, x0, y, x1, r == g_colCursor ? FILL_CURSOR : FILL_SHADE, true);
            y += ROW_PITCH;
        }
        if (help)
            Menu_DrawDialogue();
        return;
    }
    DrawGroupRows(x0, x1, 16, Menu_DialogueTop() - 16, "column", true);
    Menu_DrawDialogue();
}

void Exp_Draw() {
    SyncLayout();
    g_lineCount = 0;
    g_tableShown = false;
    g_status[0] = 0;
    g_total = 0;
    if (Wide())
        DrawWideExp();
    else
        DrawColumnExp();
}

// ───────────────────────────── State ─────────────────────────────

bool Exp_Table(ExpTable *out) {
    if (!g_active || !g_tableShown)
        return false;
    *out = g_table;
    return true;
}

const char *Exp_CursorName() {
    int i = CursorTuning();
    return i >= 0 ? kTuning[i].name : "";
}

const char *Exp_CursorValue() {
    static char buf[24];
    int i = CursorTuning();
    if (i < 0)
        return "";
    return ValueText(i, buf, sizeof buf);
}

int Exp_Where(char *buf, size_t n) {
    int i = CursorTuning();
    char val[24] = "";
    if (i >= 0)
        ValueText(i, val, sizeof val);
    if (Table()) {
        snprintf(buf, n, "%s | %s | %s", kGroups[kTuning[i].group].name, kTuning[i].name, val);
        return 1;
    }
    if (!g_inGroup) {
        snprintf(buf, n, "%s", !Wide() && g_colCursor == 0 ? "Return to previous menu" : kGroups[g_group].name);
        return 1;
    }
    snprintf(buf, n, "%s | %s | %s", kGroups[g_group].name, kTuning[i].name, val);
    return 2;
}

void Exp_WriteState(FILE *f) {
    SyncLayout();
    int i = CursorTuning();
    fprintf(f, "{\"form\":\"%s\",\"group\":", Table() ? "table" : "list");
    Menu_JsonString(f, kGroups[i >= 0 ? kTuning[i].group : g_group].name);
    fprintf(f, ",\"in_group\":%s,\"row\":", g_inGroup ? "true" : "false");
    Menu_JsonString(f, i >= 0 ? kTuning[i].name : "");
    fprintf(f, ",\"scroll\":%d,\"visible\":%d,\"total\":%d,\"status\":", g_scroll, g_visible, g_total);
    Menu_JsonString(f, g_status);
    if (g_tableShown) {
        const ExpTable &t = g_table;
        fprintf(f, ",\"table\":{\"rect\":[%d,%d,%d,%d],\"scale\":%d,\"status_top\":%d,\"value_edge\":%d,"
                   "\"marks_right\":%d,\"status_left\":%d,\"status_right\":%d}",
                t.x0, t.y0, t.x1, t.y1, t.scale, t.statusTop, t.valueEdge, t.marksRight, t.statusLeft, t.statusRight);
    }
    fprintf(f, ",\"lines\":[");
    for (int k = 0; k < g_lineCount; k++) {
        const ExpLine &d = g_lines[k];
        fprintf(f, "%s{\"text\":", k ? "," : "");
        Menu_JsonString(f, d.text);
        fprintf(f, ",\"value\":");
        Menu_JsonString(f, d.value);
        fprintf(f, ",\"head\":%s,\"first\":%s,\"experimental\":%s,\"cursor\":%s,\"changed\":%s,\"rect\":[%d,%d,%d,%d],\"text_left\":%d,"
                   "\"text_right\":%d,\"value_left\":%d,\"value_right\":%d}",
                d.head ? "true" : "false", d.first ? "true" : "false", d.experimental ? "true" : "false", d.cursor ? "true" : "false", d.changed ? "true" : "false", d.x0, d.y0, d.x1,
                d.y1, d.textLeft, d.textRight, d.valueLeft, d.valueRight);
    }
    fprintf(f, "],\"values\":{");
    for (int k = 0; k < TUNING_COUNT; k++)
        fprintf(f, "%s\"%s\":%d", k ? "," : "", kTuning[k].name, g_val[k]);
    fprintf(f, "}}");
}
