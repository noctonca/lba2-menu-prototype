#pragma once

// The pretend game's screen: a landscape drawn in code in the menu palette, so the game menu
// has something to show behind Resume game and on Load game without the game's data.
//
// lba2cc keeps a 160x120 picture in every save (SAVEGAME.CPP SaveGame: the frame shrunk by
// ScaleBox, nearest neighbour), taken from the screen when the save is written: as a game
// starts or loads and when Esc leaves play for the menu (current.lba), at every scene change
// (autosave.lba), and on Save game. The prototype does the same with this screen: a picture is
// the scene drawn at 160x120 at that moment, the screen's centre at 4:3 as lba2cc keeps the
// centred 640x480.
//
// A spot is where the pretend game is: an island (the menu's island list), a scene on it and
// the moment (clouds and waves move). Nothing here reads the game's files.

#include <SYSTEM/ADELINE_TYPES.H>

struct GameSpot {
    int island;  // 0..4: Citadel Island, Desert Island, Emerald Moon, Otringal, Celebration Island
    int scene;   // a place on it; each draws its own hills
    U32 t;       // milliseconds of play, for what moves
};

// The scene at w x h into dst (rows `pitch` bytes apart), in the menu palette. dim: the share
// of each colour kept, 1 for none (behind the game menu: what SCREEN_SHADE_LVL keeps).
void GameScene_Draw(U8 *dst, U32 pitch, U32 w, U32 h, const GameSpot &spot, float dim = 1.0f);

// The save picture of that spot: 160x120, as SaveGame keeps it.
void GameScene_Picture(U8 *out, const GameSpot &spot);
