#include "controls.h"

#include "engine.h"
#include "menu.h"
#include "menu_parts.h"
#include "trace.h"
#include "ui.h"

#include <SVGA/CLIP.H>
#include <SYSTEM/KEYBOARD_KEYS.H>

#include <stdio.h>
#include <string.h>

int sw_devices = 1, sw_padnames = 0;

// ───────────────────────────── The actions ─────────────────────────────

// Pad codes, in lba2cc's K_GAMEPAD_* order.
enum {
    PAD_A = PAD_CODE_BASE, PAD_B, PAD_X, PAD_Y, PAD_BACK, PAD_GUIDE, PAD_START, PAD_LS, PAD_RS, PAD_LB, PAD_RB,
    PAD_DUP, PAD_DDOWN, PAD_DLEFT, PAD_DRIGHT,
    PAD_LUP, PAD_LDOWN, PAD_LLEFT, PAD_LRIGHT, PAD_RUP, PAD_RDOWN, PAD_RLEFT, PAD_RRIGHT, PAD_LT, PAD_RT,
};

struct Group {
    const char *name;
    const char *help;
};

static const Group kGroups[] = {
    {"Movement", "Walking, turning and the dodge."},
    {"Actions", "Talking, fighting, and the screens you open in play."},
    {"Behaviours", "One key for each of the four behaviours."},
    {"Weapons", "One key for each weapon."},
    {"Magic and items", "The penguin, the jetpack and the spells."},
    {"Camera", "Recentring, turning and the camera's height."},
    {"Menus", "The game menu, pause, help, saving and loading."},
};
enum { GROUP_COUNT = sizeof kGroups / sizeof kGroups[0] };

struct Action {
    const char *name;  // English renderings of the comments in INPUT.H
    int group;
    int key[2];        // DefKeysDefault95 (INPUT_BINDINGS.CPP)
    int pad[2];        // GamepadKeysDefault, standard layout
    const char *help;
};

