// The game around the menu, as the menu asks about it: is one going on, what saves there
// are, and what the game menu's rows do to it.
//
// Here it is a stand-in (fake_game.cpp): no game runs and nothing is written. lba2cc has the
// game and its saves (GAMEMENU.CPP, SAVEGAME.CPP); whether its game menu, Load game and Save
// game become pages of this menu is an open question of docs/plan/MENU_PLAN.md, and until it
// is settled these calls are the list of what such pages would ask of the engine.
#pragma once

#include "game_scene.h"

enum { SAVES_MAX = 32 };

// A run's start: the saves it starts with (--saves N), and no game going on.
void Game_Start();

// ── What there is ──
bool Game_InProgress();   // a game is going on: Resume game goes back to it, Save game is offered
bool Game_CanResume();    // or current.lba has one to go back to
bool Game_HaveCurrent();  // current.lba
bool Game_HaveSaves();    // a named save or the autosave: Load game is offered (1997 counts the autosave)
bool Game_HaveAutosave();
// Played since the last save: New game would lose it, so it asks.
bool Game_Unsaved();
int Game_SaveCount();
const char *Game_SaveName(int i);
const char *Game_AutosaveName();
// The named save the game in play came from or was last saved as; -1: none, or no game.
int Game_PlayingSave();
// What it plays, for the pretend game's screen: a save's name, AUTOSAVE, or "" for a new game.
const char *Game_PlayingWhat();

// ── What the rows do ──
void Game_New();
void Game_Load(int save);    // a named save, or the autosave (save < 0)
void Game_ResumeCurrent();   // current.lba, at a start
int Game_Save(int over);     // a new save, or over one (over >= 0); returns its place in the list
void Game_DeleteSave(int i);

// ── The screen around the menu ──
// The game shows and goes on, the menu closed. 1997 writes the current save then.
void Game_Play();
bool Game_Playing();
// The Menus key in play: current.lba keeps this screen, and the menu comes up over it, dimmed.
void Game_MenuKey();
// The Options, Save or Load key in play: a page comes up over the scene as it is.
void Game_PageKey();
// The menu is up: the game stands as it was left.
void Game_Stop();
// Enter in play walks on to the next place: a scene change, which writes the autosave.
void Game_NextScene();
// Quit chosen: the screen that says the game would close. Enter there starts it again, on
// its first menu, with no game going on.
void Game_Quit();
bool Game_QuitShown();
void Game_StartAgain();
// A save made from the Save key's page: the game says so for a moment once it is back.
void Game_NoteSaved();
bool Game_JustSaved();

// ── What the view shows of it ──
GameSpot Game_Spot();             // the game as it is on screen now
bool Game_Dimmed();               // behind the game menu; not behind a page a key opened
GameSpot Game_CurrentSpot();      // the pictures: current.lba's,
GameSpot Game_SaveSpot(int save); // a named save's, or the autosave's (save < 0)

// ── For `open` (a script's way to a page, not a player's) ──
// The game menu, or one of its pages, as Esc in play brings it up. Save game is there only
// in a game, so `inGame` says one is going on.
void Game_UnderMenu(bool inGame);
// Options as the Options key opens them in play.
void Game_UnderOptions();
