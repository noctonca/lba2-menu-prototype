// LBA2 Options menu prototype on lba2cc's LIB386.
//
//   lba2menu [--data DIR] [--size WxH] [--first] [--headless] [--fixed-dt MS]
//            [--script FILE] [--stdin] [--control FILE] [--trace FILE] [--switch name=v ...]
//
// --control FILE: lines appended to FILE run as commands while the window is up,
// so a driver can steer a session someone is watching. Replies go to stdout.
//
// Commands (from --script, and from stdin with --stdin), one per line:
//   key <name> [count]   press a key: up down left right enter esc tab f1..f9
//   wait <frames>        let frames pass
//   state                print the state as one JSON line: "state {...}"
//   shot <path> [scale]  write the screen as a PNG (logical size x scale)
//   switch <name> <v>    band typing chip word island help640 gameplay overlay
//   set <setting> <v>    a fake setting by id (see `state`)
//   open <page name>     jump to a page ("Display and graphics", "original", ...)
//   quit
#include "engine.h"
#include "menu.h"
#include "shot.h"
#include "trace.h"

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
    {640, 480}, {1280, 720}, {1280, 800}, {1920, 1080}, {2400, 1080}, {2560, 1440},
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
    } else if (!strcmp(cmd, "wait")) {
        r.waitFrames = (U32)atoi(a);
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
static void Relaunch(int argc, char **argv, U32 w, U32 h) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i + 1 < argc) {
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
    bool first = false, headless = false, live = false;
    const char *script = NULL, *trace = NULL, *control = NULL;
    std::vector<std::string> switches;

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
            switches.push_back(v);
            i++;
        } else {
            fprintf(stderr, "unknown argument %s\n", a);
            return 2;
        }
    }

    if (headless) {
        setenv("SDL_VIDEO_DRIVER", "dummy", 1);
        if (!fixedDt)
            fixedDt = 16;
    }
    Trace_Open(trace);

    EngineConfig cfg;
    cfg.dataDir = data.c_str();
    cfg.physW = w;
    cfg.physH = h;
    if (!Engine_Init(cfg))
        return 1;
    if (fixedDt)
        Engine_SetFixedDt(fixedDt);
    Menu_Init(first);
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
        if (Engine_Pressed(KEY_F2)) {
            size_t count = sizeof kSizes / sizeof kSizes[0], at = 0;
            for (size_t i = 0; i < count; i++)
                if (kSizes[i][0] == w && kSizes[i][1] == h)
                    at = i;
            const U32 *nx = kSizes[(at + 1) % count];
            Relaunch(argc, argv, nx[0], nx[1]);
        }

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
