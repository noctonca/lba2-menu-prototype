// LBA2 Options menu prototype on lba2cc's LIB386.
//
//   lba2menu [--data DIR] [--size WxH] [--first] [--headless] [--fixed-dt MS]
//            [--script FILE] [--do "cmd; cmd"] [--stdin] [--control FILE] [--trace FILE]
//            [--switch name=v ...]
//   lba2menu --selftest [--size WxH] [--combo name=v,name=v ...] [--only PAGE] [--no-again] [--layout-only]
//
// --do: commands as a script would hold them, separated by ';', run after --script.
// Without --headless the window stays up afterwards, on the screen they lead to.
//
// --selftest: walk every chip of the Controls page and every row of every page at
// this size, for each switch combination (tools/sweep.py's eight, or the --combo
// ones), check that everything fits, then walk the menu by its keys and check
// what they do (Esc goes back, values come back, presets, binding a key). Print
// each problem with the command line that shows it, and exit 1 if there were any.
// --only keeps one page; --layout-only skips the walks by keys.
//
// --control FILE: lines appended to FILE run as commands while the window is up,
// so a driver can steer a session someone is watching. Replies go to stdout.
//
// Commands (from --script, and from stdin with --stdin), one per line:
//   key <name> [count]   press a key: up down left right enter esc tab del f1..f12
//   press <name>         a raw key for rebinding: an SDL key name ("J", "Left Alt",
//                        "Keypad 5") or pad:<a|b|x|y|lb|rb|lt|rt|start|back|ls|rs|dpad-up|lstick-left...>
//   wait <frames>        let frames pass
//   state                print the state as one JSON line: "state {...}"
//   shot <path> [scale]  write the screen as a PNG (logical size x scale)
//   switch <name> <v>    band typing chip word island help640 gameplay overlay devices padnames heads
//   set <setting> <v>    a fake setting by id (see `state`)
//   open <page name>     jump to a page ("Display and graphics", "Keys and buttons", "original", ...)
//   quit
#include "engine.h"
#include "menu.h"
#include "selftest.h"
#include "shot.h"
#include "trace.h"
#include "ui.h"

#include <SYSTEM/WINDOW.H>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

static const char *kDefaultData = "~/code/lba-hacking/assets/lba2-activision-us-cd";

static const U32 kSizes[][2] = {
    {640, 480}, {1280, 720}, {1280, 800}, {1920, 1080}, {2400, 1080}, {2560, 1440}, {3440, 1440}, {3840, 2160},
};

struct Runner {
    std::vector<std::string> lines;
    size_t next;
    U32 waitFrames;
    bool quit;
};

