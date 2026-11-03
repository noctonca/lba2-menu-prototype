// The self test: walks what tools/sweep.py walks (every chip of the Controls
// page, then every row of every page) in one process, without the frames a
// script spends waiting, and checks the same rules. Each problem comes with a
// command line that opens the screen it was found on.
#include "selftest.h"

#include "controls.h"
#include "engine.h"
#include "experimental.h"
#include "menu.h"
#include "menu_parts.h"
#include "trace.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

// The switch combinations tools/sweep.py runs, in its order.
// The layouts chosen (the band, one device per view) and their variants. The help line
// and prompt bar (band=0) and all key columns side by side (devices=0) were not chosen
// (2026-11-03) and are left out; --combo still tests them.
static const char *kCombos[] = {
    "band=1,gameplay=0", "band=1,gameplay=1", "help640=0,gameplay=1", "padnames=2",
    "padnames=3",        "chip=1,tablefont=2,keyrule=1",            "heads=0",
};

static const char *kPages[] = {
    "Options", "Preset", "Language and interface", "Display and graphics", "Sound",
    "Controls and camera", "Advanced and reset", "Gameplay",
};

// Actions per group on the Controls page (controls.cpp kActions).
static const int kGroupSizes[] = {5, 6, 4, 7, 4, 4, 6};
// Rows per group on the Experimental settings page (experimental.cpp kTuning).
static const int kExpGroups[] = {22, 9, 7, 3};
static const char *kExpPage = "Experimental settings";

struct Switch {
    std::string name;
    int value;
};

static bool ParseCombo(const char *spec, std::vector<Switch> *out) {
    std::string s = spec;
    size_t at = 0;
    while (at < s.size()) {
        size_t comma = s.find(',', at);
        std::string part = s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
        size_t eq = part.find('=');
        if (eq == std::string::npos || Menu_GetSwitch(part.substr(0, eq).c_str()) < 0)
            return false;
        Switch sw;
        sw.name = part.substr(0, eq);
        sw.value = atoi(part.c_str() + eq + 1);
        out->push_back(sw);
        if (comma == std::string::npos)
            break;
        at = comma + 1;
    }
    return true;
}

static int ComboValue(const std::vector<Switch> &c, const char *name, int otherwise) {
    for (size_t i = 0; i < c.size(); i++)
        if (c[i].name == name)
            return c[i].value;
    return otherwise;
}

// As Python prints a dict of ints: {"band": 1, "gameplay": 0}.
static std::string ComboLabel(const std::vector<Switch> &c) {
    std::string s = "{";
    for (size_t i = 0; i < c.size(); i++) {
        char buf[64];
        snprintf(buf, sizeof buf, "%s\"%s\": %d", i ? ", " : "", c[i].name.c_str(), c[i].value);
        s += buf;
    }
    return s + "}";
}

// As Python's repr() prints a str, so the lines match sweep.py's.
static std::string Repr(const char *s) {
    bool single = strchr(s, '\'') != NULL, dbl = strchr(s, '"') != NULL;
    char q = single && !dbl ? '"' : '\'';
    std::string out(1, q);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '\\' || c == (unsigned char)q) {
            out += '\\';
            out += *s;
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\t') {
            out += "\\t";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c < 0x20 || c == 0x7f) {
            char hex[8];
            snprintf(hex, sizeof hex, "\\x%02x", c);
            out += hex;
        } else {
            out += *s;
        }
    }
    return out + q;
}

static std::string Fmt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static std::string Fmt(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return buf;
}

// ───────────────────────────── The rules ─────────────────────────────

// sweep.py check(), rule for rule.
// The row a column head is drawn inside (heads=1), or -1 for a head above the rows.
static int HeadRow(const HeadView &h, const DrawnRow *rows, int n) {
    for (int i = 0; i < n; i++)
        if (rows[i].x0 <= h.x0 && h.x1 <= rows[i].x1 && rows[i].y0 < h.y0 && h.y1 < rows[i].y1)
            return i;
    return -1;
}