static const Action kActions[] = {
    {"Forward", 0, {K_GRAY_UP, K_NUMPAD_8}, {PAD_RT, 0}, "Walks or runs Twinsen forward."},
    {"Back", 0, {K_GRAY_DOWN, K_NUMPAD_2}, {PAD_LT, 0}, "Steps Twinsen back."},
    {"Turn left", 0, {K_GRAY_LEFT, K_NUMPAD_4}, {PAD_LLEFT, 0}, "Turns Twinsen to his left."},
    {"Turn right", 0, {K_GRAY_RIGHT, K_NUMPAD_6}, {PAD_LRIGHT, 0}, "Turns Twinsen to his right."},
    {"Dodge", 0, {K_X, 0}, {PAD_LS, PAD_RS}, "Jumps aside, out of harm's way."},

    {"Behaviour action", 1, {K_SPACE, K_NUMPAD_5}, {PAD_X, 0}, "Does what the behaviour does: jump, hit or hide."},
    {"Talk and search", 1, {K_W, K_GRAY_END}, {PAD_A, 0}, "Talks to people and searches where you stand."},
    {"Use weapon", 1, {K_ALT, 0}, {PAD_RB, 0}, "Uses the weapon you hold."},
    {"Behaviour menu", 1, {K_CTRL, 0}, {PAD_LB, 0}, "Hold it to choose a behaviour."},
    {"Inventory", 1, {K_SHIFT, 0}, {PAD_Y, 0}, "Opens the inventory."},
    {"Holomap", 1, {K_H, K_TAB}, {PAD_BACK, 0}, "Opens the holomap."},

    {"Normal", 2, {K_F5, 0}, {0, 0}, "Switches to the Normal behaviour."},
    {"Athletic", 2, {K_F6, 0}, {0, 0}, "Switches to the Athletic behaviour."},
    {"Aggressive", 2, {K_F7, 0}, {0, 0}, "Switches to the Aggressive behaviour."},
    {"Discreet", 2, {K_F8, 0}, {0, 0}, "Switches to the Discreet behaviour."},

    {"Weapon 1", 3, {K_1, 0}, {0, 0}, "Takes out the first weapon."},
    {"Weapon 2", 3, {K_2, 0}, {0, 0}, "Takes out the second weapon."},
    {"Weapon 3", 3, {K_3, 0}, {0, 0}, "Takes out the third weapon."},
    {"Weapon 4", 3, {K_4, 0}, {0, 0}, "Takes out the fourth weapon."},
    {"Weapon 5", 3, {K_5, 0}, {0, 0}, "Takes out the fifth weapon."},
    {"Weapon 6", 3, {K_6, 0}, {0, 0}, "Takes out the sixth weapon."},
    {"Weapon 7", 3, {K_7, 0}, {0, 0}, "Takes out the seventh weapon."},

    {"Penguin", 4, {K_N, 0}, {PAD_DRIGHT, 0}, "Sends out the meca-penguin."},
    {"Jetpack", 4, {K_J, 0}, {PAD_DDOWN, 0}, "Turns the jetpack on or off."},
    {"Protection spell", 4, {K_C, 0}, {PAD_DUP, 0}, "Casts the protection spell."},
    {"Lightning spell", 4, {K_F, 0}, {PAD_DLEFT, 0}, "Casts the lightning spell."},

    {"Recentre", 5, {K_ENTER, K_NUMPAD_ENTER}, {PAD_B, 0}, "Puts the camera back behind Twinsen."},
    {"Turn camera", 5, {K_BACKSPACE, 0}, {PAD_RLEFT, PAD_RRIGHT}, "Turns the camera round Twinsen."},
    {"Camera up", 5, {K_NUMPAD_PLUS, K_GRAY_PAGE_UP}, {PAD_RUP, 0}, "The next camera height."},
    {"Camera down", 5, {K_NUMPAD_MOINS, K_GRAY_PAGE_DOWN}, {PAD_RDOWN, 0}, "The camera height before."},

    {"Game menu", 6, {K_F10, 0}, {PAD_START, 0}, "Opens the game menu."},
    {"Pause", 6, {K_P, 0}, {0, 0}, "Pauses the game."},
    {"Help", 6, {K_F1, 0}, {0, 0}, "Shows the help."},
    {"Save", 6, {K_F2, K_S}, {0, 0}, "Saves the game."},
    {"Load", 6, {K_F3, K_L}, {0, 0}, "Loads a game."},
    {"Options", 6, {K_F4, K_O}, {0, 0}, "Opens these options."},
};
enum { ACTION_COUNT = sizeof kActions / sizeof kActions[0] };

// The bindings as the player has them. [0], [1]: Key 1 and Key 2; [2], [3]: the pad's.
static int g_bind[ACTION_COUNT][4];

static void ResetBindings() {
    for (int a = 0; a < ACTION_COUNT; a++) {
        g_bind[a][0] = kActions[a].key[0];
        g_bind[a][1] = kActions[a].key[1];
        g_bind[a][2] = kActions[a].pad[0];
        g_bind[a][3] = kActions[a].pad[1];
    }
}

// ───────────────────────────── Names ─────────────────────────────

// Short keyboard names for the chips; SDL's names otherwise. The 1997 binding
// screen keeps its own names.
static void KeyName(int code, char *out, size_t n) {
    if (!code) {
        out[0] = 0;
        return;
    }
    const char *sdl = Engine_ScancodeName(code);
    static const struct {
        const char *from, *to;
    } kShort[] = {
        {"Left Alt", "Alt"},       {"Right Alt", "Alt Gr"},  {"Left Ctrl", "Ctrl"},   {"Right Ctrl", "R Ctrl"},
        {"Left Shift", "Shift"},   {"Right Shift", "R Shift"}, {"Left GUI", "Cmd"},  {"Right GUI", "R Cmd"},
        {"Return", "Enter"},       {"Keypad Enter", "NumEnt"}, {"Backspace", "Bksp"}, {"Delete", "Del"},
        {"Insert", "Ins"},         {"PageUp", "PgUp"},       {"PageDown", "PgDn"},   {"Escape", "Esc"},
        {"CapsLock", "Caps"},      {"Numlock", "NumLk"},
    };
    for (size_t i = 0; i < sizeof kShort / sizeof kShort[0]; i++) {
        if (!strcmp(sdl, kShort[i].from)) {
            snprintf(out, n, "%s", kShort[i].to);
            return;
        }
    }
    if (!strncmp(sdl, "Keypad ", 7)) {
        snprintf(out, n, "Num %s", sdl + 7);
        return;
    }
    snprintf(out, n, "%s", sdl);
}

