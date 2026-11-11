#include "engine.h"
#include "vfont.h"

#include <PLATFORM/PLATFORM.H>
#include <SVGA/CLIP.H>
#include <SVGA/DIRTYBOX.H>
#include <SVGA/FONT.H>
#include <SVGA/GRAPH.H>
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

#ifdef LBA2_PROTO_SDL
#include <SDL3/SDL.h>  // the desktop window's size and title only
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cadre.h"
#include "screenbg.h"
#include "shot.h"
#include "twinsun.h"

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
static U32 s_surfW, s_surfH;  // the presented frame: the canvas, or the screen when bgres is on
static char s_ress[1024], s_screen[1024];
static U8 *s_palMenu;
static U8 *s_xplBoot;        // the XPL the boot menu shades through (citabau)
static U8 *s_xplIsland;      // Citadel's, for Options opened in play
static U8 *s_menuImage;      // 640x480, as authored
static U8 *s_arrow;          // sprites.hqr 6: the dialogue's bottom-right arrow
static bool s_islandPal;
static const U8 *s_palCurrent;
static bool s_quit;

static bool s_down[KEY_COUNT];
static bool s_pressed[KEY_COUNT];
static U32 s_repeatAt[KEY_COUNT];

static const U32 kScancode[KEY_COUNT] = {
    K_UP, K_DOWN, K_LEFT, K_RIGHT, K_ENTER, K_ESC, K_TAB, K_SUPPR,
    K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
    K_1, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9,
    K_0, K_PAGE_UP, K_PAGE_DOWN,
};

static int s_scaleRule = 1;
void Engine_SetScaleRule(int rule) { s_scaleRule = rule; }
int Engine_ScaleRule() { return s_scaleRule; }

// Rule 1: a screen 3:2 or wider takes the wide composition (960x540) at the
// largest whole step it fits; a squarer screen, or one too small for 960x540,
// takes the one column at the largest step that holds 640x480.
static bool WideByRule(U32 w, U32 h, U32 *scale) {
    if (s_scaleRule != 1 || w * 2 < h * 3)
        return false;
    U32 wx = w / 960, wy = h / 540;
    U32 ws = wx < wy ? wx : wy;
    if (ws < 1)
        return false;
    *scale = ws;
    return true;
}

bool Engine_RuleChoseWide() {
    U32 s;
    return WideByRule(s_physW, s_physH, &s);
}

U32 Engine_Scale(U32 w, U32 h) {
    U32 ws;
    if (WideByRule(w, h, &ws))
        return ws;
    U32 sx = w / 640, sy = h / 480;
    U32 s = sx < sy ? sx : sy;
    return s < 1 ? 1 : s;
}

U32 Engine_LogicalW() { return s_logW; }
U32 Engine_LogicalH() { return s_logH; }
U32 Engine_PhysW() { return s_physW; }
U32 Engine_PhysH() { return s_physH; }

static void PadRemoved(uint32_t id);

static int EventFilter(const PlatformEvent *event) {
    if (event->type == PLATFORM_EVENT_QUIT)
        s_quit = true;
    else if (event->type == PLATFORM_EVENT_GAMEPAD_REMOVED)
        PadRemoved(event->gdevice.which);
    return 0;
}

static void JoinPath(char *out, size_t n, const char *dir, const char *name) {
    size_t len = strlen(dir);
    snprintf(out, n, "%s%s%s", dir, (len && dir[len - 1] == '/') ? "" : "/", name);
}

static bool s_headless;
static bool s_fullscreen;  // the window fills the display; the screen size stays as chosen

#ifdef LBA2_PROTO_SDL
// The window opens at the logical size; grow it by a whole step so the
// picture is the size it would be on the pretend screen, as far as the
// desktop allows.
static SDL_Window *TheWindow() {
    int count = 0;
    SDL_Window **windows = SDL_GetWindows(&count);
    SDL_Window *win = windows && count > 0 ? windows[0] : NULL;
    SDL_free(windows);
    return win;
}

static int s_winPxW, s_winPxH;  // the window's pixels after the prototype last sized it

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// The page lays out the canvas (web/shell.html); its CSS box, in CSS pixels.
EM_JS(int, WebCanvasCssW, (), { var c = document.getElementById('canvas'); return c ? Math.round(c.getBoundingClientRect().width) : 0; });
EM_JS(int, WebCanvasCssH, (), { var c = document.getElementById('canvas'); return c ? Math.round(c.getBoundingClientRect().height) : 0; });
// CSS pixels to the display's, as SDL counts them at a high pixel density.
EM_JS(int, WebCanvasPx, (int css), { return Math.round(css * (window.devicePixelRatio || 1)); });

