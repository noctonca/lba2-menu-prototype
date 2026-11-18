// What a new build tells a player: a trial (whats_new.h). The builds and the things are
// stand-ins; in lba2cc each setting's table row would carry its two builds, and lba2.cfg the
// last build run and what was seen.
#include "whats_new.h"

#include "menu_internal.h"
#include "trace.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int sw_whatsnew = 0;

// A build as three numbers, "0.13.3-dev" as 0.13.3.
static int Build(const char *s) {
    int a = 0, b = 0, c = 0;
    sscanf(s, "%d.%d.%d", &a, &b, &c);
    return a * 1000000 + b * 1000 + c;
}

// The pretend history: what each thing is, the build it arrived in and the build Modern
// turned it on (NULL: Modern leaves it as it came). One is older than the player's last
// build, so the list shows it is worked out per player, not a changelog.
struct Thing {
    int setting;         // a setting row's; -1 for a More options row
    const char *more;    // a More options row's console name
    bool experimental;   // unfinished: To try
    const char *arrived;
    const char *onInModern;
};
static const Thing kThings[] = {
    {S_CAM_IN, NULL, false, "0.12.0", "0.13.3"},        // Follow is now Modern's indoors camera
    {S_PADNAMES, NULL, false, "0.13.3", NULL},          // new: Button names
    {S_BACKGROUND, NULL, false, "0.13.3", NULL},        // new: Background
    {S_MOVIE_CAM, NULL, false, "0.12.0", NULL},         // old news: the player has had it since 0.12
    {-1, "cam_world_anchor", true, "0.13.3", NULL},     // to try
};
enum { THING_COUNT = sizeof kThings / sizeof kThings[0] };

// The build the player ran before this one: what is new is counted from it, on every run of
// this build until each thing is seen. A fresh install (3) remembers none.
static const char *LastRun() { return sw_whatsnew == 1 || sw_whatsnew == 2 ? "0.13.2" : NULL; }
static bool FirstRun() { return sw_whatsnew == 1; }

static bool IsNew(int t) {
    const char *last = LastRun();
    if (!last)
        return false;
    const int was = Build(last), now = Build(kVersionString);
    const Thing &k = kThings[t];
    const bool arrived = Build(k.arrived) > was && Build(k.arrived) <= now;
    const bool turnedOn = k.onInModern && Build(k.onInModern) > was && Build(k.onInModern) <= now;
    return arrived || turnedOn;
}

static int GroupOf(int t) {
    const Thing &k = kThings[t];
    const char *last = LastRun();
    if (k.onInModern && last && Build(k.onInModern) > Build(last))
        return WN_ON_IN_MODERN;
    return k.experimental ? WN_TRY : WN_NEW;
}

static bool g_seen[THING_COUNT];
static int g_on = -1;  // the thing the cursor was on last frame
static bool g_opened;  // What's new was opened
static void StartRun();

void WhatsNew_Reset() {
    memset(g_seen, 0, sizeof g_seen);
    g_on = -1;
    g_opened = false;
    StartRun();
}

int WhatsNew_Count() {
    int n = 0;
    for (int t = 0; t < THING_COUNT; t++)
        n += IsNew(t);
    return n;
}
bool WhatsNew_Any() { return sw_whatsnew && WhatsNew_Count() > 0; }

static bool Unseen(int t);
static bool AnyUnseen() {
    for (int t = 0; t < THING_COUNT; t++)
        if (Unseen(t))
            return true;
    return false;
}
// Options' What's new row: decided as a run starts. Something on it still unseen, it is there
// for the whole run, so it never goes from under the player; the next run without anything
// unseen leaves Options as it always was (the maintainer, 2026-11-18).
static bool g_offered;
static void StartRun() { g_offered = WhatsNew_Any() && AnyUnseen(); }
bool WhatsNew_Offered() { return WhatsNew_Any() && g_offered; }
// The line is for the first run of the build only (the maintainer, 2026-11-18): after that the
// marks carry what is still unseen.
bool WhatsNew_LineShown() { return WhatsNew_Offered() && FirstRun() && !g_opened; }

void WhatsNew_NextRun() {
    if (sw_whatsnew != 1)
        return;
    sw_whatsnew = 2;  // what was seen stays seen, as lba2.cfg would keep it
    if (g_on >= 0)
        g_seen[g_on] = true;  // the cursor was on it when the run ended
    g_on = -1;
    StartRun();
    Trace("whats new: a later run of this build");
}

static int TuningIndex(const char *name) {
    for (int i = 0; i < MoreOptions_Count(); i++)
        if (!strcmp(MoreOptions_Name(i), name))
            return i;
    return -1;
}

// The thing a row is, or -1. The To try row on What's new stands for its More options row.
static int ThingOf(const Row &r) {
    for (int t = 0; t < THING_COUNT; t++) {
        const Thing &k = kThings[t];
        if (k.setting >= 0 && r.type == R_SETTING && r.arg == k.setting)
            return t;
        if (k.more && r.type == R_TUNING && r.arg == TuningIndex(k.more))
            return t;
        if (k.more && r.type == R_ACTION && r.arg == A_TRY)
            return t;
    }
    return -1;
}

static bool Unseen(int t) { return t >= 0 && IsNew(t) && !g_seen[t]; }
static bool AnyUnseenMore() {
    for (int t = 0; t < THING_COUNT; t++)
        if (kThings[t].more && Unseen(t))
            return true;
    return false;
}

static bool PageHasNew(int page);

