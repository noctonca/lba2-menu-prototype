// The menu's loop: the stack of pages, the question, the kinds of row and what each does,
// the pointer. It holds no pixel: menu_draw.cpp draws what this says is up.
#include "menu.h"

#include "menu_internal.h"

#include "bindings.h"
#include "trace.h"
#include "lang.h"
#include "whats_new.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// The kinds of row (menu_parts.h RowKind). The menu's own are set as it starts (SetOwnKinds);
// Keys and buttons and More options set theirs as they make their pages.
static const RowKind *g_kinds[R_COUNT];
static const RowKind kPlainKind = {};
const RowKind &KindOf(const Row &r) {
    assert(r.type >= 0 && r.type < R_COUNT && "a row's kind is one of RowType");
    return g_kinds[r.type] ? *g_kinds[r.type] : kPlainKind;
}
void Menu_SetRowKind(RowType type, const RowKind *kind) {
    assert(type >= 0 && type < R_COUNT && "a row's kind is one of RowType");
    g_kinds[type] = kind;
}
static void SetOwnKinds();

const char *RowLabel(const Row &r) {
    const RowKind &k = KindOf(r);
    return k.label ? k.label(r) : r.label;
}
bool RowActive(const Row &r) { return !r.active || r.active(); }

const char *PresetRowLabel(char *buf, size_t n) {
    return FormatPresetRow(buf, n, Presets_Name(Presets_Chosen()), Presets_Changed());
}

// ───────────────────────────── Navigation state ─────────────────────────────

// Where the player is: one stack of pages, each with the row its cursor is on, the same in
// every layout (the menu plan's page model). A row is its index in the page's table, never a
// position on the screen, so a layout change or a row that comes or goes leaves it in place.
Place g_stack[8];
int g_depth;
int g_scroll;       // the first shown row drawn: the view's, counted as its layout shows rows
// The question that is up, if any (menu_parts.h Question), and its cursor. Every question the
// menu asks goes through here, whichever page asks it.
const Question *g_asking;
int g_answer;        // 0 No, the safe answer, where it starts; 1 Yes
static int g_questionSave;  // the save the save-over and delete questions are about

void Menu_Ask(const Question *q) {
    g_asking = q;
    g_answer = 0;  // the safe answer first
    Trace("ask %s", q->name);
}
const Question *Menu_Asking() { return g_asking; }
int Menu_Answer() { return g_answer; }

// A question's title is in place of the page's, so the screen says it is a question (EPmager,
// 2026-11-11). The game menu asks only where something would be lost: the game in progress,
// or a save. Quit loses nothing (current.lba keeps the game), so it does not ask. 1997 asked
// none of these; lba2cc asks before a delete (SavedConfirmMenu).
static void YesClassic();
static void YesReset();
static void YesNewGame();
static void YesSaveOver();
static void YesDelete();
static const Question kAskClassic = {"classic", "Confirm Classic preset",
                                     "Play as in 1997? Sound and keys stay as they are.",
                                     {"No, keep Modern", "Yes, play Classic"}, YesClassic, NULL, NULL};
static const Question kAskReset = {"reset", "Confirm settings reset",
                                   "Put every setting back as a fresh install has it?",
                                   {"No, keep my settings", "Yes, reset settings"}, YesReset, NULL, NULL};
static const Question kAskNewGame = {"new game", "Confirm new game",
                                     "Start a new game? The game in progress ends; your saves stay.",
                                     {"No, keep my game", "Yes, start again"}, YesNewGame, NULL, NULL};
static const Question kAskSaveOver = {"save over", "Confirm save",
                                      "Save over this save? It becomes the game as it is now.",
                                      {"No, keep it", "Yes, save over it"}, YesSaveOver, NULL, NULL};
static const Question kAskDelete = {"delete", "Confirm delete", "Delete this save? It cannot be brought back.",
                                    {"No, keep it", "Yes, delete it"}, YesDelete, NULL, NULL};
bool g_quit;        // Esc on the Quit screen: the prototype ends (a fullscreen window has no close box)
// What the voice says on the game menu until the cursor moves: "Game saved." (1997 showed a
// brief overlay after a save, ShowGameSaveResult).
static const char *g_menuNote = "";
// The console (#679), opened from its row: by the device used last, the full console
// (keyboard, touch) or read-only (pad: the messages, and any button closes it).
int g_console;
Voice g_voice;
char g_toast[128];
U32 g_toastUntil;

bool Menu_Wide() {
    return Host_RuleChoseWide();
}

WideFrame Menu_WideFrame() {
    S32 W = (S32)Host_LogicalW(), H = (S32)Host_LogicalH();
    WideFrame f = {(W - 960) / 2, (H - 540) / 2, 960, 540};
    return f;
}

bool Wide() { return Menu_Wide(); }

void Toast(const char *fmt, const char *a, int v) {
    snprintf(g_toast, sizeof g_toast, fmt, a, v);
    g_toastUntil = Host_TimeMs() + 2000;
}

void Menu_Toast(const char *text) { Toast("%s", text, 0); }

void Menu_NoteScreen() {
    char note[64];
    snprintf(note, sizeof note, "%ux%u (logical %ux%u)%s", (unsigned)Host_PhysW(), (unsigned)Host_PhysH(),
             (unsigned)Host_LogicalW(), (unsigned)Host_LogicalH(), Host_Fullscreen() ? ", fullscreen" : "");
    Toast("%s", note, 0);
    Trace("screen %s", note);
}

int TopPage() { return g_stack[g_depth - 1].page; }

// A row as the layout shows it. No page shows a Return row (decided 2026-11-12, EPmager:
// clutter): Esc goes back, and a mouse or a finger has the corner Back (DrawBack). The
// "original" fixture keeps it, as today's lba2cc page has it. A row with a "shown when"
// check is there only while it holds.
static bool RowShown(const Page &p, int i) {
    const Row &r = p.rows[i];
    if (r.type == R_RETURN && !p.keepsReturn)
        return false;
    const RowKind &k = KindOf(r);
    if (k.shown && !k.shown(r))
        return false;
    return !r.shown || r.shown();
}