static void Check(std::vector<std::string> *found) {
    const S32 W = (S32)Engine_LogicalW(), H = (S32)Engine_LogicalH();
    MenuHelp v;
    Menu_Help(&v);
    if (v.needed > v.max && v.max)
        found->push_back(Fmt("help cut off (%d lines in %d, %d px): %s", v.needed, v.max, v.width, Repr(v.text).c_str()));
    bool help = strcmp(v.where, "none") != 0;
    const DrawnRow *rows;
    int n = Menu_DrawnRows(&rows);
    for (int i = 0; i < n; i++) {
        const DrawnRow &r = rows[i];
        if (r.y1 >= H || r.x1 >= W || r.x0 < 0)
            found->push_back(Fmt("row off the canvas: %s [%d, %d, %d, %d]", Repr(r.label).c_str(), r.x0, r.y0, r.x1, r.y1));
        if (help && r.y1 >= v.y0 && r.y0 <= v.y1 && r.x1 >= v.x0 && r.x0 <= v.x1)
            found->push_back(Fmt("row overlaps the %s by %d px: %s", v.where, r.y1 - v.y0 + 1, Repr(r.label).c_str()));
        if (r.textRight > r.x1)
            found->push_back(
                Fmt("text spills %d px: %s %s", r.textRight - r.x1, Repr(r.label).c_str(), Repr(r.value).c_str()));
    }
    ChipView chips[32];
    int nc = Controls_Active() ? Controls_Chips(chips, 32) : 0;
    for (int i = 0; i < nc; i++)
        if (!chips[i].fits)
            found->push_back(Fmt("key name cut in its chip: %s %s %s", Repr(chips[i].action).c_str(), chips[i].column,
                                 Repr(chips[i].name).c_str()));

    ExpTable et;
    bool table = Exp_Table(&et);
    // One cursor: exactly one row, chip or table line wears it.
    if (n || nc || table) {
        int cursors = 0;
        for (int i = 0; i < n; i++)
            cursors += !strcmp(rows[i].fill, "cursor");
        for (int i = 0; i < nc; i++)
            cursors += !strcmp(chips[i].fill, "cursor") || !strcmp(chips[i].fill, "waiting");
        for (int i = 0; table && i < et.count; i++)
            cursors += et.lines[i].cursor;
        if (cursors != 1)
            found->push_back(Fmt("%d cursors on the screen", cursors));
    }
    // A red chip under the cursor says in the help what else has its key.
    for (int i = 0; i < nc; i++)
        if (!strcmp(chips[i].fill, "cursor") && chips[i].conflict && !strstr(v.text, " is also "))
            found->push_back(Fmt("red key not explained in the help: %s %s %s", Repr(chips[i].action).c_str(),
                                 chips[i].column, Repr(chips[i].name).c_str()));
    // A clash goes both ways: the other action shows one too.
    if (Controls_Active()) {
        ConflictView cv[128];
        int ncf = Controls_Conflicts(cv, 128);
        for (int i = 0; i < ncf; i++) {
            bool back = false;
            for (int j = 0; j < ncf && !back; j++)
                back = !strcmp(cv[j].action, cv[i].other);
            if (!back)
                found->push_back(Fmt("clash one way: %s %s is also %s, which shows no clash", Repr(cv[i].action).c_str(),
                                     cv[i].column, Repr(cv[i].other).c_str()));
        }
    }
    // The Experimental settings table: its panel clear of the canvas edge and the help, its
    // lines on one strip, the names on the label edge, the values on one trailing edge clear
    // of the names, 10 apart at 1x, inside the panel above the status line, which fits; the
    // cursor's group head shown when the cursor is on the group's first row.
    if (table) {
        S32 s = et.scale;
        if (et.y1 >= H || et.x1 >= W || et.x0 < 0)
            found->push_back(Fmt("row off the canvas: 'table' [%d, %d, %d, %d]", et.x0, et.y0, et.x1, et.y1));
        if (help && et.y1 >= v.y0 && et.y0 <= v.y1 && et.x1 >= v.x0 && et.x0 <= v.x1)
            found->push_back(Fmt("row overlaps the %s by %d px: 'table'", v.where, et.y1 - v.y0 + 1));
        if (help && v.y0 - et.y1 - 1 >= 0 && v.y0 - et.y1 - 1 <= ROW_PITCH - ROW_H - 1)
            found->push_back(Fmt("proximity: %d px between table and the %s, rows are 5 apart", v.y0 - et.y1 - 1, v.where));
        if (et.statusRight > et.x1 - 24)
            found->push_back(Fmt("table: the status line ends at x %d, past %d: %s", et.statusRight, et.x1 - 24,
                                 Repr(et.status).c_str()));
        for (int i = 0; i < et.count; i++) {
            const ExpLine &l = et.lines[i];
            if (l.x0 != et.x0 + 8 || l.x1 != et.x1 - 8)
                found->push_back(Fmt("edge: %s at x %d..%d, the table's lines at %d..%d", Repr(l.text).c_str(), l.x0, l.x1,
                                     et.x0 + 8, et.x1 - 8));
            if (l.textLeft != et.x0 + 16)
                found->push_back(Fmt("edge: %s starts at x %d, not %d", Repr(l.text).c_str(), l.textLeft, et.x0 + 16));
            if (l.y0 <= et.y0 || l.y1 >= et.statusTop)
                found->push_back(Fmt("table: %s [%d..%d] outside the panel's lines (%d..%d)", Repr(l.text).c_str(), l.y0,
                                     l.y1, et.y0 + 1, et.statusTop - 1));
            if (l.y1 - l.y0 != 10 * s - 1)
                found->push_back(Fmt("pitch: %s is %d tall, not %d", Repr(l.text).c_str(), l.y1 - l.y0 + 1, 10 * s));
            if (i && l.y0 - et.lines[i - 1].y0 != 10 * s)
                found->push_back(Fmt("pitch: %s starts %d below the line above, not %d", Repr(l.text).c_str(),
                                     l.y0 - et.lines[i - 1].y0, 10 * s));
            if (l.head)
                continue;
            int want = l.cursor ? et.x1 - 24 : et.valueEdge;
            if (l.valueRight != want)
                found->push_back(Fmt("trailing: %s ends at x %d, not %d", Repr(l.text).c_str(), l.valueRight, want));
            if (l.textRight + 8 * s >= l.valueLeft)
                found->push_back(Fmt("text spills %d px: %s %s", l.textRight + 8 * s - l.valueLeft + 1, Repr(l.text).c_str(),
                                     Repr(l.value).c_str()));
            if (l.cursor && l.first && !(i && et.lines[i - 1].head))
                found->push_back(Fmt("table: %s is first in its group, and its group's head is not shown",
                                     Repr(l.text).c_str()));
        }
    }
    // The rows by pane, in draw order (the first-launch title is not a pane).
    std::vector<std::string> paneNames;
    std::map<std::string, std::vector<const DrawnRow *> > panes;
    for (int i = 0; i < n; i++) {
        if (!strcmp(rows[i].pane, "title"))
            continue;
        if (!panes.count(rows[i].pane))
            paneNames.push_back(rows[i].pane);
        panes[rows[i].pane].push_back(&rows[i]);
    }
    const bool wide = !strcmp(Menu_Layout(), "wide");
    // One edge per pane: its rows share x0 and x1; wide labels start 16 in.
    for (size_t p = 0; p < paneNames.size(); p++) {
        const std::vector<const DrawnRow *> &pr = panes[paneNames[p]];
        bool centred = paneNames[p] == "column" || paneNames[p] == "confirm";
        for (size_t i = 0; i < pr.size(); i++) {
            const DrawnRow &r = *pr[i];
            if (r.x0 != pr[0]->x0 || r.x1 != pr[0]->x1)
                found->push_back(Fmt("edge: %s at x %d..%d, its pane's rows at %d..%d", Repr(r.label).c_str(), r.x0, r.x1,
                                     pr[0]->x0, pr[0]->x1));
            if (wide && !centred && r.textLeft != r.x0 + 16)
                found->push_back(Fmt("edge: %s starts at x %d, not %d", Repr(r.label).c_str(), r.textLeft, r.x0 + 16));
        }
    }
    // The Controls page's first column head (the action names') starts on its rows' label edge,
    // when the heads are a line above the rows.
    HeadView heads[4];
    int nh = Controls_Active() ? Controls_Heads(heads, 4) : 0;
    for (int i = 0; wide && nh && HeadRow(heads[0], rows, n) < 0 && i < n; i++) {
        if (heads[0].x0 >= rows[i].x0 && heads[0].x0 <= rows[i].x1 && rows[i].y0 > heads[0].y1) {
            if (heads[0].x0 != rows[i].x0 + 16)
                found->push_back(Fmt("edge: head %s starts at x %d, not %d", Repr(heads[0].text).c_str(), heads[0].x0,
                                     rows[i].x0 + 16));
            break;
        }
    }
    // Margins: nothing on a wide page within 32 of the sides or 24 of the top and bottom.
    if (wide) {
        struct Area {
            std::string what;
            int x0, y0, x1, y1;
        };
        std::vector<Area> boxes;
        for (int i = 0; i < n; i++)
            boxes.push_back({rows[i].label, rows[i].x0, rows[i].y0, rows[i].x1, rows[i].y1});
        for (int i = 0; i < nc; i++)
            boxes.push_back({std::string(chips[i].action) + " " + chips[i].column, chips[i].x0, chips[i].y0, chips[i].x1,
                             chips[i].y1});
        for (int i = 0; i < nh; i++)
            boxes.push_back({heads[i].text, heads[i].x0, heads[i].y0, heads[i].x1, heads[i].y1});
        if (help)
            boxes.push_back({"help", v.x0, v.y0, v.x1, v.y1});
        if (table)
            boxes.push_back({"table", et.x0, et.y0, et.x1, et.y1});
        for (size_t i = 0; i < boxes.size(); i++) {
            const Area &b = boxes[i];
            if (b.x0 < 32 || b.y0 < 24 || b.x1 > W - 33 || b.y1 > H - 25)
                found->push_back(
                    Fmt("margin: %s [%d, %d, %d, %d] in the margin", Repr(b.what.c_str()).c_str(), b.x0, b.y0, b.x1, b.y1));
        }
    }
    // One trailing edge: wide, what a row shows on the right (a value, a slider, steps, its
    // last key chip) ends on the column 24 in from the row's right edge.
    if (wide)
        for (int i = 0; i < n; i++)
            if (rows[i].valueRight > 0 && rows[i].valueRight != rows[i].x1 - 24)
                found->push_back(Fmt("trailing: %s ends at x %d, not %d", Repr(rows[i].label).c_str(), rows[i].valueRight,
                                     rows[i].x1 - 24));
    // The pitch: in a pane, each row starts 56 below the one above and is 50 tall.
    for (size_t p = 0; p < paneNames.size(); p++) {
        const std::vector<const DrawnRow *> &pr = panes[paneNames[p]];
        if (pr.size() < 2)
            continue;
        for (size_t i = 1; i < pr.size(); i++)
            if (pr[i]->y0 - pr[i - 1]->y0 != ROW_PITCH)
                found->push_back(Fmt("pitch: %s starts %d below the row above, not 56", Repr(pr[i]->label).c_str(),
                                     pr[i]->y0 - pr[i - 1]->y0));
        for (size_t i = 0; i < pr.size(); i++)
            if (pr[i]->y1 - pr[i]->y0 != ROW_H)
                found->push_back(
                    Fmt("pitch: %s is %d tall, not 50", Repr(pr[i]->label).c_str(), pr[i]->y1 - pr[i]->y0));
    }
    // At most one current-value plasma in a pane.
    for (size_t p = 0; p < paneNames.size(); p++) {
        const std::vector<const DrawnRow *> &pr = panes[paneNames[p]];
        int current = 0;
        for (size_t i = 0; i < pr.size(); i++)
            current += !strcmp(pr[i]->fill, "current");
        if (current > 1)
            found->push_back(Fmt("current: %d current-value rows in the %s pane", current, paneNames[p].c_str()));
    }
    // Every setting row shows its value.
    for (int i = 0; i < n; i++)
        if (rows[i].setting && !rows[i].value[0])
            found->push_back(Fmt("value: setting %s shows no value", Repr(rows[i].label).c_str()));
    // Proximity: a pane's last row is further from the help below than rows are apart (5).
    if (help) {
        for (size_t p = 0; p < paneNames.size(); p++) {
            const std::vector<const DrawnRow *> &pr = panes[paneNames[p]];
            int bottom = pr[0]->y1;
            for (size_t i = 1; i < pr.size(); i++)
                if (pr[i]->y1 > bottom)
                    bottom = pr[i]->y1;
            int gap = v.y0 - bottom - 1;
            if (gap >= 0 && gap <= ROW_PITCH - ROW_H - 1)
                found->push_back(Fmt("proximity: %d px between %s and the %s, rows are 5 apart", gap,
                                     paneNames[p].c_str(), v.where));
        }
    }
    // Proximity: a column head is closer to the rows below it than to the rows above it.
    for (int h = 0; h < nh; h++) {
        if (HeadRow(heads[h], rows, n) >= 0)
            continue;
        int below = -1, above = -1;
        for (int i = 0; i < n; i++) {
            const DrawnRow &r = rows[i];
            if (heads[h].x0 < r.x0 || heads[h].x0 > r.x1)
                continue;
            if (r.y0 > heads[h].y1 && (below < 0 || r.y0 - heads[h].y1 - 1 < below))
                below = r.y0 - heads[h].y1 - 1;
            if (r.y1 < heads[h].y0 && (above < 0 || heads[h].y0 - r.y1 - 1 < above))
                above = heads[h].y0 - r.y1 - 1;
        }
        if (below >= 0 && above >= 0 && below >= above)
            found->push_back(Fmt("proximity: head %s is %d px above its rows, %d px below the row above",
                                 Repr(heads[h].text).c_str(), below, above));
    }
    // Proximity: a column head inside a row is in its pane's first row, above its own chip,
    // and clear of the action's name.
    for (int h = 0; h < nh; h++) {
        int ri = HeadRow(heads[h], rows, n);
        if (ri < 0)
            continue;
        const DrawnRow &r = rows[ri];
        for (int i = 0; i < n; i++)
            if (!strcmp(rows[i].pane, r.pane) && rows[i].y0 < r.y0) {
                found->push_back(Fmt("proximity: head %s in %s, not its pane's first row", Repr(heads[h].text).c_str(),
                                     Repr(r.label).c_str()));
                break;
            }
        bool chip = false;
        for (int i = 0; i < nc; i++)
            if (chips[i].x0 <= heads[h].x0 && heads[h].x0 <= chips[i].x1 && r.y0 <= chips[i].y0 &&
                chips[i].y1 <= r.y1)
                chip = chips[i].y0 > heads[h].y1;
        if (!chip)
            found->push_back(Fmt("proximity: head %s not above its chip in %s", Repr(heads[h].text).c_str(),
                                 Repr(r.label).c_str()));
        if (heads[h].x0 <= r.textRight)
            found->push_back(Fmt("proximity: head %s runs into the name of %s", Repr(heads[h].text).c_str(),
                                 Repr(r.label).c_str()));
    }
}

