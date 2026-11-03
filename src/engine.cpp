#include "engine.h"

#include <PLATFORM/PLATFORM.H>
#include <SVGA/CLIP.H>
#include <SVGA/DIRTYBOX.H>
#include <SVGA/FONT.H>
#include <SVGA/INITMODE.H>
#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/DISCIMG.H>
#include <SYSTEM/EVENTS.H>
#include <SYSTEM/HQRLOAD.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SYSTEM/LE.H>
#include <SYSTEM/LOADMALL.H>
#include <SYSTEM/RANDOM.H>
#include <SYSTEM/TIMER.H>
#include <SYSTEM/WINDOW.H>
#include <POLYGON/POLY.H>

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cadre.h"

// Resource numbers, from lba2cc SOURCES/COMMON.H.
#define RESS_PAL 0
#define RESS_FONT_GPM 1
#define RESS_XPL0 27   // Citadel
#define RESS_XPL00 42  // citabau: what ChoicePalette loads at boot
#define PCR_MENU 4

// XPL header, from lba2cc SOURCES/COMMON.H.
struct XplHeader {
    S32 Version;
    S32 OffsetPalette, OffsetShade, OffsetFog, OffsetTransp;
    S32 ShadeStartPercent, ShadeNormalLevel, ShadeEndPercent;
    S32 FogCoul, TranspStartPercent, TranspEndPercent;
};

static U32 s_physW, s_physH, s_logW, s_logH;
static char s_ress[1024], s_screen[1024];
static U8 *s_palMenu;
static U8 *s_xplBoot;        // the XPL the boot menu shades through (citabau)
static U8 *s_xplIsland;      // Citadel's, for Options opened in play
static U8 *s_menuImage;      // 640x480, as authored
static bool s_islandPal;
static const U8 *s_palCurrent;
static bool s_quit;

static bool s_down[KEY_COUNT];
static bool s_pressed[KEY_COUNT];
static U32 s_repeatAt[KEY_COUNT];

static const U32 kScancode[KEY_COUNT] = {
    K_UP, K_DOWN, K_LEFT, K_RIGHT, K_ENTER, K_ESC, K_TAB,
    K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9,
};

U32 Engine_Scale(U32 w, U32 h) {
    U32 sx = w / 640, sy = h / 480;
    U32 s = sx < sy ? sx : sy;
    return s < 1 ? 1 : s;
}

U32 Engine_LogicalW() { return s_logW; }
U32 Engine_LogicalH() { return s_logH; }
U32 Engine_PhysW() { return s_physW; }
U32 Engine_PhysH() { return s_physH; }

static int EventFilter(const PlatformEvent *event) {
    if (event->type == PLATFORM_EVENT_QUIT)
        s_quit = true;
    return 0;
}

static void JoinPath(char *out, size_t n, const char *dir, const char *name) {
    size_t len = strlen(dir);
    snprintf(out, n, "%s%s%s", dir, (len && dir[len - 1] == '/') ? "" : "/", name);
}

// The window opens at the logical size; grow it by a whole step so the
// picture is the size it would be on the pretend screen, as far as the
// desktop allows.
static void SizeWindow() {
    int count = 0;
    SDL_Window **windows = SDL_GetWindows(&count);
    if (!windows || count < 1)
        return;
    SDL_Window *win = windows[0];
    SDL_free(windows);

    U32 scale = Engine_Scale(s_physW, s_physH);
    SDL_DisplayID display = SDL_GetDisplayForWindow(win);
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(display, &usable)) {
        while (scale > 1 && ((int)(s_logW * scale) > usable.w || (int)(s_logH * scale) > usable.h - 40))
            scale--;
    }
    SDL_SetWindowSize(win, (int)(s_logW * scale), (int)(s_logH * scale));
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    char title[128];
    snprintf(title, sizeof title, "LBA2 menu prototype - %ux%u (logical %ux%u)",
             (unsigned)s_physW, (unsigned)s_physH, (unsigned)s_logW, (unsigned)s_logH);
    SDL_SetWindowTitle(win, title);
}