// The rows a page shows, in order: their indices in its table.
int ShownRows(int page, int *out) {
    const Page &p = g_pages[page];
    assert(p.count <= PAGE_ROWS_MAX && "PAGE_ROWS_MAX holds every page's rows");
    int n = 0;
    for (int i = 0; i < p.count; i++)
        if (RowShown(p, i))
            out[n++] = i;
    return n;
}

// Where a row is among the rows its page shows: 0 the first; -1 when it is not shown.
int Position(int page, int row) {
    int shown[PAGE_ROWS_MAX];
    const int n = ShownRows(page, shown);
    for (int i = 0; i < n; i++)
        if (shown[i] == row)
            return i;
    return -1;
}

// A row the cursor can be on: the row itself when shown, else the next one shown, else the
// one before (an undo row that went, after the undo).
// A group's head is shown but never under the cursor.
static bool Selectable(const Page &p, int i) { return RowShown(p, i) && p.rows[i].type != R_HEADING; }
static int ResolveRow(int page, int row) {
    const Page &p = g_pages[page];
    if (row >= 0 && row < p.count && Selectable(p, row))
        return row;
    for (int i = row + 1; i < p.count; i++)
        if (Selectable(p, i))
            return i;
    for (int i = (row < p.count ? row : p.count) - 1; i >= 0; i--)
        if (Selectable(p, i))
            return i;
    return 0;
}

// The top of the stack, its row checked as it is read: nothing has to remember to move the
// cursor off a row that went.
Place &Top() {
    Place &t = g_stack[g_depth - 1];
    t.row = ResolveRow(t.page, t.row);
    assert(Selectable(g_pages[t.page], t.row) && "the cursor is on a row its page shows, never a head");
    return t;
}

const Row *CursorRow() {
    if (g_asking)
        return NULL;
    const Place &t = Top();
    return &g_pages[t.page].rows[t.row];
}

int Menu_TopPage() { return g_depth ? TopPage() : -1; }
int Menu_TopRow() { return Top().row; }
int Menu_TopShownCount() {
    int shown[PAGE_ROWS_MAX];
    return ShownRows(TopPage(), shown);
}
int &Menu_Scroll() { return g_scroll; }

static bool EscGoesBack();

// The legend: only the keys the selected row takes.
void LegendFor(const Row *r, const char **keys, const char **words, int *n) {
    *n = 0;
    if (g_console) {
        keys[*n] = g_console == CON_READONLY ? "Any" : "Esc";
        words[(*n)++] = "Close";
        return;
    }
    if (g_asking) {
        Menu_QuestionLegend(keys, words, n);
        return;
    }
    if (r) {
        if (KindOf(*r).legend) {
            KindOf(*r).legend(*r, keys, words, n);
        } else {
            keys[*n] = "Enter"; words[(*n)++] = "Choose";
        }
    }
    if (!EscGoesBack())
        return;  // the first game menu: Esc has nothing to go back to
    keys[*n] = "Esc";
    // In play the game menu's Esc goes back to the game: the legend says so, as its first row
    // does (no corner Back there either).
    words[(*n)++] = g_depth == 1 && TopPage() == P_MAIN ? "Resume" : "Back";
}

static char g_helpBuf[256];  // a kind's built help (the undo rows'), already in the Text language

static const char *HelpFor(const Row *r) {
    if (!r)
        return "";
    // One message at a time: a dimmed row says why it is dimmed, not what it does.
    if (!RowActive(*r))
        return r->needs;
    const RowKind &k = KindOf(*r);
    const char *help = k.help ? k.help(*r) : NULL;
    if (!help)
        help = r->help;
    return help ? help : "";
}

// The short lines (decided 2026-11-03): each row's first window, the line read while
// scrolling. The longer drafts they replace said more; long help's second windows
// (kMoreHelp) say it now.
static const char *const kShortHelp[][2] = {
    {"Stops the picture tearing. Off can raise the frame rate on a slow display.", "Stops the picture tearing."},
    {"Rain, shadows and the far islands. Lower it on a slow machine.", "Rain, shadows and the far islands."},
    {"Sharp pixels as in 1997, or smoothed when scaled.", "Sharp pixels, or smoothed when scaled."},
    {"Fixes the 1997 release's known bugs. Off plays them as they were.", "Off plays the 1997 bugs as they were."},
    {"Settings still being tried. They may change or go.", "Settings still being tried."},
    {"Classic holds still and cuts; Auto turns to follow you.", "Classic cuts; Auto follows you."},
    {"Turns the camera the other way. Only with the Auto camera.", "Turns the camera the other way."},
    {"How fast the camera turns. Only with the Auto camera.", "How fast the camera turns."},
    {"Puts back the settings you had before you chose a preset.", "Puts back what you had before."},
    {"Talking, fighting, and the screens you open in play.", "Talking, fighting and the game's screens."},
    {"The game menu, pause, help, saving and loading.", "Game menu, pause, help, save and load."},
    {"Does what the behavior does: jump, hit or hide.", "Jumps, hits or hides, by behavior."},
};

// Long help's second window: what a row's short line leaves out, when or why to change it
// (sw_longhelp). Keyed by the short line as the voice says it, after kShortHelp.
static const char *const kMoreHelp[][2] = {
    {"The language the game writes in.", "The voices keep their own language, on the row below."},
    {"The language the characters speak.", "The voices exist in English, French and German only."},
    {"Shows what is said while the voices play.", "Keep it on if you don't understand the voices' language."},
    {"The picture behind the menu.", "Picture: the 1997 menu's. Twinsun: the planet turning. Off: black."},
    {"The game's letters, or sharp Zilla Slab.", "Zilla Slab is drawn at your screen's size and stays sharp."},
    {"Stops the picture tearing.", "Off can raise the frame rate on a slow display."},
    {"Shows the frame rate while you play.", "It is off again each time the game starts."},
    {"Sharp pixels, or smoothed when scaled.", "Smooth softens the pixels' edges when the picture is scaled up."},
    {"Everything at once.", "Turns all the sound up or down. The rows below set each part."},
    {"Footsteps, doors, magic.", "Lower it if the fights cover what the characters say."},
    {"The characters' voices.", "Raise it if the music or the sounds cover what is said."},
    {"The score.", "At 0 the music stops; the voices and sounds go on."},
    {"Swaps the left and right speakers.", "For speakers or headphones the wrong way round."},
    {"The pad's button names in the prompts. Auto reads the pad.", "Choose one if Auto names the wrong buttons."},
    {"Classic cuts; Auto follows you.", "Classic cuts between fixed views, as in 1997. Auto turns with Twinsen."},
    {"Cuts to the designers' shots outdoors.", "Off skips the shots that cut in once as you arrive."},
    {"Off plays the 1997 bugs as they were.", "On fixes the known bugs of the 1997 release."},
    {"Hold it to choose a behavior.", "Hold it, press left or right to choose, then let go."},
    {"Jumps, hits or hides, by behavior.", "Athletic jumps, Aggressive hits, Discreet hides."},
    {"Steps Twinsen back.", "He keeps facing the way he faced."},
};

