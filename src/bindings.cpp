#include "bindings.h"

#include "host.h"
#include "menu.h"
#include "menu_parts.h"
#include "trace.h"
#include "ui.h"

#include <SVGA/CLIP.H>
#include <SYSTEM/KEYBOARD_KEYS.H>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

int sw_padnames = 0;  // 0 the connected pad's names; 1 Xbox, 2 PlayStation, 3 Nintendo

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
    {"Behaviors", "One key for each of the four behaviors."},
    {"Weapons", "One key for each weapon."},
    {"Magic and items", "The penguin, the jetpack and the spells."},
    {"Camera", "Recentering, turning and the camera's height."},
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

    {"Behavior action", 1, {K_SPACE, K_NUMPAD_5}, {PAD_X, 0}, "Does what the behavior does: jump, hit or hide."},
    {"Talk and search", 1, {K_W, K_GRAY_END}, {PAD_A, 0}, "Talks to people and searches where you stand."},
    {"Use weapon", 1, {K_ALT, 0}, {PAD_RB, 0}, "Uses the weapon you hold."},
    {"Behavior menu", 1, {K_CTRL, 0}, {PAD_LB, 0}, "Hold it to choose a behavior."},
    {"Inventory", 1, {K_SHIFT, 0}, {PAD_Y, 0}, "Opens the inventory."},
    {"Holomap", 1, {K_H, K_TAB}, {PAD_BACK, 0}, "Opens the holomap."},

    {"Normal", 2, {K_F5, 0}, {0, 0}, "Switches to the Normal behavior."},
    {"Athletic", 2, {K_F6, 0}, {0, 0}, "Switches to the Athletic behavior."},
    {"Aggressive", 2, {K_F7, 0}, {0, 0}, "Switches to the Aggressive behavior."},
    {"Discreet", 2, {K_F8, 0}, {0, 0}, "Switches to the Discreet behavior."},

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

    {"Recenter", 5, {K_ENTER, K_NUMPAD_ENTER}, {PAD_B, 0}, "Puts the camera back behind Twinsen."},
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

// Start always opens the game menu, as Esc does on the keyboard: a pad-only player
// (Xbox, Switch, Vita, Android TV) has no other way back to these options. Its chip
// on the pad view is fixed, and Start is never bound to anything else.
static bool FixedChip(int action, int column) {
    return column >= 2 && !strcmp(kActions[action].name, "Game menu");
}

// ───────────────────────────── Names ─────────────────────────────