// The pad's own names: Xbox, PlayStation and Nintendo families, by position.
static int PadFamily() {
    if (sw_padnames)
        return sw_padnames;
    int f = Engine_PadFamily();
    return f ? f : 1;
}

static const char *kPadFamilyNames[4] = {"", "Xbox", "PlayStation", "Nintendo"};

static void PadName(int code, char *out, size_t n) {
    if (!code) {
        out[0] = 0;
        return;
    }
    // A, B, X, Y, back, guide, start, LS, RS, LB, RB, then the d-pad.
    static const char *kFaces[4][11] = {
        {0},
        {"A", "B", "X", "Y", "View", "Guide", "Menu", "LS", "RS", "LB", "RB"},
        {"Cross", "Circle", "Square", "Triangle", "Create", "PS", "Options", "L3", "R3", "L1", "R1"},
        {"B", "A", "Y", "X", "-", "Home", "+", "LS", "RS", "L", "R"},
    };
    static const char *kTriggers[4][2] = {{0}, {"LT", "RT"}, {"L2", "R2"}, {"ZL", "ZR"}};
    // CP437 arrows in the 8x8 font: up, down, left, right.
    static const char kArrows[4] = {'\x18', '\x19', '\x1b', '\x1a'};
    int f = PadFamily(), i = code - PAD_CODE_BASE;
    if (i <= 10)
        snprintf(out, n, "%s", kFaces[f][i]);
    else if (i <= 14)
        snprintf(out, n, "D-pad %c", kArrows[i - 11]);
    else if (i <= 18)
        snprintf(out, n, "L stick %c", kArrows[i - 15]);
    else if (i <= 22)
        snprintf(out, n, "R stick %c", kArrows[i - 19]);
    else
        snprintf(out, n, "%s", kTriggers[f][i - 23]);
}

// What one chip says. The pad's column shows both of its bindings when it has two.
static void ChipText(int action, int column, char *out, size_t n) {
    if (column < 2) {
        KeyName(g_bind[action][column], out, n);
        return;
    }
    char a[24], b[24];
    PadName(g_bind[action][2], a, sizeof a);
    PadName(g_bind[action][3], b, sizeof b);
    if (a[0] && b[0]) {
        // One stick's two ways read as one name: "R stick <>".
        if (!strncmp(a, b, 8) && strlen(a) == 9 && strlen(b) == 9)
            snprintf(out, n, "%.8s%c%c", a, a[8], b[8]);
        else
            snprintf(out, n, "%s / %s", a, b);
    } else {
        snprintf(out, n, "%s", a[0] ? a : b);
    }
}

// Another action with the same key on the same device, or -1.
static int ConflictOf(int action, int column) {
    int lo = column < 2 ? 0 : 2;
    int code = g_bind[action][column < 2 ? column : 2];
    if (!code)
        return -1;
    for (int a = 0; a < ACTION_COUNT; a++) {
        if (a == action)
            continue;
        if (g_bind[a][lo] == code || g_bind[a][lo + 1] == code)
            return a;
    }
    if (column >= 2 && g_bind[action][3]) {
        int code2 = g_bind[action][3];
        for (int a = 0; a < ACTION_COUNT; a++)
            if (a != action && (g_bind[a][2] == code2 || g_bind[a][3] == code2))
                return a;
    }
    return -1;
}

// ───────────────────────────── State ─────────────────────────────

