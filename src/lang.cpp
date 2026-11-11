#include "lang.h"

#include <string.h>

static const char *const kTable[][LANG_COUNT] = {
#include "lang_table.inc"
};
static const int kCount = (int)(sizeof kTable / sizeof kTable[0]);

const char *Lang_Tr(const char *english, int lang) {
    if (lang <= 0 || lang >= LANG_COUNT || !english)
        return english;
    for (int i = 0; i < kCount; i++)
        if (!strcmp(kTable[i][0], english))
            return kTable[i][lang];
    return english;
}

int Lang_Count() { return kCount; }

const char *Lang_Text(int i, int lang) { return kTable[i][lang]; }