// Give SDL's window the canvas's CSS size, so its pixels are the page's at the
// display's pixel ratio. SDL only looks on the browser's resize events, and at
// a high pixel ratio it once sized the canvas itself; the page decides here.
static void FitCanvas(SDL_Window *win) {
    const int cw = WebCanvasCssW(), ch = WebCanvasCssH();
    int w = 0, h = 0;
    if (cw > 0 && ch > 0 && SDL_GetWindowSize(win, &w, &h) && (w != cw || h != ch))
        SDL_SetWindowSize(win, cw, ch);
}
#endif

static void NoteWindowPixels(SDL_Window *win) {
    SDL_GetWindowSizeInPixels(win, &s_winPxW, &s_winPxH);
}

static void TitleWindow(SDL_Window *win) {
    char title[128];
    snprintf(title, sizeof title, "LBA2 menu prototype - %ux%u (logical %ux%u)%s",
             (unsigned)s_physW, (unsigned)s_physH, (unsigned)s_logW, (unsigned)s_logH,
             s_fullscreen ? ", fullscreen" : "");
    SDL_SetWindowTitle(win, title);
}

static void SizeWindow() {
    SDL_Window *win = TheWindow();
    if (!win)
        return;
#ifdef __EMSCRIPTEN__
    // In a browser the page sizes the canvas; a screen of another size is
    // letterboxed into it, as in fullscreen.
    const bool pageSized = true;
#else
    const bool pageSized = false;
#endif
    if (s_fullscreen || pageSized) {  // the window is the display; it is sized on the way out
        NoteWindowPixels(win);
        TitleWindow(win);
        return;
    }
    U32 scale = Engine_Scale(s_physW, s_physH);
    SDL_DisplayID display = SDL_GetDisplayForWindow(win);
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(display, &usable)) {
        while (scale > 1 && ((int)(s_logW * scale) > usable.w || (int)(s_logH * scale) > usable.h - 40))
            scale--;
    }
    SDL_SetWindowSize(win, (int)(s_logW * scale), (int)(s_logH * scale));
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    // The resize is asynchronous on some desktops; wait for it, so it is not
    // taken for one made by hand.
    SDL_SyncWindow(win);
    NoteWindowPixels(win);
    TitleWindow(win);
}
#else
// A console's window is the display: nothing to size or name.
static void SizeWindow() {}
#endif

bool Engine_SetScreen(U32 w, U32 h, bool sizeWindow) {
    w &= ~7u;
    if (w < 640)
        w = 640;
    if (h < 480)
        h = 480;
    U32 scale = Engine_Scale(w, h);
    U32 lw = w / scale, lh = h / scale;
    // With the picture at the screen's resolution the presented frame is the
    // screen; the canvas stays logical.
    U32 sw = ScreenBg_Mode() ? w : lw, sh = ScreenBg_Mode() ? h : lh;
    if (lw != s_logW || lh != s_logH || sw != s_surfW || sh != s_surfH) {
        // As RES_SWITCH.CPP: Log, then the texture and Phys, then both clips.
        EndScreen();
        if (!CreateScreenMemory(lw, lh) || !ResizeWindowSurface(sw, sh, true) || !ResizeVideoSurface(sw, sh)) {
            fprintf(stderr, "could not switch to %ux%u (logical %ux%u)\n", (unsigned)w, (unsigned)h,
                    (unsigned)lw, (unsigned)lh);
            return false;
        }
        SetClipWindow(0, 0, (S32)lw - 1, (S32)lh - 1);
        SetClip(0, 0, (S32)lw - 1, (S32)lh - 1);
        BoxChangeClip(0, 0, (S32)lw - 1, (S32)lh - 1, TRUE);
        s_surfW = sw;
        s_surfH = sh;
    }
    s_physW = w;
    s_physH = h;
    s_logW = lw;
    s_logH = lh;
    if (s_headless)
        return true;
#ifdef LBA2_PROTO_SDL
    if (sizeWindow) {
        SizeWindow();
    } else if (SDL_Window *win = TheWindow()) {
        NoteWindowPixels(win);
        TitleWindow(win);
    }
#else
    (void)sizeWindow;
#endif
    return true;
}

