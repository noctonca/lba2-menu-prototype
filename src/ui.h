// The design's vocabulary, drawn with the original's pieces.
#pragma once

#include <SYSTEM/ADELINE_TYPES.H>
#include <SVGA/CLIP.H>

#include <set>
#include <string>

// Palette indices (design system tokens.json, lba2 theme).
enum {
    INK_BLACK = 0,
    INK_WHITE = 15,             // LBAWHITE, COUL_TEXT_MENU
    INK_DISABLED = 3 * 16 + 11, // COUL_TEXT_MENU_DISABLE
    INK_COUNT = 158,            // changed values: bank 9, three steps brighter than the
                                // design's 155 so it holds 3:1 on a shaded row
    FILL_SELECTED = 166,        // COUL_SELECT_MENU
    INK_CONFIG_HEAD = 45,       // 8x8 headers, status line
    INK_CONFIG_MENU = 174,      // 8x8 menu entries, legend
    INK_CONFIG_TEXT = 203,
    DIALOGUE_HERO = 12,         // hero bank: 192..204
};

enum RowFill { FILL_SHADE, FILL_CURSOR, FILL_CURRENT };

// Menu row height and gap (GAMEMENU.CPP HAUTEUR_STANDARD, MENU_SPACE). A row spans
// its centre +-25, so it is ROW_H + 1 pixels tall: y0 .. y0 + ROW_H.
enum { ROW_H = 50, ROW_GAP = 6, ROW_PITCH = 56 };

void Ui_RowBackground(S32 x0, S32 y0, S32 x1, S32 y1, RowFill fill);
// Frame + darkened inside: PANEL_SHADE_LVL, the 1997 dialogue window's level, or
// BAND_SHADE_LVL for the wide band before 2026-11-03 (the rows' MENU_SHADE_LVL);
// the band now takes 15 (switch bandshade), which holds 3:1 over the foam too.
enum { BAND_SHADE_LVL = 11 };
void Ui_Panel(S32 x0, S32 y0, S32 x1, S32 y1, S32 level);

// Game-font text in the Text language (Ui_SetLang): s is the English, looked up in
// lang/menu.tsv; "Label: Value" is looked up a side at a time. A text with letters the
// table lacks is drawn as it is and noted (Ui_Missed). The AsIs forms take s as it is:
// the voice, already translated, and More options' tuning rows, English by design.
void Ui_SetLang(int lang);
int Ui_Lang();
const char *Ui_Tr(const char *s);
const std::set<std::string> &Ui_Missed();
void Ui_ClearMissed();
S32 Ui_TextWidthAsIs(const char *s);
S32 Ui_TextInkWidthAsIs(const char *s);
void Ui_TextAsIs(S32 x, S32 y, const char *s, U8 colour);

// s (as drawn) from x, kept within x..right: whole when it fits; wider, clipped, and with
// swing (the cursor's row) sliding to show its end and back, as 1997 did. Returns its width.
S32 Ui_TextInRoom(S32 x, S32 right, S32 y, const char *s, U8 colour, bool swing);

S32 Ui_TextWidth(const char *s);
// The width of the ink alone: Ui_TextWidth less the spacing after the last letter.
S32 Ui_TextInkWidth(const char *s);
void Ui_Text(S32 x, S32 y, const char *s, U8 colour);
void Ui_TextShadow(S32 x, S32 y, const char *s, U8 colour);  // dialogue shadow +2,+4
// A page title (the brand book's Title): white, the title shadow +4,+4, centred on
// x0..x1, its text cell from y. Returns where the rows below may start.
S32 Ui_Title(S32 x0, S32 x1, S32 y, const char *s, bool asIs = false);
// The corner Back's right edge this frame, 0 for none: a title starts right of it (and a gap)
// when centring would put it under the button, and swings in what is left when too wide.
void Ui_SetBackRight(S32 x1);
// The first title drawn this frame: its span's left and its line's top. False: none.
bool Ui_FirstTitle(S32 *x0, S32 *y);

// Two solid triangles, kin of the corner arrow sprites, either side of a value.
// One arrow of the change marks: its tip at tipX, pointing left (dir -1) or right (+1).
void Ui_Arrow(S32 tipX, S32 yc, S32 dir);
void Ui_ChangeMarks(S32 leftX, S32 rightX, S32 yCentre);
// Framed steps for a value with few of them (detail level 0..3 shows 3 steps):
// lit steps take the slider plasma, unlit ones stay dark. They end on xRight.
void Ui_Steps(S32 xRight, S32 yCentre, S32 count, S32 lit, S32 *outLeft);
// A small slider for the value column, x0..x1. Dimmed: no plasma, the filled part
// flat in the dimmed label's grey (3:1 from the live plasma), the rest dark.

