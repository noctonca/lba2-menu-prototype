#include "ui.h"

#include "game_pieces.h"
#include "host.h"
#include "screen_font.h"

#include <SVGA/BOX.H>
#include <SVGA/CLIP.H>
#include <SVGA/FONT.H>
#include <SVGA/GRAPH.H>
#include <SVGA/FIL_LINE.H>
#include <SVGA/PLOT.H>
#include <SVGA/SCREEN.H>

#include "lang.h"

#include <set>
#include <string>
#include <stdio.h>
#include <string.h>

extern "C" U8 Font8x8[256 * 8];

void Ui_RowBackground(S32 x0, S32 y0, S32 x1, S32 y1, RowFill fill) {
    GamePieces_BackupCorners(x0, y0, x1, y1);
    switch (fill) {
    case FILL_CURSOR:
        GamePieces_FireBar(x0, y0, x1, y1, PLASMA_CURSOR);
        break;
    case FILL_CURRENT:
        GamePieces_FireBar(x0, y0, x1, y1, PLASMA_CURRENT);
        break;
    default:
        GamePieces_Shade(x0, y0, x1, y1, MENU_SHADE_LVL);
        break;
    }
    GamePieces_Frame(x0, y0, x1, y1);
}

void Ui_Panel(S32 x0, S32 y0, S32 x1, S32 y1, S32 level) {
    GamePieces_BackupCorners(x0, y0, x1, y1);
    GamePieces_Shade(x0, y0, x1, y1, level);
    GamePieces_Frame(x0, y0, x1, y1);
}

// The game-font texts are drawn and measured in the Text language: one place, so every
// width the layout and the checks take is the translated one. A text with letters that
// lang/menu.tsv lacks is drawn as it is and noted for the self test.
static int s_lang;
static std::set<std::string> s_missed;

void Ui_SetLang(int lang) { s_lang = lang; }
int Ui_Lang() { return s_lang; }

// What a translation has nothing to do with: no letters, or a number first (a
// resolution, the version).
static bool HasLetters(const char *s) {
    if (*s >= '0' && *s <= '9')
        return false;
    for (; *s; s++)
        if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z'))
            return true;
    return false;
}

static const char *TrWhole(const char *s) {
    if (const char *t = Lang_Label(s, s_lang))
        return t;
    if (HasLetters(s))
        s_missed.insert(s);
    return s;
}

const char *Ui_Tr(const char *s) {
    if (s_lang <= 0 || !s)
        return s;
    if (const char *t = Lang_Label(s, s_lang))
        return t;
    // A marked text, "Name (current)", "Name (changed)" or "1920 x 1080 (native)": the text and the mark each
    // on its own.
    static const char *const kMarks[] = {"(current)", "(changed)", "(native)"};
    const size_t n = strlen(s);
    for (const char *mark : kMarks) {
        const size_t m = strlen(mark);
        if (n > m + 1 && s[n - m - 1] == ' ' && !strcmp(s + n - m, mark)) {
            static char marked[4][256];
            static int at;
            char *out = marked[at++ & 3];
            std::string name(s, n - m - 1);
            snprintf(out, sizeof marked[0], "%s %s", Ui_Tr(name.c_str()), TrWhole(mark));
            return out;
        }
    }
    // A one-column row, "Label: Value": each side on its own.
    const char *colon = strstr(s, ": ");
    if (!colon)
        return TrWhole(s);
    static char ring[8][256];
    static int next;
    char *out = ring[next++ & 7];
    std::string label(s, colon - s);
    snprintf(out, sizeof ring[0], "%s: %s", TrWhole(label.c_str()), TrWhole(colon + 2));
    return out;
}

const std::set<std::string> &Ui_Missed() { return s_missed; }
void Ui_ClearMissed() { s_missed.clear(); }

S32 Ui_TextWidthAsIs(const char *s) { return ScreenFont_On() ? ScreenFont_Width(s) : SizeFont((char *)s); }

S32 Ui_TextInkWidthAsIs(const char *s) {
    if (ScreenFont_On())
        return ScreenFont_InkWidth(s);
    return s[0] ? SizeFont((char *)s) - InterLeave : 0;
}

void Ui_TextAsIs(S32 x, S32 y, const char *s, U8 colour) {
    if (ScreenFont_On()) {
        ScreenFont_Text(x, y, s, colour);
        return;
    }
    ColorFont(colour);
    Font(x, y, s);
}

// The 1997 swing (GAMEMENU.CPP DrawOneChoice, "Shake Baby, shake..."): text wider than its
// room is clipped to it, and under the cursor it slides left until its end shows, then back,
// at 48 px a second (InitMove(&MoveChoice, 48)), from the start each time the text changes.
// A fallback: the self test still reports text that does not fit.
enum { SWING_PX_PER_S = 48 };
static char s_swingText[256];
static U32 s_swingStart;

S32 Ui_TextInRoom(S32 x, S32 right, S32 y, const char *s, U8 colour, bool swing) {
    S32 w = Ui_TextWidthAsIs(s), room = right - x + 1;
    if (w <= room) {
        Ui_TextAsIs(x, y, s, colour);
        return w;
    }
    S32 off = 0;
    if (swing) {
        if (strcmp(s, s_swingText) != 0) {
            snprintf(s_swingText, sizeof s_swingText, "%s", s);
            s_swingStart = Host_TimeMs();
        }
        S32 travel = w - room;
        S32 d = (S32)((Host_TimeMs() - s_swingStart) * SWING_PX_PER_S / 1000) % (2 * travel);
        off = d <= travel ? -d : -(2 * travel - d);
    }
    const ClipSave clip;
    SetClip(x > ClipXMin ? x : ClipXMin, ClipYMin, right < ClipXMax ? right : ClipXMax, ClipYMax);
    Ui_TextAsIs(x + off, y, s, colour);
    clip.Restore();
    return w;
}