// lba2cc's recommended modes (RES_CATALOG.CPP kRecommended) with the larger screens between.
static const U32 kSizes[][2] = {
    {640, 480}, {768, 480}, {1024, 480}, {1280, 720}, {1280, 800}, {1920, 1080}, {2400, 1080}, {2560, 1440}, {3440, 1440}, {3840, 2160},
};

int Engine_SizeCount() { return (int)(sizeof kSizes / sizeof kSizes[0]); }

void Engine_SizeAt(int i, U32 *w, U32 *h) {
    *w = kSizes[i][0];
    *h = kSizes[i][1];
}

bool Engine_StepScreen(int dir) {
    const int count = Engine_SizeCount();
    int below = 0;
    bool exact = false;
    for (int i = 0; i < count; i++) {
        if (kSizes[i][0] * kSizes[i][1] <= s_physW * s_physH)
            below = i;
        if (kSizes[i][0] == s_physW && kSizes[i][1] == s_physH) {
            below = i;
            exact = true;
            break;
        }
    }
    // Off the list, the one below counts as the previous.
    int next = dir > 0 ? below + 1 : (exact ? below - 1 : below);
    next = (next + count) % count;
    return Engine_SetScreen(kSizes[next][0], kSizes[next][1], true);
}

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
EM_JS(int, WebIsFullscreen, (), { return document.fullscreenElement ? 1 : 0; });
#endif

bool Engine_Fullscreen() {
#ifdef __EMSCRIPTEN__
    if (!s_headless)
        return WebIsFullscreen() != 0;  // the browser's; Esc leaves it without the menu
#endif
    return s_fullscreen;
}

bool Engine_SetFullscreen(bool on) {
    if (s_headless) {
        s_fullscreen = on;
        return true;
    }
#ifdef __EMSCRIPTEN__
    // The page's stage goes fullscreen, as the page's button does; the canvas
    // follows it (FitCanvas). A browser allows it only inside a key or click
    // handler, so the request waits for the next one: the release of Enter.
    const EMSCRIPTEN_RESULT r = on ? emscripten_request_fullscreen("#stage", 1) : emscripten_exit_fullscreen();
    return r == EMSCRIPTEN_RESULT_SUCCESS || r == EMSCRIPTEN_RESULT_DEFERRED;
#endif
#ifdef LBA2_PROTO_SDL
    SDL_Window *win = TheWindow();
    if (!win)
        return false;
    SetWindowFullscreen(on);  // lba2cc's own call: the window fills the display, letterboxed
    SDL_SyncWindow(win);      // macOS animates into its own space; wait for it to land
    s_fullscreen = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
    SizeWindow();  // back out: the window the screen size asks for
#else
    SetWindowFullscreen(on);
    s_fullscreen = g_platform.window.is_fullscreen();
#endif
    return s_fullscreen == on;
}

bool Engine_WindowResized(U32 *w, U32 *h) {
#ifndef LBA2_PROTO_SDL
    (void)w;
    (void)h;
    return false;  // a console's window never changes size
#else
    if (s_headless)
        return false;
    SDL_Window *win = TheWindow();
    if (!win)
        return false;
    // Fullscreen turned on or off outside the menu (the window's green button,
    // Ctrl+Cmd+F): follow it, and do not take the new size for a drag.
    const bool fs = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
    if (fs != s_fullscreen) {
        s_fullscreen = fs;
        SDL_SyncWindow(win);
        SizeWindow();
        return false;
    }
#ifdef __EMSCRIPTEN__
    FitCanvas(win);
#endif
    int pw = 0, ph = 0;
    if (!SDL_GetWindowSizeInPixels(win, &pw, &ph) || (pw == s_winPxW && ph == s_winPxH))
        return false;
    if (s_fullscreen) {  // the display's size, not a screen chosen by hand
        s_winPxW = pw;
        s_winPxH = ph;
        return false;
    }
    s_winPxW = pw;
    s_winPxH = ph;
    *w = (U32)pw;
    *h = (U32)ph;
    return true;
#endif
}

bool Engine_ShiftHeld() { return (g_platform.input.modifiers() & PLATFORM_KMOD_SHIFT) != 0; }