bool Engine_Init(const EngineConfig &cfg) {
    s_physW = cfg.physW;
    s_physH = cfg.physH;
    U32 scale = Engine_Scale(s_physW, s_physH);
    s_logW = s_physW / scale;
    s_logH = s_physH / scale;

    if (!InitEvents()) {
        fprintf(stderr, "events failed\n");
        return false;
    }
    SetEventFilter(EventFilter);
    if (!InitWindow("LBA2 menu prototype") || !InitVideo() || !InitScreen()) {
        fprintf(stderr, "window/video failed\n");
        return false;
    }
    if (!InitGraphics(s_logW, s_logH, false)) {
        fprintf(stderr, "InitGraphics %ux%u failed\n", (unsigned)s_logW, (unsigned)s_logH);
        return false;
    }
    SizeWindow();
    InitKeyboard();
    InitTimer();
    SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    BoxInit(512);

    // Data: plain files, or a disc image in the folder (the US CD is an ISO).
    char dir[1024];
    snprintf(dir, sizeof dir, "%s%s", cfg.dataDir,
             cfg.dataDir[strlen(cfg.dataDir) - 1] == '/' ? "" : "/");
    DiscImage_Mount(dir, "lba2.hqr");
    JoinPath(s_ress, sizeof s_ress, dir, "ress.hqr");
    JoinPath(s_screen, sizeof s_screen, dir, "screen.hqr");

    s_palMenu = (U8 *)LoadMalloc_HQR(s_ress, RESS_PAL);
    U8 *font = (U8 *)LoadMalloc_HQR(s_ress, RESS_FONT_GPM);
    s_xplBoot = (U8 *)LoadMalloc_HQR(s_ress, RESS_XPL00);
    s_xplIsland = (U8 *)LoadMalloc_HQR(s_ress, RESS_XPL0);
    s_menuImage = (U8 *)malloc(640 * 480 + 4096);
    if (!s_palMenu || !font || !s_xplBoot || !s_xplIsland || !Load_HQR(s_screen, s_menuImage, PCR_MENU)) {
        fprintf(stderr, "could not read the game data in %s\n", dir);
        return false;
    }
    FromLE32InPlace(s_xplBoot, sizeof(XplHeader) / 4);
    FromLE32InPlace(s_xplIsland, sizeof(XplHeader) / 4);
    // ShadeBoxBlk darkens through PtrCLUTGouraud. Checked against lba2cc's
    // `ui menu-main-options` capture: identical outside the plasma with citabau.
    PtrCLUTGouraud = s_xplBoot + ((const XplHeader *)s_xplBoot)->OffsetFog;

    SetFont(font, 2, 8);  // as PERSO.CPP sets it
    Rnd_Seed(1997);       // the plasma's start; fixed so captures repeat
    Cadre_InitPlasma();
    Engine_UseMenuPalette();
    return true;
}

void Engine_Shutdown() {
    DiscImage_Unmount();
}

void Engine_UseMenuPalette() {
    s_islandPal = false;
    s_palCurrent = s_palMenu;
    PtrCLUTGouraud = s_xplBoot + ((const XplHeader *)s_xplBoot)->OffsetFog;
    SetVideoPaletteSync(s_palMenu);
}

void Engine_UseIslandPalette(S32) {
    s_islandPal = true;
    const XplHeader *xh = (const XplHeader *)s_xplIsland;
    s_palCurrent = s_xplIsland + xh->OffsetPalette;
    PtrCLUTGouraud = s_xplIsland + xh->OffsetFog;
    SetVideoPaletteSync(s_palCurrent);
}

bool Engine_IslandPaletteActive() { return s_islandPal; }

U32 Engine_TimeMs() {
    ManageTime();
    return TimerSystemHR;
}

static bool s_injected[KEY_COUNT];
static U32 s_fixedDt;
static U32 s_frame;

static const char *kKeyNames[KEY_COUNT] = {
    "up", "down", "left", "right", "enter", "esc", "tab",
    "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9",
};