static bool g_active;
static int g_device;      // 0 keyboard, 1 gamepad (one device per view)
static bool g_focusRight; // wide: the actions; one column: the actions page is open
static int g_group;       // wide: the left cursor; one column: the groups page cursor - 1
static int g_colCursor;   // one column, groups page: 0 is the Return row
static int g_row;         // the action within the group
static int g_column;      // index into Columns()
static int g_scroll;
static bool g_waiting;
static U32 g_waitStart, g_waitFrame;
static char g_lastBound[96];
static bool g_bindingsReady;

static bool Wide() { return Engine_LogicalW() >= 840; }

// The columns this view shows: 0 Key 1, 1 Key 2, 2 the pad.
static int Columns(int *out) {
    if (!sw_devices) {
        out[0] = 0, out[1] = 1, out[2] = 2;
        return 3;
    }
    if (g_device == 0) {
        out[0] = 0, out[1] = 1;
        return 2;
    }
    out[0] = 2;
    return 1;
}

static const char *ColumnHead(int column) {
    static const char *kHeads[3] = {"Key 1", "Key 2", "Gamepad"};
    if (column == 2 && sw_devices)
        return "Button";
    return kHeads[column];
}

static int GroupFirst(int g) {
    for (int a = 0; a < ACTION_COUNT; a++)
        if (kActions[a].group == g)
            return a;
    return 0;
}

static int GroupCount(int g) {
    int n = 0;
    for (int a = 0; a < ACTION_COUNT; a++)
        n += kActions[a].group == g;
    return n;
}

static int CursorAction() { return GroupFirst(g_group) + g_row; }

static int CursorColumn() {
    int cols[3];
    int n = Columns(cols);
    if (g_column >= n)
        g_column = n - 1;
    return cols[g_column];
}

static const char *DeviceName(int d) { return d ? "Gamepad" : "Keyboard"; }

void Controls_Open() {
    g_active = true;
    g_device = Engine_LastInputWasPad() ? 1 : 0;
    g_focusRight = false;
    g_group = 0;
    g_colCursor = 1;
    g_row = 0;
    g_column = 0;
    g_scroll = 0;
    g_waiting = false;
    g_lastBound[0] = 0;
    if (!g_bindingsReady) {
        ResetBindings();
        g_bindingsReady = true;
    }
    Menu_PageEntered();
    Trace("controls open device=%s", DeviceName(g_device));
}

void Controls_Close() {
    if (!g_active)
        return;
    g_active = false;
    g_waiting = false;
    Menu_PageEntered();
    Trace("controls close");
}

bool Controls_Active() { return g_active; }

void Controls_Reset() {
    g_active = g_waiting = g_focusRight = false;
    g_device = g_group = g_colCursor = g_row = g_column = g_scroll = 0;
    g_lastBound[0] = 0;
    g_bindingsReady = false;
}

// ───────────────────────────── Input ─────────────────────────────

static void Bind(int action, int column, int code) {
    char name[32], was[32];
    if (column < 2) {
        KeyName(g_bind[action][column], was, sizeof was);
        g_bind[action][column] = code;
        if (g_bind[action][1 - column] == code)
            g_bind[action][1 - column] = 0;  // one key in both slots says nothing
        KeyName(code, name, sizeof name);
    } else {
        PadName(g_bind[action][2], was, sizeof was);
        g_bind[action][2] = code;
        g_bind[action][3] = 0;
        PadName(code, name, sizeof name);
    }
    int other = ConflictOf(action, column);
    Trace("bind action=\"%s\" column=\"%s\" key=\"%s\" was=\"%s\"%s%s%s", kActions[action].name, ColumnHead(column),
          name, was[0] ? was : "-", other >= 0 ? " conflict=\"" : "", other >= 0 ? kActions[other].name : "",
          other >= 0 ? "\"" : "");
    snprintf(g_lastBound, sizeof g_lastBound, "%s", name);
}

static void StartWaiting() {
    g_waiting = true;
    g_waitStart = Engine_TimeMs();
    g_waitFrame = Engine_Frame();
    Trace("controls wait action=\"%s\" column=\"%s\"", kActions[CursorAction()].name, ColumnHead(CursorColumn()));
}

