#include "ui.h"

#include "cadre.h"
#include "engine.h"

#include <SVGA/BOX.H>
#include <SVGA/CLIP.H>
#include <SVGA/FONT.H>
#include <SVGA/GRAPH.H>
#include <SVGA/FIL_LINE.H>
#include <SVGA/PLOT.H>
#include <SVGA/SCREEN.H>

#include <string.h>

extern "C" U8 Font8x8[256 * 8];

void Ui_RowBackground(S32 x0, S32 y0, S32 x1, S32 y1, RowFill fill) {
    Cadre_BackupCorners(x0, y0, x1, y1);
    switch (fill) {
    case FILL_CURSOR:
        Cadre_FireBar(x0, y0, x1, y1, PLASMA_CURSOR);
        break;
    case FILL_CURRENT:
        Cadre_FireBar(x0, y0, x1, y1, PLASMA_CURRENT);
        break;
    default:
        Cadre_Shade(x0, y0, x1, y1, MENU_SHADE_LVL);
        break;
    }
    Cadre_Frame(x0, y0, x1, y1);
}

void Ui_Panel(S32 x0, S32 y0, S32 x1, S32 y1, S32 level) {
    Cadre_BackupCorners(x0, y0, x1, y1);
    Cadre_Shade(x0, y0, x1, y1, level);
    Cadre_Frame(x0, y0, x1, y1);
}

S32 Ui_TextWidth(const char *s) {
    return SizeFont((char *)s);
}

S32 Ui_TextInkWidth(const char *s) {
    return s[0] ? SizeFont((char *)s) - InterLeave : 0;
}

void Ui_Text(S32 x, S32 y, const char *s, U8 colour) {
    ColorFont(colour);
    Font(x, y, s);
}

void Ui_TextShadow(S32 x, S32 y, const char *s, U8 colour) {
    ColorFont(INK_BLACK);
    Font(x + 2, y + 4, s);
    ColorFont(colour);
    Font(x, y, s);
}

S32 Ui_Title(S32 x0, S32 x1, S32 y, const char *s) {
    S32 x = (x0 + x1 + 1) / 2 - Ui_TextWidth(s) / 2;
    ColorFont(INK_BLACK);
    Font(x + 4, y + 4, s);
    ColorFont(INK_WHITE);
    Font(x, y, s);
    return y + 56;  // the title takes one row's pitch
}

static void Triangle(S32 tipX, S32 yc, S32 dir, U8 colour) {
    // 10 wide, 19 tall, pointing left (dir -1) or right (dir +1): about the
    // weight of the corner arrow sprites at the game font's size.
    for (S32 i = 0; i < 10; i++) {
        S32 x = tipX - dir * i;
        Line(x, yc - i, x, yc + i, colour);
    }
}

void Ui_ChangeMarks(S32 leftX, S32 rightX, S32 yc) {
    // Black first, offset like the dialogue shadow at a smaller step.
    Triangle(leftX + 1, yc + 2, -1, INK_BLACK);
    Triangle(rightX + 1, yc + 2, +1, INK_BLACK);
    Triangle(leftX, yc, -1, INK_WHITE);
    Triangle(rightX, yc, +1, INK_WHITE);
}

void Ui_Steps(S32 xRight, S32 yc, S32 count, S32 lit, S32 *outLeft) {
    const S32 w = 22, h = 20, gap = 6;
    S32 x = xRight + 1 - count * w - (count - 1) * gap;
    if (outLeft)
        *outLeft = x;
    for (S32 i = 0; i < count; i++) {
        S32 x0 = x + i * (w + gap), y0 = yc - h / 2;
        S32 x1 = x0 + w - 1, y1 = y0 + h - 1;
        if (i < lit)  // GAMEMENU.CPP: the slider plasma, as the 1997 slider fill
            Cadre_FireBar(x0 + 1, y0 + 1, x1 - 1, y1 - 1, PLASMA_SLIDER);
        else
            Cadre_Shade(x0 + 1, y0 + 1, x1 - 1, y1 - 1, 4);
        Line(x0, y0, x1, y0, COUL_UP_BLUE);
        Line(x0, y0, x0, y1, COUL_UP_BLUE);
        Line(x1, y0 + 1, x1, y1, COUL_DOWN_BLUE);
        Line(x0 + 1, y1, x1, y1, COUL_DOWN_BLUE);
    }
}

