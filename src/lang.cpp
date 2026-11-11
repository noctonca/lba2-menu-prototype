#include "lang.h"

#include <string.h>

static const char *const kTable[][LANG_COUNT] = {
#include "lang_help.inc"
};
static const int kCount = (int)(sizeof kTable / sizeof kTable[0]);

static const char *const kLabels[][LANG_COUNT] = {
#include "lang_menu.inc"
};
static const int kLabelCount = (int)(sizeof kLabels / sizeof kLabels[0]);

const char *Lang_Find(const char *english, int lang) {
    if (lang < 0 || lang >= LANG_COUNT || !english)
        return nullptr;
    for (int i = 0; i < kCount; i++)
        if (!strcmp(kTable[i][0], english))
            return kTable[i][lang];
    return nullptr;
}

const char *Lang_Tr(const char *english, int lang) {
    if (lang <= 0)
        return english;
    const char *t = Lang_Find(english, lang);
    return t ? t : english;
}

const char *Lang_Label(const char *english, int lang) {
    if (lang < 0 || lang >= LANG_COUNT || !english)
        return nullptr;
    for (int i = 0; i < kLabelCount; i++)
        if (!strcmp(kLabels[i][0], english))
            return kLabels[i][lang];
    return nullptr;
}

int Lang_Count() { return kCount; }

const char *Lang_Text(int i, int lang) { return kTable[i][lang]; }

int Lang_LabelCount() { return kLabelCount; }

const char *Lang_LabelText(int i, int lang) { return kLabels[i][lang]; }