static void UpdateWaiting() {
    int column = CursorColumn();
    S32 raw = Engine_RawPressed();
    if (Engine_Frame() == g_waitFrame)
        return;  // the press that started the wait
    bool cancel = raw == K_ESC || (raw == 0 && Engine_Pressed(KEY_ESC));
    if (cancel) {
        g_waiting = false;
        Trace("controls wait cancelled");
        return;
    }
    if (raw && ((column < 2) == (raw < PAD_CODE_BASE))) {
        Bind(CursorAction(), column, raw);
        g_waiting = false;
        return;
    }
    if (Engine_TimeMs() - g_waitStart > 5000) {
        g_waiting = false;
        Trace("controls wait timed out");
    }
}

static void SwitchDevice() {
    if (!sw_devices)
        return;
    g_device ^= 1;
    g_column = 0;
    Menu_PageEntered();
    Trace("controls device=%s", DeviceName(g_device));
}

static void Say();

void Controls_Update() {
    if (g_waiting) {
        UpdateWaiting();
        Say();
        return;
    }
    int cols[3];
    int ncols = Columns(cols);
    int nrows = GroupCount(g_group);
    bool onGroups = !g_focusRight;
    bool column = !Wide();

    if (Engine_Pressed(KEY_TAB))
        SwitchDevice();

    if (onGroups) {
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
            Controls_Close();
            return;
        }
        if (open) {
            g_focusRight = true;
            g_row = 0;
            g_column = 0;
            g_scroll = 0;
            Menu_PageEntered();
            Trace("controls group=%s", kGroups[g_group].name);
        } else if (Engine_Pressed(KEY_ESC)) {
            Controls_Close();
            return;
        }
    } else {
        if (Engine_Pressed(KEY_UP))
            g_row = (g_row + nrows - 1) % nrows;
        if (Engine_Pressed(KEY_DOWN))
            g_row = (g_row + 1) % nrows;
        if (Engine_Pressed(KEY_RIGHT) && g_column + 1 < ncols)
            g_column++;
        if (Engine_Pressed(KEY_LEFT)) {
            if (g_column > 0)
                g_column--;
            else if (!column)
                g_focusRight = false, Menu_PageEntered();
        }
        if (g_focusRight && Engine_Pressed(KEY_ENTER)) {
            StartWaiting();
        } else if (g_focusRight && Engine_Pressed(KEY_DEL)) {
            int a = CursorAction(), c = CursorColumn();
            if (c < 2)
                g_bind[a][c] = 0;
            else
                g_bind[a][2] = g_bind[a][3] = 0;
            Trace("clear action=\"%s\" column=\"%s\"", kActions[a].name, ColumnHead(c));
        } else if (Engine_Pressed(KEY_ESC)) {
            g_focusRight = false;
            Menu_PageEntered();
            Trace("controls back to groups");
        }
    }
    Say();
}

static void Say() {
    static char buf[256];
    const char *text = "";
    if (g_waiting) {
        snprintf(buf, sizeof buf, "Press a %s for %s", CursorColumn() < 2 ? "key" : "button",
                 kActions[CursorAction()].name);
        text = buf;
    } else if (!g_focusRight) {
        if (Wide() || g_colCursor > 0)
            text = kGroups[g_group].help;
    } else {
        int a = CursorAction(), c = CursorColumn();
        int other = ConflictOf(a, c);
        if (other >= 0) {
            char name[32];
            ChipText(a, c, name, sizeof name);
            snprintf(buf, sizeof buf, "%s %s is also %s.", kActions[a].help, name, kActions[other].name);
            text = buf;
        } else {
            text = kActions[a].help;
        }
    }
    Menu_Say(text);
}

// ───────────────────────────── Drawing ─────────────────────────────

struct DrawnChip {
    int action, column;
    char name[32];
    int x0, y0, x1, y1;
    const char *fill;
    bool conflict, fits;
};
static DrawnChip g_chips[32];
static int g_chipCount;

