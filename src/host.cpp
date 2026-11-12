#include "host.h"
#include "screen_font.h"

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
#include <SYSTEM/MOUSE.H>
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

#include "game_pieces.h"
#include "screen_picture.h"
#include "shot.h"
#include "twinsun.h"

// Resource numbers, from lba2cc SOURCES/COMMON.H.
#define RESS_PAL 0
#define RESS_FONT_GPM 1
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
static U8 *s_menuImage;      // 640x480, as authored
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

// A screen 3:2 or wider takes the wide composition (960x540) at the
// largest whole step it fits; a squarer screen, or one too small for 960x540,
// takes the one column at the largest step that holds 640x480.
static bool WideByRule(U32 w, U32 h, U32 *scale) {
    if (w * 2 < h * 3)
        return false;
    U32 wx = w / 960, wy = h / 540;
    U32 ws = wx < wy ? wx : wy;
    if (ws < 1)
        return false;
    *scale = ws;
    return true;
}

bool Host_RuleChoseWide() {
    U32 s;
    return WideByRule(s_physW, s_physH, &s);
}

U32 Host_Scale(U32 w, U32 h) {
    U32 ws;
    if (WideByRule(w, h, &ws))
        return ws;
    U32 sx = w / 640, sy = h / 480;
    U32 s = sx < sy ? sx : sy;
    return s < 1 ? 1 : s;
}

U32 Host_LogicalW() { return s_logW; }
U32 Host_LogicalH() { return s_logH; }
U32 Host_PhysW() { return s_physW; }
U32 Host_PhysH() { return s_physH; }

static void PadRemoved(uint32_t id);
static void PaintOverlay(U32 *argb, U32 pitch, U32 w, U32 h, const U8 *palette);

// Button presses and releases counted as they arrive: LIB386's Click says only what is held
// when the frame reads it, so a click quicker than a frame (a browser's, a touchpad tap) was lost.
static int s_leftDowns, s_leftUps, s_rightDowns;