void Ui_MiniSlider(S32 x0, S32 x1, S32 yc, S32 value, S32 max, bool dim) {
    const S32 h = 20;
    S32 y0 = yc - h / 2, y1 = y0 + h - 1;
    S32 fill = x0 + 1 + (x1 - x0 - 2) * value / (max ? max : 1);
    if (fill > x0 + 1) {
        if (dim)
            Box(x0 + 1, y0 + 1, fill, y1 - 1, INK_DISABLED);
        else
            Cadre_FireBar(x0 + 1, y0 + 1, fill, y1 - 1, PLASMA_SLIDER);
    }
    if (fill < x1 - 1) {
        if (dim)  // the empty track dark, as an unlit step
            Cadre_Shade(fill + 1, y0 + 1, x1 - 1, y1 - 1, 4);
        else
            Box(fill + 1, y0 + 1, x1 - 1, y1 - 1, FILL_SELECTED);
    }
    Line(x0, y0, x1, y0, COUL_UP_BLUE);
    Line(x0, y0, x0, y1, COUL_UP_BLUE);
    Line(x1, y0 + 1, x1, y1, COUL_DOWN_BLUE);
    Line(x0 + 1, y1, x1, y1, COUL_DOWN_BLUE);
}

S32 Ui_SmallWidth(const char *s, S32 scale) {
    return (S32)strlen(s) * 8 * scale;
}

void Ui_Small(S32 x, S32 y, const char *s, U8 colour, S32 scale) {
    U8 *log = (U8 *)Log;
    for (; *s; s++, x += 8 * scale) {
        const U8 *glyph = &Font8x8[(U8)*s * 8];
        for (S32 row = 0; row < 8; row++) {
            U8 bits = glyph[row];
            for (S32 col = 0; col < 8; col++) {
                if (!(bits & (0x80 >> col)))
                    continue;
                for (S32 dy = 0; dy < scale; dy++) {
                    S32 py = y + row * scale + dy;
                    if (py < ClipYMin || py > ClipYMax)
                        continue;
                    for (S32 dx = 0; dx < scale; dx++) {
                        S32 px = x + col * scale + dx;
                        if (px >= ClipXMin && px <= ClipXMax)
                            log[TabOffLine[py] + px] = colour;
                    }
                }
            }
        }
    }
}

S32 Ui_ChipWidth(const char *key, S32 scale) {
    return Ui_SmallWidth(key, scale) + 8 * scale;
}

S32 Ui_Chip(S32 x, S32 y, const char *key, S32 scale, bool lit) {
    S32 w = Ui_ChipWidth(key, scale), h = 8 * scale + 4 * scale;
    S32 x1 = x + w - 1, y1 = y + h - 1;
    if (lit)
        Cadre_FireBar(x + 1, y + 1, x1 - 1, y1 - 1, PLASMA_CURRENT);
    else
        Cadre_Shade(x + 1, y + 1, x1 - 1, y1 - 1, MENU_SHADE_LVL);
    Line(x, y, x1, y, COUL_UP_BLUE);
    Line(x, y, x, y1, COUL_UP_BLUE);
    Line(x1, y + 1, x1, y1, COUL_DOWN_BLUE);
    Line(x + 1, y1, x1, y1, COUL_DOWN_BLUE);
    Ui_Small(x + 4 * scale, y + 2 * scale, key, lit ? INK_WHITE : INK_CONFIG_MENU, scale);
    return w;
}

S32 Ui_KeyChipHeight(S32 scale) {
    return 8 * scale + 4 * scale;
}

bool Ui_KeyChip(S32 x, S32 y, S32 w, const char *name, S32 scale, ChipFill fill, bool red) {
    S32 h = Ui_KeyChipHeight(scale);
    S32 x1 = x + w - 1, y1 = y + h - 1;
    if (fill == CHIP_CURSOR)
        Cadre_FireBar(x + 1, y + 1, x1 - 1, y1 - 1, PLASMA_CURSOR);
    else if (fill == CHIP_WAITING)
        Cadre_FireBar(x + 1, y + 1, x1 - 1, y1 - 1, PLASMA_CURRENT);
    else
        Cadre_Shade(x + 1, y + 1, x1 - 1, y1 - 1, MENU_SHADE_LVL);
    U8 up = red ? COUL_UP_RED : COUL_UP_BLUE, down = red ? COUL_DOWN_RED : COUL_DOWN_BLUE;
    Line(x, y, x1, y, up);
    Line(x, y, x, y1, up);
    Line(x1, y + 1, x1, y1, down);
    Line(x + 1, y1, x1, y1, down);

    bool empty = !name || !name[0];
    const char *text = empty ? "......" : name;
    S32 inner = w - 8 * scale;
    S32 tw = Ui_SmallWidth(text, scale);
    S32 tx = tw <= inner ? x + w / 2 - tw / 2 : x + 4 * scale;
    U8 ink = fill != CHIP_SHADE ? INK_WHITE : empty ? INK_DISABLED : INK_CONFIG_MENU;
    MemoClip();
    SetClip(x + 2, y + 1, x1 - 2, y1 - 1);
    Ui_Small(tx, y + 2 * scale, text, ink, scale);
    RestoreClip();
    return tw <= inner;
}