static S32 ChipScale() { return Menu_GetSwitch("chip"); }

// Chip x positions for this view, right-aligned 12 in from the row's right edge.
static void ChipLayout(S32 rowX1, const int *cols, int ncols, S32 *x0, S32 *w) {
    S32 s = ChipScale();
    S32 chipW = 60 * s, step = chipW + 8;
    S32 right = rowX1 - 12;
    if (ncols == 1) {
        w[0] = 2 * chipW + 8;
        x0[0] = right - w[0] + 1;
        return;
    }
    for (int i = ncols - 1; i >= 0; i--) {
        w[i] = chipW;
        x0[i] = right - chipW + 1 - (ncols - 1 - i) * step;
    }
    (void)cols;
}

static void DrawActionRow(int action, S32 x0, S32 y0, S32 x1, S32 y1, const int *cols, int ncols, bool cursorRow,
                          const char *pane) {
    S32 yc = (y0 + y1) / 2;
    Ui_RowBackground(x0, y0, x1, y1, FILL_SHADE);
    MemoClip();
    SetClip(x0 + 1, y0, x1 - 1, y1);
    Ui_Text(x0 + 16, yc - 18, kActions[action].name, INK_WHITE);
    RestoreClip();

    S32 cx[3], cw[3];
    ChipLayout(x1, cols, ncols, cx, cw);
    S32 s = ChipScale(), ch = Ui_KeyChipHeight(s);
    DrawnRow &d = Menu_Record(pane, kActions[action].name, "", FILL_SHADE, x0, y0, x1, y1);
    d.textRight = x0 + 16 + Ui_TextWidth(kActions[action].name);
    if (d.textRight + 8 > cx[0])
        d.textRight = x1 + (d.textRight + 8 - cx[0]);  // the name runs into the first chip

    for (int i = 0; i < ncols; i++) {
        int c = cols[i];
        char name[32];
        ChipText(action, c, name, sizeof name);
        bool here = cursorRow && i == g_column && g_focusRight;
        ChipFill fill = here ? (g_waiting ? CHIP_WAITING : CHIP_CURSOR) : CHIP_SHADE;
        bool red = ConflictOf(action, c) >= 0;
        S32 cy = yc - ch / 2;
        bool fits = Ui_KeyChip(cx[i], cy, cw[i], name, s, fill, red);
        if (g_chipCount < 32) {
            DrawnChip &k = g_chips[g_chipCount++];
            k.action = action;
            k.column = c;
            snprintf(k.name, sizeof k.name, "%s", name);
            k.x0 = cx[i], k.y0 = cy, k.x1 = cx[i] + cw[i] - 1, k.y1 = cy + ch - 1;
            k.fill = fill == CHIP_CURSOR ? "cursor" : fill == CHIP_WAITING ? "waiting" : "shade";
            k.conflict = red;
            k.fits = fits;
        }
    }
}

static void DrawHeads(S32 nameX, S32 rowX1, const int *cols, int ncols, S32 y) {
    S32 s = ChipScale();
    S32 cx[3], cw[3];
    ChipLayout(rowX1, cols, ncols, cx, cw);
    const char *first = sw_devices ? DeviceName(g_device) : "Action";
    Ui_Small(nameX, y, first, INK_CONFIG_HEAD, s);
    for (int i = 0; i < ncols; i++)
        Ui_Small(cx[i], y, ColumnHead(cols[i]), INK_CONFIG_HEAD, s);
}

static void Legend(const char **keys, const char **words, int *n) {
    static char other[16];
    *n = 0;
    if (g_waiting) {
        keys[*n] = "Esc", words[(*n)++] = "Cancel";
        return;
    }
    if (g_focusRight) {
        keys[*n] = "Enter", words[(*n)++] = "Change";
        keys[*n] = "Del", words[(*n)++] = "Clear";
    } else {
        keys[*n] = "Enter", words[(*n)++] = "Open";
    }
    if (sw_devices) {
        snprintf(other, sizeof other, "%s", DeviceName(!g_device));
        keys[*n] = "Tab", words[(*n)++] = other;
    }
    if (!g_focusRight)
        keys[*n] = "Esc", words[(*n)++] = "Back";
}

