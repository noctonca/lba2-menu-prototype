// The self test: walks what tools/sweep.py walks (every chip of the Controls
// page, then every row of every page) in one process, without the frames a
// script spends waiting, and checks the same rules. Each problem comes with a
// command line that opens the screen it was found on.
#include "selftest.h"

#include "controls.h"
#include "engine.h"
#include "menu.h"
#include "menu_parts.h"
#include "trace.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

// The switch combinations tools/sweep.py runs, in its order.
static const char *kCombos[] = {
    "band=1,gameplay=0", "band=1,gameplay=1", "band=0,gameplay=1", "help640=1,gameplay=1",
    "devices=0",         "padnames=2",        "padnames=3",        "chip=1",
};

static const char *kPages[] = {
    "Options", "Preset", "Language and interface", "Display and graphics", "Sound",
    "Controls and camera", "Advanced and reset", "Gameplay",
};

// Actions per group on the Controls page (controls.cpp kActions).
static const int kGroupSizes[] = {5, 6, 4, 7, 4, 4, 6};

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
    if (Controls_Active()) {
        ChipView chips[32];
        int nc = Controls_Chips(chips, 32);
        for (int i = 0; i < nc; i++)
            if (!chips[i].fits)
                found->push_back(Fmt("key name cut in its chip: %s %s %s", Repr(chips[i].action).c_str(), chips[i].column,
                                     Repr(chips[i].name).c_str()));
    }
}

// ───────────────────────────── The walk ─────────────────────────────

// One step is one frame, as a script line followed by `wait 1` would be.
static void Frame() {
    Engine_BeginFrame();
    Trace_SetFrame(Engine_Frame());
    Menu_Update();
    Menu_Draw();
    Engine_Present();
}

struct Walker {
    const SelftestOptions *opt;
    std::string size, combo, again;  // again: the command line up to --do
    std::vector<std::string> path;   // commands since the last open
    std::map<std::string, std::string> problems;  // line -> the command line that shows it
    int checks;

    void Open(const char *page) {
        path.assign(1, std::string("open ") + page);
        Menu_Open(page);
        Frame();
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
    void Check(const char *expectPage = NULL) {
        checks++;
        std::vector<std::string> found;
        const char *page = Menu_PageName();
        // Wide pages name the right pane, so the hub has no page of its own there.
        if (expectPage && strcmp(expectPage, "Options") != 0 && strcmp(page, expectPage) != 0)
            found.push_back(Fmt("walk did not reach %s (on %s)", Repr(expectPage).c_str(), Repr(page).c_str()));
        ::Check(&found);
        for (size_t i = 0; i < found.size(); i++) {
            std::string line = size + " " + combo + " [" + page + "] " + found[i];
            if (!problems.count(line))
                problems[line] = again + " --do \"" + Commands() + "\"";
        }
    }
    bool Wanted(const char *page) const { return !opt->only || strcmp(opt->only, page) == 0; }

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
};

int Selftest_Run(const SelftestOptions &opt, const char *self) {
    std::vector<const char *> specs(opt.combos.begin(), opt.combos.end());
    if (specs.empty())
        specs.assign(kCombos, kCombos + sizeof kCombos / sizeof kCombos[0]);

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
        Menu_Reset();
        w.again = Fmt("%s --size %s", self, w.size.c_str());
        for (size_t s = 0; s < combo.size(); s++) {
            Menu_SetSwitch(combo[s].name.c_str(), combo[s].value);
            w.again += Fmt(" --switch %s=%d", combo[s].name.c_str(), combo[s].value);
        }
        // Help shown whole, so a check never depends on how far the typing got.
        Menu_SetSwitch("typing", 2);
        w.again += " --switch typing=2";
        w.combo = ComboLabel(combo);
        Frame();
        if (ComboValue(combo, "devices", 1))
            w.Controls(2, true);
        else
            w.Controls(3, false);
        w.Pages(ComboValue(combo, "gameplay", 0) != 0);
    }
    for (std::map<std::string, std::string>::const_iterator it = w.problems.begin(); it != w.problems.end(); ++it) {
        printf("%s\n", it->first.c_str());
        if (opt.again)
            printf("    again: %s\n", it->second.c_str());
    }
    printf("%d problem(s) in %d checks\n", (int)w.problems.size(), w.checks);
    return w.problems.empty() ? 0 : 1;
}
