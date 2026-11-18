// The pages as data: the row tables of Options and of the game menu, Load game and Save game.
#include "menu.h"

#include "menu_internal.h"

#include <stdio.h>

// What a build off main says: lba2cc's LBA2_VERSION_STRING and the commit's first 7. A release
// says its number alone. An example; the prototype has no build of its own to name.
const char *kVersion = "0.13.3-dev 011394f";
const char *kVersionString = "0.13.3-dev";  // LBA2_VERSION_STRING alone, as the log's first line has it
const char *kCommit = "011394fbc247";

// The preset row: "Preset: Modern", the page's own name (2026-11-11, after playing on the
// Xbox: "Modern settings" read as a page of settings, not the preset).
const char *FormatPresetRow(char *buf, size_t n, const char *name, bool changed) {
    snprintf(buf, n, "Preset: %s%s", name, changed ? ", changed" : "");
    return buf;
}

static bool UndoPresetOffered() { return Presets_UndoOffered(BACK_PRESET); }
static bool UndoResetOffered() { return Presets_UndoOffered(BACK_RESET); }
int Menu_ButtonNames() { return Settings_Value(S_PADNAMES); }
static bool AutoCamera() { return Settings_Value(S_CAM_OUT) == 1; }  // Invert and speed only mean something with it

#define SETTING_ROW(s) {R_SETTING, NULL, s, NULL, NULL, NULL, NULL}
#define AUTO_CAMERA_ROW(s) {R_SETTING, NULL, s, NULL, NULL, AutoCamera, "Only with the Auto camera."}
#define RETURN_ROW {R_RETURN, "Return to previous menu", 0, "", NULL, NULL, NULL}

static const Row kHubRows[] = {
    {R_RETURN, "Return to the game", 0, "Back to where you were.", NULL, NULL, NULL},
    {R_PRESET_ROW, "", P_PRESET, "As in 1997, or today's look with bugs fixed.", NULL, NULL, NULL},
    {R_CATEGORY, "Language and interface", P_LANG, "Text, voices, the font and the background.", NULL, NULL, NULL},
    {R_CATEGORY, "Display and graphics", P_DISPLAY, "Resolution, window and detail.", NULL, NULL, NULL},
    {R_CATEGORY, "Sound", P_SOUND, "Volumes.", NULL, NULL, NULL},
    {R_CATEGORY, "Controls and camera", P_CONTROLS, "Keys, pad and the camera.", NULL, NULL, NULL},
    {R_CATEGORY, "Advanced and reset", P_ADVANCED, "The rules switch, experiments and a fresh start.", NULL, NULL,
     NULL},
};

static const Row kPresetRows[] = {
    RETURN_ROW,
    {R_PRESET_CHOICE, "Modern", 0, "Today's look, cameras that follow, bugs fixed.", NULL, NULL, NULL},
    {R_PRESET_CHOICE, "Classic", 1, "The 1997 look and cameras, bugs and all.", NULL, NULL, NULL},
    {R_BACK_TO_MINE, "Undo preset", BACK_PRESET, "Puts back the settings you had before you chose a preset.",
     UndoPresetOffered, NULL, NULL},
};

static const Row kLangRows[] = {
    RETURN_ROW, SETTING_ROW(S_TEXT_LANG), SETTING_ROW(S_VOICE_LANG), SETTING_ROW(S_SUBTITLES),
    SETTING_ROW(S_BACKGROUND), SETTING_ROW(S_FONT),
};

static const Row kDisplayRows[] = {
    RETURN_ROW, SETTING_ROW(S_RESOLUTION), SETTING_ROW(S_FULLSCREEN), SETTING_ROW(S_VSYNC),
    SETTING_ROW(S_FPS), SETTING_ROW(S_IMAGE),
};

static const Row kSoundRows[] = {
    RETURN_ROW, SETTING_ROW(S_MASTER), SETTING_ROW(S_SFX), SETTING_ROW(S_VOICES), SETTING_ROW(S_MUSIC),
    SETTING_ROW(S_REVERSE),
};

static const Row kControlsRows[] = {
    RETURN_ROW,
    {R_ACTION, "Keys and buttons", A_KEYS, "Change which key or button does what.", NULL, NULL, NULL},
    SETTING_ROW(S_PADNAMES),
    SETTING_ROW(S_CAM_OUT),
    SETTING_ROW(S_CAM_IN),
    SETTING_ROW(S_MOVIE_CAM),
    AUTO_CAMERA_ROW(S_INVERT),
    AUTO_CAMERA_ROW(S_SENS),
};