// Rows for the actions of the group, scrolled to keep the cursor in view.
static void DrawActions(S32 x0, S32 x1, S32 top, S32 limit, const char *pane) {
    int cols[3];
    int ncols = Columns(cols);
    int n = GroupCount(g_group);
    int visible = (limit - top + ROW_GAP) / ROW_PITCH;
    if (visible > n)
        visible = n;
    if (g_row < g_scroll)
        g_scroll = g_row;
    if (g_row >= g_scroll + visible)
        g_scroll = g_row - visible + 1;
    if (!g_focusRight)
        g_scroll = 0;
    DrawHeads(x0 + 18, x1, cols, ncols, top - 22);
    S32 y = top;
    int first = GroupFirst(g_group);
    for (int i = g_scroll; i < g_scroll + visible; i++) {
        DrawActionRow(first + i, x0, y, x1, y + ROW_H, cols, ncols, i == g_row, pane);
        y += ROW_PITCH;
    }
    if (visible < n)
        Menu_DrawScrollBar(x1 + 9, top, top + visible * ROW_PITCH - ROW_GAP - 1, g_scroll, visible, n);
}

static void DrawWideControls() {
    S32 W = (S32)Engine_LogicalW();
    const S32 side = 32, top = 28, gap = 16;
    S32 inner = W - 2 * side - gap;
    S32 leftW = inner * 304 / 880;
    S32 lx0 = side, lx1 = side + leftW - 1;
    S32 rx0 = lx1 + 1 + gap, rx1 = W - side - 1;

    S32 y = top;
    for (int g = 0; g < GROUP_COUNT; g++) {
        RowFill fill = FILL_SHADE;
        if (g == g_group)
            fill = g_focusRight ? FILL_CURRENT : FILL_CURSOR;
        Ui_RowBackground(lx0, y, lx1, y + ROW_H, fill);
        Ui_Text(lx0 + 16, y + ROW_H / 2 - 18, kGroups[g].name, INK_WHITE);
        DrawnRow &d = Menu_Record("controls-left", kGroups[g].name, "", fill, lx0, y, lx1, y + ROW_H);
        d.textRight = lx0 + 16 + Ui_TextWidth(kGroups[g].name);
        y += ROW_PITCH;
    }
    DrawActions(rx0, rx1, top, Menu_WideRowsLimit(), "controls-right");

    const char *keys[4], *words[4];
    int nk;
    Legend(keys, words, &nk);
    Menu_DrawFoot(keys, words, nk);
}

static void DrawColumnControls() {
    S32 W = (S32)Engine_LogicalW(), H = (S32)Engine_LogicalH();
    S32 x0 = W / 2 - 275, x1 = W / 2 + 275;
    if (!g_focusRight) {
        // The groups, as a 1997 sub-menu: a Return row, then one row each.
        bool help = Menu_GetSwitch("help640") != 0;
        S32 top = 16, bottom = help ? Menu_DialogueTop() - 16 : H - 16;
        int n = GROUP_COUNT + 1;
        int visible = (bottom - top + ROW_GAP) / ROW_PITCH;
        if (visible > n)
            visible = n;
        if (g_colCursor < g_scroll)
            g_scroll = g_colCursor;
        if (g_colCursor >= g_scroll + visible)
            g_scroll = g_colCursor - visible + 1;
        S32 stackH = visible * ROW_PITCH - ROW_GAP;
        S32 y = top + (bottom - top - stackH) / 2;
        S32 y0 = y;
        for (int i = g_scroll; i < g_scroll + visible; i++) {
            const char *label = i == 0 ? "Return to previous menu" : kGroups[i - 1].name;
            RowFill fill = i == g_colCursor ? FILL_CURSOR : FILL_SHADE;
            Ui_RowBackground(x0, y, x1, y + ROW_H, fill);
            Menu_RowLabelCentred(x0, x1, y + ROW_H / 2, label, INK_WHITE);
            DrawnRow &d = Menu_Record("column", label, "", fill, x0, y, x1, y + ROW_H);
            d.textRight = (x0 + x1) / 2 + Ui_TextWidth(label) / 2 + 8;
            y += ROW_PITCH;
        }
        if (visible < n)
            Menu_DrawScrollBar(x1 + 9, y0, y0 + stackH - 1, g_scroll, visible, n);
        if (help)
            Menu_DrawDialogue();
        return;
    }
    // The actions page: column heads, the rows, and the voice below (it asks for the key).
    DrawActions(x0, x1, 28, Menu_DialogueTop() - 16, "column");
    Menu_DrawDialogue();
}

