// The game around the menu: a stand-in, as the settings are. No game runs and nothing is
// written. lba2cc has the game and its saves; none of this file goes there.
#include "menu.h"

#include "game.h"
#include "trace.h"

#include "host.h"
#include "menu_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Fakes, as the settings are: no game runs and nothing is written. A game is going on or not
// (the 1997 menu's firstloop). The saves, as 1997 keeps them (333929a GAMEMENU.CPP, and the GOG
// DOS LBA2.EXE seen 2026-11-14):
//   - named saves, in alphabetical order as PlayerGameList sorts them, named as lba2cc names a
//     save it makes ("<island> - <date time>", GAMEMENU.CPP MakeAutoSaveName), shorter:
//     "Celebration Island - 2026-11-08 15-57-23" spills a 640x480 row by up to 33 px, and
//     without the seconds still by 8 (self test, 2026-11-14), so neither seconds nor the dash;
//   - the current save (current.lba), written as a game starts, loads or is left for the menu:
//     Resume game plays it;
//   - the autosave (autosave.lba), written at every scene change: Load game's last row,
//     "AUTOSAVE" (its player name), never in Save game's list and never deleted.
// Enough named saves that the lists scroll.

static_assert(SAVES_MAX + 1 <= MORE_OPTIONS_MAX, "Save game's rows fit PAGE_ROWS_MAX");
static char g_saves[SAVES_MAX][64];
static int g_saveCount;
static int g_savesAtStart = 0;  // --saves N; 0, a fresh install, at start: no Resume game, no Load game
static bool g_haveCurrent;  // current.lba: Resume game is offered
static char g_currentWhat[64];  // what Resume game plays at a start
static char g_currentIsland[32];
static bool g_haveAutosave; // autosave.lba: Load game's AUTOSAVE row
static char g_autosaveIsland[32];
static bool g_inGame;    // a game is going on: Resume game goes back to it, Save game is offered
// Played since the last save. 1997 keeps that play in current.lba (written when Esc leaves
// play), which Resume game continues even after Quit; New game and Load write over it.
static bool g_unsaved;
static bool g_playing;   // the pretend game is on screen, the menu closed
static bool g_quitShown; // Quit chosen: the screen that says the game would close
static char g_playingWhat[64];  // what the pretend game plays: a save's name, AUTOSAVE, or a new game
static char g_island[32];       // where it is, for the next save's name
static int g_saveClock;  // the minutes the fake clock moved since the first save made here

static const char *const kIslands[] = {"Citadel Island", "Desert Island", "Emerald Moon", "Otringal",
                                       "Celebration Island"};
static const char *const kAutosaveName = "AUTOSAVE";  // the autosave's player name, as 1997 lists it

// Where the pretend game is (game_scene.h), and where each save left it: its picture is that
// spot drawn at 160x120, as lba2cc keeps the screen in a save when it writes one.
static GameSpot g_spot;         // in play, as of g_playFrom
static U32 g_playFrom;          // Host_TimeMs when the game was last shown
// In a game the menu is drawn over the game as it was left. Esc dims all of it first, as
// MainGameMenu does (ShadeBoxBlk, SCREEN_SHADE_LVL); the Options, Save and Load keys open
// their page over the scene as it is, only the boxes shaded (PERSO.CPP, I_OPTIONS and on).
static bool g_dimGame;
static U32 g_savedAt;           // Host_TimeMs of a save made from play, for "Game saved." there
static bool g_savedFromPlay;
static GameSpot g_currentSpot;  // current.lba: as a game starts or loads, and when Esc leaves play
static GameSpot g_autosaveSpot; // autosave.lba: as a game starts or loads, and at every scene change
struct SaveSpot {
    char name[64];
    GameSpot spot;
};
static SaveSpot g_saveSpots[SAVES_MAX * 4];  // the saves made here; a fake one's spot comes from its name
static int g_saveSpotCount;

static int IslandOf(const char *where) {
    for (int i = 0; i < SAVE_PICTURE_ISLANDS; i++)
        if (!strncmp(where, kIslands[i], strlen(kIslands[i])))
            return i;
    return 0;
}

// A fake save's spot: its island, and a scene and moment of its own, from its name.
static GameSpot SpotOfName(const char *name) {
    U32 h = 2166136261u;
    for (const char *p = name; *p; p++)
        h = (h ^ (U8)*p) * 16777619u;
    return {IslandOf(name), (int)(h % 6), (h >> 8) % 60000};
}

static GameSpot SpotOf(const char *name) {
    for (int i = g_saveSpotCount - 1; i >= 0; i--)
        if (!strcmp(g_saveSpots[i].name, name))
            return g_saveSpots[i].spot;
    return SpotOfName(name);
}