static const char *MoreHelp(const char *shortLine) {
    for (size_t i = 0; i < sizeof kMoreHelp / sizeof kMoreHelp[0]; i++)
        if (!strcmp(shortLine, kMoreHelp[i][0]))
            return kMoreHelp[i][1];
    return NULL;
}

const char *Menu_Tr(const char *english) { return Lang_Tr(english, Settings_Value(S_TEXT_LANG)); }

static std::set<std::string> s_voiceMissed;
const std::set<std::string> &Menu_VoiceMissed() { return s_voiceMissed; }

static void Say(const char *text, bool translate) {
    if (translate) {
        for (size_t i = 0; i < sizeof kShortHelp / sizeof kShortHelp[0]; i++)
            if (!strcmp(text, kShortHelp[i][0]))  // the short line comes first under long help too
                text = kShortHelp[i][1];
        const char *more = sw_longhelp ? MoreHelp(text) : NULL;
        if (Settings_Value(S_TEXT_LANG) > 0 && text[0] && !Lang_Find(text, Settings_Value(S_TEXT_LANG)))
            s_voiceMissed.insert(text);
        if (more && Settings_Value(S_TEXT_LANG) > 0 && !Lang_Find(more, Settings_Value(S_TEXT_LANG)))
            s_voiceMissed.insert(more);
        text = Lang_Tr(text, Settings_Value(S_TEXT_LANG));  // the voice speaks the Text language
        if (more) {  // the short line, then the second window (@P ends a window, as in TEXT.HQR)
            static char both[512];
            snprintf(both, sizeof both, "%s @P %s", text, Lang_Tr(more, Settings_Value(S_TEXT_LANG)));
            text = both;
        }
    }
    // The game font has no =, ~ or + (each draws as a stray mark); the console help has all three.
    static char said[512];
    size_t o = 0;
    for (const char *p = text; *p && o + 8 < sizeof said; p++) {
        if (!strncmp(p, " + ", 3)) {
            memcpy(said + o, " and ", 5), o += 5, p += 2;
        } else if (!strncmp(p, " ~= ", 4)) {
            memcpy(said + o, " about ", 7), o += 7, p += 3;
        } else if (!strncmp(p, " = ", 3)) {
            memcpy(said + o, ": ", 2), o += 2, p += 2;
        } else if (*p == '=') {
            memcpy(said + o, ": ", 2), o += 2;
        } else {
            said[o++] = *p;
        }
    }
    said[o] = 0;
    text = said;
    if (strcmp(g_voice.text, text) != 0)
        Voice_Say(g_voice, text, !sw_whole);
}

void Menu_Say(const char *text) { Say(text, true); }
void Menu_SayAsIs(const char *text) { Say(text, false); }

// The hooks of the page on top of the stack; NULL for a list of rows.
const PageHooks *TopHooks() { return g_depth ? g_pages[TopPage()].hooks : NULL; }

static bool TopBusy() {
    const PageHooks *h = TopHooks();
    return h && h->busy && h->busy();
}

void UpdateVoice() {
    if (g_asking) {  // a question asks, whatever page it is on and wherever the pointer is
        if (g_asking->say) {
            static char built[256];
            g_asking->say(built, sizeof built);
            Menu_SayAsIs(built);  // built from formats already in the Text language
        } else {
            Menu_Say(g_asking->voice);
        }
        return;
    }
    if (const PageHooks *h = TopHooks(); h && h->say) {
        h->say();
        return;
    }
    if (!Menu_CursorLit()) {  // on Back: no row to speak of
        Menu_SayAsIs("");
        return;
    }
    const Row *r = CursorRow();
    const char *text = HelpFor(r);
    if (text == g_helpBuf)
        Menu_SayAsIs(text);
    else
        Menu_Say(text);
}

// The row a page opens on: its first shown, past What's new's row, which is there for the eye
// and does not take the cursor from where the page has always opened.
int OpeningRow(int page) {
    const Page &p = g_pages[page];
    for (int i = 0; i < p.count; i++)
        if (Selectable(p, i) && !(p.rows[i].type == R_CATEGORY && p.rows[i].arg == P_WHATSNEW))
            return i;
    return ResolveRow(page, 0);
}

void Push(int page) {
    assert(g_depth < (int)(sizeof g_stack / sizeof g_stack[0]) && "the stack holds the deepest way in");
    g_stack[g_depth].page = page;
    g_stack[g_depth].row = OpeningRow(page);  // the first row shown: 1997's sub-menus open on their Return row
    g_depth++;
    g_scroll = 0;
    if (const PageHooks *h = g_pages[page].hooks; h && h->opened)
        h->opened();
    Trace("open page=%s", g_pages[page].name);
}

// The layout the view was last made in. The place is the same in every layout; only the
// scroll, which counts rows as a layout shows them, starts again when the layout changes.
static int g_layoutWide = -1;

void Menu_SyncView() {
    const int wide = Wide() ? 1 : 0;
    if (g_layoutWide == wide)
        return;
    const int was = g_layoutWide;
    g_layoutWide = wide;
    if (was < 0)
        return;
    g_scroll = 0;
    const Place &t = Top();
    Trace("layout %s page=%s cursor=%d", wide ? "wide" : "column", g_pages[t.page].name, Position(t.page, t.row));
}

bool MenuClosed() { return Game_Playing() || Game_QuitShown(); }

// The menu closes on the game.
static void Play() {
    Game_Play();
    g_asking = NULL;
    g_menuNote = "";
    Trace(TopPage() == P_HUB && g_depth == 1 ? "close options" : "close menu");
}

static void NewGame() {
    Game_New();
    Play();
}