void Controls_Draw() {
    g_chipCount = 0;
    if (Wide())
        DrawWideControls();
    else
        DrawColumnControls();
}

// ───────────────────────────── State ─────────────────────────────

int Controls_Chips(ChipView *out, int max) {
    int n = g_chipCount < max ? g_chipCount : max;
    for (int i = 0; i < n; i++) {
        out[i].action = kActions[g_chips[i].action].name;
        out[i].column = ColumnHead(g_chips[i].column);
        out[i].name = g_chips[i].name;
        out[i].fits = g_chips[i].fits;
    }
    return n;
}

void Controls_WriteState(FILE *f) {
    char buf[32];
    int a = CursorAction(), c = CursorColumn();
    fprintf(f, "{\"device\":\"%s\",\"devices\":%d,\"pad_names\":\"%s\",\"focus\":\"%s\",\"group\":", DeviceName(g_device),
            sw_devices, kPadFamilyNames[PadFamily()], g_focusRight ? "actions" : "groups");
    Menu_JsonString(f, kGroups[g_group].name);
    fprintf(f, ",\"action\":");
    Menu_JsonString(f, kActions[a].name);
    fprintf(f, ",\"column\":");
    Menu_JsonString(f, ColumnHead(c));
    fprintf(f, ",\"waiting\":%s,\"scroll\":%d,\"chips\":[", g_waiting ? "true" : "false", g_scroll);
    for (int i = 0; i < g_chipCount; i++) {
        const DrawnChip &k = g_chips[i];
        fprintf(f, "%s{\"action\":", i ? "," : "");
        Menu_JsonString(f, kActions[k.action].name);
        fprintf(f, ",\"column\":");
        Menu_JsonString(f, ColumnHead(k.column));
        fprintf(f, ",\"name\":");
        Menu_JsonString(f, k.name);
        fprintf(f, ",\"rect\":[%d,%d,%d,%d],\"fill\":\"%s\",\"conflict\":%s,\"fits\":%s}", k.x0, k.y0, k.x1, k.y1,
                k.fill, k.conflict ? "true" : "false", k.fits ? "true" : "false");
    }
    fprintf(f, "],\"bindings\":{");
    for (int i = 0; i < ACTION_COUNT; i++) {
        fprintf(f, "%s", i ? "," : "");
        Menu_JsonString(f, kActions[i].name);
        fprintf(f, ":[");
        for (int col = 0; col < 3; col++) {
            ChipText(i, col, buf, sizeof buf);
            fprintf(f, "%s", col ? "," : "");
            Menu_JsonString(f, buf);
        }
        fprintf(f, "]");
    }
    fprintf(f, "},\"conflicts\":[");
    bool firstOut = true;
    for (int i = 0; i < ACTION_COUNT; i++) {
        for (int col = 0; col < 3; col++) {
            int other = ConflictOf(i, col);
            if (other < 0)
                continue;
            fprintf(f, "%s[", firstOut ? "" : ",");
            Menu_JsonString(f, kActions[i].name);
            fprintf(f, ",");
            Menu_JsonString(f, ColumnHead(col));
            fprintf(f, ",");
            Menu_JsonString(f, kActions[other].name);
            fprintf(f, "]");
            firstOut = false;
        }
    }
    fprintf(f, "]}");
}
