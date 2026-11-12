// The host: starts lba2cc's LIB386 (window, palette, font, data) and gives the
// menu a frame loop, key edges, the clock and the drawing helpers it needs.
#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

struct HostConfig {
    const char *dataDir;   // folder holding the game data or a disc image
    U32 physW, physH;      // the screen the prototype pretends to run on
};

// The UI is drawn on a logical canvas of W/scale x H/scale, scale a whole step.
// Rule 0, the menu plan's: scale = max(1, floor(min(W/640, H/480))).
// Rule 1: a screen 3:2 or wider takes the wide composition (960x540) at the
// largest whole step it fits; any other takes rule 0's step and the one column.
bool Host_RuleChoseWide();  // the wide composition for this screen
U32 Host_Scale(U32 physW, U32 physH);

bool Host_Init(const HostConfig &cfg);
void Host_Shutdown();

// Another screen, in place, as lba2cc's Res_Switch does it: the canvas follows and
// the menu keeps its page, cursor and values. sizeWindow: grow or shrink the window
// to suit, as at launch; false when the window's own size is the new screen.
// The width is rounded down to a multiple of 8, and the screen is at least 640x480.
bool Host_SetScreen(U32 physW, U32 physH, bool sizeWindow);
// The window was resized by hand since the last call: its size in pixels, the
// screen it would be if the game filled it. Never in a headless run.
bool Host_WindowResized(U32 *physW, U32 *physH);
// The native size: what the screen would be if the game filled what it is shown on now, as
// turning Fullscreen off and on again gives it: a browser's canvas in its pixels, the
// display's own size on a desktop or a console, the size at launch in a headless run.
// Rounded as Host_SetScreen rounds.
void Host_NativeSize(U32 *physW, U32 *physH);
// The screen sizes F2 and the Resolution row step through, smallest first: lba2cc's list
// and the native size among them (EPmager, 2026-11-11: no way back to it but Fullscreen
// twice).
int Host_SizeCount();
void Host_SizeAt(int i, U32 *physW, U32 *physH);
// The next (dir 1) or previous (-1) size in the list, round at the ends, from the
// one nearest below the current screen (a dragged window can sit between two).
bool Host_StepScreen(int dir);
bool Host_ShiftHeld();
// The window fills the display (lba2cc's SetWindowFullscreen) or not. The screen
// size, the canvas and the menu stay; the picture is letterboxed onto the display.
bool Host_SetFullscreen(bool on);
bool Host_Fullscreen();

U32 Host_LogicalW();
U32 Host_LogicalH();
U32 Host_PhysW();
U32 Host_PhysH();

// One frame: pump events, update key edges. Returns false when the window closes.
bool Host_BeginFrame();
void Host_Present();
// Headless: no window to show, so a present only costs time. Shots read Log, not the screen.
void Host_SetHeadless(bool on);
// The self test reads what was drawn where, never the pixels: with this off the
// background and the row shading (most of a frame's time) are skipped.
void Host_SetPixels(bool on);
bool Host_Pixels();
// Forget held keys and what was touched last, as a fresh start has it.
void Host_ResetInput();
U32 Host_TimeMs();

// Keys pressed this frame (edge, with key repeat for held arrows).
enum UiKey {
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ENTER, KEY_ESC, KEY_TAB, KEY_DEL,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,  // the prototype's composition switches
    KEY_0, KEY_PGUP, KEY_PGDN,  // 0: More options' form; a window at a time (the pad's LB, RB)
    KEY_COUNT
};
bool Host_Pressed(UiKey k);
// The control channel presses keys through here; they count on the next frame.
void Host_Inject(UiKey k);
bool Host_KeyByName(const char *name, UiKey *out);
// A key pressed this frame, as if typed: what a click on the menu stands for.
void Host_PressNow(UiKey k);
void Host_Unpress(UiKey k);  // drops this frame's press of k