// 8x8 font, drawn at a whole-step scale.
S32 Ui_SmallWidth(const char *s, S32 scale);
void Ui_Small(S32 x, S32 y, const char *s, U8 colour, S32 scale);
// A small word (legend words, column heads, "1/2"): in the menu's face at the 8x8 font's
// size (capitals 7 a step) when the menu draws in Zilla Slab, else in the 8x8 font, the
// game's own small letters. Keys (chips), the More options table and developer text (frame
// rate, the corner notice, the console) stay 8x8 either way (decided 2026-11-12).
S32 Ui_WordWidth(const char *s, S32 scale);
void Ui_Word(S32 x, S32 y, const char *s, U8 colour, S32 scale);
// A key chip: the key name in the 8x8 font inside a frame. Returns its width.
// A key of two keys, "<one>" CHIP_AND "<two>", is two chips side by side (the
// legend's "<> / Enter Change": either changes the value).
#define CHIP_AND "\x1f"
enum { CHIP_AND_GAP = 3 };  // between the two chips, a step
// pad: the family whose button the chip names (1 Xbox, 2 PlayStation, 3 Nintendo), drawn
// as the pad shows it (pad_icons.h); 0 for a key.
S32 Ui_ChipWidth(const char *key, S32 scale, int pad = 0);
// roles: what a legend's key does, CHIP_ROLE_* for the key, or for a key of two keys the
// first's in bits 0-1 and the second's in bits 2-3. With g_roleInk the letters take the
// role's colour, as an Xbox pad prints A green and B red; a pad's button that has its own
// colour keeps it.
enum { CHIP_ROLE_NONE = 0, CHIP_ROLE_CONFIRM = 1, CHIP_ROLE_BACK = 2 };
extern int g_roleInk;  // 1: legend keys in their role's colour; switch roleink, F10
S32 Ui_Chip(S32 x, S32 y, const char *key, S32 scale, bool lit = false, int pad = 0, int roles = 0);
// A colour a role gives a chip's letters (the self test finds a chip's ink by colour).
bool Ui_IsRoleInk(U8 c);

// Every chip drawn this frame with its whole name inside, for the self test: the frame's
// box, the name, whether it is on the plain shade (its letters in INK_CONFIG_MENU) and
// whether it is a legend's key (a binding chip is as wide as its column).
struct ChipBox {
    S32 x0, y0, x1, y1, scale;
    bool plain, legend;
    int pad;  // the pad family whose buttons key names, 0 for a key (pad_icons.h)
    char key[32];
};
void Ui_BeginFrame();
int Ui_ChipBoxes(const ChipBox **out);

// A binding chip of a fixed width, the name centred: shade, the cursor plasma,
// the current-value plasma while it waits for a key, or empty (a row of dots).
// red: the alert pair, for a key on two actions. Returns false when the name
// does not fit inside (it is clipped).
enum ChipFill { CHIP_SHADE, CHIP_CURSOR, CHIP_WAITING };
bool Ui_KeyChip(S32 x, S32 y, S32 w, const char *name, S32 scale, ChipFill fill, bool red, int pad = 0);
S32 Ui_KeyChipHeight(S32 scale);

// The game's voice: help typed letter by letter with the 13-step colour ramp
// (MESSAGE.CPP AffAllCar). Wraps into at most maxLines of the game font.
struct Voice {
    char text[512];
    U32 start;   // when the text, or the window it turned to, began
    bool typed;  // false: shown whole
    S32 page;    // the window a player turned to (Voice_Turn)
};
void Voice_Say(Voice &v, const char *text, bool typed);
// Help in more than one window: the player turns them with a key (Voice_Turn) and each
// stays until turned. "@P" in a text ends a window there, as the 1997 dialogue's @P ends a
// page (MESSAGE.CPP GetNextLine), so a window ends where its writer chose. A window too long
// for its lines also goes on in the next: a fallback the self test reports.
// The next window, typed again; after the last, the first. False when there is one window.
bool Voice_Turn(Voice &v);
// How many windows of maxLines the text takes at that width.
S32 Voice_Pages(const char *text, S32 width, S32 maxLines);
// Paged help shows where it is with "..." at a window's end when more follows and at its
// start after the first (2026-11-12): the lines then wrap short of the width by the
// ellipsis, so it always fits.
// The most lines the voice wraps a text into.
enum { VOICE_MAX_LINES = 16 };
// How many lines of the game font the text wraps into at that width.
S32 Voice_Lines(const char *text, S32 width);
// Returns the lines drawn; *needed gets how many the whole text takes. The window shown is
// the one the player turned to.
S32 Voice_Draw(const Voice &v, S32 x0, S32 y0, S32 width, S32 maxLines, S32 *needed);
// The window the last Voice_Draw showed (0 is the first) and how many there are.
void Voice_LastPage(S32 *page, S32 *pages);

// The clip as it is, put back by Restore. LIB386's MemoClip keeps one clip, so a MemoClip
// inside another's pair (Ui_TextInRoom inside a row's clip) lost the outer one, and the
// row's clip stayed on: a German answer too wide for its row hid everything drawn after it
// (2026-11-11). Each pair keeps its own here.
struct ClipSave {
    S32 x0 = ClipXMin, y0 = ClipYMin, x1 = ClipXMax, y1 = ClipYMax;
    void Restore() const { SetClip(x0, y0, x1, y1); }
};