// A named save (i >= 0) or the autosave (i < 0).
static void LoadSave(int i) {
    Game_Load(i);
    Play();
}

// Resume game at a start: the current save.
static void ResumeCurrent() {
    Game_ResumeCurrent();
    Play();
}

// Load game and Save game open as 1997's ChoosePlayerName does: on the save the game in play
// came from or was last saved as, otherwise on the last row (AUTOSAVE, or New save).
static void OpenList(int page) {
    Push(page);
    Place &t = g_stack[g_depth - 1];
    const int at = Game_PlayingSave();
    if (at >= 0) {
        t.row = at;
        return;
    }
    const Page &p = g_pages[page];
    for (int i = p.count - 1; i >= 0; i--)
        if (RowShown(p, i)) {
            t.row = i;
            return;
        }
}

// The game menu over the game, as the Menus key opens it in play: on Resume game.
void OpenGameMenu() {
    Game_Stop();
    g_depth = 1;
    g_stack[0].page = P_MAIN;
    g_stack[0].row = ResolveRow(P_MAIN, 0);
    g_scroll = 0;
    Trace("open page=%s", g_pages[P_MAIN].name);
}

// The Options, Save and Load keys in play: their page alone, over the game as it is, and Esc
// goes back to the game (Pop on the only page plays).
static void OpenFromPlay(int page) {
    Game_PageKey();
    g_asking = NULL;
    g_menuNote = "";
    g_depth = 0;
    if (page == P_HUB) {
        Push(P_HUB);
    } else {
        OpenList(page);
    }
}

// Saved from the Save key's page: back in the game, which says so for a moment, as lba2cc
// shows "Game saved." over the scene (ShowGameSaveResult).
static void NoteSavedFromPlay() {
    if (!Game_Playing())
        return;
    Game_NoteSaved();
    Trace("game saved, back in play");
}

// Esc goes somewhere: everywhere but the game menu before a game (1997's first menu).
static bool EscGoesBack() { return !(g_depth == 1 && TopPage() == P_MAIN && !Game_InProgress()); }

static void Pop() {
    if (g_depth > 1) {
        Trace("close page=%s", g_pages[TopPage()].name);
        g_depth--;
        g_scroll = 0;
    } else if (TopPage() == P_MAIN && !Game_InProgress()) {
        // Nothing to go back to: 1997 puts the cursor on Quit (firstloop 2), so a second Esc
        // and Enter leave.
        PutCursorOn(R_GAME, M_QUIT);
        Trace("esc: cursor on Quit");
    } else {
        Play();  // the game menu, or Options as the Options key opens them in play
    }
}

void Menu_Init() {
    Settings_Fresh();
    SetOwnKinds();
    Bindings_Pages(g_pages);
    MoreOptions_Pages(g_pages);
    WhatsNew_Pages(g_pages);
    g_pages[P_MAIN] = kGamePages[0];
    g_pages[P_LOAD] = kGamePages[1];
    g_pages[P_SAVE] = kGamePages[2];
    Game_Start();
    g_menuNote = "";
    // Every start opens on the game menu, a fresh install's too, on Modern: nothing is asked and
    // nothing plays before the player could open Options (decided 2026-11-16).
    g_depth = 1;
    g_stack[0].page = P_MAIN;
    g_stack[0].row = ResolveRow(P_MAIN, 0);  // Resume game when there is a save, as 1997
    g_voice.text[0] = 0;
    g_layoutWide = Wide() ? 1 : 0;
    Trace("init layout=%s logical=%ux%u", Wide() ? "wide" : "column", (unsigned)Host_LogicalW(),
          (unsigned)Host_LogicalH());
}

// What Yes does on each of the menu's questions.
static void YesClassic() { Presets_Apply(1); }
static void YesReset() { Settings_ResetAll(); }
static void YesNewGame() { NewGame(); }
static void YesSaveOver() {
    Game_Save(g_questionSave);
    Pop();
    g_menuNote = "Game saved.";
    NoteSavedFromPlay();
}
static void YesDelete() {
    Game_DeleteSave(g_questionSave);
    if (TopPage() == P_LOAD && !Game_HaveSaves())
        Pop();  // nothing left to load: Load game goes from the game menu too
}

// ───────────────────────────── The kinds of row ─────────────────────────────
// What each kind of row does under the cursor (menu_parts.h RowKind). The loop, the legend,
// the help and the labels ask the kind; none of them names one.

static void EnterReturn(const Row &) { Pop(); }

// A row that opens a page: a category, the preset row, a group of keys.
static void EnterPage(const Row &r) { Push(r.arg); }
static void PageRowKeys(const Row &r) {
    if (Wide() && Host_Pressed(KEY_RIGHT))
        Push(r.arg);  // wide, Right opens what Enter opens
}
static void LegendOpen(const Row &, const char **keys, const char **words, int *n) {
    keys[*n] = "Enter"; words[(*n)++] = "Open";
}

static const char *SettingLabel(const Row &r) { return Settings_Def(r.arg).label; }
static const char *SettingHelp(const Row &r) { return Settings_Def(r.arg).help; }
static bool SettingSteps(const Row &r) {
    return Settings_Def(r.arg).kind == K_CHOICE || Settings_Def(r.arg).kind == K_TOGGLE;
}
static void EnterSetting(const Row &r) {
    if (SettingSteps(r))
        Settings_Step(r.arg, +1);
}
static void SettingKeys(const Row &r) {
    if (Host_Pressed(KEY_LEFT))
        Settings_Step(r.arg, -1);
    if (Host_Pressed(KEY_RIGHT))
        Settings_Step(r.arg, +1);
}
static bool SettingValue(const Row &r, bool *enterSteps) {
    *enterSteps = SettingSteps(r);
    return true;
}
static void SettingLegend(const Row &r, const char **keys, const char **words, int *n) {
    if (!RowActive(r))
        return;  // a dimmed row takes no key
    // A choice or a switch also steps on Enter, so the legend names both (EPmager,
    // 2026-11-11); a slider or steps take Left and Right only.
    const bool steps = SettingSteps(r);
    keys[*n] = steps ? "\x11\x10" CHIP_AND "Enter" : "\x11\x10";
    words[(*n)++] = steps ? "Change" : "Adjust";
}