// ───────────────────────────── The walk ─────────────────────────────

// One step is one frame, as a script line followed by `wait 1` would be.
// draw: false for frames nothing looks at (a long wait); the next frame draws.
static void Frame(bool draw = true) {
    Engine_BeginFrame();
    Trace_SetFrame(Engine_Frame());
    Menu_Update();
    if (draw) {
        Menu_Draw();
        Engine_Present();
    }
}


// The groups' first actions (controls.cpp kActions), where the binding walk tries keys.
static const char *kGroupFirst[] = {"Forward", "Behaviour action", "Normal", "Weapon 1", "Penguin", "Recentre", "Game menu"};

static bool EndsWith(const std::string &s, const char *tail) {
    size_t n = strlen(tail);
    return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

struct Walker {
    const SelftestOptions *opt;
    std::vector<Switch> switches;  // this combo's
    std::string size, combo, again;  // again: the command line up to --do
    std::vector<std::string> path;   // commands since the last open
    std::map<std::string, std::string> problems;  // line -> the command line that shows it
    int checks;

    // ── Steps ──

    // A fresh process's state under this combo, as the again line starts it.
    void Start() {
        Menu_Reset();
        for (size_t s = 0; s < switches.size(); s++)
            Menu_SetSwitch(switches[s].name.c_str(), switches[s].value);
        // Help shown whole, so a check never depends on how far the typing got.
        Menu_SetSwitch("typing", 2);
        Frame();
    }
    void Open(const char *page) {
        path.assign(1, std::string("open ") + page);
        Menu_Open(page);
        Frame();
    }
    // Another page on the same path: what was done before it stays in the again line.
    void OpenToo(const char *page) {
        path.push_back(std::string("open ") + page);
        Menu_Open(page);
        Frame();
    }
    bool RowDrawn(const char *label) const {
        const DrawnRow *rows;
        int n = Menu_DrawnRows(&rows);
        for (int i = 0; i < n; i++)
            if (!strcmp(rows[i].label, label))
                return true;
        return false;
    }
    void Fresh(const char *page) {
        Start();
        Open(page);
    }
    void Key(const char *name, int count = 1) {
        UiKey k;
        Engine_KeyByName(name, &k);
        for (int i = 0; i < count; i++) {
            path.push_back(std::string("key ") + name);
            Engine_Inject(k);
            Frame();
        }
    }
    void Press(const char *raw) {
        path.push_back(std::string("press ") + raw);
        Engine_InjectRaw(raw);
        Frame();
    }
    void Wait(int frames) {
        path.push_back(Fmt("wait %d", frames));
        for (int i = 0; i < frames; i++)
            Frame(i + 1 == frames);
    }
    // The path as --do commands, runs of one key folded into `key X n`.
    std::string Commands() const {
        std::string out;
        for (size_t i = 0; i < path.size();) {
            size_t j = i;
            while (j < path.size() && path[j] == path[i])
                j++;
            out += (out.empty() ? "" : "; ") + path[i];
            if (j - i > 1 && path[i].compare(0, 4, "key ") == 0)
                out += Fmt(" %d", (int)(j - i));
            else
                for (size_t k = i + 1; k < j; k++)
                    out += "; " + path[i];
            i = j;
        }
        return out;
    }

    // ── Reading the screen ──

    std::string Where(int *depth = NULL) const {
        char buf[320];
        int d = Menu_Where(buf, sizeof buf);
        if (depth)
            *depth = d;
        return buf;
    }
    int ControlsDepth() const {
        char buf[200];
        return Controls_Active() ? Controls_Where(buf, sizeof buf) : 0;
    }
    bool InActions() const { return ControlsDepth() == 2; }
    bool Waiting() const { return ControlsDepth() == 3; }
    // The chip under the cursor, from the last frame drawn.
    bool CursorChip(ChipView *out) const {
        ChipView c[32];
        int n = Controls_Active() ? Controls_Chips(c, 32) : 0;
        for (int i = 0; i < n; i++) {
            if (!strcmp(c[i].fill, "cursor") || !strcmp(c[i].fill, "waiting")) {
                *out = c[i];
                return true;
            }
        }
        return false;
    }
    std::string ChipText() const {
        ChipView c;
        return CursorChip(&c) ? c.name : "?";
    }
    bool ChipRed() const {
        ChipView c;
        return CursorChip(&c) && c.conflict;
    }
    static bool Clashes(const char *action) {
        ConflictView cv[128];
        int n = Controls_Conflicts(cv, 128);
        for (int i = 0; i < n; i++)
            if (!strcmp(cv[i].action, action))
                return true;
        return false;
    }
    static std::string Voice() {
        MenuHelp v;
        Menu_Help(&v);
        return v.text;
    }

    // ── Problems ──

    void Record(const std::string &what) {
        std::string line = size + " " + combo + " [" + Menu_PageName() + "] " + what;
        if (!problems.count(line))
            problems[line] = again + " --do \"" + Commands() + "\"";
    }
    // A screen check: the layout and screen rules on what the last frame drew.
    void Check(const char *expectPage = NULL) {
        checks++;
        std::vector<std::string> found;
        const char *page = Menu_PageName();
        // Wide pages name the right pane, so the hub has no page of its own there.
        if (expectPage && strcmp(expectPage, "Options") != 0 && strcmp(page, expectPage) != 0)
            found.push_back(Fmt("walk did not reach %s (on %s)", Repr(expectPage).c_str(), Repr(page).c_str()));
        ::Check(&found);
        for (size_t i = 0; i < found.size(); i++)
            Record(found[i]);
    }
    // A behaviour check that failed. The line starts with the walk's name.
    void Fail(const std::string &what) {
        checks++;
        Record(what);
    }
    // A behaviour check that passed, for the count.
    void Pass() { checks++; }
    bool Expect(bool ok, const std::string &what) {
        if (ok)
            Pass();
        else
            Fail(what);
        return ok;
    }
    bool MoveTo(const char *label) {
        for (int i = 0; i < 24; i++) {
            if (!strcmp(Menu_CursorLabel(), label))
                return true;
            Key("down");
        }
        Fail(Fmt("walk: no row %s", Repr(label).c_str()));
        return false;
    }
    bool Wanted(const char *page) const { return !opt->only || strcmp(opt->only, page) == 0; }

    // ───────────────────────────── The layout walk ─────────────────────────────

    void Controls(int cols, bool twoDevices) {
        if (!Wanted("Keys and buttons"))
            return;
        Open("Keys and buttons");
        Check("Keys and buttons");
        for (int d = 0; d < (twoDevices ? 2 : 1); d++) {
            if (d)
                Key("tab");
            int count = d ? 1 : cols;  // the pad view has one column
            for (int g = 0; g < (int)(sizeof kGroupSizes / sizeof kGroupSizes[0]); g++) {
                Check();
                Key("enter");
                for (int r = 0; r < kGroupSizes[g]; r++) {
                    for (int c = 0; c < count; c++) {
                        Check();
                        if (c + 1 < count)
                            Key("right");
                    }
                    Key("left", count - 1);
                    Key("down");
                }
                Key("esc");
                Key("down");
            }
        }
    }

    // The Experimental settings page: every row, in the table (exptable=1) or the list.
    void ExperimentalPage() {
        if (!Wanted(kExpPage))
            return;
        Open(kExpPage);
        Check(kExpPage);
        if (Menu_GetSwitch("exptable")) {
            for (int i = 0; i < Exp_Count(); i++) {
                Key("down");
                Check();
            }
            return;
        }
        for (size_t g = 0; g < sizeof kExpGroups / sizeof kExpGroups[0]; g++) {
            Check();
            Key("enter");
            for (int r = 0; r < kExpGroups[g]; r++) {
                Check();
                Key("down");
            }
            Key("esc");
            Key("down");
        }
    }

    void Pages(bool gameplay) {
        for (size_t p = 0; p < sizeof kPages / sizeof kPages[0]; p++) {
            if (!strcmp(kPages[p], "Gameplay") && !gameplay)
                continue;
            if (!Wanted(kPages[p]))
                continue;
            Open(kPages[p]);
            Check(kPages[p]);
            for (int i = 1; i < 12; i++) {
                Key("down");
                Check();
            }
        }
    }

    // ───────────────────────────── The walks by keys ─────────────────────────────

    // esc: from every row, Enter; look round what opened; Esc must put the cursor
    // back where it was. Rows that change a value on Enter get it put back with Left.
    // Returns false once something went wrong, so one fault is not reported a hundred times.
    bool TryEnter(int depth, std::set<std::string> *reached) {
        int d0;
        std::string here = Where(&d0);
        if (!strncmp(Menu_CursorLabel(), "Return", 6) || EndsWith(here, "| Return to previous menu"))
            return true;  // Return rows leave; Esc is no way back from them
        std::string v0 = Menu_CursorValue();
        Key("enter");
        int d1;
        std::string opened = Where(&d1);
        if (d1 > d0)
            reached->insert(Menu_PageName());  // opened, not just shown beside the hub
        if (d1 < d0) {
            Fail(Fmt("esc: Enter on %s closed the screen (now %s)", Repr(here.c_str()).c_str(), Repr(opened.c_str()).c_str()));
            return false;
        }
        if (d1 == d0) {
            bool active;
            if (Menu_CursorSetting(&active) && Menu_CursorValue() != v0)
                Key("left");  // a choice moved on; put it back
            return true;
        }
        Check();  // the screen Enter opened
        bool leaf = EndsWith(opened, "| confirm") || EndsWith(opened, "| waiting");
        if (!leaf && depth < 6 && !Explore(depth + 1, reached))
            return false;
        Key("esc");
        std::string back = Where();
        return Expect(back == here, Fmt("esc: Esc from %s went to %s, not back to %s", Repr(opened.c_str()).c_str(),
                                        Repr(back.c_str()).c_str(), Repr(here.c_str()).c_str()));
    }

    bool Explore(int depth, std::set<std::string> *reached) {
        std::string start = Where();
        for (int step = 0; step < 40; step++) {
            if (!TryEnter(depth, reached))
                return false;
            // Along an action row of the Controls page: each chip, then back.
            int moved = 0;
            while (InActions()) {
                std::string before = Where();
                Key("right");
                if (Where() == before)
                    break;
                moved++;
                if (!TryEnter(depth, reached))
                    return false;
            }
            Key("left", moved);
            Key("down");
            if (Where() == start)
                return true;
        }
        Fail(Fmt("walk: Down never comes back round to %s", Repr(start.c_str()).c_str()));
        return false;
    }

    // reach: every page can be opened with keys from the hub.
    void Reach(bool gameplay) {
        if (opt->only)
            return;
        Fresh("Options");
        std::set<std::string> reached;
        if (!Explore(1, &reached))
            return;
        std::vector<const char *> want(kPages + 1, kPages + sizeof kPages / sizeof kPages[0]);
        want.push_back("Keys and buttons");
        path.assign(1, "open Options");  // the walk came back round to where it started
        for (size_t i = 0; i < want.size(); i++) {
            if (!strcmp(want[i], "Gameplay") && !gameplay)
                continue;
            Expect(reached.count(want[i]) != 0, Fmt("reach: no keys lead to %s", Repr(want[i]).c_str()));
        }
    }

    // setting: Left and Right change the value drawn, and the other key puts it back;
    // a dimmed row takes neither, nor Enter.
    void Settings(const char *page) {
        if (!Wanted(page))
            return;
        Fresh(page);
        std::string start = Where();
        for (int step = 0; step < 24; step++) {
            bool active;
            if (Menu_CursorSetting(&active)) {
                std::string label = Menu_CursorLabel(), v0 = Menu_CursorValue();
                if (!active) {
                    const char *keys[] = {"right", "left", "enter"};
                    for (int k = 0; k < 3; k++) {
                        Key(keys[k]);
                        if (!Expect(Menu_CursorValue() == v0, Fmt("setting: %s changed dimmed %s from %s to %s", keys[k],
                                                                  Repr(label.c_str()).c_str(), Repr(v0.c_str()).c_str(),
                                                                  Repr(Menu_CursorValue()).c_str())))
                            return;
                    }
                } else {
                    Key("right");
                    std::string v1 = Menu_CursorValue();
                    Key("left");
                    std::string v2 = Menu_CursorValue();
                    if (v1 != v0) {
                        if (!Expect(v2 == v0, Fmt("setting: Right then Left leaves %s at %s, not %s", Repr(label.c_str()).c_str(),
                                                  Repr(v2.c_str()).c_str(), Repr(v0.c_str()).c_str())))
                            return;
                    } else {
                        // At the top already: the Left just pressed must have moved it, Right bring it back.
                        if (!Expect(v2 != v0, Fmt("setting: neither Left nor Right changes %s", Repr(label.c_str()).c_str())))
                            return;
                        Key("right");
                        if (!Expect(Menu_CursorValue() == v0, Fmt("setting: Left then Right leaves %s at %s, not %s",
                                                                  Repr(label.c_str()).c_str(), Repr(Menu_CursorValue()).c_str(),
                                                                  Repr(v0.c_str()).c_str())))
                            return;
                    }
                }
            }
            Key("down");
            if (Where() == start)
                return;
        }
    }

    // setting: invert is dimmed with the Classic camera and wakes with Auto.
    void Wake() {
        if (!Wanted("Controls and camera"))
            return;
        Fresh("Controls and camera");
        bool active = false;
        if (!MoveTo("Invert camera"))
            return;
        Menu_CursorSetting(&active);
        if (!Expect(!active, "setting: Invert camera is live with the Classic camera"))
            return;
        if (!MoveTo("Outdoors camera"))
            return;
        Key("right");
        if (!MoveTo("Invert camera"))
            return;
        Menu_CursorSetting(&active);
        Expect(active, "setting: Invert camera stays dimmed with the Auto camera");
    }

    // preset: Classic and Reset settings ask first and the safe answer is first; Back to
    // my settings undoes either; a changed setting marks the preset row.
    void Presets() {
        if (Wanted("Preset")) {
            Fresh("Preset");
            std::string p0 = Menu_PresetLabel();
            if (!MoveTo("Classic"))
                return;
            Key("enter");
            std::string w = Where();
            if (!Expect(EndsWith(w, "| confirm"), "preset: Classic does not ask first"))
                return;
            Expect(EndsWith(w, "| No | confirm"), "preset: the question does not start on No");
            Key("esc");
            if (!Expect(!EndsWith(Where(), "| confirm") && Menu_PresetLabel() == p0,
                        Fmt("preset: Esc on the question does not answer No (preset row %s)", Repr(Menu_PresetLabel()).c_str())))
                return;
            Key("enter");
            Key("enter");
            if (!Expect(Menu_PresetLabel() == p0, Fmt("preset: No changes the preset row to %s", Repr(Menu_PresetLabel()).c_str())))
                return;
            Key("enter");
            Key("down");
            Key("enter");
            std::string p1 = Menu_PresetLabel();
            if (!Expect(p1.find("Classic") != std::string::npos,
                        Fmt("preset: Yes leaves the preset row at %s", Repr(p1.c_str()).c_str())))
                return;
            if (!MoveTo("Back to my settings"))
                return;
            Key("enter");
            Expect(Menu_PresetLabel() == p0, Fmt("preset: Back to my settings leaves the preset row at %s, not %s",
                                                 Repr(Menu_PresetLabel()).c_str(), Repr(p0.c_str()).c_str()));
        }
        if (Wanted("Advanced and reset")) {
            Fresh("Advanced and reset");
            std::string p0 = Menu_PresetLabel();
            if (!MoveTo("Fix retail bugs"))
                return;
            Key("right");
            if (!Expect(std::string(Menu_PresetLabel()).find(", changed") != std::string::npos,
                        Fmt("preset: changing Fix retail bugs leaves the preset row at %s", Repr(Menu_PresetLabel()).c_str())))
                return;
            Key("left");
            if (!Expect(Menu_PresetLabel() == p0, Fmt("preset: putting Fix retail bugs back leaves the preset row at %s",
                                                      Repr(Menu_PresetLabel()).c_str())))
                return;
            ResetSettings();
        }
    }

    // preset: Reset settings, from Classic and a setting changed: the question starts on
    // No, Esc and No change nothing, Yes resets, and Back to my settings (the row right
    // after it) puts the setting and the preset back, then goes.
    void ResetSettings() {
        Fresh("Preset");
        std::string fresh = Menu_PresetLabel();
        if (!MoveTo("Classic"))
            return;
        Key("enter");
        Key("down");
        Key("enter");
        if (!Expect(std::string(Menu_PresetLabel()).find("Classic") != std::string::npos,
                    Fmt("preset: Yes leaves the preset row at %s", Repr(Menu_PresetLabel()).c_str())))
            return;
        OpenToo("Advanced and reset");
        const char *setting = "Show experimental settings";
        if (!MoveTo(setting))
            return;
        std::string v0 = Menu_CursorValue();
        Key("right");
        std::string mine = Menu_CursorValue(), pMine = Menu_PresetLabel();
        if (!Expect(mine != v0, Fmt("preset: Right does not change %s", setting)))
            return;
        if (!MoveTo("Reset settings"))
            return;
        std::string here = Where();
        Key("enter");
        std::string w = Where();
        if (!Expect(EndsWith(w, "| confirm"), "preset: Reset settings does not ask first"))
            return;
        Expect(EndsWith(w, "| No | confirm"), "preset: the Reset settings question does not start on No");
        Key("esc");
        if (!Expect(Where() == here && Menu_PresetLabel() == pMine,
                    Fmt("preset: Esc on the Reset settings question does not answer No (%s, preset row %s)",
                        Repr(Where().c_str()).c_str(), Repr(Menu_PresetLabel()).c_str())))
            return;
        Key("enter");
        Key("enter");
        if (!Expect(Where() == here && Menu_PresetLabel() == pMine,
                    Fmt("preset: No on the Reset settings question changes the preset row to %s",
                        Repr(Menu_PresetLabel()).c_str())))
            return;
        Key("enter");
        Key("down");
        Key("enter");
        if (!Expect(Menu_PresetLabel() == fresh, Fmt("preset: Reset settings leaves the preset row at %s, not %s",
                                                    Repr(Menu_PresetLabel()).c_str(), Repr(fresh.c_str()).c_str())))
            return;
        Key("down");
        if (!Expect(!strcmp(Menu_CursorLabel(), "Back to my settings"),
                    Fmt("preset: after a reset, the row after Reset settings is %s, not Back to my settings",
                        Repr(Menu_CursorLabel()).c_str())))
            return;
        if (!MoveTo(setting))
            return;
        if (!Expect(Menu_CursorValue() == v0, Fmt("preset: Reset settings leaves %s at %s, not %s", setting,
                                                  Repr(Menu_CursorValue()).c_str(), Repr(v0.c_str()).c_str())))
            return;
        if (!MoveTo("Back to my settings"))
            return;
        Key("enter");
        Expect(Menu_PresetLabel() == pMine, Fmt("preset: Back to my settings after a reset leaves the preset row at %s, not %s",
                                                Repr(Menu_PresetLabel()).c_str(), Repr(pMine.c_str()).c_str()));
        Expect(!RowDrawn("Back to my settings"), "preset: Back to my settings stays after it put the settings back");
        if (!MoveTo(setting))
            return;
        Expect(Menu_CursorValue() == mine, Fmt("preset: Back to my settings after a reset leaves %s at %s, not %s", setting,
                                               Repr(Menu_CursorValue()).c_str(), Repr(mine.c_str()).c_str()));
    }

    // experimental: the row comes and goes with Show experimental settings; Enter opens the
    // page and Esc comes back to the row; on every row Right and Left change the value and
    // the other key puts it back, and Del puts the default back; Page Down and Page Up go to
    // the ends; a changed value marks the preset changed, Modern and Reset settings put it
    // back to its default, and Back to my settings names it and puts it back.
    bool Table() const { return Menu_GetSwitch("exptable") != 0; }
    void ToRows() {  // the list opens on its groups
        if (!Table())
            Key("enter");
    }
    void LeaveExp() { Key("esc", Table() ? 1 : 2); }
    bool OpenAgain(const char *page) {
        OpenToo(page);
        return Expect(!strcmp(Menu_PageName(), page), Fmt("experimental: open %s lands on %s", page,
                                                          Repr(Menu_PageName()).c_str()));
    }
    void ExpRow() {
        std::string name = Exp_CursorName(), v0 = Exp_CursorValue();
        Key("right");
        std::string v1 = Exp_CursorValue();
        Key("left");
        std::string v2 = Exp_CursorValue();
        if (v1 != v0) {
            if (!Expect(v2 == v0, Fmt("experimental: Right then Left leaves %s at %s, not %s", name.c_str(),
                                      Repr(v2.c_str()).c_str(), Repr(v0.c_str()).c_str())))
                return;
            Key("right");
        } else {
            if (!Expect(v2 != v0, Fmt("experimental: neither Left nor Right changes %s", name.c_str())))
                return;
            Key("right");
            if (!Expect(Exp_CursorValue() == v0, Fmt("experimental: Left then Right leaves %s at %s, not %s", name.c_str(),
                                                     Repr(Exp_CursorValue()).c_str(), Repr(v0.c_str()).c_str())))
                return;
            Key("left");
        }
        // Moved off the default now (a fresh page has every value at it): Del puts it back.
        Expect(Menu_PresetLabel() && std::string(Menu_PresetLabel()).find(", changed") != std::string::npos,
               Fmt("experimental: %s changed leaves the preset row at %s", name.c_str(), Repr(Menu_PresetLabel()).c_str()));
        Key("del");
        Expect(Exp_CursorValue() == v0, Fmt("experimental: Del leaves %s at %s, not its default %s", name.c_str(),
                                            Repr(Exp_CursorValue()).c_str(), Repr(v0.c_str()).c_str()));
    }

    void Experimental() {
        if (!Wanted(kExpPage))
            return;
        const char *row = "Experimental settings", *sw = "Show experimental settings";
        Fresh("Advanced and reset");
        if (!Expect(!RowDrawn(row), "experimental: the row shows with Show experimental settings off"))
            return;
        if (!MoveTo(sw))
            return;
        Key("right");
        if (!Expect(RowDrawn(row), "experimental: Show experimental settings on does not show the row"))
            return;
        if (!MoveTo(row))
            return;
        int d0, d1;
        std::string here = Where(&d0);
        Key("enter");
        Where(&d1);
        if (!Expect(Exp_Active() && d1 == d0 + 1, "experimental: Enter on the row does not open the page"))
            return;
        Check(kExpPage);
        Key("esc");
        if (!Expect(Where() == here, Fmt("experimental: Esc from the page went to %s, not back to %s",
                                         Repr(Where().c_str()).c_str(), Repr(here.c_str()).c_str())))
            return;
        if (!MoveTo(sw))
            return;
        Key("left");
        if (!Expect(!RowDrawn(row), "experimental: the row stays with Show experimental settings off"))
            return;

        // Every row.
        Fresh(kExpPage);
        ToRows();
        std::string first = Exp_CursorName();
        for (size_t g = 0; g < sizeof kExpGroups / sizeof kExpGroups[0]; g++) {
            for (int r = 0; r < kExpGroups[g]; r++) {
                ExpRow();
                Key("down");
            }
            if (!Table()) {
                Key("esc");
                Key("down");
                if (EndsWith(Where(), "| Return to previous menu"))
                    Key("down");  // one column: the groups page has a Return row
                Key("enter");
            }
        }
        if (!Expect(Exp_CursorName() == first, Fmt("experimental: Down does not come back round to %s (on %s)",
                                                   first.c_str(), Exp_CursorName())))
            return;

        // The ends, a window at a time.
        if (Table()) {
            Key("pgdn");
            std::string one = Exp_CursorName();
            Expect(one != first && Where().find(Exp_Name(1)) == std::string::npos,
                   Fmt("experimental: Page Down moves to %s, not a window on", one.c_str()));
            Key("pgdn", 10);
            Expect(!strcmp(Exp_CursorName(), Exp_Name(Exp_Count() - 1)),
                   Fmt("experimental: Page Down stops at %s, not the last row", Exp_CursorName()));
            Key("pgup", 10);
            Expect(Exp_CursorName() == first, Fmt("experimental: Page Up stops at %s, not the first row", Exp_CursorName()));
        }

        // Presets: changed, Modern, Back to my settings.
        Fresh(kExpPage);
        ToRows();
        std::string v0 = Exp_CursorValue(), name = Exp_CursorName();
        Key("right");
        std::string v1 = Exp_CursorValue();
        LeaveExp();
        if (!OpenAgain("Preset") || !MoveTo("Modern"))
            return;
        Key("enter");
        Expect(std::string(Menu_PresetLabel()).find(", changed") == std::string::npos,
               Fmt("experimental: Modern leaves the preset row at %s", Repr(Menu_PresetLabel()).c_str()));
        if (!MoveTo("Back to my settings"))
            return;
        Expect(Voice().find(name) != std::string::npos || Voice().find(" more.") != std::string::npos,
               Fmt("experimental: Back to my settings does not name %s: %s", name.c_str(), Repr(Voice().c_str()).c_str()));
        if (!OpenAgain(kExpPage))
            return;
        ToRows();
        if (!Expect(Exp_CursorValue() == v0, Fmt("experimental: Modern leaves %s at %s, not %s", name.c_str(),
                                                 Repr(Exp_CursorValue()).c_str(), Repr(v0.c_str()).c_str())))
            return;
        LeaveExp();
        if (!OpenAgain("Preset") || !MoveTo("Back to my settings"))
            return;
        Key("enter");
        if (!OpenAgain(kExpPage))
            return;
        ToRows();
        if (!Expect(Exp_CursorValue() == v1, Fmt("experimental: Back to my settings leaves %s at %s, not %s", name.c_str(),
                                                 Repr(Exp_CursorValue()).c_str(), Repr(v1.c_str()).c_str())))
            return;

        // Reset settings, and its undo.
        LeaveExp();
        if (!MoveTo("Reset settings"))
            return;
        Key("enter");
        Key("down");
        Key("enter");
        Expect(!strcmp(Menu_CursorLabel(), "Reset settings"),
               Fmt("experimental: after a reset the cursor is on %s, not Reset settings", Repr(Menu_CursorLabel()).c_str()));
        Expect(std::string(Menu_PresetLabel()).find(", changed") == std::string::npos,
               Fmt("experimental: Reset settings leaves the preset row at %s", Repr(Menu_PresetLabel()).c_str()));
        Key("down");
        if (!Expect(!strcmp(Menu_CursorLabel(), "Back to my settings"),
                    Fmt("experimental: after a reset the row after Reset settings is %s", Repr(Menu_CursorLabel()).c_str())))
            return;
        Expect(Voice().find(name) != std::string::npos || Voice().find(" more.") != std::string::npos,
               Fmt("experimental: Back to my settings after a reset does not name %s: %s", name.c_str(),
                   Repr(Voice().c_str()).c_str()));
        Key("enter");
        if (!OpenAgain(kExpPage))
            return;
        ToRows();
        Expect(Exp_CursorValue() == v1, Fmt("experimental: Back to my settings after a reset leaves %s at %s, not %s",
                                             name.c_str(), Repr(Exp_CursorValue()).c_str(), Repr(v1.c_str()).c_str()));
    }

    // console: the Console row shows the version; Enter opens the full console after a key and
    // the read-only one after a pad button; Esc closes the full one and only Esc, any button
    // the read-only one; the cursor is back on Console. Frame rate does not mark the preset.
    void Console() {
        if (!Wanted("Advanced and reset"))
            return;
        Fresh("Advanced and reset");
        if (!MoveTo("Console"))
            return;
        std::string here = Where();
        Expect(Menu_CursorValue()[0] != 0, "console: the Console row shows no version");
        Key("enter");
        if (!Expect(!strcmp(Menu_ConsoleMode(), "full"),
                    Fmt("console: Enter after a key opens %s, not the full console", Menu_ConsoleMode())))
            return;
        Check();
        Key("down");
        Key("enter");
        Expect(!strcmp(Menu_ConsoleMode(), "full"), "console: Down or Enter closed the full console");
        Key("esc");
        if (!Expect(Where() == here, Fmt("console: Esc from the full console went to %s, not %s", Repr(Where().c_str()).c_str(),
                                         Repr(here.c_str()).c_str())))
            return;
        const char *closers[] = {"pad:b", "pad:dpad-down", "J"};
        for (int i = 0; i < 3; i++) {
            Press("pad:a");
            Key("enter");
            if (!Expect(!strcmp(Menu_ConsoleMode(), "read-only"),
                        Fmt("console: Enter after a pad button opens %s, not the read-only console", Menu_ConsoleMode())))
                return;
            Check();
            Press(closers[i]);
            Expect(Where() == here, Fmt("console: %s leaves the read-only console at %s", closers[i], Repr(Where().c_str()).c_str()));
        }

        int fpsrow = Menu_GetSwitch("fpsrow");
        const char *page = fpsrow ? "Advanced and reset" : "Display and graphics";
        if (!Wanted(page))
            return;
        Fresh(page);
        if (!MoveTo("Frame rate"))
            return;
        std::string p0 = Menu_PresetLabel();
        Key("right");
        Expect(Menu_PresetLabel() == p0, Fmt("console: Frame rate on changes the preset row to %s", Repr(Menu_PresetLabel()).c_str()));
    }

    // bind: on the first action of each group, in each column.
    void ToChip(int device, int group, int column) {
        Fresh("Keys and buttons");
        if (device)
            Key("tab");
        Key("down", group);
        Key("enter");
        Key("right", column);
    }

    void BindOne(int device, int group, int column, bool pad) {
        ToChip(device, group, column);
        std::string before = ChipText();
        Key("enter");
        if (!Expect(Waiting(), "bind: Enter does not wait for a key"))
            return;
        Expect(Voice().compare(0, 8, "Press a ") == 0, Fmt("bind: the voice does not ask for the key: %s", Repr(Voice().c_str()).c_str()));
        Check();
        Key("esc");
        if (!Expect(!Waiting(), "bind: Esc leaves the wait for a key going"))
            return;
        if (!Expect(ChipText() == before,
                    Fmt("bind: Esc changed the chip to %s (was %s)", Repr(ChipText().c_str()).c_str(), Repr(before.c_str()).c_str())))
            return;

        // The other device's input does not bind.
        Key("enter");
        Press(pad ? "J" : "pad:a");
        if (!Expect(Waiting(), Fmt("bind: a %s ended the wait for a %s (chip %s)", pad ? "key" : "pad button",
                                   pad ? "button" : "key", Repr(ChipText().c_str()).c_str())))
            return;
        Key("esc");

        const char *partner = pad ? "Talk and search" : "Jetpack";
        char want[32];
        Controls_Binding(partner, pad ? 2 : 0, want, sizeof want);
        if (Menu_GetSwitch("keyrule") == 0) {
            // A key another action has is refused: the chip keeps its key, nothing
            // goes red, and the help names the action that has it.
            std::string kept = ChipText();
            Key("enter");
            Press(pad ? "pad:a" : "J");
            if (!Expect(!Waiting() && ChipText() == kept,
                        Fmt("bind: %s, another action's, replaced %s (chip %s)", Repr(want).c_str(), Repr(kept.c_str()).c_str(),
                            Repr(ChipText().c_str()).c_str())))
                return;
            Check();
            Expect(!ChipRed() && !Clashes(partner), Fmt("bind: a refused %s left a clash", Repr(want).c_str()));
            Expect(Voice() == std::string("Clear ") + want + " on " + partner + " first.",
                   Fmt("bind: the help does not say %s has it: %s", partner, Repr(Voice().c_str()).c_str()));
            Key("del");
            Expect(ChipText().empty(), Fmt("bind: Del leaves %s", Repr(ChipText().c_str()).c_str()));
            return;
        }

        // Allowed (keyrule 1): the chip takes it, both go red, the help says so.
        Key("enter");
        Press(pad ? "pad:a" : "J");
        if (!Expect(!Waiting() && ChipText() == want,
                    Fmt("bind: the chip shows %s, not %s", Repr(ChipText().c_str()).c_str(), Repr(want).c_str())))
            return;
        Check();
        Expect(ChipRed() && Clashes(partner),
               Fmt("bind: %s is also %s's, and the two chips are not both red", Repr(want).c_str(), partner));
        Expect(Voice().find(std::string("is also ") + partner) != std::string::npos,
               Fmt("bind: the help does not name %s: %s", partner, Repr(Voice().c_str()).c_str()));

        Key("del");
        if (!Expect(ChipText().empty(), Fmt("bind: Del leaves %s", Repr(ChipText().c_str()).c_str())))
            return;
        Expect(!ChipRed() && !Clashes(partner), Fmt("bind: %s stays red after Del", partner));
    }

    void Binding(bool twoDevices) {
        if (!Wanted("Keys and buttons"))
            return;
        const int groups = (int)(sizeof kGroupFirst / sizeof kGroupFirst[0]);
        for (int d = 0; d < (twoDevices ? 2 : 1); d++) {
            int cols = !twoDevices ? 3 : d ? 1 : 2;
            for (int g = 0; g < groups; g++)
                for (int c = 0; c < cols; c++)
                    BindOne(d, g, c, d == 1 || c == 2);
        }

        // Forward's Key 2 put in Key 1 leaves Key 2 empty, and no clash with itself.
        ToChip(0, 0, 0);
        char key2[32];
        Controls_Binding("Forward", 1, key2, sizeof key2);
        std::string k2 = key2;
        Key("enter");
        Press("Keypad 8");
        Controls_Binding("Forward", 1, key2, sizeof key2);
        if (Expect(k2 == "Num 8" && ChipText() == "Num 8" && !key2[0],
                   Fmt("bind: Num 8 moved to Forward's Key 1 leaves Key 1 %s, Key 2 %s", Repr(ChipText().c_str()).c_str(),
                       Repr(key2).c_str())))
            Expect(!ChipRed(), "bind: a key moved from Key 2 to Key 1 shows a clash");

        // The wait gives up after 5 s and changes nothing.
        ToChip(0, 0, 0);
        std::string before = ChipText();
        Key("enter");
        Wait(5000 / 16 + 5);
        Expect(!Waiting() && ChipText() == before,
               Fmt("bind: 5 s on, the wait %s (chip %s)", Waiting() ? "goes on" : "ended", Repr(ChipText().c_str()).c_str()));
    }

    void Behaviour(bool gameplay, bool twoDevices) {
        Reach(gameplay);
        for (size_t p = 0; p < sizeof kPages / sizeof kPages[0]; p++)
            if (strcmp(kPages[p], "Gameplay") || gameplay)
                Settings(kPages[p]);
        Wake();
        Presets();
        Experimental();
        Console();
        Binding(twoDevices);
    }
};

int Selftest_Run(const SelftestOptions &opt, const char *self) {
    std::vector<const char *> specs(opt.combos.begin(), opt.combos.end());
    if (specs.empty())
        specs.assign(kCombos, kCombos + sizeof kCombos / sizeof kCombos[0]);

    Engine_SetPixels(false);
    Walker w;
    w.opt = &opt;
    w.size = Fmt("%ux%u", (unsigned)Engine_PhysW(), (unsigned)Engine_PhysH());
    w.checks = 0;
    for (size_t i = 0; i < specs.size(); i++) {
        std::vector<Switch> combo;
        if (!ParseCombo(specs[i], &combo)) {
            fprintf(stderr, "bad --combo %s (name=value,name=value with the switch names of `state`)\n", specs[i]);
            return 2;
        }
        w.switches = combo;
        w.again = Fmt("%s --size %s", self, w.size.c_str());
        for (size_t s = 0; s < combo.size(); s++)
            w.again += Fmt(" --switch %s=%d", combo[s].name.c_str(), combo[s].value);
        w.again += " --switch typing=2";
        w.combo = ComboLabel(combo);
        w.Start();
        bool twoDevices = ComboValue(combo, "devices", 1) != 0;
        bool gameplay = ComboValue(combo, "gameplay", 0) != 0;
        if (twoDevices)
            w.Controls(2, true);
        else
            w.Controls(3, false);
        w.Pages(gameplay);
        w.ExperimentalPage();
        if (!opt.layoutOnly)
            w.Behaviour(gameplay, twoDevices);
    }
    for (std::map<std::string, std::string>::const_iterator it = w.problems.begin(); it != w.problems.end(); ++it) {
        printf("%s\n", it->first.c_str());
        if (opt.again)
            printf("    again: %s\n", it->second.c_str());
    }
    printf("%d problem(s) in %d checks\n", (int)w.problems.size(), w.checks);
    return w.problems.empty() ? 0 : 1;
}
