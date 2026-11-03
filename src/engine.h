// Engine glue: starts lba2cc's LIB386 (window, palette, font, data) and gives
// the menu a frame loop, key edges and the drawing helpers it needs.
#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

struct EngineConfig {
    const char *dataDir;   // folder holding the game data or a disc image
    U32 physW, physH;      // the screen the prototype pretends to run on
};

// The UI is drawn on a logical canvas of W/scale x H/scale, scale a whole step.
// Rule 0, the menu plan's: scale = max(1, floor(min(W/640, H/480))).
// Rule 1: a screen 3:2 or wider takes the wide composition (960x540) at the
// largest whole step it fits; any other takes rule 0's step and the one column.
void Engine_SetScaleRule(int rule);  // before Engine_Init
int Engine_ScaleRule();
bool Engine_RuleChoseWide();  // rule 1 picked the wide composition for this screen
U32 Engine_Scale(U32 physW, U32 physH);

bool Engine_Init(const EngineConfig &cfg);
void Engine_Shutdown();

U32 Engine_LogicalW();
U32 Engine_LogicalH();
U32 Engine_PhysW();
U32 Engine_PhysH();

// One frame: pump events, update key edges. Returns false when the window closes.
bool Engine_BeginFrame();
void Engine_Present();
// Headless: no window to show, so a present only costs time. Shots read Log, not the screen.
void Engine_SetHeadless(bool on);
// The self test reads what was drawn where, never the pixels: with this off the
// background and the row shading (most of a frame's time) are skipped.
void Engine_SetPixels(bool on);
bool Engine_Pixels();
// Forget held keys and what was touched last, as a fresh start has it.
void Engine_ResetInput();
U32 Engine_TimeMs();

// Keys pressed this frame (edge, with key repeat for held arrows).
enum UiKey {
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ENTER, KEY_ESC, KEY_TAB, KEY_DEL,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,  // the prototype's composition switches
    KEY_COUNT
};
bool Engine_Pressed(UiKey k);
// The control channel presses keys through here; they count on the next frame.
void Engine_Inject(UiKey k);
bool Engine_KeyByName(const char *name, UiKey *out);
const char *Engine_KeyName(UiKey k);

// Raw keys, for rebinding. A code is an SDL scancode (1..255) or
// PAD_CODE_BASE + n, n in lba2cc's K_GAMEPAD_* order (KEYBOARD_KEYS.H).
enum { PAD_CODE_BASE = 1024, PAD_CODE_COUNT = 25 };
// The first key or pad input that went down this frame, or 0.
S32 Engine_RawPressed();
// The control channel: "J", "Left Alt", "Keypad 5" (SDL names) or "pad:<n|name>"
// (pad:a, pad:lb, pad:lstick-up ...). Counts on the next frame.
bool Engine_InjectRaw(const char *name);
// What the player touched last: true when it was the pad.
bool Engine_LastInputWasPad();
// The connected pad's family: 0 none, 1 Xbox-like, 2 PlayStation, 3 Nintendo.
S32 Engine_PadFamily();
// The SDL name of a keyboard scancode.
const char *Engine_ScancodeName(S32 code);

// Fixed clock: every frame advances the clock by dtMs, so runs repeat exactly.
void Engine_SetFixedDt(U32 dtMs);
U32 Engine_Frame();

// The palette in use, 256 x RGB (8 bits).
const U8 *Engine_Palette();

// Draw the menu background (the PCR_MENU image): fit 0 stretches it like the game
// does (Image_LoadStretched); fit 1 scales it evenly to cover the canvas, cropped
// and centred.
void Engine_SetBackgroundFit(int fit);
void Engine_DrawBackground();

// Palette work: switch between the menu palette and an island's.
void Engine_UseMenuPalette();
void Engine_UseIslandPalette(S32 island);  // 0 = Citadel
bool Engine_IslandPaletteActive();
