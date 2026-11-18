// The settings the Options rows show and change, and the presets over them.
//
// This is the menu's one door to the values. Here they are examples held in memory
// (menu_settings.cpp); in lba2cc the same calls go to its settings tables, through
// Settings_Apply, and to the menu's own table of ring, presets-set and switch fields
// (docs/plan/MENU_PLAN.md). The loop, the rows and the drawing in menu.cpp read and change
// a setting only through these, so carrying them over changes this file's other side alone.
#pragma once

#include "more_options.h"

#include <stddef.h>

enum SettingKind { K_CHOICE, K_TOGGLE, K_STEPS, K_SLIDER };

struct Setting {
    const char *id;
    const char *label;
    SettingKind kind;
    int max;                 // last value (choices: count - 1)
    const char *names[10];   // value names for choices, toggles and steps
    int fresh;               // what a fresh install has (= Modern)
    int classic;             // what Classic sets, when presetSets
    bool presetSets;
    const char *help;
};

enum {
    S_TEXT_LANG, S_VOICE_LANG, S_SUBTITLES, S_BACKGROUND, S_FONT,
    S_RESOLUTION, S_FULLSCREEN, S_VSYNC, S_FPS, S_IMAGE,
    S_MASTER, S_SFX, S_VOICES, S_MUSIC, S_REVERSE,
    S_PADNAMES, S_CAM_OUT, S_CAM_IN, S_MOVIE_CAM, S_INVERT, S_SENS,
    S_FIXBUGS,
    S_COUNT
};

const Setting &Settings_Def(int s);
int Settings_Value(int s);
// Sets a value as it is given (a slider's drag, a script's `set`); false when out of its range.
bool Settings_Put(int s, int value);
// Left (-1) or Right (+1) on its row: a choice goes round, a slider stops at its ends. The
// Resolution and Fullscreen rows change the screen itself.
void Settings_Step(int s, int dir);
// What its row shows: the value's name, a slider's number, the screen in use.
const char *Settings_ValueText(int s, char *buf, size_t n);
// "Changed is a colour": it differs from a fresh install or, for a setting presets set, from
// the chosen preset.
bool Settings_Changed(int s);
// Every value as a fresh install has it, on Modern.
void Settings_Fresh();
// The value itself, for the prototype's switch that steps a row from a function key (padnames).
int *Settings_Slot(int s);

// The presets: 0 Modern, 1 Classic.
int Presets_Chosen();
const char *Presets_Name(int preset);
bool Presets_Changed();  // the player's values differ from the chosen preset's
// Keeps a settings backup first (Undo preset), then sets every value presets set.
void Presets_Apply(int preset);
// Every setting but the languages as a fresh install has it, every More options row, the
// bindings and the preset, after a backup (Undo reset).
void Settings_ResetAll();

// The two settings backups and their undo rows: a preset's, and Reset settings'.
enum { BACK_PRESET, BACK_RESET };
enum { BACKUP_DIFF_MAX = 1 + S_COUNT + 1 + MORE_OPTIONS_MAX };
// What the undo would change, in the order its help names it (names holds BACKUP_DIFF_MAX);
// 0: nothing, and its row is not offered.
int Presets_UndoNames(int which, const char **names);
bool Presets_UndoOffered(int which);
void Presets_Undo(int which);
void Presets_ForgetBackups();  // as a fresh start has it