static const Row kAdvancedRows[] = {
    RETURN_ROW,
    SETTING_ROW(S_FIXBUGS),
    // Always offered (decided 2026-11-03): its help carries the warning a switch once did.
    // The page holds every console setting without a row of its own: tuning for finished
    // features and the few unfinished ones, which it marks experimental.
    // Help within 750 px (2026-11-12); an experimental row's own help says it is experimental.
    {R_ACTION, "More options", A_EXP, "Tuning and 1997 extras, in English.", NULL, NULL, NULL},
    // The row's value is the version, so a bug report can name the build from the menu.
    {R_ACTION, "Console", A_CONSOLE, "Messages, and commands from a keyboard.", NULL, NULL, NULL},
    {R_ACTION, "Reset settings", A_RESET, "Puts every setting back as a fresh install has it.", NULL, NULL, NULL},
    {R_BACK_TO_MINE, "Undo reset", BACK_RESET, "Puts back what you had before the reset.", UndoResetOffered, NULL,
     NULL},
};

// A fixture: today's Options page from the boot menu, to diff against lba2cc's
// `ui menu-main-options` capture.
static const Row kOriginalRows[] = {
    RETURN_ROW,
    {R_ACTION, "Display", 0, "", NULL, NULL, NULL},
    {R_ACTION, "Sound volume", 0, "", NULL, NULL, NULL},
    {R_ACTION, "Choose language", 0, "", NULL, NULL, NULL},
    {R_ACTION, "Advanced options", 0, "", NULL, NULL, NULL},
    {R_ACTION, "Controls", 0, "", NULL, NULL, NULL},
};

// The 1997 main menu (GAMEMENU.CPP RealGameMainMenu, texts 70 to 75), each row there only when
// BuildGameMainMenu would show it. No help: each row's name says what it does (docs/UX.md,
// "Rows that say what they do"); the voice speaks only when something would be lost.
static const Row kMainRows[] = {
    {R_GAME, "Resume game", M_RESUME, "", Game_CanResume, NULL, NULL},
    {R_GAME, "New game", M_NEW, "", NULL, NULL, NULL},
    {R_GAME, "Load game", M_LOAD, "", Game_HaveSaves, NULL, NULL},
    {R_GAME, "Save game", M_SAVE, "", Game_InProgress, NULL, NULL},
    {R_GAME, "Options", M_OPTIONS, "", NULL, NULL, NULL},
    {R_GAME, "Quit", M_QUIT, "", NULL, NULL, NULL},
};

#define SAVE_ROW(i) {R_SAVE, NULL, i, "", NULL, NULL, NULL}
#define SAVE_ROWS_8(i) SAVE_ROW(i), SAVE_ROW(i + 1), SAVE_ROW(i + 2), SAVE_ROW(i + 3), SAVE_ROW(i + 4), \
    SAVE_ROW(i + 5), SAVE_ROW(i + 6), SAVE_ROW(i + 7)
#define SAVE_ROWS SAVE_ROWS_8(0), SAVE_ROWS_8(8), SAVE_ROWS_8(16), SAVE_ROWS_8(24)
// 1997's order: the named saves, then AUTOSAVE in Load game and the new save's row in Save game.
static const Row kLoadRows[] = {SAVE_ROWS, {R_AUTOSAVE, NULL, 0, "", Game_HaveAutosave, NULL, NULL}};
static const Row kSaveRows[] = {SAVE_ROWS, {R_NEW_SAVE, "New save", 0, "", NULL, NULL, NULL}};
static_assert(sizeof kLoadRows / sizeof kLoadRows[0] == SAVES_MAX + 1, "a row for every save, and the autosave");
#undef SAVE_ROWS
#undef SAVE_ROWS_8
#undef SAVE_ROW

// The Options pages; Keys and buttons and its groups are filled in from bindings.cpp's tables.
#define PAGE(name, rows) {name, rows, (int)(sizeof rows / sizeof rows[0])}
Page g_pages[P_COUNT] = {
    PAGE("Options", kHubRows),
    PAGE("Preset", kPresetRows),
    PAGE("Language and interface", kLangRows),
    PAGE("Display and graphics", kDisplayRows),
    PAGE("Sound", kSoundRows),
    PAGE("Controls and camera", kControlsRows),
    PAGE("Advanced and reset", kAdvancedRows),
    {"original", kOriginalRows, (int)(sizeof kOriginalRows / sizeof kOriginalRows[0]), NULL, true},
};
const Page kGamePages[3] = {PAGE("Game menu", kMainRows), PAGE("Load game", kLoadRows), PAGE("Save game", kSaveRows)};
#undef PAGE