S32 Ui_TextWidth(const char *s) { return Ui_TextWidthAsIs(Ui_Tr(s)); }
S32 Ui_TextInkWidth(const char *s) { return Ui_TextInkWidthAsIs(Ui_Tr(s)); }
void Ui_Text(S32 x, S32 y, const char *s, U8 colour) { Ui_TextAsIs(x, y, Ui_Tr(s), colour); }

void Ui_TextShadow(S32 x, S32 y, const char *s, U8 colour) {
    Ui_Text(x + 2, y + 4, s, INK_BLACK);
    Ui_Text(x, y, s, colour);
}

S32 Ui_Title(S32 x0, S32 x1, S32 y, const char *s, bool asIs) {
    if (!asIs)
        s = Ui_Tr(s);
    S32 x = (x0 + x1 + 1) / 2 - Ui_TextWidthAsIs(s) / 2;
    Ui_TextAsIs(x + 4, y + 4, s, INK_BLACK);
    Ui_TextAsIs(x, y, s, INK_WHITE);
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
            GamePieces_FireBar(x0 + 1, y0 + 1, x1 - 1, y1 - 1, PLASMA_SLIDER);
        else
            GamePieces_Shade(x0 + 1, y0 + 1, x1 - 1, y1 - 1, 4);
        Line(x0, y0, x1, y0, COUL_UP_BLUE);
        Line(x0, y0, x0, y1, COUL_UP_BLUE);
        Line(x1, y0 + 1, x1, y1, COUL_DOWN_BLUE);
        Line(x0 + 1, y1, x1, y1, COUL_DOWN_BLUE);
    }
}

S32 Ui_SmallWidth(const char *s, S32 scale) {
    return ScreenFont_On() ? ScreenFont_SmallWidth(s, 7 * scale) : Ui_Font8Width(s, scale);
}

void Ui_Small(S32 x, S32 y, const char *s, U8 colour, S32 scale) {
    if (ScreenFont_On())
        ScreenFont_SmallText(x, y, s, colour, 7 * scale);
    else
        Ui_Font8(x, y, s, colour, scale);
}

S32 Ui_Font8Width(const char *s, S32 scale) {
    return (S32)strlen(s) * 8 * scale;
}

void Ui_Font8(S32 x, S32 y, const char *s, U8 colour, S32 scale) {
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
    if (const char *and_ = strchr(key, CHIP_AND[0])) {
        std::string first(key, and_ - key);
        return Ui_ChipWidth(first.c_str(), scale) + CHIP_AND_GAP * scale + Ui_ChipWidth(and_ + 1, scale);
    }
    return Ui_SmallWidth(key, scale) + 8 * scale;
}

S32 Ui_Chip(S32 x, S32 y, const char *key, S32 scale, bool lit) {
    if (const char *and_ = strchr(key, CHIP_AND[0])) {
        std::string first(key, and_ - key);
        S32 w = Ui_Chip(x, y, first.c_str(), scale, lit) + CHIP_AND_GAP * scale;
        return w + Ui_Chip(x + w, y, and_ + 1, scale, lit);
    }
    S32 w = Ui_ChipWidth(key, scale), h = 8 * scale + 4 * scale;
    S32 x1 = x + w - 1, y1 = y + h - 1;
    if (lit)
        GamePieces_FireBar(x + 1, y + 1, x1 - 1, y1 - 1, PLASMA_CURRENT);
    else
        GamePieces_Shade(x + 1, y + 1, x1 - 1, y1 - 1, MENU_SHADE_LVL);
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
        GamePieces_FireBar(x + 1, y + 1, x1 - 1, y1 - 1, PLASMA_CURSOR);
    else if (fill == CHIP_WAITING)
        GamePieces_FireBar(x + 1, y + 1, x1 - 1, y1 - 1, PLASMA_CURRENT);
    else
        GamePieces_Shade(x + 1, y + 1, x1 - 1, y1 - 1, MENU_SHADE_LVL);
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
    const ClipSave clip;
    SetClip(x + 2, y + 1, x1 - 2, y1 - 1);
    Ui_Small(tx, y + 2 * scale, text, ink, scale);
    clip.Restore();
    return tw <= inner;
}

void Voice_Say(Voice &v, const char *text, bool typed) {
    if (strcmp(v.text, text) == 0 && v.typed == typed)
        return;
    strncpy(v.text, text, sizeof v.text - 1);
    v.text[sizeof v.text - 1] = 0;
    v.start = Host_TimeMs();
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
            if (Ui_TextWidthAsIs(buf) > width)  // the voice: already in the Text language
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

static const S32 s_msPerWord = 300;  // how long a window of help is held, per word
static S32 s_lastPage, s_lastPages;


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
    S32 elapsed = (S32)(Host_TimeMs() - v.start);
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
            if (ScreenFont_On()) {
                if (c != ' ')
                    ScreenFont_Char(x + 2, y + 4, (U8)c, INK_BLACK);
                x += ScreenFont_Char(x, y, (U8)c, (U8)(DIALOGUE_HERO * 16 + age));
                continue;
            }
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
