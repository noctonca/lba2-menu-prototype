// Engine glue: starts lba2cc's LIB386 (window, palette, font, data) and gives
// the menu a frame loop, key edges and the drawing helpers it needs.
#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

struct EngineConfig {
    const char *dataDir;   // folder holding the game data or a disc image
    U32 physW, physH;      // the screen the prototype pretends to run on
};

// Scale rule from the menu plan: scale = max(1, floor(min(W/640, H/480))),
// and the UI is drawn on a logical canvas of W/scale x H/scale.
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
U32 Engine_TimeMs();

// Keys pressed this frame (edge, with key repeat for held arrows).
enum UiKey {
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ENTER, KEY_ESC, KEY_TAB,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9,
    KEY_COUNT
};
bool Engine_Pressed(UiKey k);
// The control channel presses keys through here; they count on the next frame.
void Engine_Inject(UiKey k);
bool Engine_KeyByName(const char *name, UiKey *out);
const char *Engine_KeyName(UiKey k);

// Fixed clock: every frame advances the clock by dtMs, so runs repeat exactly.
void Engine_SetFixedDt(U32 dtMs);
U32 Engine_Frame();

// The palette in use, 256 x RGB (8 bits).
const U8 *Engine_Palette();

// Draw the menu background (the PCR_MENU image, stretched like the game does).
void Engine_DrawBackground();

// Palette work: switch between the menu palette and an island's.
void Engine_UseMenuPalette();
void Engine_UseIslandPalette(S32 island);  // 0 = Citadel
bool Engine_IslandPaletteActive();