// Short keyboard names for the chips; SDL's names otherwise. The 1997 binding
// screen keeps its own names.
static void KeyName(int code, char *out, size_t n) {
    if (!code) {
        out[0] = 0;
        return;
    }
    const char *sdl = Host_ScancodeName(code);
    static const struct {
        const char *from, *to;
    } kShort[] = {
        {"Left Alt", "Alt"},       {"Right Alt", "Alt Gr"},  {"Left Ctrl", "Ctrl"},   {"Right Ctrl", "R Ctrl"},
        {"Left Shift", "Shift"},   {"Right Shift", "RShift"}, {"Left GUI", "Cmd"},  {"Right GUI", "R Cmd"},
        {"Return", "Enter"},       {"Keypad Enter", "NumEnt"}, {"Backspace", "Bksp"}, {"Delete", "Del"},
        {"Insert", "Ins"},         {"PageUp", "PgUp"},       {"PageDown", "PgDn"},   {"Escape", "Esc"},
        {"CapsLock", "Caps"},      {"Numlock", "NumLk"},
        // A chip holds 6 letters: the keys whose SDL names are longer.
        {"PrintScreen", "PrtSc"},  {"ScrollLock", "ScrLk"},
        {"NonUSBackslash", "ISO \\"}, {"Application", "Apps"},  // SDL has a "Menu" key too
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
    int f = Host_PadFamily();
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

// A key or button as a sentence names it, as the 1997 text does ("<ESC> exit, <ENTER>
// Select a key", "press <SHIFT> or simply press <TAB>"): in capitals inside angle
// brackets. A chip keeps the name as 1997's key list writes it ("Enter", "Left shift").
// The game font has no arrows or +, so they are said: "<D-PAD UP>", "<PLUS>".
void Bindings_SaidKey(const char *name, char *out, size_t n) {
    static const struct {
        char c;
        const char *word;
    } kSaid[] = {{'\x18', "UP"}, {'\x19', "DOWN"}, {'\x1b', "LEFT"}, {'\x1a', "RIGHT"}, {'+', "PLUS"}};
    size_t o = 0;
    if (o + 1 < n)
        out[o++] = '<';
    for (const char *p = name; *p && o + 1 < n; p++) {
        const char *word = NULL;
        for (size_t i = 0; i < sizeof kSaid / sizeof kSaid[0]; i++)
            if (*p == kSaid[i].c)
                word = kSaid[i].word;
        if (word) {
            for (; *word && o + 1 < n; word++)
                out[o++] = *word;
        } else {
            out[o++] = (char)toupper((unsigned char)*p);
        }
    }
    if (o + 1 < n)
        out[o++] = '>';
    out[o] = 0;
}

// Start as the voice says it: "<MENU>" on an Xbox pad, "<PLUS>" on a Nintendo one.
void Bindings_StartSaid(char *out, size_t n) {
    char name[16];
    PadName(PAD_START, name, sizeof name);
    Bindings_SaidKey(name, out, n);
}

// A binding as a sentence names it.
static void SaidCode(int column, int code, char *out, size_t n);

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

// Another action that already has this key or button on the same device, or -1;
// *slot gets which of its slots holds it.
static int HolderOf(int action, int column, int code, int *slot) {
    int lo = column < 2 ? 0 : 2;
    for (int a = 0; a < ACTION_COUNT; a++) {
        for (int k = lo; k < lo + 2; k++) {
            if (a != action && g_bind[a][k] == code) {
                if (slot)
                    *slot = k;
                return a;
            }
        }
    }
    return -1;
}


// ───────────────────────────── State ─────────────────────────────

// Where the cursor is lives on the menu's stack (Menu_TopPage, Menu_TopRow): the groups page
// (P_KEYS: its Return row, then a row for each group) or a group's page of actions. The page
// keeps only what a binding row has besides: the chip along it, and the device it shows.
static int g_device;      // 0 keyboard, 1 gamepad (one device per view)
static int g_column;      // the chip: an index into Columns()
static bool g_waiting;
static U32 g_waitStart, g_waitFrame;
static char g_lastBound[96];
// A key another action has: the page asks before it moves it,
// as Options asks before Classic or Reset settings: the voice says who has it, two
// answer rows take the actions' place, No first. Yes swaps (the other action gets
// this chip's old key, or nothing); No or Esc keeps both as they were.
static struct {
    int action = -1, column;  // the chip the key was meant for
    int code;                 // the key or button pressed
    int holder, slot;         // the action that has it, and its slot
    int answer;               // 0 No (the safe answer, first), 1 Yes
} g_swap;

static bool g_bindingsReady;

static bool Wide() { return Menu_Wide(); }

// Wide, the column names sit inside the first row, so the actions start on the top margin
// level with the groups. The one column keeps its heads above, as 1997's page.
static bool HeadsInRow() { return Wide(); }

// The columns this view shows: 0 Key 1, 1 Key 2, 2 the pad. One device per view; Tab switches.
static int Columns(int *out) {
    if (g_device == 0) {
        out[0] = 0, out[1] = 1;
        return 2;
    }
    out[0] = 2;
    return 1;
}

static const char *ColumnHead(int column) {
    static const char *kHeads[3] = {"Key 1", "Key 2", "Gamepad"};
    if (column == 2)
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

static_assert(GROUP_COUNT == KEY_GROUPS, "menu_parts.h KEY_GROUPS counts kGroups");

static bool OnGroups() { return Menu_TopPage() == P_KEYS; }

// The group whose page is open, or the one under the cursor on the groups page; -1 on its
// Return row.
static int CurrentGroup() {
    const int p = Menu_TopPage();
    return p >= P_KEY_GROUP ? p - P_KEY_GROUP : Menu_TopRow() - 1;
}

// The action under the cursor on a group's page; -1 on the groups page.
static int CursorAction() { return OnGroups() ? -1 : GroupFirst(CurrentGroup()) + Menu_TopRow(); }

// The pages, made from the tables: the groups (a Return row, then one row each, opening the
// group's page) and each group's actions, a binding row each.
static Row s_groupRows[1 + GROUP_COUNT];
static Row s_actionRows[ACTION_COUNT];

void Bindings_Pages(Page *pages) {
    s_groupRows[0] = {R_RETURN, "Return to previous menu", 0, "", NULL, NULL, NULL};
    for (int g = 0; g < GROUP_COUNT; g++)
        s_groupRows[1 + g] = {R_CATEGORY, kGroups[g].name, P_KEY_GROUP + g, kGroups[g].help, NULL, NULL, NULL};
    pages[P_KEYS] = {"Keys and buttons", s_groupRows, 1 + GROUP_COUNT};
    for (int a = 0; a < ACTION_COUNT; a++) {
        assert((a == 0 || kActions[a].group >= kActions[a - 1].group) && "a group's actions are together");
        s_actionRows[a] = {R_BINDING, kActions[a].name, a, kActions[a].help, NULL, NULL, NULL};
    }
    for (int g = 0; g < GROUP_COUNT; g++)
        pages[P_KEY_GROUP + g] = {kGroups[g].name, &s_actionRows[GroupFirst(g)], GroupCount(g)};
}

static int CursorColumn() {
    int cols[3];
    int n = Columns(cols);
    if (g_column >= n)
        g_column = n - 1;
    return cols[g_column];
}

static const char *DeviceName(int d) { return d ? "Gamepad" : "Keyboard"; }

// From the Keys and buttons row, before the menu opens the page: the device used last.
void Bindings_Open() {
    g_device = Host_LastInputWasPad() || !Host_KeyboardSeen() ? 1 : 0;
    g_column = 0;
    g_waiting = false;
    g_lastBound[0] = 0;
    g_swap.action = -1;
    if (!g_bindingsReady) {
        ResetBindings();
        g_bindingsReady = true;
    }
    Trace("bindings open device=%s", DeviceName(g_device));
}

// The page left by other means (`open` another page): nothing waits or asks.
void Bindings_Close() {
    g_waiting = false;
    g_swap.action = -1;
}

bool Bindings_Active() {
    const int p = Menu_TopPage();
    return p >= P_KEYS && p < P_KEY_GROUP + KEY_GROUPS;
}

void Bindings_GroupOpened() { g_column = 0; }

void Bindings_Reset() {
    g_waiting = false;
    g_device = g_column = 0;
    g_lastBound[0] = 0;
    g_swap.action = -1;
    g_bindingsReady = false;
}

static_assert(ACTION_COUNT * 4 <= BINDINGS_MAX, "BINDINGS_MAX holds every binding");

static void BindingsReady() {
    if (!g_bindingsReady) {
        ResetBindings();
        g_bindingsReady = true;
    }
}

int Bindings_Count() { return ACTION_COUNT * 4; }

void Bindings_Get(int *out) {
    BindingsReady();
    memcpy(out, g_bind, sizeof g_bind);
}

void Bindings_Put(const int *in) {
    memcpy(g_bind, in, sizeof g_bind);
    g_bindingsReady = true;
}

void Bindings_PutDefaults() {
    ResetBindings();
    g_bindingsReady = true;
}

bool Bindings_AreDefault() {
    if (!g_bindingsReady)
        return true;
    for (int a = 0; a < ACTION_COUNT; a++)
        if (g_bind[a][0] != kActions[a].key[0] || g_bind[a][1] != kActions[a].key[1] ||
            g_bind[a][2] != kActions[a].pad[0] || g_bind[a][3] != kActions[a].pad[1])
            return false;
    return true;
}

// ───────────────────────────── Input ─────────────────────────────

static void CodeName(int column, int code, char *out, size_t n) {
    if (column < 2)
        KeyName(code, out, n);
    else
        PadName(code, out, n);
}

static void SaidCode(int column, int code, char *out, size_t n) {
    char name[32];
    CodeName(column, code, name, sizeof name);
    if (name[0])
        Bindings_SaidKey(name, out, n);
    else
        out[0] = 0;
}

static void Bind(int action, int column, int code) {
    char name[32], was[32];
    if (FixedChip(action, column) || (column >= 2 && code == PAD_START))
        return;  // the wait never hands these over; this keeps it so
    // Nothing changes without the player's say (decided 2026-11-11, replacing the refusal
    // of 2026-11-03): a key another action has is offered as a swap, and the voice says
    // what the other action is left with.
    int slot;
    int holder = HolderOf(action, column, code, &slot);
    if (holder >= 0) {
        CodeName(column, code, name, sizeof name);
        g_swap.action = action, g_swap.column = column, g_swap.code = code;
        g_swap.holder = holder, g_swap.slot = slot, g_swap.answer = 0;
        Trace("bind asks action=\"%s\" column=\"%s\" key=\"%s\" holder=\"%s\"", kActions[action].name,
              ColumnHead(column), name, kActions[holder].name);
        return;
    }
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

// The key or button the chip had before, which the other action gets on a swap.
static int OldCode(int action, int column) { return g_bind[action][column < 2 ? column : 2]; }

static void Swap() {
    int a = g_swap.action, c = g_swap.column, h = g_swap.holder, k = g_swap.slot;
    int old = OldCode(a, c);
    char name[32], gave[32];
    CodeName(c, g_swap.code, name, sizeof name);
    CodeName(c, old, gave, sizeof gave);
    g_bind[h][k] = old;
    int other = k ^ 1;  // the holder's other slot on the same device
    if (old && g_bind[h][other] == old)
        g_bind[h][k] = 0;  // it had the old key already: one key in both slots says nothing
    g_swap.action = -1;
    Bind(a, c, g_swap.code);  // the holder no longer has it, so this goes in
    Trace("swap action=\"%s\" key=\"%s\" holder=\"%s\" gets=\"%s\"", kActions[a].name, name, kActions[h].name,
          gave[0] ? gave : "-");
}

// The question's answers: what each leaves the other action with. shown: in the Text
// language, as drawn; otherwise English, as recorded.
static void SwapAnswer(int i, char *out, size_t n, bool shown = false) {
    const Action &h = kActions[g_swap.holder];
    const char *name = shown ? Ui_Tr(h.name) : h.name;
#define TR(s) (shown ? Ui_Tr(s) : (s))
    if (i == 0) {
        snprintf(out, n, TR("No, keep it on %s"), name);
        return;
    }
    char old[32], kept[32];
    int c = g_swap.column;
    // Answers are rows, which name keys as the chips do; only the voice brackets them.
    CodeName(c, OldCode(g_swap.action, c), old, sizeof old);
    CodeName(c, g_bind[g_swap.holder][g_swap.slot ^ 1], kept, sizeof kept);
    if (old[0])
        snprintf(out, n, TR("Yes, %s gets %s"), name, old);
    else if (kept[0])
        snprintf(out, n, TR("Yes, %s keeps %s"), name, kept);
    else
        snprintf(out, n, TR("Yes, %s has no key"), name);
#undef TR
}

static void StartWaiting() {
    g_swap.action = -1;
    g_waiting = true;
    g_waitStart = Host_TimeMs();
    g_waitFrame = Host_Frame();
    Trace("bindings wait action=\"%s\" column=\"%s\"", kActions[CursorAction()].name, ColumnHead(CursorColumn()));
}

static void UpdateWaiting() {
    int column = CursorColumn();
    S32 raw = Host_RawPressed();
    if (Host_Frame() == g_waitFrame)
        return;  // the press that started the wait
    // Esc on the keyboard view, Start on the pad view: the button no binding can take.
    bool cancel = raw == K_ESC || (raw == 0 && Host_Pressed(KEY_ESC)) || (column >= 2 && raw == PAD_START);
    if (cancel) {
        g_waiting = false;
        Trace("bindings wait cancelled");
        return;
    }
    if (raw && ((column < 2) == (raw < PAD_CODE_BASE))) {
        Bind(CursorAction(), column, raw);
        g_waiting = false;
        return;
    }
    if (Host_TimeMs() - g_waitStart > 5000) {
        g_waiting = false;
        Trace("bindings wait timed out");
    }
}

void Bindings_SwitchDevice() {
    if (!Host_KeyboardSeen())  // no keyboard, no Keyboard view
        return;
    g_device ^= 1;
    g_column = 0;
    Trace("bindings device=%s", DeviceName(g_device));
}

bool Bindings_StepColumn(int dir) {
    int cols[3];
    const int n = Columns(cols);
    if (g_column + dir < 0 || g_column + dir >= n)
        return false;
    g_column += dir;
    return true;
}

void Bindings_Enter() {
    if (FixedChip(CursorAction(), CursorColumn()))
        Trace("fixed action=\"%s\"", kActions[CursorAction()].name);
    else
        StartWaiting();
}

void Bindings_Clear() {
    const int a = CursorAction(), c = CursorColumn();
    if (FixedChip(a, c)) {
        Trace("fixed action=\"%s\"", kActions[a].name);
        return;
    }
    if (c < 2)
        g_bind[a][c] = 0;
    else
        g_bind[a][2] = g_bind[a][3] = 0;
    Trace("clear action=\"%s\" column=\"%s\"", kActions[a].name, ColumnHead(c));
}

bool Bindings_Busy() { return g_waiting || g_swap.action >= 0; }

// The wait for a key, and the question about a key another action has: each takes every key.
void Bindings_UpdateBusy() {
    if (g_waiting) {
        UpdateWaiting();
    } else {
        if (Host_Pressed(KEY_UP) || Host_Pressed(KEY_DOWN)) {
            g_swap.answer ^= 1;
            Trace("cursor %s", g_swap.answer ? "Yes" : "No");
        }
        if (Host_Pressed(KEY_ENTER) && g_swap.answer) {
            Trace("answer yes");
            Swap();
        } else if (Host_Pressed(KEY_ENTER) || Host_Pressed(KEY_ESC)) {
            g_swap.action = -1;
            Trace("answer no");
        }
    }
    Bindings_Say();
}

void Bindings_Say() {
    static char buf[256];
    const char *text = "";
    if (g_waiting) {
        // The way out, said: Esc on the keyboard, Start on the pad, the two inputs no
        // binding can take (decided 2026-11-11, replacing "or wait to keep it" of
        // 2026-11-03). The action is the row under the cursor, so the prompt fits one line.
        if (CursorColumn() < 2) {
            text = "Press a key, or <ESC> to keep it.";  // 1997: "Press a key or <ESC> to cancel"
        } else {
            char start[16];
            Bindings_StartSaid(start, sizeof start);
            snprintf(buf, sizeof buf, Menu_Tr("Press a button, or %s to keep it."), start);
            text = buf;
        }
    } else if (OnGroups()) {
        if (CurrentGroup() >= 0)
            text = kGroups[CurrentGroup()].help;
    } else {
        int a = CursorAction(), c = CursorColumn();
        int other = ConflictOf(a, c);
        if (g_swap.action >= 0 && (g_swap.action != a || g_swap.column != c))
            g_swap.action = -1;  // the cursor moved on
        if (g_swap.action < 0 && FixedChip(a, c)) {
            char start[16];
            Bindings_StartSaid(start, sizeof start);
            snprintf(buf, sizeof buf, Menu_Tr("%s always opens the game menu, so it can't be changed."), start);
            text = buf;
        } else if (g_swap.action >= 0) {
            const Action &h = kActions[g_swap.holder];
            char key[32], where[64] = "";
            SaidCode(c, g_swap.code, key, sizeof key);
            // One pane at a time shows one group: name the other action's when it is not this
            // one. What each answer leaves it with is on the answer rows.
            if (h.group != CurrentGroup())
                snprintf(where, sizeof where, Menu_Tr(", in %s"), Ui_Tr(kGroups[h.group].name));
            snprintf(buf, sizeof buf, Menu_Tr("%s is on %s%s."), key, Ui_Tr(h.name), where);
            text = buf;
        } else if (other >= 0) {
            char name[32];
            ChipText(a, c, name, sizeof name);
            // One message at a time (decided 2026-11-03): the note replaces the help.
            snprintf(buf, sizeof buf, Menu_Tr("%s is also %s."), name, Ui_Tr(kActions[other].name));
            text = buf;
        } else {
            text = kActions[a].help;
        }
    }
    if (text == buf)
        Menu_SayAsIs(text);  // built from formats already in the Text language
    else
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
// The column heads, above the actions or inside the first row, in the 8x8 font.
static struct {
    const char *text;
    int x0, y0, x1, y1;
} g_heads[4];
static int g_headCount;

static S32 ChipScale() { return 2; }  // the 8x8 key font at 2x (decided 2026-11-03)

// Chip x positions for this view. Wide, the last chip's right frame is the value edge, 24
// in from the row's right edge, where every value ends (docs/UX.md, the grid); the one
// column has no value edge and keeps them 12 in, to leave the centred names more room.
static void ChipLayout(S32 rowX1, const int *cols, int ncols, S32 *x0, S32 *w) {
    S32 s = ChipScale();
    // One column: 58 holds the same 6 letters of the 8x8 font as 60 (and the pad view's one
    // chip the same 14), and leaves "Behavior action" 8 clear of its chip (decided 2026-11-03).
    S32 chipW = (Wide() ? 60 : 58) * s, step = chipW + 8;
    S32 right = rowX1 - (Wide() ? 24 : 12);
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

// A chip is 24 tall at 2x on a 51-tall row, so it can't be centred exactly: 13 above it
// and 12 below, or 12 and 13. With heads inside the row it takes the lower place, which
// gives a head 1 px more and puts a 2x key name on the action name's cap band.
static S32 ChipTop(S32 yc, S32 s) { return yc - Ui_KeyChipHeight(s) / 2 + (HeadsInRow() ? 1 : 0); }

static void DrawActionRow(int action, S32 x0, S32 y0, S32 x1, S32 y1, const int *cols, int ncols, bool cursorRow,
                          const char *pane) {
    S32 yc = (y0 + y1) / 2;
    Ui_RowBackground(x0, y0, x1, y1, FILL_SHADE);
    S32 cx[3], cw[3];
    ChipLayout(x1, cols, ncols, cx, cw);
    // The name up to 8 before the first chip; wider, it swings while the row has the cursor.
    const ClipSave clip;
    SetClip(x0 + 1, y0, x1 - 1, y1);
    Ui_TextInRoom(x0 + 16, cx[0] - 8 - 1, yc - 18, Ui_Tr(kActions[action].name), INK_WHITE, cursorRow);
    clip.Restore();

    S32 s = ChipScale(), ch = Ui_KeyChipHeight(s);
    DrawnRow &d = Menu_Record(pane, kActions[action].name, "", FILL_SHADE, x0, y0, x1, y1);
    d.textLeft = x0 + 16;
    d.textRight = x0 + 16 + Ui_TextWidth(kActions[action].name);
    if (d.textRight + 8 > cx[0])
        d.textRight = x1 + (d.textRight + 8 - cx[0]);  // the name runs into the first chip

    for (int i = 0; i < ncols; i++) {
        int c = cols[i];
        char name[32];
        ChipText(action, c, name, sizeof name);
        bool here = cursorRow && i == g_column;
        ChipFill fill = here ? (g_waiting ? CHIP_WAITING : CHIP_CURSOR) : CHIP_SHADE;
        bool red = ConflictOf(action, c) >= 0;
        S32 cy = ChipTop(yc, s);
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
        d.valueRight = cx[i] + cw[i] - 1;
    }
}

static void DrawHead(S32 x, S32 y, const char *text, S32 s) {
    const char *shown = Ui_Tr(text);  // the record keeps the English
    Ui_Small(x, y, shown, INK_CONFIG_HEAD, s);
    if (g_headCount < 4) {
        g_heads[g_headCount].text = text;
        g_heads[g_headCount].x0 = x, g_heads[g_headCount].y0 = y;
        g_heads[g_headCount].x1 = x + Ui_SmallWidth(shown, s) - 1, g_heads[g_headCount].y1 = y + 8 * s - 1;
        g_headCount++;
    }
}

static void DrawHeads(S32 nameX, S32 rowX1, const int *cols, int ncols, S32 y) {
    S32 s = ChipScale();
    S32 cx[3], cw[3];
    ChipLayout(rowX1, cols, ncols, cx, cw);
    DrawHead(nameX, y, DeviceName(g_device), s);
    for (int i = 0; i < ncols; i++)
        DrawHead(cx[i], y, ColumnHead(cols[i]), s);
}

// The column names inside a row from y0, each over its chip at 1x: 3 above the chip, and no
// nearer than 3 to the row's top frame. A 2x name (16) doesn't fit in the 13 above a 2x chip;
// at 1x the cell's last row, a descender, comes within 2 of it. With no device head, the
// names say the device: Key 1 and Key 2 are keys, and the pad's column says Gamepad, as in
// the side-by-side view, not Button.
static void DrawHeadsInRow(S32 rowY0, S32 rowX1, const int *cols, int ncols) {
    S32 cx[3], cw[3];
    ChipLayout(rowX1, cols, ncols, cx, cw);
    S32 y = ChipTop(rowY0 + ROW_H / 2, ChipScale()) - 3 - 8;
    if (y < rowY0 + 4)
        y = rowY0 + 4;
    for (int i = 0; i < ncols; i++)
        DrawHead(cx[i], y, cols[i] == 2 ? DeviceName(1) : ColumnHead(cols[i]), 1);
}

static void Legend(const char **keys, const char **words, int *n) {
    static char other[16];
    *n = 0;
    if (g_waiting) {
        static char start[16];
        if (CursorColumn() < 2)
            snprintf(start, sizeof start, "Esc");
        else
            PadName(PAD_START, start, sizeof start);
        keys[*n] = start, words[(*n)++] = "Cancel";
        return;
    }
    if (g_swap.action >= 0) {  // as Options' questions
        keys[*n] = "\x18\x19", words[(*n)++] = "Choose";
        keys[*n] = "Enter", words[(*n)++] = "Answer";
        return;
    }
    if (!OnGroups() && !FixedChip(CursorAction(), CursorColumn())) {
        keys[*n] = "Enter", words[(*n)++] = "Change";
        keys[*n] = "Del", words[(*n)++] = "Clear";
    } else if (OnGroups()) {
        keys[*n] = "Enter", words[(*n)++] = "Open";
    }
    if (Host_KeyboardSeen()) {
        snprintf(other, sizeof other, "%s", DeviceName(!g_device));
        keys[*n] = "Tab", words[(*n)++] = other;
    }
    if (OnGroups())
        keys[*n] = "Esc", words[(*n)++] = "Back";
}

// The column heads from y top, then rows for the actions of the group 22 below, scrolled
// to keep the cursor in view; with the heads inside the first row, the rows from y top.
// The swap question's two answers in place of the actions, as Options draws its
// questions: wide, labels on the pane's label edge; one column, centred.
static void DrawSwapAnswers(S32 x0, S32 x1, S32 top, const char *pane) {
    bool centred = !Wide();
    for (int i = 0; i < 2; i++) {
        char label[96], shown[128];
        SwapAnswer(i, label, sizeof label);
        SwapAnswer(i, shown, sizeof shown, true);
        S32 y0 = top + i * ROW_PITCH, yc = y0 + ROW_H / 2;
        RowFill fill = i == g_swap.answer ? FILL_CURSOR : FILL_SHADE;
        Ui_RowBackground(x0, y0, x1, y0 + ROW_H, fill);
        DrawnRow &d = Menu_Record(pane, label, "", fill, x0, y0, x1, y0 + ROW_H);
        if (centred) {
            Menu_RowLabelCentred(x0, x1, yc, shown, INK_WHITE, true, i == g_swap.answer);
            d.textLeft = (x0 + x1) / 2 - Ui_TextWidthAsIs(shown) / 2;
            d.textRight = d.textLeft + Ui_TextWidthAsIs(shown);
        } else {
            const ClipSave clip;
            SetClip(x0 + 1, y0, x1 - 1, y0 + ROW_H);
            Ui_TextInRoom(x0 + 16, x1 - 16, yc - 18, shown, INK_WHITE, i == g_swap.answer);
            clip.Restore();
            d.textLeft = x0 + 16;
            d.textRight = x0 + 16 + Ui_TextWidthAsIs(shown);
        }
    }
}

static void DrawActions(S32 x0, S32 x1, S32 headsTop, S32 limit, const char *pane) {
    if (g_swap.action >= 0) {
        DrawSwapAnswers(x0, x1, headsTop, pane);
        return;
    }
    int cols[3];
    int ncols = Columns(cols);
    const int g = CurrentGroup(), row = Menu_TopRow();
    int &scroll = Menu_Scroll();
    int n = GroupCount(g);
    S32 top = HeadsInRow() ? headsTop : headsTop + 22;
    int visible = (limit - top + ROW_GAP) / ROW_PITCH;
    if (visible > n)
        visible = n;
    if (row < scroll)
        scroll = row;
    if (row >= scroll + visible)
        scroll = row - visible + 1;
    if (!HeadsInRow())
        DrawHeads(x0 + 16, x1, cols, ncols, headsTop);  // on the label edge, as the names below
    S32 y = top;
    int first = GroupFirst(g);
    for (int i = scroll; i < scroll + visible; i++) {
        DrawActionRow(first + i, x0, y, x1, y + ROW_H, cols, ncols, i == row, pane);
        y += ROW_PITCH;
    }
    if (HeadsInRow() && visible > 0)
        DrawHeadsInRow(top, x1, cols, ncols);  // the first row shown, scrolled or not
    if (visible < n)
        Menu_DrawScrollBar(x1 + 9, top, top + visible * ROW_PITCH - ROW_GAP - 1, scroll, visible, n);
}

static void DrawWideControls() {
    // One pane at a time, centred (decided 2026-11-03): the groups from the top margin (seven
    // from 24 end at 410, clear of the band at 430), or a group's title and its actions, the
    // column names inside the first row.
    WideFrame f = Menu_WideFrame();
    const S32 top = f.y0 + 24;
    const S32 x0 = f.x0 + (f.w - DRILL_W) / 2, x1 = x0 + DRILL_W - 1;
    if (OnGroups()) {
        S32 y = top;
        for (int g = 0; g < GROUP_COUNT; g++) {
            RowFill fill = g == CurrentGroup() ? FILL_CURSOR : FILL_SHADE;
            Ui_RowBackground(x0, y, x1, y + ROW_H, fill);
            Ui_TextInRoom(x0 + 16, x1 - 16, y + ROW_H / 2 - 18, Ui_Tr(kGroups[g].name), INK_WHITE, fill == FILL_CURSOR);
            DrawnRow &d = Menu_Record("bindings-left", kGroups[g].name, "", fill, x0, y, x1, y + ROW_H);
            d.textLeft = x0 + 16;
            d.textRight = x0 + 16 + Ui_TextWidth(kGroups[g].name);
            y += ROW_PITCH;
        }
    } else {
        S32 atop = Ui_Title(x0, x1, top, kGroups[CurrentGroup()].name);
        DrawActions(x0, x1, atop, Menu_WideRowsLimit(), "bindings-right");
    }

    const char *keys[4], *words[4];
    int nk;
    Legend(keys, words, &nk);
    Menu_DrawFoot(keys, words, nk);
}

static void DrawColumnControls() {
    S32 W = (S32)Host_LogicalW(), H = (S32)Host_LogicalH();
    S32 x0 = W / 2 - 275, x1 = W / 2 + 275;
    if (OnGroups()) {
        // The groups, as a 1997 sub-menu: a Return row, then one row each.
        const int cursor = Menu_TopRow();
        int &scroll = Menu_Scroll();
        S32 top = 16, bottom = Menu_DialogueTop() - 16;
        int n = GROUP_COUNT + 1;
        int visible = (bottom - top + ROW_GAP) / ROW_PITCH;
        if (visible > n)
            visible = n;
        if (cursor < scroll)
            scroll = cursor;
        if (cursor >= scroll + visible)
            scroll = cursor - visible + 1;
        S32 stackH = visible * ROW_PITCH - ROW_GAP;
        S32 y = top + (bottom - top - stackH) / 2;
        S32 y0 = y;
        for (int i = scroll; i < scroll + visible; i++) {
            const char *label = i == 0 ? "Return to previous menu" : kGroups[i - 1].name;
            RowFill fill = i == cursor ? FILL_CURSOR : FILL_SHADE;
            Ui_RowBackground(x0, y, x1, y + ROW_H, fill);
            Menu_RowLabelCentred(x0, x1, y + ROW_H / 2, label, INK_WHITE, false, i == cursor);
            DrawnRow &d = Menu_Record("column", label, "", fill, x0, y, x1, y + ROW_H);
            d.textLeft = (x0 + x1) / 2 - Ui_TextWidth(label) / 2;
            d.textRight = (x0 + x1) / 2 + Ui_TextWidth(label) / 2 + 8;
            y += ROW_PITCH;
        }
        if (visible < n)
            Menu_DrawScrollBar(x1 + 9, y0, y0 + stackH - 1, scroll, visible, n);
        const char *keys[4], *words[4];
        int nk;
        Legend(keys, words, &nk);
        Menu_DrawDialogue(keys, words, nk);
        return;
    }
    // The actions page: column heads, the rows, and the voice below (it asks for the key).
    DrawActions(x0, x1, 6, Menu_DialogueTop() - 16, "column");
    const char *keys[4], *words[4];
    int nk;
    Legend(keys, words, &nk);
    Menu_DrawDialogue(keys, words, nk);
}

void Bindings_Draw() {
    g_chipCount = g_headCount = 0;
    if (Wide())
        DrawWideControls();
    else
        DrawColumnControls();
}

// ───────────────────────────── State ─────────────────────────────

int Bindings_Chips(ChipView *out, int max) {
    int n = g_chipCount < max ? g_chipCount : max;
    for (int i = 0; i < n; i++) {
        out[i].action = kActions[g_chips[i].action].name;
        out[i].column = ColumnHead(g_chips[i].column);
        out[i].name = g_chips[i].name;
        out[i].fill = g_chips[i].fill;
        out[i].fits = g_chips[i].fits;
        out[i].conflict = g_chips[i].conflict;
        out[i].x0 = g_chips[i].x0, out[i].y0 = g_chips[i].y0, out[i].x1 = g_chips[i].x1, out[i].y1 = g_chips[i].y1;
    }
    return n;
}

int Bindings_Heads(HeadView *out, int max) {
    int n = g_headCount < max ? g_headCount : max;
    for (int i = 0; i < n; i++) {
        out[i].text = g_heads[i].text;
        out[i].x0 = g_heads[i].x0, out[i].y0 = g_heads[i].y0, out[i].x1 = g_heads[i].x1, out[i].y1 = g_heads[i].y1;
    }
    return n;
}

int Bindings_Conflicts(ConflictView *out, int max) {
    int n = 0;
    for (int a = 0; a < ACTION_COUNT; a++) {
        for (int c = 0; c < 3; c++) {
            int other = ConflictOf(a, c);
            if (other < 0 || n >= max)
                continue;
            out[n].action = kActions[a].name;
            out[n].column = ColumnHead(c);
            out[n].other = kActions[other].name;
            n++;
        }
    }
    return n;
}

int Bindings_Where(char *buf, size_t n) {
    if (OnGroups()) {
        const int g = CurrentGroup();
        snprintf(buf, n, "%s | %s", DeviceName(g_device), g < 0 ? "Return to previous menu" : kGroups[g].name);
        return 1;
    }
    char chip[32];
    int a = CursorAction(), c = CursorColumn();
    ChipText(a, c, chip, sizeof chip);
    snprintf(buf, n, "%s | %s | %s | %s | %s%s", DeviceName(g_device), kGroups[CurrentGroup()].name, kActions[a].name,
             ColumnHead(c), chip, g_waiting ? " | waiting" : g_swap.action < 0 ? "" : g_swap.answer ? " | asking | Yes" : " | asking | No");
    return g_waiting || g_swap.action >= 0 ? 3 : 2;  // Esc takes either back to the chip
}

bool Bindings_Text(const char *action, int column, char *out, size_t n) {
    for (int a = 0; a < ACTION_COUNT; a++) {
        if (!strcmp(kActions[a].name, action)) {
            if (!g_bindingsReady) {
                ResetBindings();
                g_bindingsReady = true;
            }
            ChipText(a, column, out, n);
            return true;
        }
    }
    out[0] = 0;
    return false;
}

void Bindings_WriteState(FILE *f) {
    char buf[32];
    const int g = CurrentGroup(), a = CursorAction();
    fprintf(f, "{\"device\":\"%s\",\"pad_names\":\"%s\",\"focus\":\"%s\",\"group\":", DeviceName(g_device),
            kPadFamilyNames[PadFamily()], OnGroups() ? "groups" : "actions");
    Menu_JsonString(f, g >= 0 ? kGroups[g].name : "");
    fprintf(f, ",\"action\":");
    Menu_JsonString(f, a >= 0 ? kActions[a].name : "");
    fprintf(f, ",\"column\":");
    Menu_JsonString(f, a >= 0 ? ColumnHead(CursorColumn()) : "");
    fprintf(f, ",\"waiting\":%s,\"scroll\":%d,\"chips\":[", g_waiting ? "true" : "false", Menu_Scroll());
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
    fprintf(f, "],\"heads\":[");
    for (int i = 0; i < g_headCount; i++) {
        fprintf(f, "%s{\"text\":", i ? "," : "");
        Menu_JsonString(f, g_heads[i].text);
        fprintf(f, ",\"rect\":[%d,%d,%d,%d]}", g_heads[i].x0, g_heads[i].y0, g_heads[i].x1, g_heads[i].y1);
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

// A legend's key as the player's device names it: after a pad press, the pad's
// button for the menu's fixed keys (host.cpp kPadMenuKeys: A Enter, B Esc,
// X Del, View Tab). The wait for a key keeps its own names: there B is not Esc.
const char *Bindings_LegendKey(const char *key) {
    // Two keys: each named, joined again.
    if (const char *and_ = strchr(key, CHIP_AND[0])) {
        static char joined[48];
        std::string first(key, and_ - key);
        snprintf(joined, sizeof joined, "%s", Bindings_LegendKey(first.c_str()));
        strncat(joined, CHIP_AND, sizeof joined - strlen(joined) - 1);
        strncat(joined, Bindings_LegendKey(and_ + 1), sizeof joined - strlen(joined) - 1);
        return joined;
    }
    static char names[4][16];
    static const struct {
        const char *key;
        int pad;
    } kFixed[] = {{"Enter", 0}, {"Esc", 1}, {"Del", 2}, {"Tab", 4}};
    // With no keyboard (a console before any key is typed) the pad's names from the start.
    if ((!Host_LastInputWasPad() && Host_KeyboardSeen()) || g_waiting)
        return key;
    for (int i = 0; i < 4; i++) {
        if (!strcmp(key, kFixed[i].key)) {
            PadName(PAD_CODE_BASE + kFixed[i].pad, names[i], sizeof names[i]);
            return names[i];
        }
    }
    return key;
}