static const char *PresetChoiceLabel(const Row &r) {
    if (r.arg != Presets_Chosen())
        return r.label;
    static char marked[2][64];  // two rows can be drawn before the first is used
    static int next;
    char *out = marked[next++ & 1];
    // Changed since it was chosen, it is no longer what it says: wide, the row's value
    // says Changed; in the one column, where a choice shows no value, the mark does.
    if (Presets_Changed()) {
        if (Menu_Wide())
            return r.label;
        snprintf(out, sizeof marked[0], "%s (changed)", r.label);
        return out;
    }
    snprintf(out, sizeof marked[0], "%s (current)", r.label);  // Ui_Tr takes the parts apart
    return out;
}
static void EnterPresetChoice(const Row &r) {
    if (r.arg == 1 && Presets_Chosen() != 1)
        Menu_Ask(&kAskClassic);
    else
        Presets_Apply(r.arg);
}
// The preset in use, changed since: Enter puts its settings back, and says so.
static const char *PresetChoiceHelp(const Row &r) {
    return r.arg == Presets_Chosen() && Presets_Changed() ? "You changed some of its settings. Restore puts them back." : NULL;
}
static void PresetChoiceLegend(const Row &r, const char **keys, const char **words, int *n) {
    if (r.arg != Presets_Chosen()) {
        keys[*n] = "Enter"; words[(*n)++] = "Choose";
    } else if (Presets_Changed()) {  // the preset in use: Enter does nothing until it is changed
        keys[*n] = "Enter"; words[(*n)++] = "Restore";
    }
}

static void EnterUndo(const Row &r) {
    Presets_Undo(r.arg);
    Trace("back to my settings");
}
// What the undo puts back (Presets_UndoNames, as Presets_Undo does it); as many named as the
// voice holds, the rest counted, so the help never pages.
static const char *UndoHelp(const Row &r) {
    char *buf = g_helpBuf;
    const size_t bufSize = sizeof g_helpBuf;
    const char *names[BACKUP_DIFF_MAX];
    const int n = Presets_UndoNames(r.arg, names);
    for (int shown = n; shown > 0; shown--) {
        char list[200] = "";
        for (int i = 0; i < shown; i++) {
            if (i)
                strncat(list, i + 1 == n ? Menu_Tr(" and ") : ", ", sizeof list - strlen(list) - 1);
            strncat(list, names[i], sizeof list - strlen(list) - 1);
        }
        if (shown < n) {
            char more[24];
            snprintf(more, sizeof more, Menu_Tr(" and %d more"), n - shown);
            strncat(list, more, sizeof list - strlen(list) - 1);
        }
        snprintf(buf, bufSize, Menu_Tr("Puts back what you had: %s."), list);
        if (VoiceFits(buf, &r))
            return buf;
    }
    return NULL;
}

// What Enter does on an action row or a game menu row, and whether it opens something (the
// legend then says Open, not Choose).
struct RowDo {
    void (*run)();
    bool opens;
};
static void OpenKeys() { Push(P_KEYS); }
static void AskReset() { Menu_Ask(&kAskReset); }
static void OpenMore() { Push(P_MORE); }
static void OpenConsole() {
    g_console = Host_LastInputWasPad() ? CON_READONLY : CON_FULL;
    Trace("open console mode=%s", g_console == CON_READONLY ? "read-only" : "full");
}
// What's new's To try row: More options, on the row it names.
static void OpenTry() {
    Push(P_MORE);
    for (int i = 0; i < MoreOptions_Count(); i++)
        if (!strcmp(MoreOptions_Name(i), "cam_world_anchor"))
            PutCursorOn(R_TUNING, i);
}
static const RowDo kActionDo[] = {{OpenKeys, true}, {AskReset, false}, {OpenMore, true}, {OpenConsole, true},
                                  {OpenTry, true}};
static_assert(sizeof kActionDo / sizeof kActionDo[0] == A_COUNT, "what Enter does for every A_ action");

static void GameResume() {
    if (!Game_InProgress())  // at the start: the current save
        ResumeCurrent();
    else
        Play();
}
static void GameNew() {
    if (Game_Unsaved())  // in play, or kept in current.lba from before a Quit
        Menu_Ask(&kAskNewGame);
    else
        NewGame();
}
static void GameLoad() { OpenList(P_LOAD); }
static void GameSave() { OpenList(P_SAVE); }
static void GameOptions() { Push(P_HUB); }
static void GameQuit() { Game_Quit(); }
static const RowDo kGameDo[] = {{GameResume, false}, {GameNew, false}, {GameLoad, true},
                                {GameSave, true},    {GameOptions, true}, {GameQuit, false}};
static_assert(sizeof kGameDo / sizeof kGameDo[0] == M_COUNT, "what Enter does for every M_ game row");

static void EnterAction(const Row &r) { kActionDo[r.arg].run(); }
static void ActionLegend(const Row &r, const char **keys, const char **words, int *n) {
    keys[*n] = "Enter"; words[(*n)++] = kActionDo[r.arg].opens ? "Open" : "Choose";
}
static void EnterGame(const Row &r) {
    g_menuNote = "";
    kGameDo[r.arg].run();
}
static void GameLegend(const Row &r, const char **keys, const char **words, int *n) {
    keys[*n] = "Enter"; words[(*n)++] = kGameDo[r.arg].opens ? "Open" : "Choose";
}
static const char *GameHelp(const Row &) {
    if (g_menuNote[0])
        return g_menuNote;
    // After an update, until What's new is opened (whats_new.h), on the first menu of the run,
    // before a game: in play the legend's Esc Resume leaves the strip 204 to 248 px at 640x480,
    // too little for it in five languages; the dots carry it there. It names the row by its own
    // name, short enough for the strip at 640x480 in every language (the sentence with a count
    // was not: 2026-11-18, measured).
    if (WhatsNew_LineShown() && !Game_InProgress())
        return "What's new: Options.";
    return NULL;
}