bool Engine_Init(const EngineConfig &cfg) {
    if (!InitEvents()) {
        fprintf(stderr, "events failed\n");
        return false;
    }
    SetEventFilter(EventFilter);
    if (!InitWindow("LBA2 menu prototype") || !InitVideo() || !InitScreen()) {
        fprintf(stderr, "window/video failed\n");
        return false;
    }
    s_physW = cfg.physW;
    s_physH = cfg.physH;
    if (!s_physW || !s_physH) {  // no size given: the display's (a console's TV, a browser's page)
        uint32_t dw = 0, dh = 0;
#ifdef __EMSCRIPTEN__
        // The window is made later (InitGraphics); the canvas is already laid out.
        dw = (uint32_t)WebCanvasPx(WebCanvasCssW());
        dh = (uint32_t)WebCanvasPx(WebCanvasCssH());
        if (dw < 640 || dh < 480)
            dw = dw < 640 ? 640 : dw, dh = dh < 480 ? 480 : dh;
#else
        if (!g_platform.window.display_size(&dw, &dh) || dw < 640 || dh < 480)
            dw = 640, dh = 480;
#endif
        s_physW = dw & ~7u;
        s_physH = dh;
        fprintf(stderr, "screen: the display's, %ux%u\n", (unsigned)s_physW, (unsigned)s_physH);
    }
    U32 scale = Engine_Scale(s_physW, s_physH);
    s_logW = s_physW / scale;
    s_logH = s_physH / scale;
    if (!InitGraphics(s_logW, s_logH, false)) {
        fprintf(stderr, "InitGraphics %ux%u failed\n", (unsigned)s_logW, (unsigned)s_logH);
        return false;
    }
    s_surfW = s_logW;
    s_surfH = s_logH;
    SizeWindow();
    InitKeyboard();
    InitTimer();
    if (!g_platform.input.init_gamepads())
        fprintf(stderr, "pad: none (gamepads did not start)\n");
    BoxInit(512);

    // Data: plain files, or a disc image in the folder (the US CD is an ISO).
    char dir[1024];
    snprintf(dir, sizeof dir, "%s%s", cfg.dataDir,
             cfg.dataDir[strlen(cfg.dataDir) - 1] == '/' ? "" : "/");
    DiscImage_Mount(dir, "lba2.hqr");
    JoinPath(s_ress, sizeof s_ress, dir, "ress.hqr");
    JoinPath(s_screen, sizeof s_screen, dir, "screen.hqr");
    char sprites[1024];
    JoinPath(sprites, sizeof sprites, dir, "sprites.hqr");
    s_arrow = (U8 *)LoadMalloc_HQR(sprites, 6);  // as PERSO.CPP's HQRPtrSprite, entry 6
    if (!s_arrow)
        fprintf(stderr, "no sprites.hqr in %s: paged help has no arrow\n", dir);

    s_palMenu = (U8 *)LoadMalloc_HQR(s_ress, RESS_PAL);
    U8 *font = (U8 *)LoadMalloc_HQR(s_ress, RESS_FONT_GPM);
    s_xplBoot = (U8 *)LoadMalloc_HQR(s_ress, RESS_XPL00);
    s_xplIsland = (U8 *)LoadMalloc_HQR(s_ress, RESS_XPL0);
    char holomap[1024];
    JoinPath(holomap, sizeof holomap, dir, "holomap.hqr");
    Twinsun_Init(holomap);  // the Background row's Twinsun; black without it
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
    VFont_Init();
    SetOverlayPrePresentCallback(VFont_Paint);  // after the smooth picture's hook
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
    "up", "down", "left", "right", "enter", "esc", "tab", "del",
    "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12",
    "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "0", "pgup", "pgdn",
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

// The menu's pad buttons are fixed, as the arrows, Enter and Esc are on the keyboard:
// no binding moves them, so a pad-only player can always use the menu. By pad input,
// in lba2cc's K_GAMEPAD order (PadRawDown): A Enter, B Esc, View Tab, X Del, LB and RB
// a page, the d-pad and the left stick the arrows.
struct PadMenuKey {
    UiKey key;
    int pad;
};
static const PadMenuKey kPadMenuKeys[] = {
    {KEY_ENTER, 0}, {KEY_ESC, 1},   {KEY_DEL, 2},   {KEY_TAB, 4},    {KEY_PGUP, 9},   {KEY_PGDN, 10},
    {KEY_UP, 11},   {KEY_DOWN, 12}, {KEY_LEFT, 13}, {KEY_RIGHT, 14}, {KEY_UP, 15},    {KEY_DOWN, 16},
    {KEY_LEFT, 17}, {KEY_RIGHT, 18},
};
static bool PadRawDown(int n);

// A pad, read through lba2cc's platform layer, so the same code runs on SDL and
// on the Xbox (lba2cc's joystick code is in the game layer, which this does not take).
static PlatformGamepad *s_pad;
static uint32_t s_padId;

// A pad that sleeps or is unplugged goes; the next frame opens whichever is there.
static void PadRemoved(uint32_t id) {
    if (s_pad && id == s_padId) {
        g_platform.input.close_gamepad(s_pad);
        s_pad = NULL;
        fprintf(stderr, "pad: gone\n");
    }
}

static bool PadDown(int k) {
    if (!s_pad) {
        int32_t n = 0;
        uint32_t *ids = g_platform.input.gamepads(&n);
        if (ids && n > 0) {
            s_pad = g_platform.input.open_gamepad(ids[0]);
            s_padId = ids[0];
            if (s_pad) {
                const char *name = g_platform.input.gamepad_name(s_pad);
                fprintf(stderr, "pad: %s\n", name ? name : "(no name)");
            }
        }
        if (ids)
            g_platform.host.free_memory(ids);
        if (!s_pad)
            return false;
    }
    for (const PadMenuKey &m : kPadMenuKeys)
        if (m.key == k && PadRawDown(m.pad))
            return true;
    return false;
}

// Raw state: every scancode and every pad input, for rebinding.
static bool s_rawDown[256 + PAD_CODE_COUNT];
static S32 s_rawPressed, s_rawInjected;
static bool s_lastPad;
#ifdef LBA2_PROTO_SDL
static bool s_keyboardSeen = true;
#else
static bool s_keyboardSeen;
#endif

static int Axis(PlatformGamepadAxis a) { return g_platform.input.gamepad_axis(s_pad, a); }

static bool PadRawDown(int n) {
    if (!s_pad)
        return false;
    const int dead = 16000;
    if (n < PLATFORM_GAMEPAD_BUTTON_COUNT)  // the platform's button order is lba2cc's K_GAMEPAD order
        return g_platform.input.gamepad_button(s_pad, (PlatformGamepadButton)n);
    switch (n) {
    case 15: return Axis(PLATFORM_GAMEPAD_AXIS_LEFTY) < -dead;
    case 16: return Axis(PLATFORM_GAMEPAD_AXIS_LEFTY) > dead;
    case 17: return Axis(PLATFORM_GAMEPAD_AXIS_LEFTX) < -dead;
    case 18: return Axis(PLATFORM_GAMEPAD_AXIS_LEFTX) > dead;
    case 19: return Axis(PLATFORM_GAMEPAD_AXIS_RIGHTY) < -dead;
    case 20: return Axis(PLATFORM_GAMEPAD_AXIS_RIGHTY) > dead;
    case 21: return Axis(PLATFORM_GAMEPAD_AXIS_RIGHTX) < -dead;
    case 22: return Axis(PLATFORM_GAMEPAD_AXIS_RIGHTX) > dead;
    case 23: return Axis(PLATFORM_GAMEPAD_AXIS_LEFT_TRIGGER) > dead;
    case 24: return Axis(PLATFORM_GAMEPAD_AXIS_RIGHT_TRIGGER) > dead;
    }
    return false;
}

static void ScanRaw() {
    s_rawPressed = s_rawInjected;
    s_rawInjected = 0;
    if (s_rawPressed)
        s_lastPad = s_rawPressed >= PAD_CODE_BASE;
    if (s_rawPressed && s_rawPressed < PAD_CODE_BASE)
        s_keyboardSeen = true;
    for (int i = 0; i < 256 + PAD_CODE_COUNT; i++) {
        bool down = i < 256 ? (i > 0 && CheckKey((U32)i) != 0) : PadRawDown(i - 256);
        if (down && !s_rawDown[i]) {
            S32 code = i < 256 ? i : PAD_CODE_BASE + (i - 256);
            if (!s_rawPressed)
                s_rawPressed = code;
            s_lastPad = i >= 256;
            if (i < 256)
                s_keyboardSeen = true;
        }
        s_rawDown[i] = down;
    }
}

S32 Engine_RawPressed() { return s_rawPressed; }

bool Engine_KeyboardSeen() { return s_keyboardSeen; }
void Engine_SetKeyboardSeen(bool seen) { s_keyboardSeen = seen; }

bool Engine_DrawMoreArrow(S32 x1, S32 y1) {
    if (!s_arrow)
        return false;
    AffGraph(0, x1 - 27, y1 - 27, s_arrow);  // INVENT.CPP DrawAngles, ARROW_ID
    return true;
}
bool Engine_LastInputWasPad() { return s_lastPad; }

static const char *kPadCodeNames[PAD_CODE_COUNT] = {
    "a", "b", "x", "y", "back", "guide", "start", "ls", "rs", "lb", "rb",
    "dpad-up", "dpad-down", "dpad-left", "dpad-right",
    "lstick-up", "lstick-down", "lstick-left", "lstick-right",
    "rstick-up", "rstick-down", "rstick-left", "rstick-right", "lt", "rt",
};

static bool SameNoCase(const char *a, const char *b) {
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
    return *a == *b;
}

static bool NameHas(const char *name, const char *part) {
    const size_t n = strlen(part);
    for (; *name; name++) {
        size_t i = 0;
        while (i < n && name[i] && tolower((unsigned char)name[i]) == tolower((unsigned char)part[i]))
            i++;
        if (i == n)
            return true;
    }
    return false;
}

bool Engine_InjectRaw(const char *name) {
    if (!strncmp(name, "pad:", 4)) {
        for (int n = 0; n < PAD_CODE_COUNT; n++) {
            if (!strcmp(name + 4, kPadCodeNames[n])) {
                s_rawInjected = PAD_CODE_BASE + n;
                return true;
            }
        }
        return false;
    }
    // A key by its name, as the platform names it ("J", "Left Alt", "Keypad 5"), any case.
    for (int sc = 1; sc < 256; sc++) {
        const char *known = g_platform.input.scancode_name(sc);
        if (known && *known && SameNoCase(known, name)) {
            s_rawInjected = sc;
            return true;
        }
    }
    return false;
}

bool Engine_InjectPad(const char *name) {
    for (int n = 0; n < PAD_CODE_COUNT; n++) {
        if (!strcmp(name, kPadCodeNames[n])) {
            s_rawInjected = PAD_CODE_BASE + n;
            for (const PadMenuKey &m : kPadMenuKeys)
                if (m.pad == n)
                    s_injected[m.key] = true;
            return true;
        }
    }
    return false;
}

const char *Engine_ScancodeName(S32 code) {
    const char *name = g_platform.input.scancode_name(code);
    return name ? name : "";
}

// The pad's family from its name: the platform layer has no pad type.
// 1 Xbox (and anything else), 2 PlayStation, 3 Nintendo.
S32 Engine_PadFamily() {
    if (!s_pad)
        return 0;
    const char *name = g_platform.input.gamepad_name(s_pad);
    if (!name)
        return 1;
    if (NameHas(name, "PS3") || NameHas(name, "PS4") || NameHas(name, "PS5") || NameHas(name, "DualShock") ||
        NameHas(name, "DualSense") || NameHas(name, "PlayStation"))
        return 2;
    if (NameHas(name, "Nintendo") || NameHas(name, "Switch") || NameHas(name, "Joy-Con") ||
        NameHas(name, "Pro Controller"))
        return 3;
    return 1;
}

bool Engine_BeginFrame() {
    ManageEvents();
    ManageKeyboard();
    ScanRaw();
    if (s_fixedDt)
        Timer_FixedDtAdvance();
    s_frame++;
    U32 now = Engine_TimeMs();
    for (int k = 0; k < KEY_COUNT; k++) {
        bool down = CheckKey(kScancode[k]) != 0 || PadDown(k);
        if (k == KEY_DEL)  // a Mac keyboard's delete key is Backspace
            down = down || CheckKey(K_BACKSPACE) != 0;
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

void Engine_SetHeadless(bool on) { s_headless = on; }

static bool s_pixels = true;
void Engine_SetPixels(bool on) { s_pixels = on; }
bool Engine_Pixels() { return s_pixels; }

static void PaintSmooth(U32 *argb, U32 pitch, U32 w, U32 h, const U8 *palette) {
    ScreenBg_PaintSmooth(argb, pitch, w, h, palette, s_menuImage);
}

void Engine_SetBackgroundRes(int mode) {
    ScreenBg_SetMode(mode);
    SetPrePresentCallback(mode == 2 ? PaintSmooth : NULL);
    Engine_SetScreen(s_physW, s_physH, false);
}

int Engine_BackgroundRes() { return ScreenBg_Mode(); }

void Engine_Present() {
    if (s_headless)
        return;
    if (!ScreenBg_Mode()) {
        BoxStaticFullflip();
        return;
    }
    // The screen's frame straight into Phys; an empty copy marks it for the present.
    ScreenBg_Compose((U8 *)Phys, VideoSurfacePitch(), s_surfW, s_surfH, s_logW, s_logH,
                     Engine_Scale(s_physW, s_physH), s_menuImage);
    T_BOX none = {0, 0, 0, 0, NULL};
    LockVideoSurface();
    CopyVideoArea(Phys, Log, TabOffPhysLine, &none);
    UnlockVideoSurface();
}

bool Engine_ShotScreen(const char *path) {
    const U32 w = s_physW, h = s_physH;
    U8 *idx = (U8 *)malloc((size_t)w * h);
    U32 *argb = (U32 *)malloc((size_t)w * h * 4);
    if (!idx || !argb) {
        free(idx);
        free(argb);
        return false;
    }
    ScreenBg_Compose(idx, w, w, h, s_logW, s_logH, Engine_Scale(w, h), s_menuImage);
    for (size_t i = 0; i < (size_t)w * h; i++) {
        const U8 *rgb = s_palCurrent + idx[i] * 3;
        argb[i] = 0xff000000u | (U32)rgb[0] << 16 | (U32)rgb[1] << 8 | rgb[2];
    }
    ScreenBg_PaintSmooth(argb, w * 4, w, h, s_palCurrent, s_menuImage);
    VFont_Paint(argb, w * 4, w, h, s_palCurrent);
    bool ok = Shot_WriteArgb(path, w, h, argb);
    free(idx);
    free(argb);
    return ok;
}

void Engine_ResetInput() {
    memset(s_down, 0, sizeof s_down);
    memset(s_pressed, 0, sizeof s_pressed);
    memset(s_injected, 0, sizeof s_injected);
    memset(s_rawDown, 0, sizeof s_rawDown);
    s_rawPressed = s_rawInjected = 0;
    s_lastPad = false;
}

static int s_bgFit = 1;
void Engine_SetBackgroundFit(int fit) { s_bgFit = fit; }

static int s_backdrop;
void Engine_SetBackdrop(int backdrop) { s_backdrop = backdrop; }

void Engine_DrawBackground() {
    if (!s_pixels)
        return;
    U8 *dst = (U8 *)Log;
    if (s_backdrop == 2) {  // Twinsun turning on stars; the screen outside the canvas black
        Twinsun_Draw(s_logW, s_logH, Engine_TimeMs());
        ScreenBg_NoPicture(0);
        return;
    }
    if (s_backdrop) {
        const U8 fill = s_backdrop == 1 ? 181 : 0;  // a blue from the picture; black
        for (U32 y = 0; y < s_logH; y++)
            memset(dst + TabOffLine[y], fill, s_logW);
        ScreenBg_NoPicture(fill);
        return;
    }
    if (s_bgFit == 1) {
        // Cover: one factor for both axes, the larger of W/640 and H/480 (as a
        // fraction num/den), the overflow cropped evenly; nearest neighbour.
        U32 num, den;
        if (s_logW * 480 >= s_logH * 640) {
            num = s_logW;
            den = 640;
        } else {
            num = s_logH;
            den = 480;
        }
        // Scaled size 640*num/den x 480*num/den; offsets centre it.
        U32 ox = (640 * num / den - s_logW) / 2, oy = (480 * num / den - s_logH) / 2;
        for (U32 y = 0; y < s_logH; y++) {
            U32 sy = (y + oy) * den / num;
            const U8 *row = s_menuImage + (sy < 480 ? sy : 479) * 640;
            U8 *out = dst + TabOffLine[y];
            for (U32 x = 0; x < s_logW; x++) {
                U32 sx = (x + ox) * den / num;
                out[x] = row[sx < 640 ? sx : 639];
            }
        }
        ScreenBg_BeginFrame(s_logW, s_logH);
        return;
    }
    // Nearest-neighbour stretch of the 640x480 image, as Image_LoadStretched does.
    for (U32 y = 0; y < s_logH; y++) {
        const U8 *row = s_menuImage + (y * 480 / s_logH) * 640;
        U8 *out = dst + TabOffLine[y];
        for (U32 x = 0; x < s_logW; x++)
            out[x] = row[x * 640 / s_logW];
    }
    ScreenBg_BeginFrame(s_logW, s_logH);
}