// The mouse (or a finger, which arrives as one), in the canvas's logical pixels, this frame.
struct HostPointer {
    S32 x, y;
    bool moved;         // it moved since the last frame
    bool held;          // the left button is down
    bool pressed;       // the left button went down this frame
    bool released;      // and up
    bool rightPressed;  // the right button went down this frame
    S32 wheel;          // notches turned, up positive
};
void Host_Pointer(HostPointer *out);
// Whether lba2cc's arrow is shown where the pointer is: a real mouse moved in the last 3 s.
// The host paints it over the frame as it is presented.
bool Host_PointerShown();
// A script's mouse: `mouse X Y` moves it, `click [right]` presses and lets go a frame
// later, `wheel N` turns it.
void Host_InjectPointer(S32 x, S32 y);
// `mousedown` / `mouseup`: the left button held until let go, for a drag.
void Host_InjectButton(bool down);
void Host_InjectClick(bool right);
// `tap X Y`: a finger at X, Y, pressed and let go a frame later (a click marked as touch).
void Host_InjectTap(S32 x, S32 y);
void Host_InjectWheel(S32 notches);
const char *Host_KeyName(UiKey k);

// Raw keys, for rebinding. A code is an SDL scancode (1..255) or
// PAD_CODE_BASE + n, n in lba2cc's K_GAMEPAD_* order (KEYBOARD_KEYS.H).
enum { PAD_CODE_BASE = 1024, PAD_CODE_COUNT = 25 };
// The first key or pad input that went down this frame, or 0.
S32 Host_RawPressed();
// The control channel: "J", "Left Alt", "Keypad 5" (SDL names) or "pad:<n|name>"
// (pad:a, pad:lb, pad:lstick-up ...). Counts on the next frame.
bool Host_InjectRaw(const char *name);
// A pad button as a pad gives it: the raw press and the menu key it is fixed to
// (a Enter, b Esc ...), by its pad: name without the prefix ("a", "start").
bool Host_InjectPad(const char *name);
// What the player touched last: true when it was the pad.
bool Host_LastInputWasPad();
// What the player used last, if it was a pointer: POINTER_MOUSE after the mouse moved,
// clicked or turned its wheel, POINTER_TOUCH after a finger; POINTER_NONE once a key or a
// pad's button is pressed. The menu shows its pointer-only pieces (the corner Back) by it.
enum { POINTER_NONE = 0, POINTER_MOUSE = 1, POINTER_TOUCH = 2 };
int Host_LastPointer();
// A keyboard is offered (the Keyboard view, Tab) only where there is one: always on a
// desktop; on a console once a key has been typed on one.
bool Host_KeyboardSeen();
void Host_SetKeyboardSeen(bool seen);  // the "keyboard" switch: try a console's view on a desktop
// The dialogue's "more to read" arrow (sprites.hqr 6, MESSAGE.CPP AffFleche), its
// 25x25 drawn with its bottom-right corner 2 inside (x1, y1). False when the
// game data has no sprites.hqr.
// The connected pad's family: 0 none, 1 Xbox-like, 2 PlayStation, 3 Nintendo.
S32 Host_PadFamily();
// The family a pad's name tells: the vendor a browser writes into it, else words such as
// DualSense or Joy-Con. 1 Xbox (and anything else), 2 PlayStation, 3 Nintendo.
S32 Host_PadFamilyOfName(const char *name);
// The SDL name of a keyboard scancode.
const char *Host_ScancodeName(S32 code);

// Fixed clock: every frame advances the clock by dtMs, so runs repeat exactly.
void Host_SetFixedDt(U32 dtMs);
U32 Host_Frame();

// The palette in use, 256 x RGB (8 bits).
const U8 *Host_Palette();

// Draw the menu background (the PCR_MENU image): fit 0 stretches it like the game
// does (Image_LoadStretched); fit 1 scales it evenly to cover the canvas, cropped
// and centred.
void Host_DrawBackground();
// What Host_DrawBackground draws: 0 the picture, 1 plain blue, 2 Twinsun, 3 black.
void Host_SetBackdrop(int backdrop);
// The picture at the screen's resolution (screen_picture.h): 0 off, 1 whole step, 2 smooth.
void Host_SetBackgroundRes(int mode);
int Host_BackgroundRes();
// The screen as presented (the canvas at its whole step, the picture per bgres), as a PNG.
bool Host_ShotScreen(const char *path);

// Palette work: switch between the menu palette and an island's.
void Host_UseMenuPalette();