// A save, or the autosave: Save game asks before it writes over one, Load game loads.
static const char *SaveLabel(const Row &r) { return Game_SaveName(r.arg); }
static const char *AutosaveLabel(const Row &) { return Game_AutosaveName(); }
static bool SaveShown(const Row &r) { return r.arg < Game_SaveCount(); }
static void EnterSave(const Row &r) {
    g_questionSave = r.type == R_AUTOSAVE ? -1 : r.arg;
    if (TopPage() == P_SAVE)
        Menu_Ask(&kAskSaveOver);  // a save would be lost
    else
        LoadSave(g_questionSave);  // no question: choosing a save on Load game is the choice,
                                   // and runners load often (AUTOSAVE keeps the last scene)
}
static void SaveDel(const Row &r) {
    g_questionSave = r.arg;
    Menu_Ask(&kAskDelete);
}
static void SaveLegend(const Row &, const char **keys, const char **words, int *n) {
    keys[*n] = "Enter"; words[(*n)++] = TopPage() == P_SAVE ? "Save" : "Load";
    keys[*n] = "Del"; words[(*n)++] = "Delete";
}
static void AutosaveLegend(const Row &, const char **keys, const char **words, int *n) {
    keys[*n] = "Enter"; words[(*n)++] = "Load";  // the autosave cannot be deleted
}
static void EnterNewSave(const Row &) {
    Game_Save(-1);
    Pop();  // back on the game menu's Save game row, as 1997 after a save
    g_menuNote = "Game saved.";
    NoteSavedFromPlay();
}
static void NewSaveLegend(const Row &, const char **keys, const char **words, int *n) {
    keys[*n] = "Enter"; words[(*n)++] = "Save";
}