bool WhatsNew_RowNew(const Row &r) {
    if (!WhatsNew_Any())
        return false;
    if (r.type == R_CATEGORY && r.arg == P_WHATSNEW)
        return !g_opened && WhatsNew_Offered();  // the page itself, until it is opened
    if (const int t = ThingOf(r); t >= 0)
        return Unseen(t);
    if (r.type == R_CATEGORY)
        return PageHasNew(r.arg);
    if (r.type == R_ACTION && r.arg == A_EXP)
        return AnyUnseenMore();
    if (r.type == R_GAME && r.arg == M_OPTIONS)
        return PageHasNew(P_HUB);
    return false;
}

bool WhatsNew_TuningNew(int i) {
    if (!WhatsNew_Any())
        return false;
    const Row r = {R_TUNING, NULL, i, NULL, NULL, NULL, NULL};
    return Unseen(ThingOf(r));
}

// A row on the page wears the mark (the hub's categories, a category's settings).
static bool PageHasNew(int page) {
    const Page &p = g_pages[page];
    for (int i = 0; i < p.count; i++) {
        const Row &r = p.rows[i];
        if (r.shown && !r.shown())
            continue;
        if (r.type == R_CATEGORY && r.arg == page)
            continue;
        if (WhatsNew_RowNew(r))
            return true;
    }
    return false;
}

void WhatsNew_Frame(const Row *cursor) {
    if (!WhatsNew_Any())
        return;
    const int t = cursor ? ThingOf(*cursor) : -1;
    if (g_on >= 0 && t != g_on)
        g_seen[g_on] = true;  // the cursor was on it and left
    g_on = t;
}

// ── The page ──

// A row of the page is there while what it names is new to this player; a group's head
// while one of its rows is.
static bool GroupShown(int g) {
    for (int t = 0; t < THING_COUNT; t++)
        if (IsNew(t) && GroupOf(t) == g)
            return true;
    return false;
}
template <int T> static bool ThingShown() { return IsNew(T); }
template <int G> static bool HeadShown() { return GroupShown(G); }
static void Opened() {
    g_opened = true;
    Trace("whats new opened");
}

// Each thing's row under each group's head it can be in, shown in the one that holds:
// a row that Modern turned on is in Now on in Modern and nowhere else. The rows are the
// settings themselves, so a player turns one off right there.
template <int T, int G> static bool ThingIn() { return IsNew(T) && GroupOf(T) == G; }
#define THING_ROW(t, g)                                                                     \
    {kThings[t].setting >= 0 ? R_SETTING : R_ACTION, kThings[t].setting >= 0 ? NULL : "Camera holds its direction", \
     kThings[t].setting >= 0 ? kThings[t].setting : A_TRY,                                   \
     kThings[t].setting >= 0 ? NULL : "Unfinished: the camera faces one way as Twinsen turns.", \
     ThingIn<t, g>, NULL, NULL}
#define HEAD_ROW(g, name) {R_HEADING, name, g, "", HeadShown<g>, NULL, NULL}
static Row s_rows[3 + 3 * THING_COUNT];

void WhatsNew_Pages(Page *pages) {
    static_assert(THING_COUNT == 5, "a row for each thing in each group below");
    const Row rows[] = {
        HEAD_ROW(WN_ON_IN_MODERN, "Now on in Modern"),
        THING_ROW(0, WN_ON_IN_MODERN), THING_ROW(1, WN_ON_IN_MODERN), THING_ROW(2, WN_ON_IN_MODERN),
        THING_ROW(3, WN_ON_IN_MODERN), THING_ROW(4, WN_ON_IN_MODERN),
        HEAD_ROW(WN_NEW, "New settings"),
        THING_ROW(0, WN_NEW), THING_ROW(1, WN_NEW), THING_ROW(2, WN_NEW), THING_ROW(3, WN_NEW), THING_ROW(4, WN_NEW),
        HEAD_ROW(WN_TRY, "To try"),
        THING_ROW(0, WN_TRY), THING_ROW(1, WN_TRY), THING_ROW(2, WN_TRY), THING_ROW(3, WN_TRY), THING_ROW(4, WN_TRY),
    };
    static_assert(sizeof rows / sizeof rows[0] == sizeof s_rows / sizeof s_rows[0], "s_rows holds the page");
    memcpy(s_rows, rows, sizeof rows);
    static const PageHooks kHooks = {Opened, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    pages[P_WHATSNEW] = {"What's new", s_rows, (int)(sizeof s_rows / sizeof s_rows[0]), &kHooks, false};
}
#undef THING_ROW
#undef HEAD_ROW

void WhatsNew_WriteState(FILE *f) {
    fprintf(f, "{\"run\":\"%s\",\"last_run\":", FirstRun() ? "first" : "later");
    Menu_JsonString(f, LastRun() ? LastRun() : "");
    fprintf(f, ",\"count\":%d,\"line\":%s,\"opened\":%s,\"new\":[", WhatsNew_Count(),
            WhatsNew_LineShown() ? "true" : "false", g_opened ? "true" : "false");
    bool first = true;
    for (int t = 0; t < THING_COUNT; t++) {
        if (!IsNew(t))
            continue;
        static const char *const kGroup[] = {"on in modern", "new", "try"};
        const char *name = kThings[t].setting >= 0 ? Settings_Def(kThings[t].setting).id : kThings[t].more;
        fprintf(f, "%s{\"id\":\"%s\",\"group\":\"%s\",\"seen\":%s}", first ? "" : ",", name, kGroup[GroupOf(t)],
                g_seen[t] ? "true" : "false");
        first = false;
    }
    fprintf(f, "]}");
}