void Engine_Inject(UiKey k) { s_injected[k] = true; }

bool Engine_KeyByName(const char *name, UiKey *out) {
    for (int k = 0; k < KEY_COUNT; k++) {
        if (strcmp(kKeyNames[k], name) == 0) {
            *out = (UiKey)k;
            return true;
        }
    }
    return false;
}

const char *Engine_KeyName(UiKey k) { return kKeyNames[k]; }

void Engine_SetFixedDt(U32 dtMs) {
    s_fixedDt = dtMs;
    if (dtMs)
        Timer_EnableFixedDt(dtMs);
}

U32 Engine_Frame() { return s_frame; }

const U8 *Engine_Palette() { return s_palCurrent; }

// A pad, read straight from SDL (lba2cc's joystick code is in the game layer).
static SDL_Gamepad *s_pad;

static bool PadDown(int k) {
    if (!s_pad) {
        int n = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&n);
        if (ids && n > 0) {
            s_pad = SDL_OpenGamepad(ids[0]);
            if (s_pad)
                fprintf(stderr, "pad: %s\n", SDL_GetGamepadName(s_pad));
        }
        SDL_free(ids);
        if (!s_pad)
            return false;
    }
    switch (k) {
    case KEY_UP: return SDL_GetGamepadButton(s_pad, SDL_GAMEPAD_BUTTON_DPAD_UP) || SDL_GetGamepadAxis(s_pad, SDL_GAMEPAD_AXIS_LEFTY) < -16000;
    case KEY_DOWN: return SDL_GetGamepadButton(s_pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN) || SDL_GetGamepadAxis(s_pad, SDL_GAMEPAD_AXIS_LEFTY) > 16000;
    case KEY_LEFT: return SDL_GetGamepadButton(s_pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || SDL_GetGamepadAxis(s_pad, SDL_GAMEPAD_AXIS_LEFTX) < -16000;
    case KEY_RIGHT: return SDL_GetGamepadButton(s_pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || SDL_GetGamepadAxis(s_pad, SDL_GAMEPAD_AXIS_LEFTX) > 16000;
    case KEY_ENTER: return SDL_GetGamepadButton(s_pad, SDL_GAMEPAD_BUTTON_SOUTH);
    case KEY_ESC: return SDL_GetGamepadButton(s_pad, SDL_GAMEPAD_BUTTON_EAST);
    case KEY_TAB: return SDL_GetGamepadButton(s_pad, SDL_GAMEPAD_BUTTON_BACK);
    default: return false;
    }
}

bool Engine_BeginFrame() {
    ManageEvents();
    ManageKeyboard();
    if (s_fixedDt)
        Timer_FixedDtAdvance();
    s_frame++;
    U32 now = Engine_TimeMs();
    for (int k = 0; k < KEY_COUNT; k++) {
        bool down = CheckKey(kScancode[k]) != 0 || PadDown(k);
        s_pressed[k] = s_injected[k];
        s_injected[k] = false;
        if (down && !s_down[k]) {
            s_pressed[k] = true;
            s_repeatAt[k] = now + 350;
        } else if (down && k <= KEY_RIGHT && now >= s_repeatAt[k]) {
            s_pressed[k] = true;
            s_repeatAt[k] = now + 90;
        }
        s_down[k] = down;
    }
    return !s_quit;
}

bool Engine_Pressed(UiKey k) { return s_pressed[k]; }

void Engine_Present() {
    BoxStaticFullflip();
}

void Engine_DrawBackground() {
    // Nearest-neighbour stretch of the 640x480 image, as Image_LoadStretched does.
    U8 *dst = (U8 *)Log;
    for (U32 y = 0; y < s_logH; y++) {
        const U8 *row = s_menuImage + (y * 480 / s_logH) * 640;
        U8 *out = dst + TabOffLine[y];
        for (U32 x = 0; x < s_logW; x++)
            out[x] = row[x * 640 / s_logW];
    }
}