static bool RunCommand(Runner &r, const char *line) {
    char cmd[32] = "", a[512] = "", b[64] = "";
    int n = sscanf(line, "%31s %511s %63s", cmd, a, b);
    if (n <= 0 || cmd[0] == '#')
        return true;
    Trace("cmd %s", line);
    if (!strcmp(cmd, "key")) {
        UiKey k;
        if (!Engine_KeyByName(a, &k)) {
            printf("err unknown key %s\n", a);
            return false;
        }
        int count = n >= 3 ? atoi(b) : 1;
        // One press a frame, so each one is an edge.
        Engine_Inject(k);
        for (int i = 1; i < count; i++) {
            char again[64];
            snprintf(again, sizeof again, "key %s", a);
            r.lines.insert(r.lines.begin() + (long)r.next, std::string("wait 1"));
            r.lines.insert(r.lines.begin() + (long)r.next + 1, std::string(again));
            r.next += 0;
        }
        r.waitFrames = 1;
    } else if (!strcmp(cmd, "press")) {
        const char *name = line + 5;
        while (*name == ' ')
            name++;
        if (!Engine_InjectRaw(name)) {
            printf("err unknown raw key %s\n", name);
            return false;
        }
        r.waitFrames = 1;
    } else if (!strcmp(cmd, "wait")) {
        r.waitFrames = (U32)atoi(a);
    } else if (!strcmp(cmd, "measure")) {
        printf("measure ");
        Menu_WriteMeasure(stdout);
    } else if (!strcmp(cmd, "widths")) {
        // widths FILE [VOICE_W]: each line "tag<TAB>text" (CP850, as TEXT.HQR holds it);
        // prints the text's width in the game font and the voice lines it needs.
        FILE *in = fopen(a, "rb");
        if (!in) {
            printf("err cannot open %s\n", a);
            return false;
        }
        S32 voiceW = n >= 3 ? atoi(b) : 620;
        char buf[2048];
        while (fgets(buf, sizeof buf, in)) {
            buf[strcspn(buf, "\r\n")] = 0;
            char *tab = strchr(buf, '\t');
            if (!tab)
                continue;
            *tab = 0;
            printf("width %s\t%d\t%d\n", buf, (int)Ui_TextWidth(tab + 1), (int)Voice_Lines(tab + 1, voiceW));
        }
        fclose(in);
    } else if (!strcmp(cmd, "state")) {
        printf("state ");
        Menu_WriteState(stdout);
    } else if (!strcmp(cmd, "shot")) {
        int scale = n >= 3 ? atoi(b) : 1;
        bool ok = Shot_Write(a, Engine_Palette(), scale);
        printf("%s %s\n", ok ? "shot" : "err shot", a);
    } else if (!strcmp(cmd, "switch")) {
        if (!Menu_SetSwitch(a, atoi(b)))
            printf("err unknown switch %s\n", a);
    } else if (!strcmp(cmd, "set")) {
        if (!Menu_Set(a, atoi(b)))
            printf("err bad setting %s %s\n", a, b);
    } else if (!strcmp(cmd, "open")) {
        // Page names may have spaces: take the rest of the line.
        const char *name = line + 4;
        while (*name == ' ')
            name++;
        if (!Menu_Open(name))
            printf("err unknown page %s\n", name);
    } else if (!strcmp(cmd, "quit")) {
        r.quit = true;
    } else {
        printf("err unknown command %s\n", cmd);
        return false;
    }
    fflush(stdout);
    return true;
}

static std::string ExpandHome(const char *p) {
    if (p[0] == '~') {
        const char *home = getenv("HOME");
        return std::string(home ? home : "") + (p + 1);
    }
    return p;
}

// F2: start again at the next size, keeping the other arguments.
// Start again at another screen size or scale rule, with the switches as they are now.
static void Relaunch(int argc, char **argv, U32 w, U32 h, int scaleRule) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; i++) {
        if ((!strcmp(argv[i], "--size") || !strcmp(argv[i], "--switch")) && i + 1 < argc) {
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--first"))
            continue;
        args.push_back(argv[i]);
    }
    char size[32];
    snprintf(size, sizeof size, "%ux%u", (unsigned)w, (unsigned)h);
    args.push_back("--size");
    args.push_back(size);
    std::vector<std::string> sw;
    Menu_SwitchArgs(sw);
    for (size_t i = 0; i < sw.size(); i++) {
        if (!sw[i].compare(0, 6, "scale="))
            continue;
        args.push_back("--switch");
        args.push_back(sw[i]);
    }
    args.push_back("--switch");
    args.push_back("scale=" + std::to_string(scaleRule));
    std::vector<char *> cargs;
    for (size_t i = 0; i < args.size(); i++)
        cargs.push_back((char *)args[i].c_str());
    cargs.push_back(NULL);
    Trace("relaunch size=%s", size);
    Engine_Shutdown();
    execv(argv[0], cargs.data());
    perror("execv");
}