// The game as it is on screen now.
static GameSpot LiveSpot() {
    GameSpot s = g_spot;
    if (g_playing)
        s.t += Host_TimeMs() - g_playFrom;
    return s;
}

static int CompareSaves(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static void SortSaves() { qsort(g_saves, (size_t)g_saveCount, sizeof g_saves[0], CompareSaves); }

// The fake saves a run starts with: a week of play, a few hours apart. The newest is where the
// current save and the autosave are too.
static void MakeSaves(int n) {
    g_saveCount = n < SAVES_MAX ? n : SAVES_MAX;
    int day = 14, hour = 21, minute = 40;
    for (int i = 0; i < g_saveCount; i++) {
        snprintf(g_saves[i], sizeof g_saves[i], "%s 2026-11-%02d %02d:%02d", kIslands[(i / 4) % 5], day, hour,
                 minute);
        hour -= 3 + i % 4;
        minute = (minute + 23) % 60;
        if (hour < 9) {
            hour += 13;
            day--;
        }
    }
    g_haveCurrent = g_haveAutosave = g_saveCount > 0;
    snprintf(g_currentWhat, sizeof g_currentWhat, "%s", g_saveCount ? g_saves[0] : "");
    snprintf(g_currentIsland, sizeof g_currentIsland, "%s", kIslands[0]);
    snprintf(g_autosaveIsland, sizeof g_autosaveIsland, "%s", kIslands[0]);
    g_currentSpot = SpotOfName(g_currentWhat);
    g_autosaveSpot = SpotOfName(kAutosaveName);
    g_autosaveSpot.island = 0;
    g_saveSpotCount = 0;
    SortSaves();
}

static int FindSave(const char *name) {
    for (int i = 0; i < g_saveCount; i++)
        if (!strcmp(g_saves[i], name))
            return i;
    return -1;
}

// A save made now: on the island the game is on, at the fake clock's time. Returns its place.
static int SaveNow(int over) {
    if (over >= 0) {  // Save over a save: it goes, and the new one takes its place in the order
        for (int i = over; i + 1 < g_saveCount; i++)
            memcpy(g_saves[i], g_saves[i + 1], sizeof g_saves[i]);
        g_saveCount--;
    }
    if (g_saveCount == SAVES_MAX)
        g_saveCount--;  // the list is full: the last goes
    g_saveClock += 7;
    const int minutes = 22 * 60 + g_saveClock;  // from 22:00, after the newest fake save
    char *made = g_saves[g_saveCount++];
    snprintf(made, sizeof g_saves[0], "%s 2026-11-%02d %02d:%02d", g_island, 14 + minutes / (24 * 60),
             minutes / 60 % 24, minutes % 60);
    snprintf(g_playingWhat, sizeof g_playingWhat, "%s", made);  // the game goes on from it
    if (g_saveSpotCount < (int)(sizeof g_saveSpots / sizeof g_saveSpots[0])) {
        SaveSpot &k = g_saveSpots[g_saveSpotCount++];
        snprintf(k.name, sizeof k.name, "%s", made);
        k.spot = LiveSpot();
    }
    SortSaves();
    g_unsaved = false;
    Trace("save %s", g_playingWhat);
    return FindSave(g_playingWhat);
}

static void DeleteSave(int i) {
    Trace("delete %s", g_saves[i]);
    for (; i + 1 < g_saveCount; i++)
        memcpy(g_saves[i], g_saves[i + 1], sizeof g_saves[i]);
    g_saveCount--;
}

static bool HaveSaves() { return g_saveCount > 0 || g_haveAutosave; }  // 1997 counts the autosave

void Menu_SetSavesAtStart(int n) { g_savesAtStart = n < 0 ? 0 : n; }
const char *Menu_PlayingWhat() { return g_playingWhat; }

// ───────────────────────────── What the menu asks (game.h) ─────────────────────────────

void Game_Start() {
    MakeSaves(g_savesAtStart);
    g_inGame = g_unsaved = g_playing = g_quitShown = false;
    g_playingWhat[0] = 0;
    snprintf(g_island, sizeof g_island, "%s", kIslands[0]);
    g_saveClock = 0;
}

bool Game_InProgress() { return g_inGame; }
bool Game_CanResume() { return g_inGame || g_haveCurrent; }
bool Game_HaveCurrent() { return g_haveCurrent; }
bool Game_HaveSaves() { return HaveSaves(); }
bool Game_HaveAutosave() { return g_haveAutosave; }
bool Game_Unsaved() { return g_unsaved; }
int Game_SaveCount() { return g_saveCount; }
const char *Game_SaveName(int i) { return g_saves[i]; }
const char *Game_AutosaveName() { return kAutosaveName; }
int Game_PlayingSave() { return g_inGame && g_playingWhat[0] ? FindSave(g_playingWhat) : -1; }
const char *Game_PlayingWhat() { return g_playingWhat; }

void Game_New() {
    g_playingWhat[0] = 0;
    snprintf(g_island, sizeof g_island, "%s", kIslands[0]);  // a new game starts on Citadel Island
    g_spot = {0, 0, 0};
    Trace("new game");
}

void Game_Load(int i) {
    if (i < 0) {
        snprintf(g_playingWhat, sizeof g_playingWhat, "%s", kAutosaveName);
        snprintf(g_island, sizeof g_island, "%s", g_autosaveIsland);
        g_spot = g_autosaveSpot;
    } else {
        snprintf(g_playingWhat, sizeof g_playingWhat, "%s", g_saves[i]);
        snprintf(g_island, sizeof g_island, "%s", g_saves[i]);
        if (char *date = strstr(g_island, " 2026-"))
            *date = 0;
        g_spot = SpotOf(g_saves[i]);
    }
    Trace("load %s", g_playingWhat);
}

void Game_ResumeCurrent() {
    snprintf(g_playingWhat, sizeof g_playingWhat, "%s", g_currentWhat);
    snprintf(g_island, sizeof g_island, "%s", g_currentIsland);
    g_spot = g_currentSpot;
    Trace("resume %s", g_playingWhat);
}

int Game_Save(int over) { return SaveNow(over); }
void Game_DeleteSave(int i) { DeleteSave(i); }

// The pretend game counts as played the moment it shows: leaving it for another game loses
// something until the next save.
void Game_Play() {
    g_playing = true;
    g_inGame = true;
    g_unsaved = true;
    g_playFrom = Host_TimeMs();
    // Started, loaded or resumed: 1997 writes the current save then (CurrentSaveGame), and the
    // first scene change the autosave. Each keeps the screen as it is now.
    g_haveCurrent = g_haveAutosave = true;
    snprintf(g_currentWhat, sizeof g_currentWhat, "%s", g_playingWhat);
    snprintf(g_currentIsland, sizeof g_currentIsland, "%s", g_island);
    snprintf(g_autosaveIsland, sizeof g_autosaveIsland, "%s", g_island);
    g_currentSpot = g_autosaveSpot = LiveSpot();
}
bool Game_Playing() { return g_playing; }

void Game_MenuKey() {
    g_spot = LiveSpot();
    g_currentSpot = g_spot;
    snprintf(g_currentIsland, sizeof g_currentIsland, "%s", g_island);
    g_dimGame = true;
}

void Game_PageKey() {
    g_spot = LiveSpot();
    g_playing = false;
    g_dimGame = false;
}

void Game_Stop() { g_playing = false; }

void Game_NextScene() {
    g_spot = LiveSpot();
    g_playFrom = Host_TimeMs();
    if (++g_spot.scene % 3 == 0) {  // every third place, a boat to the next island
        g_spot.island = (g_spot.island + 1) % SAVE_PICTURE_ISLANDS;
        snprintf(g_island, sizeof g_island, "%s", kIslands[g_spot.island]);
    }
    g_autosaveSpot = g_spot;
    snprintf(g_autosaveIsland, sizeof g_autosaveIsland, "%s", g_island);
    Trace("scene %s %d", g_island, g_spot.scene);
}

void Game_Quit() {
    g_quitShown = true;
    Trace("quit chosen");
}
bool Game_QuitShown() { return g_quitShown; }
void Game_StartAgain() {
    g_quitShown = false;
    g_inGame = false;  // g_unsaved stays: current.lba keeps the play for Resume game
}

void Game_NoteSaved() {
    g_savedAt = Host_TimeMs();
    g_savedFromPlay = true;
}
bool Game_JustSaved() { return g_savedFromPlay && Host_TimeMs() - g_savedAt < 2500; }

GameSpot Game_Spot() { return LiveSpot(); }
bool Game_Dimmed() { return g_dimGame; }
GameSpot Game_CurrentSpot() { return g_currentSpot; }
GameSpot Game_SaveSpot(int save) { return save < 0 ? g_autosaveSpot : SpotOf(g_saves[save]); }

void Game_UnderMenu(bool inGame) {
    g_quitShown = false;
    if (inGame)
        g_inGame = true;
    g_dimGame = true;  // as Esc opens the game menu
}

void Game_UnderOptions() {
    g_playing = g_quitShown = false;
    g_inGame = true;
    g_dimGame = false;  // the Options key's: the scene as it is
}