// enter, keys, del, value, legend, label, help, shown
static const RowKind kReturnKind = {EnterReturn, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static const RowKind kPageRowKind = {EnterPage, PageRowKeys, NULL, NULL, LegendOpen, NULL, NULL, NULL};
static const RowKind kSettingKind = {EnterSetting, SettingKeys, NULL, SettingValue, SettingLegend, SettingLabel,
                                     SettingHelp, NULL};
static const RowKind kPresetChoiceKind = {EnterPresetChoice, NULL, NULL, NULL, PresetChoiceLegend, PresetChoiceLabel,
                                          PresetChoiceHelp, NULL};
static const RowKind kUndoKind = {EnterUndo, NULL, NULL, NULL, NULL, NULL, UndoHelp, NULL};
static const RowKind kActionKind = {EnterAction, NULL, NULL, NULL, ActionLegend, NULL, NULL, NULL};
static const RowKind kGameKind = {EnterGame, NULL, NULL, NULL, GameLegend, NULL, GameHelp, NULL};
static const RowKind kSaveKind = {EnterSave, NULL, SaveDel, NULL, SaveLegend, SaveLabel, NULL, SaveShown};
static const RowKind kNewSaveKind = {EnterNewSave, NULL, NULL, NULL, NewSaveLegend, NULL, NULL, NULL};
static const RowKind kAutosaveKind = {EnterSave, NULL, NULL, NULL, AutosaveLegend, AutosaveLabel, NULL, NULL};

static void SetOwnKinds() {
    Menu_SetRowKind(R_RETURN, &kReturnKind);
    Menu_SetRowKind(R_CATEGORY, &kPageRowKind);
    Menu_SetRowKind(R_PRESET_ROW, &kPageRowKind);
    Menu_SetRowKind(R_SETTING, &kSettingKind);
    Menu_SetRowKind(R_PRESET_CHOICE, &kPresetChoiceKind);
    Menu_SetRowKind(R_BACK_TO_MINE, &kUndoKind);
    Menu_SetRowKind(R_ACTION, &kActionKind);
    Menu_SetRowKind(R_GAME, &kGameKind);
    Menu_SetRowKind(R_SAVE, &kSaveKind);
    Menu_SetRowKind(R_NEW_SAVE, &kNewSaveKind);
    Menu_SetRowKind(R_AUTOSAVE, &kAutosaveKind);
}

// Enter (or a click) on a row: its kind's.
void Activate(const Row &r) {
    if (KindOf(r).enter)
        KindOf(r).enter(r);
}

void Menu_Back() { Pop(); }
void Menu_TopRowTo(int row) { g_stack[g_depth - 1].row = row; }

static void Move(int dir) {
    Place &t = Top();
    int shown[PAGE_ROWS_MAX];
    const int n = ShownRows(t.page, shown);
    if (n <= 0)
        return;
    int at = Position(t.page, t.row);
    for (int k = 0; k < n; k++) {  // past a group's head
        at = (at + dir + n) % n;
        if (Selectable(g_pages[t.page], shown[at]))
            break;
    }
    t.row = shown[at];
    g_menuNote = "";
    const Row *r = CursorRow();
    Trace("cursor %s", r ? (r->type == R_PRESET_ROW ? "preset row" : RowLabel(*r)) : "-");
}

// The cursor on the top page's row of that kind, as Enter on it leaves it.
void PutCursorOn(RowType type, int arg) {
    Place &t = g_stack[g_depth - 1];
    const Page &p = g_pages[t.page];
    for (int i = 0; i < p.count; i++) {
        if (p.rows[i].type == type && p.rows[i].arg == arg) {
            t.row = i;
            return;
        }
    }
}

// ── The pointer (EPmager, 2026-11-11: the prototype took no mouse) ──
// As lba2cc's game menu: hover moves the cursor when the mouse moves, a press puts it there
// and a release on the same target acts. The right button is Esc, the wheel Up and Down.

PointTarget g_points[POINT_MAX];
int g_pointCount;
static PointTarget g_press;  // what the left button went down on
static bool g_pressOn;
int g_wheelView;      // notches turned this frame, for a page that scrolls (Menu_WheelView)
ScrollBarInfo g_bar;   // the last draw's scroll bar
const ScrollBarInfo &Menu_ScrollBar() { return g_bar; }
int g_scrollTo = -1;  // the first line the scroll bar asked for this frame; -1 none
static S32 g_barGrab = -1;   // dragging the thumb: where on it the pointer took it; -1 not

void Menu_Point(PointKind kind, S32 x0, S32 y0, S32 x1, S32 y1, int a, int b) {
    assert(g_pointCount < POINT_MAX && "a frame has more pointer targets than POINT_MAX: the pointer would miss the rest");
    if (g_pointCount < POINT_MAX)
        g_points[g_pointCount++] = PointTarget{kind, x0, y0, x1, y1, a, b};
}

int Menu_Points(const MenuPoint **out) {
    *out = g_points;
    return g_pointCount;
}

static const PointTarget *PointAt(S32 x, S32 y) {
    for (int i = g_pointCount - 1; i >= 0; i--) {
        const PointTarget &p = g_points[i];
        if (x >= p.x0 && x <= p.x1 && y >= p.y0 && y <= p.y1)
            return &p;
    }
    return NULL;
}

static bool SameTarget(const PointTarget &a, const PointTarget &b) {
    return a.kind == b.kind && a.a == b.a && a.b == b.b;
}

void CursorTo(int page, int row) {
    if (!g_depth || TopPage() != page)
        return;
    Place &t = g_stack[g_depth - 1];
    if (t.row == row || row < 0 || row >= g_pages[page].count || !RowShown(g_pages[page], row))
        return;
    t.row = row;
    const Row *r = CursorRow();
    Trace("cursor %s (pointer)", r ? (r->type == R_PRESET_ROW ? "preset row" : RowLabel(*r)) : "-");
}

void Menu_CursorTo(int row) { CursorTo(TopPage(), row); }

int Menu_WheelView(int &scroll, int visible, int total, int cursor) {
    const int n = g_wheelView, to = g_scrollTo;
    g_wheelView = 0;
    g_scrollTo = -1;
    if ((!n && to < 0) || visible >= total)
        return cursor;
    const int from = scroll;
    if (to >= 0)
        scroll = to;
    scroll -= n;  // up is positive: the view goes toward the top
    if (scroll > total - visible)
        scroll = total - visible;
    if (scroll < 0)
        scroll = 0;
    if (scroll == from)
        return cursor;
    Trace("scroll %d (%s)", scroll, to >= 0 ? "bar" : "wheel");
    return cursor < scroll ? scroll : cursor >= scroll + visible ? scroll + visible - 1 : cursor;
}

// The corner Back is an option a mouse can stand on: while it is, no row is lit (decided
// 2026-11-13). It stays on Back over empty space, as a row keeps the cursor; a key hides Back
// and the cursor is on its row again.
static bool g_onBack;
// The corner Back is offered: a pointer is in use and Esc goes somewhere. Not on the game
// menu itself: Resume game is its first row (the maintainer, 2026-11-16).
bool BackShown() {
    return LastPointer() != POINTER_NONE && !MenuClosed() && !g_console && EscGoesBack() &&
           !(g_depth == 1 && TopPage() == P_MAIN);
}

bool Menu_CursorLit() {
    if (!g_onBack || !BackShown())
        return true;
    HostPointer in;
    Host_Pointer(&in);
    return !(LastPointer() == POINTER_MOUSE || in.held);  // a finger is on Back only while pressing it
}

// The cursor where the pointer is.
static void PointTo(const PointTarget &p) {
    const bool wasBack = g_onBack;
    g_onBack = p.kind == POINT_KEY && p.b == POINT_KEY_BACK;
    if (g_onBack && !wasBack)
        Trace("cursor Back (pointer)");
    switch (p.kind) {
    case POINT_ROW:
        CursorTo(p.a, p.b);
        break;
    case POINT_CHIP:
        CursorTo(TopPage(), p.a);
        if (const PageHooks *h = TopHooks(); h && h->chip)
            h->chip(p.b);
        break;
    case POINT_STEP:
    case POINT_SLIDER:
        CursorTo(TopPage(), p.b);
        break;
    case POINT_ANSWER:
        if (g_asking && g_answer != p.a) {
            g_answer = p.a;
            Trace("cursor %s (pointer)", g_answer ? "Yes" : "No");
        }
        break;
    case POINT_KEY:
    case POINT_SCROLL:
        break;
    }
}

// The first line of the view with the thumb's top where the pointer puts it.
static int BarFirstAt(S32 y) {
    const ScrollBarInfo &b = g_bar;
    const S32 h = b.y1 - b.y0 - 4;
    if (h <= 0)
        return b.first;
    const S32 top = y - g_barGrab - (b.y0 + 2);
    int first = (int)((top * b.total + h / 2) / h);
    return first < 0 ? 0 : first > b.total - b.visible ? b.total - b.visible : first;
}

// A slider moves by a drag, never by a press: the 1997 slider is its whole row, label and
// all, so a press set Master volume from 100 to 5 when it landed on the word (2026-11-12).
// Past a few pixels sideways the value follows the pointer by how far it went, from where it
// was, across the row's width for the whole range.
enum { SLIDE_START_PX = 6 };
static S32 g_slideFromX, g_slideFromValue;
static bool g_sliding;

static void SlideTo(const PointTarget &p, S32 x) {
    const Setting &st = Settings_Def(p.a);
    if (!g_sliding) {
        if (x - g_slideFromX < SLIDE_START_PX && g_slideFromX - x < SLIDE_START_PX)
            return;
        g_sliding = true;
    }
    S32 w = p.x1 - p.x0;
    int v = w > 0 ? (int)(g_slideFromValue + ((x - g_slideFromX) * st.max + (x >= g_slideFromX ? w / 2 : -w / 2)) / w)
                  : g_slideFromValue;
    v = v < 0 ? 0 : v > st.max ? st.max : v;
    if (v != Settings_Value(p.a)) {
        Settings_Put(p.a, v);
        char buf[32];
        Trace("set %s=%s (pointer)", st.id, Settings_ValueText(p.a, buf, sizeof buf));
    }
}

static void UpdatePointer() {
    HostPointer in;
    Host_Pointer(&in);
    const PointTarget *at = PointAt(in.x, in.y);
    if (g_onBack && LastPointer() == POINTER_NONE && !MenuClosed() && !g_console) {
        // A key while on Back only brings the cursor back to its row (decided 2026-11-13); Esc
        // still goes back, as Back would have.
        g_onBack = false;
        for (int k = 0; k < KEY_COUNT; k++)
            if (k != KEY_ESC)
                Host_Unpress((UiKey)k);
        Trace("cursor back on its row (key)");
    } else if (!BackShown()) {
        g_onBack = false;
    }
    if (TopBusy()) {
        // A click is not a key to bind: only the legend's Cancel and the right button act.
        if (in.pressed) {
            g_pressOn = at && at->kind == POINT_KEY;
            if (g_pressOn)
                g_press = *at;
        }
        if (in.released && g_pressOn) {
            g_pressOn = false;
            if (at && SameTarget(*at, g_press))
                Host_PressNow((UiKey)at->a);
        }
        if (in.rightPressed)
            Host_PressNow(KEY_ESC);
        return;
    }
    if (g_console || MenuClosed()) {
        // A click closes the console, opens the game menu over the game, or (on Quit's
        // screen) starts the game again.
        if (in.pressed)
            Host_PressNow(Game_QuitShown() ? KEY_ENTER : KEY_ESC);
        else if (in.rightPressed && g_console)
            Host_PressNow(KEY_ESC);
        return;
    }
    if (in.moved && at && !in.held)
        PointTo(*at);
    if (in.pressed) {
        g_pressOn = at != NULL;
        g_barGrab = -1;
        if (at) {
            g_press = *at;
            PointTo(*at);
            if (at->kind == POINT_SLIDER) {
                g_slideFromX = in.x;
                g_slideFromValue = Settings_Value(at->a);
                g_sliding = false;
            }
            if (at->kind == POINT_SCROLL && g_bar.shown) {
                if (in.y < g_bar.ty0)  // the groove above the thumb: a viewful up
                    g_scrollTo = g_bar.first > g_bar.visible ? g_bar.first - g_bar.visible : 0;
                else if (in.y > g_bar.ty1)  // below it: a viewful down
                    g_scrollTo = g_bar.first + g_bar.visible;
                else
                    g_barGrab = in.y - g_bar.ty0;  // the thumb: dragged from here
            }
        }
    } else if (in.held && in.moved && g_pressOn && g_press.kind == POINT_SLIDER) {
        SlideTo(g_press, in.x);  // dragged along the bar, even off it
    } else if (in.held && in.moved && g_pressOn && g_press.kind == POINT_SCROLL && g_barGrab >= 0 && g_bar.shown) {
        g_scrollTo = BarFirstAt(in.y);  // the thumb follows, even off the groove
    }
    if (in.released)
        g_barGrab = -1;
    if (in.released && g_pressOn) {
        g_pressOn = false;
        if (at && SameTarget(*at, g_press)) {
            switch (at->kind) {
            case POINT_ROW:
            case POINT_CHIP:
            case POINT_ANSWER:
                Host_PressNow(KEY_ENTER);
                break;
            case POINT_STEP:
                Host_PressNow(at->a < 0 ? KEY_LEFT : KEY_RIGHT);
                break;
            case POINT_KEY:
                Host_PressNow((UiKey)at->a);
                break;
            case POINT_SLIDER:
            case POINT_SCROLL:
                break;
            }
        }
    }
    if (in.rightPressed)
        Host_PressNow(KEY_ESC);
    g_wheelView = in.wheel;  // taken by this frame's draw if its page scrolls, else dropped
}

void Menu_Update() {
    Menu_SyncView();
    UpdatePointer();
    if (TopBusy()) {  // the wait for a key: it takes every key and button
        TopHooks()->updateBusy();
        UpdateVoice();
        return;
    }
    if (g_console) {
        // Full: Esc closes (F12 in the game; F12 is a prototype switch here). Read-only: anything does.
        bool any = Host_RawPressed() != 0;
        for (int k = 0; k < KEY_COUNT && !any; k++)
            any = Host_Pressed((UiKey)k);
        if (g_console == CON_READONLY ? any : Host_Pressed(KEY_ESC)) {
            g_console = CON_CLOSED;
            Trace("close console");
        }
        return;
    }
    if (Game_Playing()) {
        if (Host_Pressed(KEY_ESC)) {  // the Menus key in play: current.lba keeps this screen
            Game_MenuKey();
            OpenGameMenu();
        } else if (Bindings_ActionPressed("Options")) {
            OpenFromPlay(P_HUB);
        } else if (Bindings_ActionPressed("Save")) {
            OpenFromPlay(P_SAVE);
        } else if (Bindings_ActionPressed("Load")) {
            if (Game_HaveSaves())
                OpenFromPlay(P_LOAD);
        } else if (Host_Pressed(KEY_ENTER)) {  // on to the next place: a scene change writes the autosave
            Game_NextScene();
        }
        return;
    }
    if (Game_QuitShown()) {
        if (Host_Pressed(KEY_ENTER)) {  // the game starts again, on its first menu
            Game_StartAgain();
            WhatsNew_NextRun();
            OpenGameMenu();
        } else if (Host_Pressed(KEY_ESC)) {
            g_quit = true;
            Trace("quit");
        }
        return;
    }
    if (g_asking) {
        const Question *q = g_asking;
        if (Host_Pressed(KEY_UP) || Host_Pressed(KEY_DOWN)) {
            g_answer ^= 1;
            Trace("cursor %s", g_answer ? "Yes" : "No");
        }
        if (Host_Pressed(KEY_ESC)) {
            g_asking = NULL;
            Trace("answer no (esc)");
        }
        if (Host_Pressed(KEY_ENTER)) {
            g_asking = NULL;
            Trace("answer %s", g_answer ? "yes" : "no");
            if (g_answer)
                q->yes();
        }
        UpdateVoice();
        return;
    }

    if (Host_Pressed(KEY_MORE)) {
        if (Voice_Turn(g_voice))
            Trace("help window %d", (int)g_voice.page + 1);
        else
            Trace("help more: one window");
    }
    if (Host_Pressed(KEY_UP))
        Move(-1);
    if (Host_Pressed(KEY_DOWN))
        Move(+1);
    if (const PageHooks *h = TopHooks(); h && h->keys)
        h->keys();  // the page's own: Tab, Page Up and Page Down

    // The row under the cursor: its kind's keys, then Enter, then Del. Each may move the
    // cursor or open a page, so the row is read again before the next.
    const Row *r = CursorRow();
    if (r && KindOf(*r).keys && RowActive(*r))
        KindOf(*r).keys(*r);
    r = CursorRow();
    if (r && Host_Pressed(KEY_ENTER) && RowActive(*r))
        Activate(*r);
    r = CursorRow();
    if (Host_Pressed(KEY_DEL)) {
        if (r && KindOf(*r).del)
            KindOf(*r).del(*r);
        else if (const PageHooks *h = TopHooks(); h && h->del)
            h->del();
    }

    if (Host_Pressed(KEY_ESC))
        Pop();

    WhatsNew_Frame(MenuClosed() || !Menu_CursorLit() ? NULL : CursorRow());
    UpdateVoice();
}