int main(int argc, char **argv) {
    std::string data = ExpandHome(getenv("LBA2_GAME_DIR") ? getenv("LBA2_GAME_DIR") : kDefaultData);
    U32 w = 640, h = 480, fixedDt = 0;
    bool first = false, headless = false, live = false, selftest = false;
    const char *script = NULL, *trace = NULL, *control = NULL;
    std::vector<std::string> switches, inline_cmds;
    SelftestOptions test;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : "";
        if (!strcmp(a, "--data")) {
            data = ExpandHome(v);
            i++;
        } else if (!strcmp(a, "--size")) {
            unsigned sw, sh;
            if (sscanf(v, "%ux%u", &sw, &sh) == 2) {
                w = sw;
                h = sh;
            }
            i++;
        } else if (!strcmp(a, "--first")) {
            first = true;
        } else if (!strcmp(a, "--headless")) {
            headless = true;
        } else if (!strcmp(a, "--fixed-dt")) {
            fixedDt = (U32)atoi(v);
            i++;
        } else if (!strcmp(a, "--script")) {
            script = v;
            i++;
        } else if (!strcmp(a, "--control")) {
            control = v;
            i++;
        } else if (!strcmp(a, "--stdin")) {
            live = true;
        } else if (!strcmp(a, "--trace")) {
            trace = v;
            i++;
        } else if (!strcmp(a, "--switch")) {
            if (!strncmp(v, "scale=", 6))
                Engine_SetScaleRule(atoi(v + 6));  // the canvas depends on it: before Engine_Init
            else
                switches.push_back(v);
            i++;
        } else if (!strcmp(a, "--do")) {
            // Split on ';' into script lines.
            std::string all = v;
            size_t at = 0;
            while (at <= all.size()) {
                size_t semi = all.find(';', at);
                std::string cmd = all.substr(at, semi == std::string::npos ? std::string::npos : semi - at);
                size_t b = cmd.find_first_not_of(' '), e = cmd.find_last_not_of(' ');
                if (b != std::string::npos)
                    inline_cmds.push_back(cmd.substr(b, e - b + 1));
                if (semi == std::string::npos)
                    break;
                at = semi + 1;
            }
            i++;
        } else if (!strcmp(a, "--selftest")) {
            selftest = headless = true;
        } else if (!strcmp(a, "--combo")) {
            test.combos.push_back(v);
            i++;
        } else if (!strcmp(a, "--only")) {
            test.only = v;
            i++;
        } else if (!strcmp(a, "--no-again")) {
            test.again = false;
        } else if (!strcmp(a, "--layout-only")) {
            test.layoutOnly = true;
        } else {
            fprintf(stderr, "unknown argument %s\n", a);
            return 2;
        }
    }

    if (selftest && (!switches.empty() || script || !inline_cmds.empty() || first)) {
        fprintf(stderr, "--selftest takes --combo and --only, not --switch, --script, --do or --first\n");
        return 2;
    }
    if (headless) {
        setenv("SDL_VIDEO_DRIVER", "dummy", 1);
        if (!fixedDt)
            fixedDt = 16;
    }
    Trace_Open(trace);
    Trace_Quiet(selftest);

    EngineConfig cfg;
    cfg.dataDir = data.c_str();
    cfg.physW = w;
    cfg.physH = h;
    if (!Engine_Init(cfg))
        return 1;
    // lba2cc turns vsync on with the renderer, and on the dummy driver SDL fakes it by
    // sleeping a refresh per present: 90% of a headless run. The fixed clock keeps runs
    // repeatable without it (lba2cc does the same for --exit runs, CONTROL.CPP).
    if (headless) {
        Window_SetVSync(0);
        Engine_SetHeadless(true);
    }
    if (fixedDt)
        Engine_SetFixedDt(fixedDt);
    Menu_Init(first);
    if (selftest) {
        int result = Selftest_Run(test, argv[0]);
        Engine_Shutdown();
        return result;
    }
    for (size_t i = 0; i < switches.size(); i++) {
        char name[32];
        int value;
        if (sscanf(switches[i].c_str(), "%31[^=]=%d", name, &value) == 2)
            Menu_SetSwitch(name, value);
    }

    Runner run;
    run.next = 0;
    run.waitFrames = 0;
    run.quit = false;
    if (script) {
        FILE *f = fopen(script, "r");
        if (!f) {
            fprintf(stderr, "cannot read %s\n", script);
            return 2;
        }
        char line[1024];
        while (fgets(line, sizeof line, f)) {
            line[strcspn(line, "\r\n")] = 0;
            run.lines.push_back(line);
        }
        fclose(f);
    }
    run.lines.insert(run.lines.end(), inline_cmds.begin(), inline_cmds.end());
    int liveFd = -1;
    if (live) {
        liveFd = 0;
        fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
    }
    if (control) {
        // A plain file the driver appends lines to; read from where we left off.
        liveFd = open(control, O_RDONLY | O_CREAT | O_TRUNC, 0644);
        if (liveFd < 0) {
            perror(control);
            return 2;
        }
        live = true;
        Trace("control file %s", control);
    }
    std::string pending;

    while (!run.quit) {
        if (!Engine_BeginFrame())
            break;
        Trace_SetFrame(Engine_Frame());

        // Commands: the script in order (honouring waits), then whatever arrived on stdin.
        if (run.waitFrames > 0) {
            run.waitFrames--;
        } else {
            while (run.next < run.lines.size() && run.waitFrames == 0 && !run.quit) {
                std::string line = run.lines[run.next++];
                RunCommand(run, line.c_str());
            }
        }
        if (live) {
            char buf[1024];
            ssize_t got;
            while ((got = read(liveFd, buf, sizeof buf)) > 0)
                pending.append(buf, (size_t)got);
            size_t nl;
            while ((nl = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, nl);
                pending.erase(0, nl + 1);
                run.lines.push_back(line);
            }
            if (got == 0 && !control && pending.empty() && run.next >= run.lines.size() && headless)
                run.quit = true;  // stdin closed
        }

        // The prototype's own keys.
        if (Engine_Pressed(KEY_F1))
            Menu_SetSwitch("overlay", !Menu_GetSwitch("overlay"));
        if (Engine_Pressed(KEY_F3))
            Menu_SetSwitch("band", !Menu_GetSwitch("band"));
        if (Engine_Pressed(KEY_F4))
            Menu_SetSwitch("typing", Menu_GetSwitch("typing") + 1);
        if (Engine_Pressed(KEY_F5))
            Menu_SetSwitch("chip", Menu_GetSwitch("chip") == 2 ? 1 : 2);
        if (Engine_Pressed(KEY_F6))
            Menu_SetSwitch("word", Menu_GetSwitch("word") + 1);
        if (Engine_Pressed(KEY_F7))
            Menu_SetSwitch("island", !Menu_GetSwitch("island"));
        if (Engine_Pressed(KEY_F8))
            Menu_SetSwitch("help640", !Menu_GetSwitch("help640"));
        if (Engine_Pressed(KEY_F9))
            Menu_SetSwitch("gameplay", !Menu_GetSwitch("gameplay"));
        if (Engine_Pressed(KEY_F10))
            Menu_SetSwitch("devices", !Menu_GetSwitch("devices"));
        if (Engine_Pressed(KEY_F11))
            Menu_SetSwitch("padnames", Menu_GetSwitch("padnames") + 1);
        if (Engine_Pressed(KEY_F12))
            Menu_SetSwitch("heads", !Menu_GetSwitch("heads"));
        if (Engine_Pressed(KEY_F2)) {
            size_t count = sizeof kSizes / sizeof kSizes[0], at = 0;
            for (size_t i = 0; i < count; i++)
                if (kSizes[i][0] == w && kSizes[i][1] == h)
                    at = i;
            const U32 *nx = kSizes[(at + 1) % count];
            Relaunch(argc, argv, nx[0], nx[1], Engine_ScaleRule());
        }
        // The composition switches (design pass, 2026-11-03).
        if (Engine_Pressed(KEY_1))
            Menu_SetSwitch("sheet", !Menu_GetSwitch("sheet"));
        if (Engine_Pressed(KEY_2))
            Menu_SetSwitch("legend", !Menu_GetSwitch("legend"));
        if (Engine_Pressed(KEY_3))
            Menu_SetSwitch("bg", !Menu_GetSwitch("bg"));
        if (Engine_Pressed(KEY_4))
            Relaunch(argc, argv, w, h, !Engine_ScaleRule());
        if (Engine_Pressed(KEY_5))
            Menu_SetSwitch("panes", !Menu_GetSwitch("panes"));
        if (Engine_Pressed(KEY_6))
            Menu_SetSwitch("bandshade", Menu_GetSwitch("bandshade") ? 0 : 15);
        if (Engine_Pressed(KEY_7))
            Menu_SetSwitch("clash", !Menu_GetSwitch("clash"));
        if (Engine_Pressed(KEY_8))
            Menu_SetSwitch("short", !Menu_GetSwitch("short"));

        Menu_Update();
        Menu_Draw();
        Engine_Present();

        // A script with no stdin ends a headless run when it runs out.
        if (headless && !live && run.next >= run.lines.size() && run.waitFrames == 0)
            break;
    }
    Trace("exit");
    Engine_Shutdown();
    return 0;
}