static int EventFilter(const PlatformEvent *event) {
    if (event->type == PLATFORM_EVENT_MOUSE_BUTTON_DOWN || event->type == PLATFORM_EVENT_MOUSE_BUTTON_UP) {
        const bool down = event->type == PLATFORM_EVENT_MOUSE_BUTTON_DOWN;
        if (event->button.button == 1)
            (down ? s_leftDowns : s_leftUps)++;
        else if (event->button.button == 3 && down)
            s_rightDowns++;
    }
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
// The pointer over the canvas in thousandths of its CSS box, from the canvas's own events:
// SDL's window-to-canvas mapping came out at twice the place on a 2x display (2026-11-12).
// -1 before the pointer has been over it.
EM_JS(int, WebPointerMilli, (int axis), {
    var c = document.getElementById('canvas');
    if (!c) return -1;
    if (!Module.ptrHooked) {
        Module.ptrHooked = true;
        Module.ptrAt = [-1, -1];
        var at = function (e) {
            var r = c.getBoundingClientRect();
            if (r.width > 0 && r.height > 0)
                Module.ptrAt = [Math.round((e.clientX - r.left) * 1000 / r.width), Math.round((e.clientY - r.top) * 1000 / r.height)];
        };
        c.addEventListener('pointermove', at);
        c.addEventListener('pointerdown', at);
    }
    return Module.ptrAt[axis];
});

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
    U32 scale = Host_Scale(s_physW, s_physH);
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

bool Host_SetScreen(U32 w, U32 h, bool sizeWindow) {
    w &= ~7u;
    if (w < 640)
        w = 640;
    if (h < 480)
        h = 480;
    U32 scale = Host_Scale(w, h);
    U32 lw = w / scale, lh = h / scale;
    // With the picture at the screen's resolution the presented frame is the
    // screen; the canvas stays logical.
    U32 sw = ScreenPicture_Mode() ? w : lw, sh = ScreenPicture_Mode() ? h : lh;
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

static U32 s_launchW, s_launchH;  // the screen at launch: a headless run's native size

void Host_NativeSize(U32 *w, U32 *h) {
    U32 nw = s_launchW, nh = s_launchH;
    if (!s_headless) {
#ifdef __EMSCRIPTEN__
        nw = (U32)WebCanvasPx(WebCanvasCssW());
        nh = (U32)WebCanvasPx(WebCanvasCssH());
#else
        uint32_t dw = 0, dh = 0;
        if (g_platform.window.display_size(&dw, &dh))
            nw = dw, nh = dh;
#endif
    }
    nw &= ~7u;
    *w = nw < 640 ? 640 : nw;
    *h = nh < 480 ? 480 : nh;
}

// lba2cc's list with the native size put in by area, unless it is one of them.
static int SizeList(U32 out[][2]) {
    const int fixed = (int)(sizeof kSizes / sizeof kSizes[0]);
    U32 nw, nh;
    Host_NativeSize(&nw, &nh);
    int n = 0;
    bool placed = false;
    for (int i = 0; i < fixed; i++) {
        if (kSizes[i][0] == nw && kSizes[i][1] == nh)
            placed = true;
        if (!placed && kSizes[i][0] * kSizes[i][1] > nw * nh) {
            out[n][0] = nw, out[n][1] = nh, n++;
            placed = true;
        }
        out[n][0] = kSizes[i][0], out[n][1] = kSizes[i][1], n++;
    }
    if (!placed)
        out[n][0] = nw, out[n][1] = nh, n++;
    return n;
}

int Host_SizeCount() {
    U32 list[16][2];
    return SizeList(list);
}

void Host_SizeAt(int i, U32 *w, U32 *h) {
    U32 list[16][2];
    SizeList(list);
    *w = list[i][0];
    *h = list[i][1];
}

bool Host_StepScreen(int dir) {
    U32 list[16][2];
    const int count = SizeList(list);
    int below = 0;
    bool exact = false;
    for (int i = 0; i < count; i++) {
        if (list[i][0] * list[i][1] <= s_physW * s_physH)
            below = i;
        if (list[i][0] == s_physW && list[i][1] == s_physH) {
            below = i;
            exact = true;
            break;
        }
    }
    // Off the list, the one below counts as the previous.
    int next = dir > 0 ? below + 1 : (exact ? below - 1 : below);
    next = (next + count) % count;
    return Host_SetScreen(list[next][0], list[next][1], true);
}

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
EM_JS(int, WebIsFullscreen, (), { return document.fullscreenElement ? 1 : 0; });
#endif

bool Host_Fullscreen() {
#ifdef __EMSCRIPTEN__
    if (!s_headless)
        return WebIsFullscreen() != 0;  // the browser's; Esc leaves it without the menu
#endif
    return s_fullscreen;
}

bool Host_SetFullscreen(bool on) {
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

bool Host_WindowResized(U32 *w, U32 *h) {
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

bool Host_ShiftHeld() { return (g_platform.input.modifiers() & PLATFORM_KMOD_SHIFT) != 0; }

bool Host_Init(const HostConfig &cfg) {
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
    s_launchW = s_physW;
    s_launchH = s_physH;
    U32 scale = Host_Scale(s_physW, s_physH);
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

    s_palMenu = (U8 *)LoadMalloc_HQR(s_ress, RESS_PAL);
    U8 *font = (U8 *)LoadMalloc_HQR(s_ress, RESS_FONT_GPM);
    s_xplBoot = (U8 *)LoadMalloc_HQR(s_ress, RESS_XPL00);
    char holomap[1024];
    JoinPath(holomap, sizeof holomap, dir, "holomap.hqr");
    Twinsun_Init(holomap);  // the Background row's Twinsun; black without it
    s_menuImage = (U8 *)malloc(640 * 480 + 4096);
    if (!s_palMenu || !font || !s_xplBoot || !Load_HQR(s_screen, s_menuImage, PCR_MENU)) {
        fprintf(stderr, "could not read the game data in %s\n", dir);
        return false;
    }
    FromLE32InPlace(s_xplBoot, sizeof(XplHeader) / 4);
    // ShadeBoxBlk darkens through PtrCLUTGouraud. Checked against lba2cc's
    // `ui menu-main-options` capture: identical outside the plasma with citabau.
    PtrCLUTGouraud = s_xplBoot + ((const XplHeader *)s_xplBoot)->OffsetFog;

    SetFont(font, 2, 8);  // as PERSO.CPP sets it
    ScreenFont_Init();
    SetOverlayPrePresentCallback(PaintOverlay);  // after the smooth picture's hook
    Rnd_Seed(1997);       // the plasma's start; fixed so captures repeat
    GamePieces_InitPlasma();
    Host_UseMenuPalette();
    return true;
}

void Host_Shutdown() {
    DiscImage_Unmount();
}

void Host_UseMenuPalette() {
    s_palCurrent = s_palMenu;
    PtrCLUTGouraud = s_xplBoot + ((const XplHeader *)s_xplBoot)->OffsetFog;
    SetVideoPaletteSync(s_palMenu);
}

// Under a fixed clock, time counts from when it was armed, so the Twinsun's turn and anything
// else drawn from the time repeats from run to run; lba2cc seeds its virtual clock from the
// wall clock at that moment.
static U32 s_timeOrigin;

U32 Host_TimeMs() {
    ManageTime();
    return TimerSystemHR - s_timeOrigin;
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

void Host_Inject(UiKey k) { s_injected[k] = true; }


bool Host_KeyByName(const char *name, UiKey *out) {
    for (int k = 0; k < KEY_COUNT; k++) {
        if (strcmp(kKeyNames[k], name) == 0) {
            *out = (UiKey)k;
            return true;
        }
    }
    return false;
}

const char *Host_KeyName(UiKey k) { return kKeyNames[k]; }

void Host_SetFixedDt(U32 dtMs) {
    s_fixedDt = dtMs;
    if (dtMs) {
        Timer_EnableFixedDt(dtMs);
        s_timeOrigin = TimerSystemHR;  // the virtual clock's seed
    }
}

U32 Host_Frame() { return s_frame; }

const U8 *Host_Palette() { return s_palCurrent; }

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
                static const char *const kFamily[] = {"", "Xbox", "PlayStation", "Nintendo"};
                const char *name = g_platform.input.gamepad_name(s_pad);
                fprintf(stderr, "pad: %s (%s names)\n", name ? name : "(no name)", kFamily[Host_PadFamily()]);
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
static int s_lastPointer;  // POINTER_*: the pointer, if it was used last
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
    if (s_rawPressed) {
        s_lastPad = s_rawPressed >= PAD_CODE_BASE;
        s_lastPointer = POINTER_NONE;
    }
    if (s_rawPressed && s_rawPressed < PAD_CODE_BASE)
        s_keyboardSeen = true;
    for (int i = 0; i < 256 + PAD_CODE_COUNT; i++) {
        bool down = i < 256 ? (i > 0 && CheckKey((U32)i) != 0) : PadRawDown(i - 256);
        if (down && !s_rawDown[i]) {
            S32 code = i < 256 ? i : PAD_CODE_BASE + (i - 256);
            if (!s_rawPressed)
                s_rawPressed = code;
            s_lastPad = i >= 256;
            s_lastPointer = POINTER_NONE;
            if (i < 256)
                s_keyboardSeen = true;
        }
        s_rawDown[i] = down;
    }
}

S32 Host_RawPressed() { return s_rawPressed; }

bool Host_KeyboardSeen() { return s_keyboardSeen; }
void Host_SetKeyboardSeen(bool seen) { s_keyboardSeen = seen; }

bool Host_LastInputWasPad() { return s_lastPad; }

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

bool Host_InjectRaw(const char *name) {
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

bool Host_InjectPad(const char *name) {
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

const char *Host_ScancodeName(S32 code) {
    const char *name = g_platform.input.scancode_name(code);
    return name ? name : "";
}

// The family a USB vendor makes: 0 when it is not one of the three.
static S32 VendorFamily(unsigned vendor) {
    switch (vendor) {
    case 0x045e: return 1;  // Microsoft
    case 0x054c: return 2;  // Sony
    case 0x057e: return 3;  // Nintendo
    default: return 0;
    }
}

// The vendor a browser writes into the pad's name: Chrome's "Name (STANDARD GAMEPAD
// Vendor: 057e Product: 2009)", Firefox's "57e-2009-Name". 0 when there is none.
static unsigned NameVendor(const char *name) {
    if (const char *v = strstr(name, "Vendor: "))
        return (unsigned)strtoul(v + 8, NULL, 16);
    const char *p = name;
    int digits = 0;
    while (isxdigit((unsigned char)*p) && digits < 5)
        p++, digits++;
    if (digits >= 1 && digits <= 4 && *p == '-') {
        const char *q = p + 1;
        int product = 0;
        while (isxdigit((unsigned char)*q) && product < 5)
            q++, product++;
        if (product >= 1 && product <= 4 && *q == '-')
            return (unsigned)strtoul(name, NULL, 16);
    }
    return 0;
}

S32 Host_PadFamilyOfName(const char *name) {
    if (!name)
        return 1;
    // The vendor first: a Switch pad in a browser on Windows is "Wireless Gamepad"
    // (EPmager, 2026-11-11).
    if (S32 f = VendorFamily(NameVendor(name)))
        return f;
    if (NameHas(name, "PS3") || NameHas(name, "PS4") || NameHas(name, "PS5") || NameHas(name, "DualShock") ||
        NameHas(name, "DualSense") || NameHas(name, "PlayStation"))
        return 2;
    if (NameHas(name, "Nintendo") || NameHas(name, "Switch") || NameHas(name, "Joy-Con") ||
        NameHas(name, "Pro Controller"))
        return 3;
    return 1;
}

// The pad's family: SDL's own type where it knows one (from the vendor and product, on a
// desktop), else from the name. lba2cc's platform layer has no pad type yet; the plan puts
// one there.
S32 Host_PadFamily() {
    if (!s_pad)
        return 0;
#ifdef LBA2_PROTO_SDL
    switch (SDL_GetGamepadTypeForID((SDL_JoystickID)s_padId)) {
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE:
        return 1;
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
        return 2;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return 3;
    default:
        break;
    }
#endif
    return Host_PadFamilyOfName(g_platform.input.gamepad_name(s_pad));
}

// ── The pointer ──
static HostPointer s_ptr;
static bool s_ptrInjected;        // a script put it; a real move takes it back
static S32 s_injX, s_injY;
static bool s_injMoved, s_injDown, s_injRight, s_injHeld, s_injTouch;
static S32 s_injWheel;

void Host_InjectPointer(S32 x, S32 y) {
    s_ptrInjected = true;
    s_injX = x, s_injY = y;
    s_injMoved = true;
}
void Host_InjectClick(bool right) {
    if (right)
        s_injRight = true;
    else
        s_injDown = true;
}
void Host_InjectTap(S32 x, S32 y) {
    Host_InjectPointer(x, y);
    s_injDown = true;
    s_injTouch = true;
}
void Host_InjectWheel(S32 notches) { s_injWheel += notches; }
int Host_LastPointer() { return s_lastPointer; }
void Host_InjectButton(bool down) { s_injHeld = down; }
void Host_Pointer(HostPointer *out) { *out = s_ptr; }

// The pointer's arrow, by lba2cc's menu rules (LIB386 MOUSE.CPP): shown once a real mouse
// moves, hidden after 3 s still and while a finger points. Painted on the screen's frame
// after the screen font (PaintOverlay), not on the canvas: the font leaves out a word whose
// canvas changed under it, and the picture's present does not go through LIB386's flip,
// which draws the arrow in the game.
enum { POINTER_IDLE_HIDE_MS = 3000 };
extern U8 BinGphMouse[];  // LIB386 SYSTEM/MOUSEDAT.CPP: lba2cc's arrow
static U32 s_ptrActiveAt;
static bool s_ptrSeen;

bool Host_PointerShown() {
    return s_ptrSeen && !LastInputWasTouch && Host_TimeMs() - s_ptrActiveAt < POINTER_IDLE_HIDE_MS;
}

// The arrow at the canvas's whole step, as the canvas is shown. Its pixels are found by
// drawing it on the canvas twice over two fills (a pixel it leaves is the fill); the canvas
// is put back after.
static void PaintPointer(U32 *argb, U32 pitch, U32 w, U32 h, const U8 *palette) {
    if (!Host_PointerShown())
        return;
    S32 gx0, gy0, gx1, gy1;
    GetBoxGraph(0, &gx0, &gy0, &gx1, &gy1, BinGphMouse);
    const S32 x0 = s_ptr.x + gx0, y0 = s_ptr.y + gy0;
    const S32 bw = gx1 - gx0 + 1, bh = gy1 - gy0 + 1;
    enum { ARROW_MAX = 64 };
    if (bw <= 0 || bh <= 0 || bw > ARROW_MAX || bh > ARROW_MAX)
        return;
    static U8 saved[ARROW_MAX][ARROW_MAX], pass[2][ARROW_MAX][ARROW_MAX];
    const S32 cx0 = ClipXMin, cy0 = ClipYMin, cx1 = ClipXMax, cy1 = ClipYMax;
    ClipXMin = 0, ClipYMin = 0, ClipXMax = (S32)s_logW - 1, ClipYMax = (S32)s_logH - 1;
    auto at = [](S32 x, S32 y) -> U8 & { return ((U8 *)Log)[TabOffLine[y] + x]; };
    auto inside = [](S32 x, S32 y) { return x >= 0 && y >= 0 && x < (S32)s_logW && y < (S32)s_logH; };
    for (S32 j = 0; j < bh; j++)
        for (S32 i = 0; i < bw; i++)
            if (inside(x0 + i, y0 + j))
                saved[j][i] = at(x0 + i, y0 + j);
    for (int p = 0; p < 2; p++) {
        for (S32 j = 0; j < bh; j++)
            for (S32 i = 0; i < bw; i++)
                if (inside(x0 + i, y0 + j))
                    at(x0 + i, y0 + j) = (U8)(254 + p);
        AffGraph(0, s_ptr.x, s_ptr.y, BinGphMouse);
        for (S32 j = 0; j < bh; j++)
            for (S32 i = 0; i < bw; i++)
                pass[p][j][i] = inside(x0 + i, y0 + j) ? at(x0 + i, y0 + j) : (U8)(254 + p);
    }
    for (S32 j = 0; j < bh; j++)
        for (S32 i = 0; i < bw; i++)
            if (inside(x0 + i, y0 + j))
                at(x0 + i, y0 + j) = saved[j][i];
    ClipXMin = cx0, ClipYMin = cy0, ClipXMax = cx1, ClipYMax = cy1;

    U32 k = w / s_logW < h / s_logH ? w / s_logW : h / s_logH;
    if (k < 1)
        k = 1;
    const S32 bx = (S32)(w - k * s_logW) / 2, by = (S32)(h - k * s_logH) / 2;
    for (S32 j = 0; j < bh; j++)
        for (S32 i = 0; i < bw; i++) {
            if (pass[0][j][i] != pass[1][j][i])
                continue;  // the fill showed through: not the arrow's
            const U8 *rgb = palette + pass[0][j][i] * 3;
            const U32 c = 0xff000000u | (U32)rgb[0] << 16 | (U32)rgb[1] << 8 | rgb[2];
            for (U32 dy = 0; dy < k; dy++) {
                const S32 y = by + (y0 + j) * (S32)k + (S32)dy;
                if (y < 0 || y >= (S32)h)
                    continue;
                U32 *row = (U32 *)((U8 *)argb + (size_t)y * pitch);
                for (U32 dx = 0; dx < k; dx++) {
                    const S32 x = bx + (x0 + i) * (S32)k + (S32)dx;
                    if (x >= 0 && x < (S32)w)
                        row[x] = c;
                }
            }
        }
}

// What is painted on the screen's frame over the canvas: the screen font, then the arrow.
static void PaintOverlay(U32 *argb, U32 pitch, U32 w, U32 h, const U8 *palette) {
    ScreenFont_Paint(argb, pitch, w, h, palette);
    PaintPointer(argb, pitch, w, h, palette);
}

static void ReadPointer() {
    const bool realMove = MouseConsumeMotionFlag();
    if (realMove)
        s_ptrInjected = false;
    const bool wasHeld = s_ptr.held;
    static bool rightHeld;
    s_ptr.moved = realMove || s_injMoved;
    s_ptr.x = s_ptrInjected ? s_injX : (S32)MouseX;
    s_ptr.y = s_ptrInjected ? s_injY : (S32)MouseY;
#ifdef __EMSCRIPTEN__
    const int mx = WebPointerMilli(0), my = WebPointerMilli(1);
    if (!s_ptrInjected && mx >= 0 && my >= 0) {
        s_ptr.x = (S32)((S64)mx * (S32)s_logW / 1000);
        s_ptr.y = (S32)((S64)my * (S32)s_logH / 1000);
        s_ptr.x = s_ptr.x >= (S32)s_logW ? (S32)s_logW - 1 : s_ptr.x;
        s_ptr.y = s_ptr.y >= (S32)s_logH ? (S32)s_logH - 1 : s_ptr.y;
    }
#endif
    s_ptr.held = (Click & 1) != 0 || s_injDown || s_injHeld;
    s_ptr.pressed = (s_ptr.held && !wasHeld) || s_leftDowns > 0;
    s_ptr.released = (!s_ptr.held && wasHeld) || (s_leftUps > 0 && !s_ptr.held);
    const bool right = (Click & 2) != 0;
    s_ptr.rightPressed = (right && !rightHeld) || s_rightDowns > 0 || s_injRight;
    rightHeld = right;
    s_leftDowns = s_leftUps = s_rightDowns = 0;
    s_ptr.wheel = ConsumeMouseWheelY() + s_injWheel;
    s_injMoved = s_injDown = s_injRight = false;
    s_injWheel = 0;
    if (s_ptr.moved || s_ptr.pressed || s_ptr.rightPressed || s_ptr.wheel) {
        s_lastPad = false;  // the legend names keys again
        const bool touch = s_ptrInjected ? s_injTouch : LastInputWasTouch != 0;
        s_lastPointer = touch ? POINTER_TOUCH : POINTER_MOUSE;
    }
    if (!s_ptr.held)
        s_injTouch = false;
    if (!s_ptrInjected && (realMove || s_ptr.pressed || s_ptr.rightPressed || s_ptr.wheel)) {
        s_ptrSeen = true;
        s_ptrActiveAt = Host_TimeMs();
    }
}

bool Host_BeginFrame() {
    ClearMouseBox();  // the canvas as it is now: LIB386 keeps the pointer inside its box
    ManageEvents();
    ManageKeyboard();
    ScanRaw();
    ReadPointer();
    if (s_fixedDt)
        Timer_FixedDtAdvance();
    s_frame++;
    U32 now = Host_TimeMs();
    for (int k = 0; k < KEY_COUNT; k++) {
        bool down = CheckKey(kScancode[k]) != 0 || PadDown(k);
        if (k == KEY_DEL)  // a Mac keyboard's delete key is Backspace
            down = down || CheckKey(K_BACKSPACE) != 0;
        s_pressed[k] = s_injected[k];
        if (s_injected[k])
            s_lastPointer = POINTER_NONE;  // a script's `key`, as a typed key
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

bool Host_Pressed(UiKey k) { return s_pressed[k]; }
void Host_PressNow(UiKey k) { s_pressed[k] = true; }
void Host_Unpress(UiKey k) { s_pressed[k] = false; }

void Host_SetHeadless(bool on) { s_headless = on; }

static bool s_pixels = true;
void Host_SetPixels(bool on) { s_pixels = on; }
bool Host_Pixels() { return s_pixels; }

static void PaintSmooth(U32 *argb, U32 pitch, U32 w, U32 h, const U8 *palette) {
    ScreenPicture_PaintSmooth(argb, pitch, w, h, palette, s_menuImage);
}

void Host_SetBackgroundRes(int mode) {
    ScreenPicture_SetMode(mode);
    SetPrePresentCallback(mode == 2 ? PaintSmooth : NULL);
    Host_SetScreen(s_physW, s_physH, false);
}

int Host_BackgroundRes() { return ScreenPicture_Mode(); }

void Host_Present() {
    if (s_headless)
        return;
    if (!ScreenPicture_Mode()) {
        BoxStaticFullflip();
        return;
    }
    // The screen's frame straight into Phys; an empty copy marks it for the present.
    ScreenPicture_Compose((U8 *)Phys, VideoSurfacePitch(), s_surfW, s_surfH, s_logW, s_logH,
                     Host_Scale(s_physW, s_physH), s_menuImage);
    T_BOX none = {0, 0, 0, 0, NULL};
    LockVideoSurface();
    CopyVideoArea(Phys, Log, TabOffPhysLine, &none);
    UnlockVideoSurface();
}

bool Host_ShotScreen(const char *path) {
    const U32 w = s_physW, h = s_physH;
    U8 *idx = (U8 *)malloc((size_t)w * h);
    U32 *argb = (U32 *)malloc((size_t)w * h * 4);
    if (!idx || !argb) {
        free(idx);
        free(argb);
        return false;
    }
    ScreenPicture_Compose(idx, w, w, h, s_logW, s_logH, Host_Scale(w, h), s_menuImage);
    for (size_t i = 0; i < (size_t)w * h; i++) {
        const U8 *rgb = s_palCurrent + idx[i] * 3;
        argb[i] = 0xff000000u | (U32)rgb[0] << 16 | (U32)rgb[1] << 8 | rgb[2];
    }
    ScreenPicture_PaintSmooth(argb, w * 4, w, h, s_palCurrent, s_menuImage);
    PaintOverlay(argb, w * 4, w, h, s_palCurrent);
    bool ok = Shot_WriteArgb(path, w, h, argb);
    free(idx);
    free(argb);
    return ok;
}

void Host_ResetInput() {
    memset(s_down, 0, sizeof s_down);
    memset(s_pressed, 0, sizeof s_pressed);
    memset(s_injected, 0, sizeof s_injected);
    memset(s_rawDown, 0, sizeof s_rawDown);
    s_rawPressed = s_rawInjected = 0;
    s_lastPad = false;
    s_lastPointer = POINTER_NONE;
}

static int s_backdrop;
void Host_SetBackdrop(int backdrop) { s_backdrop = backdrop; }

void Host_DrawBackground() {
    if (!s_pixels)
        return;
    U8 *dst = (U8 *)Log;
    if (s_backdrop == 2) {  // Twinsun turning on stars; the screen outside the canvas black
        Twinsun_Draw(s_logW, s_logH, Host_TimeMs());
        ScreenPicture_NoPicture(0);
        return;
    }
    if (s_backdrop) {
        const U8 fill = s_backdrop == 1 ? 181 : 0;  // a blue from the picture; black
        for (U32 y = 0; y < s_logH; y++)
            memset(dst + TabOffLine[y], fill, s_logW);
        ScreenPicture_NoPicture(fill);
        return;
    }
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
    ScreenPicture_BeginFrame(s_logW, s_logH);
}