void Voice_Say(Voice &v, const char *text, bool typed) {
    if (strcmp(v.text, text) == 0 && v.typed == typed)
        return;
    strncpy(v.text, text, sizeof v.text - 1);
    v.text[sizeof v.text - 1] = 0;
    v.start = Engine_TimeMs();
    v.typed = typed;
}

// Word wrap into lines of the game font, as the dialogue does.
static S32 Wrap(const char *text, S32 width, const char **starts, S32 *lens, S32 maxLines) {
    S32 lines = 0;
    const char *p = text;
    while (*p && lines < maxLines) {
        while (*p == ' ')
            p++;
        const char *lineStart = p, *lastBreak = NULL;
        char buf[512];
        S32 n = 0;
        while (p[n]) {
            memcpy(buf, lineStart, n + 1);
            buf[n + 1] = 0;
            if (SizeFont(buf) > width)
                break;
            if (p[n] == ' ')
                lastBreak = p + n;
            n++;
        }
        S32 len = n;
        if (p[n] && lastBreak)
            len = (S32)(lastBreak - lineStart);
        starts[lines] = lineStart;
        lens[lines] = len;
        lines++;
        p = lineStart + len;
    }
    return lines;
}

S32 Voice_Lines(const char *text, S32 width) {
    const char *starts[VOICE_MAX_LINES];
    S32 lens[VOICE_MAX_LINES];
    return Wrap(text, width, starts, lens, VOICE_MAX_LINES);
}

static S32 s_msPerWord = 300;
static S32 s_lastPage, s_lastPages;

void Voice_SetMsPerWord(S32 ms) { s_msPerWord = ms; }

void Voice_LastPage(S32 *page, S32 *pages) {
    *page = s_lastPage;
    *pages = s_lastPages;
}

// Letters are typed one per 20 ms; a letter takes 13 steps of the colour ramp to settle.
enum { MS_PER_LETTER = 20, RAMP_STEPS = 13, MIN_HOLD_MS = 1500 };

static S32 HoldMs(const char *const *starts, const S32 *lens, S32 first, S32 count) {
    S32 words = 0;
    for (S32 l = first; l < first + count; l++)
        for (S32 i = 0; i < lens[l]; i++)
            if (starts[l][i] != ' ' && (i == 0 || starts[l][i - 1] == ' '))
                words++;
    S32 ms = words * s_msPerWord;
    return ms < MIN_HOLD_MS ? MIN_HOLD_MS : ms;
}

S32 Voice_Draw(const Voice &v, S32 x0, S32 y0, S32 width, S32 maxLines, S32 *needed) {
    const char *starts[VOICE_MAX_LINES];
    S32 lens[VOICE_MAX_LINES];
    if (maxLines > VOICE_MAX_LINES)
        maxLines = VOICE_MAX_LINES;
    if (maxLines < 1)
        maxLines = 1;
    S32 all = Wrap(v.text, width, starts, lens, VOICE_MAX_LINES);
    if (needed)
        *needed = all;

    // Which window is showing, and since when: each is typed (when the voice types),
    // then held for its words, then the next begins. The last one stays.
    S32 pages = all > 0 ? (all + maxLines - 1) / maxLines : 1;
    S32 elapsed = (S32)(Engine_TimeMs() - v.start);
    S32 page = 0;
    for (; page + 1 < pages; page++) {
        S32 first = page * maxLines;
        S32 count = all - first < maxLines ? all - first : maxLines;
        S32 letters = 0;
        for (S32 l = first; l < first + count; l++)
            letters += lens[l];
        S32 span = (v.typed ? (letters + RAMP_STEPS) * MS_PER_LETTER : 0) + HoldMs(starts, lens, first, count);
        if (elapsed < span)
            break;
        elapsed -= span;
    }
    s_lastPage = page;
    s_lastPages = pages;

    S32 first = page * maxLines;
    S32 lines = all - first < maxLines ? all - first : maxLines;
    if (lines < 0)
        lines = 0;

    // Letters shown so far in this window: one per 20 ms. Spaces count, as in the dialogue.
    S32 total = 0;
    for (S32 i = first; i < first + lines; i++)
        total += lens[i];
    S32 shown = v.typed ? elapsed / MS_PER_LETTER : total + RAMP_STEPS;

    S32 index = 0;
    for (S32 l = 0; l < lines; l++) {
        S32 x = x0, y = y0 + l * 38;
        const char *line = starts[first + l];
        for (S32 i = 0; i < lens[first + l]; i++, index++) {
            if (index >= shown)
                return lines;
            char c = line[i];
            S32 age = shown - 1 - index;
            if (age > RAMP_STEPS - 1)
                age = RAMP_STEPS - 1;
            if (c != ' ') {
                ColorFont(INK_BLACK);
                CarFont(x + 2, y + 4, (U8)c);
                ColorFont(DIALOGUE_HERO * 16 + age);
            }
            x += CarFont(x, y, (U8)c);
        }
    }
    return lines;
}
