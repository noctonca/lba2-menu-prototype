// The self test: walks what tools/sweep.py walks (every chip of the Controls
// page, then every row of every page) in one process, without the frames a
// script spends waiting, and checks the same rules. Each problem comes with a
// command line that opens the screen it was found on.
#include "selftest.h"

#include "bindings.h"
#include "host.h"
#include "more_options.h"
#include "lang.h"
#include "menu.h"
#include "menu_parts.h"
#include "trace.h"
#include "ui.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

// The switch combinations tools/sweep.py runs, in its order: the pad's names of each
// family (0 is the connected pad's, Xbox without one), and the green plasma on the chosen preset.
static const char *kCombos[] = {"padnames=0", "padnames=2", "padnames=3", "greencurrent=1"};

static const char *kPages[] = {
    "Options", "Preset", "Language and interface", "Display and graphics", "Sound",
    "Controls and camera", "Advanced and reset",
};

// Actions per group on the Controls page (bindings.cpp kActions).
static const int kGroupSizes[] = {5, 6, 4, 7, 4, 4, 6};
// Rows per group on the More options page (more_options.cpp kTuning).
static const int kExpGroups[] = {22, 9, 7, 3, 2};
static const char *kExpPage = "More options";

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

// As Python prints a dict of ints: {"padnames": 2}.
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
// The row a column head is drawn inside (wide), or -1 for a head above the rows (one column).
static int HeadRow(const HeadView &h, const DrawnRow *rows, int n) {
    for (int i = 0; i < n; i++)
        if (rows[i].x0 <= h.x0 && h.x1 <= rows[i].x1 && rows[i].y0 < h.y0 && h.y1 < rows[i].y1)
            return i;
    return -1;
}

static void Check(std::vector<std::string> *found) {
    const S32 W = (S32)Host_LogicalW(), H = (S32)Host_LogicalH();
    MenuHelp v;
    Menu_Help(&v);
    // Help longer than its window pages on, a window at a time; it is cut off only past
    // the most lines the voice wraps.
    if (v.needed >= VOICE_MAX_LINES)
        found->push_back(Fmt("help cut off (%d lines or more, %d px): %s", v.needed, v.width, Repr(v.text).c_str()));
    if (v.max && v.needed && v.pages != (v.needed + v.max - 1) / v.max)
        found->push_back(Fmt("help pages: %d lines in %d make %d windows, not %d: %s", v.needed, v.max, v.pages,
                             (v.needed + v.max - 1) / v.max, Repr(v.text).c_str()));
    // Help never pages (2026-11-12): within budget it fits its window, and console help is in
    // the More options table's status lines.
    if (v.pages > 1)
        found->push_back(Fmt("help in %d windows: %s", v.pages, Repr(v.text).c_str()));
    // Console help in the table's status lines is never cut off.
    MoreOptionsTable rt;
    if (MoreOptions_Table(&rt) && rt.rowHelpCut)
        found->push_back(Fmt("table: row help cut off: %s", Repr(rt.rowHelp).c_str()));
    // Every clip set while drawing is put back: a clip left on hides what is drawn after it.
    if (ClipXMin != 0 || ClipYMin != 0 || ClipXMax != W - 1 || ClipYMax != H - 1)
        found->push_back(Fmt("clip left on after the frame: [%d, %d, %d, %d]", (int)ClipXMin, (int)ClipYMin,
                             (int)ClipXMax, (int)ClipYMax));
    // A value Enter also steps names Enter beside the arrows; one it leaves does not
    // (EPmager, 2026-11-11). Only where a legend is drawn.
    bool isValue;
    const bool entersteps = Menu_CursorEnterSteps(&isValue);
    if (isValue && *Menu_Legend() && (strstr(Menu_Legend(), "[<>/") != NULL) != entersteps)
        found->push_back(Fmt("legend %s Enter on a value Enter %s: %s", entersteps ? "leaves out" : "names",
                             entersteps ? "steps" : "leaves", Repr(Menu_Legend()).c_str()));
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
    int nc = Bindings_Active() ? Bindings_Chips(chips, 32) : 0;
    for (int i = 0; i < nc; i++)
        if (!chips[i].fits)
            found->push_back(Fmt("key name cut in its chip: %s %s %s", Repr(chips[i].action).c_str(), chips[i].column,
                                 Repr(chips[i].name).c_str()));

    MoreOptionsTable et;
    bool table = MoreOptions_Table(&et);
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
    // A red chip under the cursor says in the help what else has its key: the words between
    // the two names of "%s is also %s.", in the Text language.
    std::string also = Menu_Tr("%s is also %s.");
    also = also.substr(2, also.rfind("%s") - 2);
    for (int i = 0; i < nc; i++)
        if (!strcmp(chips[i].fill, "cursor") && chips[i].conflict && !strstr(v.text, also.c_str()))
            found->push_back(Fmt("red key not explained in the help: %s %s %s", Repr(chips[i].action).c_str(),
                                 chips[i].column, Repr(chips[i].name).c_str()));
    // A clash goes both ways: the other action shows one too.
    if (Bindings_Active()) {
        ConflictView cv[128];
        int ncf = Bindings_Conflicts(cv, 128);
        for (int i = 0; i < ncf; i++) {
            bool back = false;
            for (int j = 0; j < ncf && !back; j++)
                back = !strcmp(cv[j].action, cv[i].other);
            if (!back)
                found->push_back(Fmt("clash one way: %s %s is also %s, which shows no clash", Repr(cv[i].action).c_str(),
                                     cv[i].column, Repr(cv[i].other).c_str()));
        }
    }
    // The More options table: its panel clear of the canvas edge and the help, its
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
            const MoreOptionsLine &l = et.lines[i];
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
    int nh = Bindings_Active() ? Bindings_Heads(heads, 4) : 0;
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
    // The Preset page names the chosen preset with "(current)", on that row alone (EPmager,
    // 2026-11-11: the green plasma alone read like a second cursor).
    if (!strcmp(Menu_PageName(), "Preset")) {
        int marked = 0, presets = 0;  // no preset rows while the Classic question is up
        for (int i = 0; i < n; i++) {
            const size_t len = strlen(rows[i].label);
            presets += !strncmp(rows[i].label, "Modern", 6) || !strncmp(rows[i].label, "Classic", 7);
            marked += len > 10 && !strcmp(rows[i].label + len - 10, " (current)");
        }
        if (presets && marked != 1)
            found->push_back(Fmt("current: %d rows on Preset say (current), not 1", marked));
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
    // Wide, a page has no Return row: Esc goes back (docs/UX.md, Fitts's Law).
    for (int i = 0; wide && i < n; i++)
        if (!strncmp(rows[i].label, "Return to ", 10))
            found->push_back(Fmt("return: the wide layout shows %s", Repr(rows[i].label).c_str()));
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
    Host_BeginFrame();
    Trace_SetFrame(Host_Frame());
    Menu_Update();
    if (draw) {
        Menu_Draw();
        Host_Present();
    }
}


// The groups' first actions (bindings.cpp kActions), where the binding walk tries keys.
static const char *kGroupFirst[] = {"Forward", "Behavior action", "Normal", "Weapon 1", "Penguin", "Recenter", "Game menu"};

static bool EndsWith(const std::string &s, const char *tail) {
    size_t n = strlen(tail);
    return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

struct Walker {
    const SelftestOptions *opt;
    std::vector<Switch> switches;  // this combo's
    std::string size, combo, again;  // again: the command line up to --do
    int lang = 0;                    // the Text language the walk draws in
    U32 physW = 0, physH = 0;        // the screen the run was started at (--size)
    std::vector<std::string> path;   // commands since the last open
    std::map<std::string, std::string> problems;  // line -> the command line that shows it
    int checks;

    // ── Steps ──

    // A fresh process's state under this combo, as the again line starts it. Presets set the
    // screen, so a walk can end at Classic's 640x480: put the run's screen back first, before
    // Menu_Reset takes it as Modern's (2026-11-11: from the first walk that chose Classic,
    // every later walk of a --size 1920x1080 run ran at 640x480).
    void Start() {
        if (Host_PhysW() != physW || Host_PhysH() != physH)
            Host_SetScreen(physW, physH, false);
        if (Host_Fullscreen())
            Host_SetFullscreen(false);
        Menu_Reset();
        for (size_t s = 0; s < switches.size(); s++)
            Menu_SetSwitch(switches[s].name.c_str(), switches[s].value);
        Menu_Set("text_lang", lang);
        // Help shown whole, so a check never depends on how far the typing got.
        Menu_SetSwitch("whole", 1);
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
    // The label of a drawn row that starts with prefix, or "".
    std::string RowStarting(const std::string &prefix) const {
        const DrawnRow *rows;
        int n = Menu_DrawnRows(&rows);
        for (int i = 0; i < n; i++)
            if (!strncmp(rows[i].label, prefix.c_str(), prefix.size()))
                return rows[i].label;
        return "";
    }
    void Fresh(const char *page) {
        Start();
        Open(page);
    }
    void Key(const char *name, int count = 1) {
        UiKey k;
        Host_KeyByName(name, &k);
        for (int i = 0; i < count; i++) {
            path.push_back(std::string("key ") + name);
            Host_Inject(k);
            Frame();
        }
    }
    void Press(const char *raw) {
        path.push_back(std::string("press ") + raw);
        Host_InjectRaw(raw);
        Frame();
    }
    // A pad button, as a pad-only player presses it: the raw press and its fixed menu key.
    void Pad(const char *name, int count = 1) {
        for (int i = 0; i < count; i++) {
            path.push_back(std::string("pad ") + name);
            Host_InjectPad(name);
            Frame();
        }
    }
    // A key's name as the voice says it ("<J>").
    static std::string Said(const char *name) {
        char out[48];
        Bindings_SaidKey(name, out, sizeof out);
        return out;
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
    int BindingsDepth() const {
        char buf[200];
        return Bindings_Active() ? Bindings_Where(buf, sizeof buf) : 0;
    }
    bool InActions() const { return BindingsDepth() == 2; }
    std::string BindingsWhere() const {
        char buf[200] = "";
        if (Bindings_Active())
            Bindings_Where(buf, sizeof buf);
        return buf;
    }
    bool Waiting() const { return BindingsWhere().find("| waiting") != std::string::npos; }
    bool Asking() const { return BindingsWhere().find("| asking") != std::string::npos; }
    // The chip under the cursor, from the last frame drawn.
    bool CursorChip(ChipView *out) const {
        ChipView c[32];
        int n = Bindings_Active() ? Bindings_Chips(c, 32) : 0;
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
        int n = Bindings_Conflicts(cv, 128);
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
        if (expectPage && strcmp(page, expectPage) != 0)
            found.push_back(Fmt("walk did not reach %s (on %s)", Repr(expectPage).c_str(), Repr(page).c_str()));
        ::Check(&found);
        for (size_t i = 0; i < found.size(); i++)
            Record(found[i]);
    }
    // A behavior check that failed. The line starts with the walk's name.
    void Fail(const std::string &what) {
        checks++;
        Record(what);
    }
    // A behavior check that passed, for the count.
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
            // The chosen preset reads "Modern (current)": the name is enough to find it.
            const std::string here = Menu_CursorLabel(), marked = std::string(label) + " (current)";
            if (here == label || here == marked)
                return true;
            Key("down");
        }
        Fail(Fmt("walk: no row %s", Repr(label).c_str()));
        return false;
    }
    bool Wanted(const char *page) const { return !opt->only || strcmp(opt->only, page) == 0; }

    // ───────────────────────────── The layout walk ─────────────────────────────

    void Controls() {
        if (!Wanted("Keys and buttons"))
            return;
        Open("Keys and buttons");
        Check("Keys and buttons");
        for (int d = 0; d < 2; d++) {
            if (d)
                Key("tab");
            int count = d ? 1 : 2;  // Key 1 and Key 2; the pad view has one column
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

    // The More options table: every row.
    void MoreOptionsPage() {
        if (!Wanted(kExpPage))
            return;
        Open(kExpPage);
        Check(kExpPage);
        for (int i = 0; i < MoreOptions_Count(); i++) {
            Key("down");
            Check();
        }
    }

    void Pages() {
        for (size_t p = 0; p < sizeof kPages / sizeof kPages[0]; p++) {
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
        // More options has its own walk (more_options:), Down there stops at the ends.
        bool leaf = EndsWith(opened, "| confirm") || EndsWith(opened, "| waiting") || MoreOptions_Active();
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
    void Reach() {
        if (opt->only)
            return;
        Fresh("Options");
        std::set<std::string> reached;
        if (!Explore(1, &reached))
            return;
        std::vector<const char *> want(kPages + 1, kPages + sizeof kPages / sizeof kPages[0]);
        want.push_back("Keys and buttons");
        path.assign(1, "open Options");  // the walk came back round to where it started
        for (size_t i = 0; i < want.size(); i++)
            Expect(reached.count(want[i]) != 0, Fmt("reach: no keys lead to %s", Repr(want[i]).c_str()));
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

    // setting: invert is live with the Auto camera (Modern's, a fresh start's) and dimmed
    // with the Classic camera.
    void Wake() {
        if (!Wanted("Controls and camera"))
            return;
        Fresh("Controls and camera");
        bool active = false;
        if (!MoveTo("Invert camera"))
            return;
        Menu_CursorSetting(&active);
        if (!Expect(active, "setting: Invert camera is dimmed with the Auto camera"))
            return;
        Key("up", 3);
        if (!Expect(!strcmp(Menu_CursorLabel(), "Outdoors camera"),
                    Fmt("setting: Outdoors camera is not three rows above Invert camera (%s)", Repr(Menu_CursorLabel()).c_str())))
            return;
        Key("right");
        if (!MoveTo("Invert camera"))
            return;
        Menu_CursorSetting(&active);
        Expect(!active, "setting: Invert camera is live with the Classic camera");
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
            if (!MoveTo("Undo preset"))
                return;
            Key("enter");
            Expect(Menu_PresetLabel() == p0, Fmt("preset: Undo preset leaves the preset row at %s, not %s",
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

    // preset: Undo preset gives back every value a preset sets and keeps a player's setting
    // changed since (the menu plan's test); its help names what it puts back, not the
    // player's setting, in one window.
    void UndoPreset() {
        if (!Wanted("Preset"))
            return;
        Fresh("Preset");
        std::string p0 = Menu_PresetLabel();
        if (!MoveTo("Classic"))
            return;
        Key("enter");
        Key("down");
        Key("enter");
        const char *setting = "Music", *page = "Sound";
        OpenToo(page);
        if (!MoveTo(setting))
            return;
        Key("left");
        std::string mine = Menu_CursorValue();
        OpenToo("Preset");
        if (!MoveTo("Undo preset"))
            return;
        Check("Preset");
        Expect(Voice().find(Ui_Tr(setting)) == std::string::npos,
               Fmt("preset: Undo preset names %s, a player's setting: %s", setting, Repr(Voice().c_str()).c_str()));
        Key("enter");
        Expect(Menu_PresetLabel() == p0, Fmt("preset: Undo preset leaves the preset row at %s, not %s",
                                             Repr(Menu_PresetLabel()).c_str(), Repr(p0.c_str()).c_str()));
        OpenToo(page);
        if (!MoveTo(setting))
            return;
        Expect(Menu_CursorValue() == mine, Fmt("preset: Undo preset puts %s, changed since the preset, from %s to %s",
                                               setting, Repr(mine.c_str()).c_str(), Repr(Menu_CursorValue()).c_str()));
    }

    // preset: Reset settings, from Classic and a setting changed: the question starts on
    // No, Esc and No change nothing, Yes resets, and Undo preset (the row right
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
        // A player's setting, which no preset sets: Reset settings still puts it back.
        const char *setting = "Vsync", *page = "Display and graphics";
        OpenToo(page);
        if (!MoveTo(setting))
            return;
        std::string v0 = Menu_CursorValue();
        Key("right");
        std::string mine = Menu_CursorValue(), pMine = Menu_PresetLabel();
        if (!Expect(mine != v0, Fmt("preset: Right does not change %s", setting)))
            return;
        // The languages stay (the menu plan): Voices changed here, Text as the walk set it.
        OpenToo("Language and interface");
        if (!MoveTo("Voices"))
            return;
        Key("right");
        std::string voices = Menu_CursorValue();
        // The bindings go back: Forward's Key 1 cleared here.
        OpenToo("Keys and buttons");
        Key("enter");
        char bound[32], key1[32];
        Bindings_Text("Forward", 0, key1, sizeof key1);
        Key("del");
        Bindings_Text("Forward", 0, bound, sizeof bound);
        if (!Expect(key1[0] && !bound[0], Fmt("preset: Del on Forward's Key 1 leaves %s", Repr(bound).c_str())))
            return;
        OpenToo("Advanced and reset");
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
        if (!Expect(!strcmp(Menu_CursorLabel(), "Undo reset"),
                    Fmt("preset: after a reset, the row after Reset settings is %s, not Undo reset",
                        Repr(Menu_CursorLabel()).c_str())))
            return;
        Check("Advanced and reset");  // its help names what it puts back, in one window
        Expect(Ui_Lang() == lang, Fmt("preset: Reset settings changes Text from %d to %d", lang, Ui_Lang()));
        Bindings_Text("Forward", 0, bound, sizeof bound);
        Expect(!strcmp(bound, key1), Fmt("preset: Reset settings leaves Forward's Key 1 at %s, not %s",
                                         Repr(bound).c_str(), Repr(key1).c_str()));
        OpenToo(page);
        if (!MoveTo(setting))
            return;
        if (!Expect(Menu_CursorValue() == v0, Fmt("preset: Reset settings leaves %s at %s, not %s", setting,
                                                  Repr(Menu_CursorValue()).c_str(), Repr(v0.c_str()).c_str())))
            return;
        OpenToo("Language and interface");
        if (!MoveTo("Voices"))
            return;
        Expect(Menu_CursorValue() == voices, Fmt("preset: Reset settings changes Voices from %s to %s",
                                                 Repr(voices.c_str()).c_str(), Repr(Menu_CursorValue()).c_str()));
        OpenToo("Advanced and reset");
        if (!MoveTo("Undo reset"))
            return;
        Key("enter");
        Expect(Menu_PresetLabel() == pMine, Fmt("preset: Undo reset after a reset leaves the preset row at %s, not %s",
                                                Repr(Menu_PresetLabel()).c_str(), Repr(pMine.c_str()).c_str()));
        Expect(!RowDrawn("Undo reset"), "preset: Undo reset stays after it put the settings back");
        Bindings_Text("Forward", 0, bound, sizeof bound);
        Expect(!bound[0], Fmt("preset: Undo reset leaves Forward's Key 1 at %s, not cleared", Repr(bound).c_str()));
        OpenToo(page);
        if (!MoveTo(setting))
            return;
        Expect(Menu_CursorValue() == mine, Fmt("preset: Undo reset after a reset leaves %s at %s, not %s", setting,
                                               Repr(Menu_CursorValue()).c_str(), Repr(mine.c_str()).c_str()));
    }

    // more_options: the row is always on Advanced and reset; Enter opens the
    // page and Esc comes back to the row; on every row Right and Left change the value and
    // the other key puts it back, and Del puts the default back; Page Down and Page Up go to
    // the ends; a changed value marks the preset changed, Modern and Reset settings put it
    // back to its default, and Undo preset names it and puts it back.
    void LeaveExp() { Key("esc"); }
    bool OpenAgain(const char *page) {
        OpenToo(page);
        return Expect(!strcmp(Menu_PageName(), page), Fmt("more_options: open %s lands on %s", page,
                                                          Repr(Menu_PageName()).c_str()));
    }
    void MoreOptionsRow() {
        std::string name = MoreOptions_CursorName(), v0 = MoreOptions_CursorValue();
        Key("right");
        std::string v1 = MoreOptions_CursorValue();
        Key("left");
        std::string v2 = MoreOptions_CursorValue();
        if (v1 != v0) {
            if (!Expect(v2 == v0, Fmt("more_options: Right then Left leaves %s at %s, not %s", name.c_str(),
                                      Repr(v2.c_str()).c_str(), Repr(v0.c_str()).c_str())))
                return;
            Key("right");
        } else {
            if (!Expect(v2 != v0, Fmt("more_options: neither Left nor Right changes %s", name.c_str())))
                return;
            Key("right");
            if (!Expect(MoreOptions_CursorValue() == v0, Fmt("more_options: Left then Right leaves %s at %s, not %s", name.c_str(),
                                                     Repr(MoreOptions_CursorValue()).c_str(), Repr(v0.c_str()).c_str())))
                return;
            Key("left");
        }
        // Moved off the default now (a fresh page has every value at it): a tuning value marks
        // the preset changed, a player's setting (mouse, pad, filters, From 1997) does not.
        // Del puts it back.
        const bool marks = MoreOptions_CursorPresetsSet();
        const bool changed = std::string(Menu_PresetLabel()).find(", changed") != std::string::npos;
        Expect(changed == marks, marks ? Fmt("more_options: %s changed leaves the preset row at %s", name.c_str(),
                                             Repr(Menu_PresetLabel()).c_str())
                                       : Fmt("more_options: %s, a player's setting, changed makes the preset row %s",
                                             name.c_str(), Repr(Menu_PresetLabel()).c_str()));
        Key("del");
        if (!Expect(MoreOptions_CursorValue() == v0, Fmt("more_options: Del leaves %s at %s, not its default %s", name.c_str(),
                                                 Repr(MoreOptions_CursorValue()).c_str(), Repr(v0.c_str()).c_str())))
            return;
        // Enter steps a switch or a choice, as Right does; a number it leaves alone.
        const bool number = v0.find_first_not_of("-0123456789") == std::string::npos;
        Key("enter");
        const std::string ve = MoreOptions_CursorValue();
        if (number)
            Expect(ve == v0, Fmt("more_options: Enter changes %s, a number, from %s to %s", name.c_str(),
                                 Repr(v0.c_str()).c_str(), Repr(ve.c_str()).c_str()));
        else
            Expect(ve != v0, Fmt("more_options: Enter leaves %s at %s", name.c_str(), Repr(v0.c_str()).c_str()));
        Key("del");
    }

    void MoreOptions() {
        if (!Wanted(kExpPage))
            return;
        const char *row = "More options";
        Fresh("Advanced and reset");
        if (!Expect(RowDrawn(row), "more_options: Advanced and reset has no More options row"))
            return;
        if (!MoveTo(row))
            return;
        int d0, d1;
        std::string here = Where(&d0);
        Key("enter");
        Where(&d1);
        if (!Expect(MoreOptions_Active() && d1 == d0 + 1, "more_options: Enter on the row does not open the page"))
            return;
        Check(kExpPage);
        Key("esc");
        if (!Expect(Where() == here, Fmt("more_options: Esc from the page went to %s, not back to %s",
                                         Repr(Where().c_str()).c_str(), Repr(here.c_str()).c_str())))
            return;

        // Every row.
        Fresh(kExpPage);
        std::string first = MoreOptions_CursorName();
        for (size_t g = 0; g < sizeof kExpGroups / sizeof kExpGroups[0]; g++) {
            for (int r = 0; r < kExpGroups[g]; r++) {
                MoreOptionsRow();
                // The table's voice says the help of the row's own group (2026-11-11: it
                // stayed on the first group's all the way down).
                Expect(Voice() == Menu_Tr(MoreOptions_CursorGroupHelp()),
                       Fmt("more_options: on %s the voice says %s", MoreOptions_CursorName(), Repr(Voice().c_str()).c_str()));
                Key("down");
            }
        }
        if (!Expect(MoreOptions_CursorName() == first, Fmt("more_options: Down does not come back round to %s (on %s)",
                                                   first.c_str(), MoreOptions_CursorName())))
            return;

        // The ends, a window at a time.
        Key("pgdn");
        std::string one = MoreOptions_CursorName();
        Expect(one != first && Where().find(MoreOptions_Name(1)) == std::string::npos,
               Fmt("more_options: Page Down moves to %s, not a window on", one.c_str()));
        Key("pgdn", 10);
        Expect(!strcmp(MoreOptions_CursorName(), MoreOptions_Name(MoreOptions_Count() - 1)),
               Fmt("more_options: Page Down stops at %s, not the last row", MoreOptions_CursorName()));
        Key("pgup", 10);
        Expect(MoreOptions_CursorName() == first, Fmt("more_options: Page Up stops at %s, not the first row", MoreOptions_CursorName()));

        // Opened again after leaving it at the end, the page starts at its top, its first
        // group's head showing.
        Key("pgdn", 10);
        Key("esc");
        Key("enter");
        MoreOptionsTable table;
        if (Expect(MoreOptions_Table(&table), "more_options: Enter on the row again does not open the page"))
            Expect(table.scroll == 0 && MoreOptions_CursorName() == first,
                   Fmt("more_options: opened again, the table starts at line %d on %s", table.scroll,
                       MoreOptions_CursorName()));

        // Presets: changed, Modern, Undo preset.
        Fresh(kExpPage);
        std::string v0 = MoreOptions_CursorValue(), name = MoreOptions_CursorName();
        Key("right");
        std::string v1 = MoreOptions_CursorValue();
        LeaveExp();
        if (!OpenAgain("Preset") || !MoveTo("Modern"))
            return;
        Key("enter");
        Expect(std::string(Menu_PresetLabel()).find(", changed") == std::string::npos,
               Fmt("more_options: Modern leaves the preset row at %s", Repr(Menu_PresetLabel()).c_str()));
        if (!MoveTo("Undo preset"))
            return;
        Expect(Voice().find(name) != std::string::npos || Voice().find(" more.") != std::string::npos,
               Fmt("more_options: Undo preset does not name %s: %s", name.c_str(), Repr(Voice().c_str()).c_str()));
        if (!OpenAgain(kExpPage))
            return;
        if (!Expect(MoreOptions_CursorValue() == v0, Fmt("more_options: Modern leaves %s at %s, not %s", name.c_str(),
                                                 Repr(MoreOptions_CursorValue()).c_str(), Repr(v0.c_str()).c_str())))
            return;
        LeaveExp();
        if (!OpenAgain("Preset") || !MoveTo("Undo preset"))
            return;
        Key("enter");
        if (!OpenAgain(kExpPage))
            return;
        if (!Expect(MoreOptions_CursorValue() == v1, Fmt("more_options: Undo preset leaves %s at %s, not %s", name.c_str(),
                                                 Repr(MoreOptions_CursorValue()).c_str(), Repr(v1.c_str()).c_str())))
            return;

        // Reset settings, and its undo.
        LeaveExp();
        if (!MoveTo("Reset settings"))
            return;
        Key("enter");
        Key("down");
        Key("enter");
        Expect(!strcmp(Menu_CursorLabel(), "Reset settings"),
               Fmt("more_options: after a reset the cursor is on %s, not Reset settings", Repr(Menu_CursorLabel()).c_str()));
        Expect(std::string(Menu_PresetLabel()).find(", changed") == std::string::npos,
               Fmt("more_options: Reset settings leaves the preset row at %s", Repr(Menu_PresetLabel()).c_str()));
        Key("down");
        if (!Expect(!strcmp(Menu_CursorLabel(), "Undo reset"),
                    Fmt("more_options: after a reset the row after Reset settings is %s", Repr(Menu_CursorLabel()).c_str())))
            return;
        Expect(Voice().find(name) != std::string::npos || Voice().find(" more.") != std::string::npos,
               Fmt("more_options: Undo reset after a reset does not name %s: %s", name.c_str(),
                   Repr(Voice().c_str()).c_str()));
        Key("enter");
        if (!OpenAgain(kExpPage))
            return;
        Expect(MoreOptions_CursorValue() == v1, Fmt("more_options: Undo reset after a reset leaves %s at %s, not %s",
                                             name.c_str(), Repr(MoreOptions_CursorValue()).c_str(), Repr(v1.c_str()).c_str()));

        // A player's setting (the last row, From 1997's Movies): a preset leaves it.
        Fresh(kExpPage);
        Key("pgdn", 10);
        std::string mineName = MoreOptions_CursorName();
        if (!Expect(!MoreOptions_CursorPresetsSet(), Fmt("more_options: %s, the last row, is a tuning value", mineName.c_str())))
            return;
        Key("right");
        std::string mine = MoreOptions_CursorValue();
        LeaveExp();
        if (!OpenAgain("Preset") || !MoveTo("Classic"))
            return;
        Key("enter");
        Key("down");
        Key("enter");
        if (!OpenAgain(kExpPage))
            return;
        Key("pgdn", 10);
        Expect(MoreOptions_CursorValue() == mine, Fmt("more_options: Classic puts %s, a player's setting, from %s to %s",
                                              mineName.c_str(), Repr(mine.c_str()).c_str(), Repr(MoreOptions_CursorValue()).c_str()));
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

        const char *page = "Display and graphics";
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
        char start[32], said[32];
        Bindings_Text("Game menu", 2, start, sizeof start);
        Bindings_StartSaid(said, sizeof said);
        if (pad && !strcmp(kGroupFirst[group], "Game menu")) {
            // Start always opens the game menu: Enter does not wait and Del leaves it, and the
            // voice says why.
            Key("enter");
            std::string why = Fmt(Menu_Tr("%s always opens the game menu, so it can't be changed."), said);
            Expect(!Waiting() && Voice() == why,
                   Fmt("bind: Game menu's %s waits or reads %s", Repr(start).c_str(), Repr(Voice().c_str()).c_str()));
            Check();
            Key("del");
            Expect(ChipText() == start, Fmt("bind: Del changed Game menu's %s to %s", Repr(start).c_str(), Repr(ChipText().c_str()).c_str()));
            return;
        }
        Key("enter");
        if (!Expect(Waiting(), "bind: Enter does not wait for a key"))
            return;
        std::string ask = pad ? Fmt(Menu_Tr("Press a button, or %s to keep it."), said) : Menu_Tr("Press a key, or <ESC> to keep it.");
        Expect(Voice() == ask, Fmt("bind: the voice does not ask for the key and say the way out: %s", Repr(Voice().c_str()).c_str()));
        Check();
        Key("esc");
        if (!Expect(!Waiting(), "bind: Esc leaves the wait for a key going"))
            return;
        if (pad) {  // and Start, the pad's way out
            Key("enter");
            Press("pad:start");
            if (!Expect(!Waiting() && !Asking() && ChipText() == before,
                        Fmt("bind: %s on the pad's wait left %s (was %s)%s", start, Repr(ChipText().c_str()).c_str(),
                            Repr(before.c_str()).c_str(), Waiting() ? ", still waiting" : "")))
                return;
        }
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
        Bindings_Text(partner, pad ? 2 : 0, want, sizeof want);
        // A key another action has asks first, as Options' questions do: the voice names
        // the action that has it, the answers take the actions' place, No first, and Esc
        // keeps both as they were.
        std::string kept = ChipText();
        Key("enter");
        Press(pad ? "pad:a" : "J");
        if (!Expect(Asking() && EndsWith(BindingsWhere(), "| No"),
                    Fmt("bind: %s, another action's, did not ask on No first (%s)", Repr(want).c_str(),
                        Repr(BindingsWhere().c_str()).c_str())))
            return;
        Check();
        std::string q = Voice();
        std::string on = Fmt(Menu_Tr("%s is on %s%s."), Said(want).c_str(), Ui_Tr(partner), "");
        Expect(q.find(on.substr(0, on.size() - 1)) == 0,
               Fmt("bind: the question does not say %s has %s: %s", partner, Repr(want).c_str(), Repr(q.c_str()).c_str()));
        Expect(RowDrawn((std::string("No, keep it on ") + partner).c_str()) && !RowStarting("Yes, ").empty(),
               "bind: the question's answers are not drawn");
        Key("esc");
        char still[32];
        Bindings_Text(partner, pad ? 2 : 0, still, sizeof still);
        if (!Expect(!Asking() && InActions() && ChipText() == kept && !strcmp(still, want) && !Clashes(partner),
                    Fmt("bind: Esc on the question changed something (chip %s, %s has %s)", Repr(ChipText().c_str()).c_str(),
                        partner, Repr(still).c_str())))
            return;
        Key("del");
        Expect(ChipText().empty(), Fmt("bind: Del leaves %s", Repr(ChipText().c_str()).c_str()));
    }

    void Binding() {
        if (!Wanted("Keys and buttons"))
            return;
        const int groups = (int)(sizeof kGroupFirst / sizeof kGroupFirst[0]);
        for (int d = 0; d < 2; d++) {
            int cols = d ? 1 : 2;
            for (int g = 0; g < groups; g++)
                for (int c = 0; c < cols; c++)
                    BindOne(d, g, c, d == 1 || c == 2);
        }

        // Forward's Key 2 put in Key 1 leaves Key 2 empty, and no clash with itself.
        ToChip(0, 0, 0);
        char key2[32];
        Bindings_Text("Forward", 1, key2, sizeof key2);
        std::string k2 = key2;
        Key("enter");
        Press("Keypad 8");
        Bindings_Text("Forward", 1, key2, sizeof key2);
        if (Expect(k2 == "Num 8" && ChipText() == "Num 8" && !key2[0],
                   Fmt("bind: Num 8 moved to Forward's Key 1 leaves Key 1 %s, Key 2 %s", Repr(ChipText().c_str()).c_str(),
                       Repr(key2).c_str())))
            Expect(!ChipRed(), "bind: a key moved from Key 2 to Key 1 shows a clash");

        Swap();

        // The wait gives up after 5 s and changes nothing.
        ToChip(0, 0, 0);
        std::string before = ChipText();
        Key("enter");
        Wait(5000 / 16 + 5);
        Expect(!Waiting() && ChipText() == before,
               Fmt("bind: 5 s on, the wait %s (chip %s)", Waiting() ? "goes on" : "ended", Repr(ChipText().c_str()).c_str()));
    }

    // keys reset: Del on the groups page asks, as Reset settings does; Esc and No keep the
    // keys; Yes puts back the device in view, and only that one (EPmager, 2026-11-11).
    void ResetKeys() {
        if (!Wanted("Keys and buttons"))
            return;
        char key1[32], pad[32], def1[32], defPad[32];
        Fresh("Keys and buttons");
        Bindings_Text("Forward", 0, def1, sizeof def1);
        Bindings_Text("Forward", 2, defPad, sizeof defPad);
        ToChip(0, 0, 0);
        Key("enter");
        Press("Keypad 1");  // Forward's Key 1: Num 1
        Key("tab");
        Key("del");  // Forward's buttons cleared
        Key("tab");
        Key("esc");  // the groups, the keyboard in view
        auto forward = [&]() {
            Bindings_Text("Forward", 0, key1, sizeof key1);
            Bindings_Text("Forward", 2, pad, sizeof pad);
        };
        forward();
        if (!Expect(std::string(key1) == "Num 1" && !pad[0],
                    Fmt("reset keys: setting up gave Forward %s and %s", Repr(key1).c_str(), Repr(pad).c_str())))
            return;
        Key("del");
        if (!Expect(Asking(), Fmt("reset keys: Del on the groups does not ask (%s)", BindingsWhere().c_str())))
            return;
        Check();
        Key("esc");
        forward();
        Expect(!Asking() && std::string(key1) == "Num 1",
               Fmt("reset keys: Esc %s and left Forward's Key 1 %s", Asking() ? "kept asking" : "answered", Repr(key1).c_str()));
        Key("del");
        Key("enter");  // No, the first answer
        forward();
        Expect(!Asking() && std::string(key1) == "Num 1", Fmt("reset keys: No left Forward's Key 1 %s", Repr(key1).c_str()));
        Key("del");
        Key("down");
        Key("enter");
        forward();
        Expect(std::string(key1) == def1 && !pad[0],
               Fmt("reset keys: Yes on the keyboard left Forward %s and %s, not %s and the buttons cleared", Repr(key1).c_str(),
                   Repr(pad).c_str(), Repr(def1).c_str()));
        Key("tab");
        Key("del");
        Key("down");
        Key("enter");
        forward();
        Expect(std::string(pad) == defPad,
               Fmt("reset keys: Yes on the gamepad left Forward's buttons %s, not %s", Repr(pad).c_str(), Repr(defPad).c_str()));
        Check();
    }

    // keys: the Keys and buttons pages go where their keys say, as the Options pages do. Each
    // group says its own help; a group opens on its first chip, whatever chip the group before
    // was left on; Tab switches the device on a group's page too; wide, Right opens a group as
    // Enter does and Left from the first chip goes back to it (the one column: neither).
    void KeysPages() {
        if (!Wanted("Keys and buttons"))
            return;
        const int groups = (int)(sizeof kGroupFirst / sizeof kGroupFirst[0]);
        const bool wide = !strcmp(Menu_Layout(), "wide");
        Fresh("Keys and buttons");
        std::set<std::string> said;
        for (int g = 0; g < groups; g++) {
            std::string v = Voice();
            Expect(!v.empty() && !said.count(v), Fmt("keys: on %s the voice says %s", Repr(BindingsWhere().c_str()).c_str(),
                                                     Repr(v.c_str()).c_str()));
            said.insert(v);
            Key("down");
        }

        Fresh("Keys and buttons");
        Key("enter");
        Key("right");  // Key 2
        Key("esc");
        Key("down");
        Key("enter");
        Expect(BindingsWhere().find("| Key 1 |") != std::string::npos,
               Fmt("keys: a group opened after Key 2 of another opens on %s", Repr(BindingsWhere().c_str()).c_str()));
        Key("tab");
        Expect(BindingsWhere().find("Gamepad") == 0,
               Fmt("keys: Tab on a group's page leaves %s", Repr(BindingsWhere().c_str()).c_str()));

        Fresh("Keys and buttons");
        Key("down");
        const std::string group = BindingsWhere();
        Key("right");
        if (!Expect(InActions() == wide, Fmt("keys: %s Right on %s %s", wide ? "wide," : "in the one column,",
                                             Repr(group.c_str()).c_str(), InActions() ? "opens it" : "does not open it")))
            return;
        if (!wide)
            Key("enter");
        Key("left");
        if (wide)
            Expect(BindingsWhere() == group, Fmt("keys: wide, Left from the first chip goes to %s, not %s",
                                                 Repr(BindingsWhere().c_str()).c_str(), Repr(group.c_str()).c_str()));
        else
            Expect(InActions(), Fmt("keys: in the one column, Left from the first chip leaves the group (now %s)",
                                    Repr(BindingsWhere().c_str()).c_str()));
    }

    // The swap question: No (first) changes nothing; Yes gives Forward J and Jetpack Forward's
    // key, with no key on two actions; swapping back puts both back. Onto an empty chip the
    // Yes answer says what Jetpack is left with.
    void Swap() {
        ToChip(0, 0, 0);
        std::string mine = ChipText();
        char theirs[32];
        Bindings_Text("Jetpack", 0, theirs, sizeof theirs);
        std::string j = theirs;
        Key("enter");
        Press("J");
        if (!Expect(Asking(), "swap: J, Jetpack's, did not ask"))
            return;
        std::string q = Voice();
        std::string where = Fmt(Menu_Tr(", in %s"), Ui_Tr("Magic and items"));
        Expect(q == Fmt(Menu_Tr("%s is on %s%s."), "<J>", Ui_Tr("Jetpack"), where.c_str()),
               Fmt("swap: the question reads %s", Repr(q.c_str()).c_str()));
        std::string yes = "Yes, Jetpack gets " + mine;
        Expect(RowDrawn(yes.c_str()), Fmt("swap: no %s answer (Yes reads %s)", Repr(yes.c_str()).c_str(),
                                          Repr(RowStarting("Yes, ").c_str()).c_str()));
        Key("enter");  // on No
        Bindings_Text("Jetpack", 0, theirs, sizeof theirs);
        if (!Expect(!Asking() && ChipText() == mine && theirs == j,
                    Fmt("swap: No left Forward %s and Jetpack %s", Repr(ChipText().c_str()).c_str(), Repr(theirs).c_str())))
            return;
        Key("enter");
        Press("J");
        Key("down");
        Key("enter");
        Bindings_Text("Jetpack", 0, theirs, sizeof theirs);
        if (!Expect(!Asking() && ChipText() == j && theirs == mine && !ChipRed() && !Clashes("Jetpack"),
                    Fmt("swap: after Yes Forward has %s and Jetpack %s (want %s and %s)", Repr(ChipText().c_str()).c_str(),
                        Repr(theirs).c_str(), Repr(j.c_str()).c_str(), Repr(mine.c_str()).c_str())))
            return;
        Key("enter");
        Press(mine == "Num 8" ? "Keypad 8" : "Up");
        Key("down");
        Key("enter");
        Bindings_Text("Jetpack", 0, theirs, sizeof theirs);
        Expect(ChipText() == mine && theirs == j,
               Fmt("swap: swapping back left Forward %s and Jetpack %s", Repr(ChipText().c_str()).c_str(), Repr(theirs).c_str()));

        ToChip(0, 0, 1);
        if (ChipText().empty()) {
            Key("enter");
            Press("J");
            std::string a = RowStarting("Yes, ");
            Expect(Asking() && (a.find("Yes, Jetpack keeps ") == 0 || a == "Yes, Jetpack has no key"),
                   Fmt("swap: onto an empty chip Yes reads %s", Repr(a.c_str()).c_str()));
            Key("esc");
        }
    }

    // A pad-only player (Xbox, Switch, Vita, Android TV) cannot lock themselves out: with
    // only pad buttons, every pad binding is offered Start and then cleared, and the pad
    // still has Start on Game menu and still leaves and reopens these options.
    void PadOnly() {
        if (!Wanted("Keys and buttons"))
            return;
        Fresh("Keys and buttons");
        char start[32];
        Bindings_Text("Game menu", 2, start, sizeof start);
        Pad("back");
        const int groups = (int)(sizeof kGroupFirst / sizeof kGroupFirst[0]);
        for (int g = 0; g < groups; g++) {
            Pad("dpad-down", g ? 1 : 0);
            Pad("a");
            for (int r = 0; r < kGroupSizes[g]; r++) {
                std::string where = BindingsWhere(), before = ChipText();
                bool fixed = where.find("| Game menu |") != std::string::npos;
                if (!Expect(where.find("Gamepad") != std::string::npos,
                            Fmt("padonly: Back did not switch to the Gamepad view (%s)", Repr(where.c_str()).c_str())))
                    return;
                Pad("a");
                if (Waiting()) {
                    Pad("start");
                    if (!Expect(!Waiting() && !Asking() && ChipText() == before,
                                Fmt("padonly: %s on %s left %s%s", start, Repr(where.c_str()).c_str(),
                                    Repr(ChipText().c_str()).c_str(), Asking() ? ", asking" : "")))
                        return;
                } else if (!Expect(fixed, Fmt("padonly: A does not wait on %s", Repr(where.c_str()).c_str()))) {
                    return;
                }
                Pad("x");
                if (!Expect(fixed ? ChipText() == start : ChipText().empty(),
                            Fmt("padonly: X left %s on %s", Repr(ChipText().c_str()).c_str(), Repr(where.c_str()).c_str())))
                    return;
                Pad("dpad-down");
            }
            Pad("b");
        }
        char now[32];
        Bindings_Text("Game menu", 2, now, sizeof now);
        Expect(!strcmp(now, start), Fmt("padonly: every pad binding cleared leaves Game menu %s, not %s", Repr(now).c_str(), Repr(start).c_str()));
        Pad("b");
        if (!Expect(!Bindings_Active(), "padonly: B does not leave Keys and buttons with every pad binding cleared"))
            return;
        Pad("a");
        Expect(Bindings_Active(), "padonly: A does not reopen Keys and buttons with every pad binding cleared");
    }

    void Behavior() {
        Reach();
        for (size_t p = 0; p < sizeof kPages / sizeof kPages[0]; p++)
            Settings(kPages[p]);
        Wake();
        Presets();
        UndoPreset();
        MoreOptions();
        Console();
        Binding();
        KeysPages();
        ResetKeys();
        PadOnly();
        Devices();
        Languages();
        PadNames();
    }

    // The pad's family from the names browsers and SDL give (2026-11-12): the vendor a
    // browser writes in wins over the words, so a Switch pad named "Wireless Gamepad" on
    // Windows gets Nintendo's names.
    void PadNames() {
        if (opt->only)
            return;
        static const struct {
            const char *name;
            S32 family;
        } kNames[] = {
            {"Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 0b13)", 1},
            {"Wireless Gamepad (STANDARD GAMEPAD Vendor: 057e Product: 2009)", 3},
            {"Controller (STANDARD GAMEPAD Vendor: 057e Product: 2009)", 3},
            {"57e-2009-Wireless Gamepad", 3},
            {"54c-ce6-Wireless Controller", 2},
            {"DualSense Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 0ce6)", 2},
            {"Joy-Con L+R", 3},
            {"PS4 Controller", 2},
            {"Xbox 360 Controller (XInput STANDARD GAMEPAD)", 1},
            {"2dc8-6001-8BitDo SN30 Pro", 1},
            {"Wireless Gamepad", 1},
        };
        for (const auto &n : kNames) {
            const S32 got = Host_PadFamilyOfName(n.name);
            Expect(got == n.family, Fmt("padnames: %s is family %d, not %d", Repr(n.name).c_str(), (int)got, (int)n.family));
        }
    }

    // Every voice text in lang/help.tsv fits its window in every Text language and both
    // fonts (2026-11-12): two lines of the band (620 px) and three of the 640x480 dialogue
    // window (591 px); first launch's own window, three lines. Formats are filled with long
    // names; the fragments they are built from (" and ", ", in %s") are not checked alone.
    void Languages() {
        if (opt->only)
            return;
        static const char *const kFill[] = {"Right Shift", "Behavior menu", ", in Magic and items"};
        for (int font = 0; font < 2; font++) {
            Menu_Set("font", font);
            for (int i = 0; i < Lang_Count(); i++) {
                const char *en = Lang_Text(i, 0);
                if (en[0] == ' ' || en[0] == ',')
                    continue;
                const bool first = strstr(en, "later in the Options Menu") != nullptr;
                for (int lang = 0; lang < LANG_COUNT; lang++) {
                    std::string t;
                    int k = 0;
                    for (const char *p = Lang_Text(i, lang); *p; p++) {
                        if (p[0] == '%' && p[1] == 's') {
                            t += kFill[k < 2 ? k : 2], k++, p++;
                        } else if (p[0] == '%' && p[1] == 'd') {
                            t += "12", p++;
                        } else {
                            t += *p;
                        }
                    }
                    S32 band = Voice_Lines(t.c_str(), 620), dlg = Voice_Lines(t.c_str(), 591);
                    Expect(!((!first && band > 2) || dlg > 3), Fmt("language %d, font %d: %s needs %d band lines, %d dialogue lines", lang, font,
                                            Repr(t.c_str()).c_str(), (int)band, (int)dlg));
                }
            }
        }
        Menu_Set("font", 0);
    }

    // The player's device (2026-11-11, after playing on the Xbox): after a pad press the
    // legend names the pad's buttons, after a key the keyboard's; with no keyboard the
    // Keys and buttons page shows the pad and Tab offers no Keyboard view.
    void Devices() {
        if (Wanted("Advanced and reset")) {
            Fresh("Advanced and reset");
            Pad("dpad-down");
            Pad("dpad-up");
            std::string l = Menu_Legend();
            Expect(l.empty() || (l.find("[Enter]") == std::string::npos && l.find("[Esc]") == std::string::npos),
                   Fmt("device: after a pad press the legend reads %s", Repr(l.c_str()).c_str()));
            Press("Down");
            l = Menu_Legend();
            Expect(l.empty() || (l.find("[Esc]") != std::string::npos && l.find("[B]") == std::string::npos),
                   Fmt("device: after a key the legend reads %s", Repr(l.c_str()).c_str()));
        }
        if (Wanted("Keys and buttons")) {
            Menu_SetSwitch("keyboard", 0);
            Fresh("Keys and buttons");
            std::string where = BindingsWhere(), l = Menu_Legend();
            Expect(where.find("Gamepad") == 0, Fmt("device: with no keyboard Keys and buttons opens on %s", Repr(where.c_str()).c_str()));
            Expect(l.find("Tab") == std::string::npos && l.find("Keyboard") == std::string::npos,
                   Fmt("device: with no keyboard the legend reads %s", Repr(l.c_str()).c_str()));
            Key("tab");
            where = BindingsWhere();
            Expect(where.find("Gamepad") == 0, Fmt("device: with no keyboard Tab switched to %s", Repr(where.c_str()).c_str()));
            Menu_SetSwitch("keyboard", 1);
        }
        if (Wanted("Advanced and reset")) {  // no keyboard, no press yet: the pad's names already
            Menu_SetSwitch("keyboard", 0);
            Fresh("Advanced and reset");
            std::string l = Menu_Legend();
            Expect(l.empty() || (l.find("[Enter]") == std::string::npos && l.find("[Esc]") == std::string::npos),
                   Fmt("device: with no keyboard and no press the legend reads %s", Repr(l.c_str()).c_str()));
            Menu_SetSwitch("keyboard", 1);
        }
    }
};

int Selftest_Run(const SelftestOptions &opt, const char *self) {
    std::vector<const char *> specs(opt.combos.begin(), opt.combos.end());
    if (specs.empty())
        specs.assign(kCombos, kCombos + sizeof kCombos / sizeof kCombos[0]);

    Host_SetPixels(false);
    Walker w;
    w.opt = &opt;
    w.size = Fmt("%ux%u", (unsigned)Host_PhysW(), (unsigned)Host_PhysH());
    w.physW = Host_PhysW();
    w.physH = Host_PhysH();
    w.checks = 0;
    Ui_ClearMissed();
    // Every walk in every Text language (2026-11-12): the whole menu is drawn in it, so
    // every label, value and title must fit, and every one drawn must have a translation.
    for (size_t k = 0; k < LANG_COUNT * specs.size(); k++) {
        const int lang = (int)(k / specs.size());
        const size_t i = k % specs.size();
        w.lang = lang;
        std::vector<Switch> combo;
        if (!ParseCombo(specs[i], &combo)) {
            fprintf(stderr, "bad --combo %s (name=value,name=value with the switch names of `state`)\n", specs[i]);
            return 2;
        }
        w.switches = combo;
        w.again = Fmt("%s --size %s", self, w.size.c_str());
        for (size_t s = 0; s < combo.size(); s++)
            w.again += Fmt(" --switch %s=%d", combo[s].name.c_str(), combo[s].value);
        w.again += " --switch whole=1";
        if (lang)
            w.again += Fmt(" --do \"set text_lang %d\"", lang);
        w.combo = ComboLabel(combo) + (lang ? Fmt(" lang=%d", lang) : "");
        w.Start();
        w.Controls();
        w.Pages();
        w.MoreOptionsPage();
        if (!opt.layoutOnly)
            w.Behavior();
    }
    for (std::set<std::string>::const_iterator it = Ui_Missed().begin(); it != Ui_Missed().end(); ++it)
        w.problems[Fmt("lang/menu.tsv has no row for %s", Repr(it->c_str()).c_str())] = "tools/lang.py after adding it";
    for (std::set<std::string>::const_iterator it = Menu_VoiceMissed().begin(); it != Menu_VoiceMissed().end(); ++it)
        w.problems[Fmt("lang/help.tsv has no row for %s", Repr(it->c_str()).c_str())] = "tools/lang.py after adding it";
    for (std::map<std::string, std::string>::const_iterator it = w.problems.begin(); it != w.problems.end(); ++it) {
        printf("%s\n", it->first.c_str());
        if (opt.again)
            printf("    again: %s\n", it->second.c_str());
    }
    printf("%d problem(s) in %d checks\n", (int)w.problems.size(), w.checks);
    return w.problems.empty() ? 0 : 1;
}
