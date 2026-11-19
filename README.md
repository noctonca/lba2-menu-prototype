# LBA2 menu prototype

A playable prototype of the Modern Options menu from lba2cc's menu plan
(docs/plan/MENU_PLAN.md) and its design review, with the game menu around it,
drawn with the game's own pieces: the menu palette, the LBA font, the plasma,
the chamfered frames and the darkening, read from the real game data.

Settings and saves are fakes held in memory, and no game is played. Nothing
here writes `lba2.cfg` or a save.

## What it is built from

- `lba2cc/` is a submodule of [lba2-classic-community](https://github.com/LBALab/lba2-classic-community).
  Its `LIB386` libraries are built from the submodule as they are (window,
  palette, font, HQR and disc-image reading, polygon fill).
- `SOURCES/PLASMA.CPP`, `FIRE.CPP` and `ANIMTEX.CPP` are compiled straight from
  the submodule for the menu plasma.
- `src/game_pieces.cpp` copies the game-layer drawing helpers this needs
  (`DrawCadre`, `BackupAngles`/`RestoreAngles`, `ShadeBoxBlk`, `DrawFireBar`)
  and names where each came from.
- `assets/fonts/` holds Zilla Slab Bold (SIL Open Font License,
  `assets/fonts/OFL.txt`), baked into the binary by `cmake/embed_binary.cmake`
  for the Font row; `third_party/stb_truetype.h` (public domain) draws it.
  All three come from lba2cc's parked HD UI branch, `feat/hd-ui-scale`.

GPL-2.0, as lba2cc.

## Where the menu's code is

The menu is in files with one job each, cut where the menu plan cuts lba2cc's
("Where the code goes"), so that it is carried over a file at a time:

| File | Holds | In lba2cc |
|---|---|---|
| `src/menu.cpp` | the loop: the stack of pages, the question, the kinds of row and what each does, the pointer | `OPTIONS_MENU.CPP` |
| `src/menu_pages.cpp` | the row tables of the Options pages and of the game menu, Load game and Save game | `OPTIONS_PAGES.CPP` |
| `src/menu_draw.cpp` | the view: both layouts, the draw record, the pointer's targets | `OPTIONS_DRAW.CPP` |
| `src/bindings.cpp` | Keys and buttons: its pages, its kind of row, its own view | `OPTIONS_KEYS.CPP` |
| `src/more_options.cpp` | More options: its page, its kind of row, its own view | `OPTIONS_MORE.CPP` |
| `src/menu_settings.cpp` | a stand-in: the example settings, and the presets, undo and reset over them | the presets go to `OPTIONS_PRESETS.CPP`; the values are lba2cc's settings tables |
| `src/fake_game.cpp` | a stand-in: the saves and the game around the menu | none: lba2cc has the game |
| `src/menu_state.cpp` | what scripts and tests read and set: the switches, `set`, `open`, `state` | the console's `ui` commands |
| `src/whats_new.cpp` | a trial: what a new build tells a player, its marks, its line and its page | not decided (MENU_PLAN.md open question 16) |

Three headers say what crosses between them:

- `src/menu_parts.h` is what a page in its own file needs: the page model
  (`Row`, `Page`), a kind of row's handlers (`RowKind`), a page's (`PageHooks`),
  the question (`Question`), the draw record and the pointer's targets. Keys
  and buttons and More options use nothing else of the menu.
- `src/menu_settings.h` and `src/game.h` are the menu's two doors to what is
  not the menu: the settings and presets, and the game and its saves. The
  loop, the tables and the view read and change a value only through them.
- `src/menu_internal.h` is what the menu's own four files share. It is the
  coupling between them, kept as one list so that it can be seen and kept
  short.

To add a kind of row, give it a `RowKind` (what Enter, its own keys and Del do,
what its legend, label and help are) and set it with `Menu_SetRowKind`; the
loop asks the kind and has no list of kinds to add to. To add a page with its
own keys or its own view, give it `PageHooks`. To ask something, fill in a
`Question` and call `Menu_Ask`: the loop, the pointer, the legend and both
layouts already know how to show one and take its answer.

Once a file here has moved to lba2cc, lba2cc's is the only copy (the menu
plan, "One copy, and a bench to try it on"): this repository then builds it
from the lba2cc submodule against the stand-ins, and is the bench a change to
it is tried on. So a file in the table above is to reach the rest of this
repository through the doors alone, and what lba2cc will not take (the
Background and Font rows in the tables, the switches) comes out of it before
it moves.

## Build

    git submodule update --init
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build build

Needs SDL3 (Homebrew `sdl3`). The keyboard and the pad are read through
lba2cc's platform layer (`g_platform`); SDL is used directly only to size and
name the desktop window.

### Xbox (Developer Mode)

    LLVM_MINGW_ROOT=<llvm-mingw> bash tools/build-xbox.sh

builds with lba2cc's UWP backend and packs `out/xbox/lba2menu-xbox-x64.appx`,
signed with a throwaway certificate. `LLVM_MINGW_ROOT` is an unpacked
llvm-mingw release; lba2cc's `scripts/dev/build-uwp.sh` downloads the one it
pins (`out/toolchains/` of an lba2cc checkout). It is its own app, "LBA2 menu
prototype" (`LBA2CC.MenuProto`): lba2cc's game, settings and saves are not
touched. Install it as lba2cc's `docs/XBOX.md` says, start it once, then put
`RESS.HQR` and `SCREEN.HQR` (any 1997 or Classic copy; the menu picture is the
same) in its `LocalState\Game`. The screen is the TV's (1920x1080, the wide
layout); what it says goes to `LocalState\lba2menu.log`. `--stdin` is not
available there; a `LocalState\args.txt` gives it other arguments, one a line.

### Web browser

    bash tools/build-web.sh

builds with Emscripten (Homebrew `emscripten`; its SDL3 port, through
`cmake/emscripten-sdl3/`) into `out/web/`: `index.html`, `lba2menu.js` and
`lba2menu.wasm`, a static site for any web server. The page (`web/shell.html`)
asks for the player's game folder and keeps `RESS.HQR`, `SCREEN.HQR` and
`HOLOMAP.HQR` in the browser (IndexedDB); no game data is
served. Any copy works, the 1997 demo included. The menu loop runs as it does
on a desktop and hands each frame back to the browser (ASYNCIFY).

The menu fills the page under a thin bar, and its screen is the canvas in the
display's pixels (CSS size x pixel ratio, so a Retina window is drawn at full
resolution). Resizing the browser window resizes the screen, as dragging the
desktop window does; F2 still picks a size from the list, shown with black
bars. The Fullscreen row and the bar's button put the page's stage in the
browser's fullscreen; the row's request waits for the release of Enter, as a
browser allows fullscreen only inside a key or click handler.
Browsers keep Esc for leaving fullscreen, so on the web Q is the menu's Esc
too (by the key's name, so it follows the keyboard layout). In fullscreen,
Chrome and Edge also lock Esc for the page (Keyboard Lock): a press goes back,
holding it leaves fullscreen. Firefox and Safari have no such lock.
`tools/publish-web.sh` builds and puts `out/web` on the `gh-pages` branch.

The page around the menu (the bar, the file picker, About) follows the LBA
Classic UI design system, theme lba2: its colour tokens, the bevelled frame,
menu rows that take the plasma under the cursor, the voice colour for what the
page tells the player, and key chips in lba2cc's 8x8 font
(`web/fonts/LBASystem8x8.ttf`, drawn from `LIB386/SVGA/AFFSTR.CPP`). The game
font is game data (`RESS.HQR`) and is not served; the page's display face is
Zilla Slab Bold, the menu's Modern font.

How it works (reached from About and the file picker) lists seven promises
the menu keeps, in a player's words, each with a way to see it: nothing is
lost by accident, Back is one press, every row explains itself, Classic looks
like 1997, keyboard and pad are equal, one language for the whole menu, the
picture is never stretched. They are drawn from `docs/UX.md`; a change to a
rule there that breaks a promise changes the panel too.

## Play

    ./play.sh                      # 640x480, one column, a fresh install: no saves, on Modern
    ./play.sh --size 1920x1080     # the wide layout (logical 960x540 at 2x)
    ./play.sh --saves 20           # a returning player: 20 saves, Resume game first
    ./play.sh --options            # in a game, Options open as the Options key opens them

### The game menu

It starts on the game menu (1997's main menu, GAMEMENU.CPP), before any game, as
a fresh install: New game, Options and Quit, on Modern, after the logos: nothing is asked,
and nothing plays before the player could open Options (decided 2026-11-16;
1997 went straight into the intro with no save). New game plays the intro. Its rows come and go as 1997's do:
Resume game when there is a current save (1997's current.lba: the game last
started, loaded or resumed), Load game when there are saves, Save game only in
a game; New game, Options and Quit always. There is nothing for Esc to go back
to before a game, so the legend leaves it out and Esc puts the cursor on Quit,
as 1997's does.

The game is a landscape drawn in code (`src/game_scene.h`): sky, sea and an
island's hills, a lighthouse on Citadel Island, towers on Otringal, the Emerald
Moon at night, with clouds and waves moving. It names what it plays (a save, or
a new game). Enter walks on to the next place, as a scene change does in the
game, and every third place takes the boat to the next island. Esc opens the
game menu over it, on Resume game, as the Menus key does in play: the game
stays behind the menu, all of it dimmed as lba2cc's MainGameMenu shades it
(`SCREEN_SHADE_LVL`). Esc on the game menu goes back to the game. The Options,
Save and Load keys (O, S and L, or what Keys and buttons binds them to; their
F4, F2 and F3 stay the prototype's switches here) open their page on its own over the game as it is, only the boxes
shaded, as lba2cc's do in play; Esc goes back to the game, and so does New
save, the game then saying "Game saved." for a moment. Load has nothing to
open with no save. The Background setting is what is behind the menu before a
game; in a game the game is.

A game goes as lba2cc's does: New game, play, Esc and Quit; then the game
starts again (Enter on Quit's screen) on the game menu with Resume game and
the picture of where Esc left it, and Resume game plays from there. Nothing is
written to disk: closing the prototype is a fresh install again. `./play-flow.sh`
walks it step by step.

Load game and Save game are lists of the saves in alphabetical order, as
1997 lists them, named as lba2cc names a save it makes, shorter (see
docs/UX.md). `--saves 20` starts with 20 fake ones, enough that the lists scroll. Load game's last row is AUTOSAVE, the save the game
writes as it goes: it loads like the others and cannot be deleted, and Save
game does not list it. Save game's last row, New save, adds one and goes back
to the game menu, where the voice says "Game saved." Del on a save deletes
it. Both lists open on the save the game in play came from, otherwise on
their last row, as 1997's do.

The save picture is where 1997 shows it: Resume game's on the first game
menu, in place of the title, and on Load game the picture of the save under
the cursor, under the title. In play the game menu has none: six rows and a
picture do not fit 640x480. As in lba2cc a save keeps the screen as it was
when the save was written, 160x120: the current save as a game starts or loads
and when Esc leaves play, the autosave then and at every scene change, a named
save on Save game. Here that is the landscape drawn at 160x120 at that moment
(the screen's centre at 4:3), so no picture comes from the game's data. A fake
save's picture is drawn from its name: its island, and a place and moment of
its own.

Off Resume game, and on the first game menu with no save, the title is the
product's name. It is centred in the picture's place, and with no save it
keeps that place: the name and New game are where a player with a save has
the name and Resume game.
In Zilla Slab the name is the game's logo from the 1997 intro with a
lower-case cc under its 2, so it reads LBA, 2, cc. It is drawn in code at the
screen's resolution from a traced outline (`assets/logo/`, `src/video_logo.h`)
and builds in once as the intro builds it: it fades in, a ring of water with
blue beads crosses it and bends the letters, the 2 lands in a swirl and sends
a second ripple through them, and the cc arrives last. Every 30 seconds a
gentle ripple crosses it. In play the game menu's title is the same logo,
smaller and at rest.
Switch `videologo` 0 (F9) gives the wordmark it replaced: LBA large with a
raised 2, CLASSIC COMMUNITY small under it. There, flames rise through LBA and the 2 in the menu palette's fire
ramp (plasma bank 15, orange to white), in tongues that rise and fall slowly
(5 steps a second, the heat carried up smoothly between them), with a
gold rim round each letter, thinner on the 2. They are painted at the screen's
resolution like the letters, not on the canvas. In the game font the name is
one line.

The game menu asks only where something would be lost, starting on No: New
game while a game in progress has play since the last save; saving over a
save; deleting one. A save makes the next New game ask nothing. Quit never
asks: 1997 writes the game to current.lba when Esc leaves play, and Resume
game carries on from it at the next start, so New game still asks then. Load
never asks: choosing a save on Load game is the choice, runners load often,
and AUTOSAVE keeps the play up to the last scene change. Quit shows a screen saying the game would close here:
Enter starts it again, Esc ends the prototype.

Game data: `LBA2_GAME_DIR`, or `--data DIR`; the default is
`~/code/lba-hacking/assets/lba2-activision-us-cd` (a disc image is mounted as
the engine does).

The mouse works as lba2cc's game menu has it: moving it puts the cursor on the
row, chip or answer under it, and a click (press and let go on the same thing)
does what Enter does. A value's arrows step it, a slider moves by a drag (by
how far it goes; a click alone leaves it, as the 1997 slider is its whole row,
label and all), and a legend entry presses its key (Back, Defaults, Keyboard
and the rest); it lights under the mouse, as a row does, so it reads as a
button (under a finger only while pressed). The right button is Esc. Once the mouse or a finger was used
last, a Back button sits on the title's line (see docs/UX.md); a key or a pad's
button takes it away. The wheel scrolls a page that
has a scroll bar, a row a notch, and takes the cursor along only when it would
leave the view; on a page that fits, it does nothing. The scroll bar takes the
pointer too: drag its thumb, or tap the groove above or below it to move a
viewful, so a finger reaches every row without keys. lba2cc's arrow shows once
the mouse moves and hides after 3 s still, painted over the finished frame (so
`shotscreen` shows it, `shot` does not). While
the game waits for a key to bind, only the right button and the legend's
Cancel act. The targets are placed as each page draws, so a click can't miss
what is drawn.

### Screen sizes

The screen size changes in place, as lba2cc's own resolution switch does it:
the page, the cursor and the settings stay as they were, and a
note in the top margin says the new size and its logical canvas.

- F2 steps to the next size in the list, Shift+F2 back to the previous one
  (640x480, 768x480, 1024x480, 1280x720, 1280x800, 1920x1080, 2400x1080,
  2560x1440, 3440x1440, 3840x2160), with the native size put in by area when it
  is not one of them. Native is what Fullscreen off and on again would give:
  the display's size on a desktop or a console, the canvas's pixels in a
  browser, the launch size in a headless run. 768x480 and 1024x480 are lba2cc's
  widescreen modes: 480 high, they keep the one column on a wider canvas. The window is sized to suit, as at launch.
- Dragging the window's edge makes its size in pixels the screen: any size in
  between, the layout following as you drag. On a Retina display that is twice
  the window's size in points (a 1100x650 window is a 2200x1236 screen). The
  width is rounded down to a multiple of 8, as lba2cc requires, and the screen
  is at least 640x480.
- The Resolution row on Display and graphics does it as the game would: Left
  and Right step through the same list, and the row shows the screen in use
  (a dragged size too, "1512 x 904"), marked "(native)" when it is the native
  size. It is the player's screen, not a preset's
  choice, so it is never drawn as changed; the prototype's presets and Reset
  settings set it all the same (below).
- The Fullscreen row turns the window into fullscreen and back, with lba2cc's
  own call. As in the game, fullscreen and the resolution are separate: the
  screen size, the canvas and the place in the menu stay, and the picture is
  letterboxed onto the display (Resolution still steps it there). The row
  follows macOS's own fullscreen too (the green button, Ctrl+Cmd+F), and like
  Resolution it is never drawn as changed.
- `size WxH` does the same from a script or the control file.

`./play-sizes.sh` walks it in one window, with steps, and prints each change
from the trace afterwards.

When a change crosses between the one column and the wide layout, the place is
carried across: the page you were on stays open, on the same row.

To quit, choose Quit on the game menu, then Esc on the screen that follows
(Enter starts the game again); in fullscreen there is no close box. Cmd+Q
works too.

Keys: arrows, Enter, Esc; a pad's d-pad or left stick, A and B. Space (the pad's
Y) turns the help to its next window when the legend shows More. The prototype's
own keys:

| Key | What it does |
| --- | --- |
| F1 | these keys on one line in the top margin (switch `overlay`); a switch's note takes the line for 2 s |
| Shift+F1 | a trial of what a new build tells a player (switch `whatsnew`, off at start; see `src/whats_new.h`): 1 the first run after an update from a pretend 0.13.2: a gold dot after each new row's name and on the path to it, until the cursor has been on the row and left it; "What's new: Options." on the start screen until What's new is opened; and What's new first on Options, in three groups (now on in Modern, new settings, to try), for the whole run, and gone from the next run that starts with everything on it seen; 2 a later run of that build (as is Quit and Enter in the prototype); 3 a fresh install, which sees none of it. The dot is gold, chosen over pale cyan and soft green |
| F2 | the next screen size; Shift+F2 the previous (see Screen sizes) |
| F3 | the green current-value plasma on the chosen preset, besides its "(current)" (switch `greencurrent`, off at start) |
| F4 | long help: a row's short line, then a second window on Space or Y saying when or why to change it (switch `longhelp`, on at start; off is the short help alone) |
| F5 | how the menu scales (switch `propscale`, 5 at start; Shift+F5 back): 0 whole steps only; 1 everything linear; 2 to 6 frames linear and the game font drawn over them: 2 nearest neighbour, 3 at the whole step below, 4 at the whole step above, 5 each font pixel an exact square with its edges blended by coverage, 6 as 5 after Scale2x (see `src/prop_scale.h`) |
| F6 | Zilla Slab's letters placed at quarter pixels instead of whole ones (switch `zsub`, off at start) |
| F7 | `propscale` straight between 5 and 1, the last two compared |
| F8 | the help window's legend at the size that fits its width, between 1x and 2x, instead of dropping to 1x (switch `legendfit`, on at start, decided 2026-11-16; needs `propscale` 5 or 6; `./play-legendfit.sh`) |
| F9 | the product's name: the intro's LBA 2 logo instead of the wordmark, drawn in code at the screen's resolution and built in as the intro builds it (switch `videologo`, 1 at start; 0 is the wordmark it replaced; the build-in always has the intro's full ripple and blue ring, and 1 to 3 choose the ripple every 30 seconds after it: half the intro's height, the intro's height, the intro's height with the blue ring; stepping from one to the next plays that ripple at once; in play the game menu has it smaller, at a title's size, at rest; Shift+F9 steps switch `logocc`, the lower-case cc of Classic Community under the logo's 2, which is ours, in the logo's colours, and arrives after the intro's build-in (1 at start; 0 is the logo as the intro has it); switch `logosub` 1 keeps CLASSIC COMMUNITY under it before a game; see `src/video_logo.h`, `assets/logo/README.md`; `./play-logo.sh`) |
| Shift+F10 | Resume game's opening (switch `opening`, on at start; turned on again it plays from the build-in): on the first game menu with a save the logo builds in where the picture goes, rests 1.5 s and cross-fades into the save's picture over 0.5 s, once a run; Enter does not wait for it, and the cursor off Resume game ends it; 0 shows the picture at once. The fade is painted at the screen's resolution over what was under the logo; see `VideoLogo_Opening` in `src/video_logo.h`; `./play-opening.sh` |
| F11 | steps the Button names row: Auto / Xbox / PlayStation / Nintendo (switch `padnames`) |

Two more switches have no key: `keyboard` 0 plays as a console before a key is
typed (no Keyboard view or Tab, the legend names the pad), and `whole` 1 shows
help whole instead of typing it, as the self test runs.

Every review switch was pruned on 2026-11-12, keeping what was decided: the
band (help and legend), help typed on every cursor move and shown in the
dialogue window on every one-column page, key chips at 2x (58 wide in the one
column), the preset row "Preset: Modern", one pane at a time with the page's
name as a title, the 960x540 composition centred,
the picture scaled evenly and cropped at the screen's own resolution as the
Image row says, one device per view on Keys and buttons with the column names
inside the first row, a key in use offered as a swap, a clash's note alone in
the voice, the shorter help, the More options table at 1x with each row's
help in its status lines, the 1997 slider, the page mark on paged help, Frame
rate on Display and graphics. Presets set the look too: Classic the 1997
picture, the game font, Classic cameras and 640 x 480; Modern Twinsun, Zilla
Slab, Auto and Follow cameras and the native size as it is when Modern is
chosen (not the size at launch, which a dragged window can make smaller). Undo
preset and Undo reset put the screen back too. The plan has presets set the
rules only; this is the prototype's call, to show the difference. The
Gameplay category (Dodge, Behavior hints) went with its switch.

## The Controls page

Controls and camera > Keys and buttons opens the binding page of the design
review (`ControlsWideReview`). The 36 actions of lba2cc's `INPUT.H` sit in
seven groups, with the defaults of `INPUT_BINDINGS.CPP`; rebinding is held in
memory like the other settings.

- Wide: the groups on the left, the group's actions on the right with their
  chips. Right or Enter opens a group; Left from the first chip goes back.
- 640x480: the groups as a 1997 sub-menu, then a page of actions with the voice
  below.
- One device per view: Tab (the pad's View button) switches Keyboard and
  Gamepad; the page opens on the one touched last.
- The pad's buttons are named as its maker names them: SDL's own pad type on a
  desktop, and in a browser the vendor in the pad's name (Chrome's
  "Vendor: 057e", Firefox's "57e-2009-"), then words such as DualSense or
  Joy-Con. Anything else gets Xbox names. Button names on Controls and camera
  holds one family instead. The pad and the names chosen are printed on
  connect ("pad: ... (Nintendo names)", the browser's console on the web).
- Enter waits for a key: the chip takes the current-value plasma and the voice
  asks for it and says the way out: "Press a key, or <ESC> to keep it." on the
  keyboard view, "Press a button, or <MENU> to keep it." on the pad view (Start,
  by the pad's own name, in capitals inside angle brackets as the 1997 text
  writes "<ESC>" and "<TAB>"; decided 2026-11-11, replacing "or wait to keep it").
  Esc or Start cancels, and so does waiting 5 seconds. A pad view only takes
  pad input, a keyboard view only keys.
- Start always opens the game menu, as Esc does on the keyboard: Game menu's
  pad chip is fixed (Enter and Del leave it, and the voice says why), and Start
  is never bound to another action. The menu's own pad buttons are fixed too
  (A Enter, B Esc, X Del, View Tab, d-pad and left stick the arrows). So a
  player with only a pad (Xbox, Switch, Vita, Android TV) cannot rebind their
  way out of reaching these options (decided 2026-11-11).
- Del (Backspace on a Mac keyboard, X on a pad) clears a chip.
- A key on two actions frames both chips red, and the voice names the other
  action.

## Languages

The whole menu is in the language the Text row picks: English, Français,
Deutsch, Español, Italiano, Português. The voice (help, questions, messages) is
in `lang/help.tsv`; labels, values, titles, answers, column heads and legend
words in `lang/menu.tsv`, a column per language, looked up by their English.
"Label: Value" rows are looked up a side at a time; `legend|Back` keeps the
legend's Back apart from the walking-backwards action. Key names, numbers and
More options' tuning rows stay as they are. The translations are stand-ins,
made to measure lengths, not reviewed by native speakers. After editing either
file run `tools/lang.py`, which writes `src/lang_help.inc` and
`src/lang_menu.inc` in the game font's code page (CP850). The self test runs
every walk in every language, checks every voice text in both fonts against
its window, and reports any drawn text that `lang/menu.tsv` lacks and any
spoken text that `lang/help.tsv` lacks. Console help and messages built from
formats already translated are said as they are, not looked up.

## Driving it

Every run can be steered and read back without a person at the keys.

    build/lba2menu --headless --script steps.txt     # no window, fixed 16 ms clock
    build/lba2menu --do "open Sound; key down 2"     # commands inline; the window stays up on that screen
    build/lba2menu --control FILE                    # append commands to FILE while the window is up

Commands, one per line: `key <up|down|left|right|enter|esc|tab|del|f1..f12> [count]`,
`press <key>` (a raw key for rebinding: an SDL name such as `J`, `Left Alt` or
`Keypad 5`, or `pad:a`, `pad:lb`, `pad:lstick-up` ...), `pad <button>` (a pad
button as a pad gives it, its menu key too: `a`, `b`, `dpad-down` ...),
`wait <frames>`, `mouse <x> <y>` (logical pixels), `click [right]`, `tap <x> <y>` (a click as touch), `wheel <notches>`, `mousedown`, `mouseup`, `state`, `measure`, `widths <file> [voice width]`, `shot <path.png> [scale]`, `switch <name> <value>`,
`set <setting> <value>`, `open <page name>`, `size <WxH>`, `quit`.

`open` takes an Options page as the Options key opens it in a game (Esc from
Options goes back to the game), `Game menu`, `Load game` or `Save game` as the
game menu opens them (Save game puts you in a game).

`measure` prints the width in the game font of every setting's label, its
widest value and its help, and of the hub's rows, for sizing panes.
`widths` measures any text the same way: each line of the file is
`tag<TAB>text` in CP850 (as TEXT.HQR holds it), and it prints the tag, the
width and the voice lines the text needs (620 wide unless given). It is how
the 1997 game's six languages were compared for the help review.

- `state` prints one JSON line: layout, sizes, page, the stack of pages with the
  row each cursor is on (the same in both layouts; Keys and buttons and a group's
  page of actions, named after the group, are pages on it too), the cursor's
  place among the rows shown, every row
  drawn (label, value, fill, rectangle, dimmed, changed, change marks, whether
  it is a setting, where its text starts and ends: `text_left`, `text_right`,
  and the last pixel column of what it shows on the right, a value, a slider,
  steps or its last key chip: `value_right`, 0 for nothing), the help text
  with how many letters are typed, which window of it shows of how many
  (`page`, `pages`) and whether it was cut off, the legend, the
  switches and every setting.
- On the Controls page `state` adds `bindings`: the device, the group under the
  cursor or open, and on a group's page the action and column under the cursor, whether it waits for a key, every chip drawn
  (name, rectangle, fill, conflict, whether the name fits), the column heads
  (`heads`: text and rectangle; inside the first row wide, above the actions
  in the one column),
  every binding and every conflict.
- `shot` writes exactly what is in the frame buffer through the current palette.
- `shotscreen` writes the screen as presented, at the screen's size: the canvas at its
  scale, the game font over it and the picture at the screen's resolution.
- Every key and every change is a trace line (`trace t=<ms> f=<frame> ...`) on
  stderr and in `--trace FILE`.
- Headless runs use a fixed clock and a fixed plasma seed, so a script gives
  the same pictures every time. They run with vsync off, as fast as the
  machine draws.

## More options

A More options row on Advanced and reset opens a page of the 41
values lba2cc's settings tables give a console name and no row in Options
(the Auto camera 22, the mouse 9, the gamepad 7, graphics 3), with their
defaults, ranges and console help as lba2cc has them, and two rows from
1997 (below). It is the menu plan's step 8, generated from a table rather
than made row by row, in English. The row is always there (decided
2026-11-03, no Show experimental settings switch); its help says the page is
in English and the rows marked experimental may change or go.

Most of the 41 tune finished features; two are what lba2cc calls experimental
(`cam_zone_anchor`, `cam_world_anchor`; docs/CAMERA.md). Those carry an
"experimental" tag after their name, in one column, and their help starts
"Experimental:". The page was "Experimental settings" until 2026-11-11, which
said more than its rows are. "More options" says what it is, the settings of
the other pages in more depth, and leaves the warning to the rows it is
about. It says options, not settings, by the rule in lba2cc's glossary: options
is the place (the menu, Advanced options, a page), settings are the values
chosen there (Reset settings).

The settings tables have no field saying which rows are experimental yet; the plan's ring field would give it, and the hand list in
`more_options.cpp` stands in for it.

A fifth group, From 1997, holds two rows the 1997 Options had for the
machines of the day: Detail level (each step down takes away animated water,
rain, the far scenery, then shadows) and Movies (at original size or fit to
screen; original size was for slow CD drives). They came off Display
(decided 2026-11-11); they keep their game labels, having no console
name, and the plan's 1997 Options page keeps them where they were. Movie
camera (1997's scenario cameras: off skips the outdoor shots that cut once) is
camera behavior, not a machine's limit, so it went to Controls and camera
instead, and Reverse stereo, which is for speakers or headphones the wrong way
round rather than a 1997 machine, went back to Sound (EPmager, 2026-11-11).

- One dense table in the 8x8 font at 1x, as the 1997 key binding screen
  (decided 2026-11-03): a line per value under its group's head, the cursor a
  plasma strip, and at the foot the row's help on two lines with a status line
  under it (the default, the range and the lba2.cfg key when it fits). 31 lines
  are in view on a wide screen.
- Left and Right change a value, Enter turns a switch, Del puts the default
  back, Page Up and Page Down (the pad's LB and RB) move a window at a time.
  A changed value takes the count colour.
- The Auto camera rows are tuning values: a preset puts them back to their
  defaults, and a changed one marks the preset changed. The mouse, gamepad,
  graphics and From 1997 rows are player's settings, as the menu plan has
  them: no preset touches them, and changing one does not mark the preset.
  Reset settings puts every row back to its default. Undo preset puts back
  the tuning values, Undo reset every row, and each names what it puts back.
- The row's help is the console help, whole, in the table's status lines
  (decided 2026-11-12); the voice says the help of the row's group. The self
  test visits every row (see Testing).

## Frame rate, the console and the version

- **Frame rate** (Display and graphics, under Vsync, decided 2026-11-03) shows the frame count in the bottom left corner, white,
  in the 8x8 font, as lba2cc's console `fps` and the 1997 SPEED code do. It is
  not kept in lba2.cfg: off at every start. Presets leave it alone. lba2cc
  draws it only in play; here it shows over the menu so the row can be judged.
  On Display it makes seven rows, so at 960x540 the page scrolls (Image is
  below).
- **Console** (Advanced and reset, above Reset settings) is issue #679's row.
  Its value is the version, `0.13.3-dev 011394f` (lba2cc's version string and
  the commit's first 7; a release shows its number alone; an example here), in
  the dimmed grey: it reads, it does not change. Enter opens the console as
  lba2cc draws it: the top half, darkened, the 8x8 font, the messages of the
  launch (examples) with the version and the full commit in the first two
  lines. After a key it is the full console with the prompt, and Esc closes it
  (F12 in the game). After a pad button it is
  read-only, says so on its last line, and any button closes it.


## Testing

`lba2menu --selftest` visits every row of every page, every chip of the
Controls page and every row of More options at one screen size, for the pad's names of each family, and
lists what does not fit: help cut off, rows running into the help or off the
screen, labels running into their values or their key chips, key names cut in
their chips. On every screen it also checks that exactly one row or chip has
the cursor, that a red key chip under the cursor is explained in the help, and
that a clash shows on both actions. It also checks the rules of
[docs/UX.md](docs/UX.md) a screen can show (each problem starts with the rule's
name): `edge:` the rows of a pane share their left and right edges and, wide,
every label starts 16 in, the Controls page's first column head too when the
heads are a line above the rows; `margin:` nothing on a wide page is drawn in
its margins (32 at the sides, 24 at the top and bottom); `trailing:` wide, what
a row shows on the right ends 24 in from its right edge; `pitch:` rows in a pane
are 56 apart and 50 tall; `current:` at most one current-value row in a pane;
`value:` every setting row shows its value; `return:` no layout shows a Return row
(Esc goes back; a mouse or a finger has the corner Back); `proximity:` a pane's last row is
further from the help below than rows are from each other, a column head above
the rows is nearer them than the row above it, and a column head inside a row
is in its pane's first row, above its own chip, and clear of the action's name.
On the More options table the same rules read its lines: the panel
clear of the help (and its margins); every line on one strip with its name on
the label edge (`edge:`); every value ending on one edge and the cursor's marks
24 in (`trailing:`); no name running into its value (`text spills`); lines 10
apart at 1x (`pitch:`); and (`table:`) every line inside the panel above the
status line, the status line inside the panel, and a group's head shown with
the cursor on the group's first row.

Then it walks the menu by its keys and checks what they do. Each problem a
walk finds starts with the walk's name:

- `opening:` Resume game's opening, when the logo is on: off, the picture at
  once; on, the logo and no picture at the start and at 4 s, both while it
  fades at 4.8 s, the picture alone at 5.3 s; Enter during the build-in plays;
  off Resume game and back shows the picture.
- `flow:` the game menu around Options. With 20 saves (`--saves 20`), at a
  start: Resume game first, no Save game, Load game with a row for each of the 20 saves in alphabetical
  order and AUTOSAVE last, opening on AUTOSAVE, where Del does nothing and the
  legend has no Del. The first game menu has Resume game's picture and no
  title, Load game its title and a picture; in play the game menu has its
  title and no picture. The legend has no Esc, and Esc puts the cursor on Quit.
  Resume game plays; Esc in play opens the game menu on Resume game, now with
  Save game and no scroll bar, and Esc goes back to the game. Save game opens
  on the save in play. New game after play asks and Esc keeps playing; New
  save, the last row, goes back on Save game, the voice says "Game saved.",
  Save game has one row more and opens on the new save, and New game then
  plays without asking. In a new game the lists open on their last row.
  Saving over a save asks, on No. Del on a save asks; Yes takes that save
  away, the one above stays and the cursor goes to the next. Load after play
  plays at once. Options opens on the preset row and Esc comes back to
  Options. Quit after play asks nothing and shows Quit's screen; Enter starts
  again on the game menu with Resume game and without Save game, where New
  game asks, No keeps the game and Resume game plays it; Load there plays at
  once. Deleting every named save leaves
  AUTOSAVE alone on Load game. With no saves (a fresh install, the start) the game menu
  starts on New game without Resume game and Load game, and both come after
  the first game.

- `esc:` from every row of every page, opened from the hub with Enter, Esc
  puts the cursor back where it was (the Controls page, its groups, the
  questions, and the wait for a key too).
- `reach:` Enter opens every page from the hub.
- `setting:` Left or Right changes the value shown and the other key puts it
  back; a dimmed row ignores Left, Right and Enter; Invert camera is live
  with the Auto camera (a fresh start's) and dimmed with the Classic camera.
- `preset:` Classic asks first, starting on No, and Esc answers No; Undo
  preset puts the preset row back; changing Fix retail bugs marks the preset
  changed. Music changed after choosing Classic keeps its value through Undo
  preset, whose help does not name it. With Classic chosen, Vsync (a player's
  setting) and Voices changed and Forward's Key 1 cleared, Reset settings asks
  first, starting on No; Esc and No change nothing; Yes puts the setting, the
  key and the preset back as a fresh install has them and keeps Text and
  Voices, and an Undo reset row comes right after Reset settings; Enter on it
  puts back the setting, the cleared key and Classic, and the row goes.
- `more_options:` Advanced and reset has the More options row; Enter opens the page and Esc comes back to the row;
  on every row Right and Left change the value and the other key puts it back,
  the change marks the preset row on a tuning row and not on a player's
  setting, Del puts the default back, and Enter steps a switch or a choice
  and leaves a number; Page Down and Page Up stop at the last and first rows,
  and the page opened again starts at its top; Modern puts a changed tuning value back to its default,
  Undo preset names it and puts it back; Reset settings and Undo reset do the
  same, and the cursor stays on Reset settings; Movies, the last row
  and a player's setting, keeps its value through Classic.
- `console:` the Console row shows the version; Enter after a key opens the
  full console, which Down and Enter leave open and Esc closes; Enter after a
  pad button opens it read-only, and a pad button, the d-pad or a key closes
  it; each time the cursor is back on Console. Turning Frame rate on leaves the
  preset row as it was.
- `bind:` on the first action of each group, in each column: Enter waits and
  the voice asks for the key; Esc cancels; the other device's input is ignored;
  a key another action has asks first, on No (the voice says "J is on
  Jetpack, ...", the answers take the actions' place), and Esc keeps both as
  they were; Del clears it. On the pad, Start leaves the wait as Esc does, and
  Game menu's chip neither waits nor clears. Also: Forward's Key 2 put in
  Key 1 leaves Key 2 empty, and the wait gives up after 5 s.
- `reset keys:` Del on the Keys and buttons groups page asks first (legend
  "Del Defaults"); Esc and No keep the keys; Yes puts back the device in view
  as the game ships it and leaves the other device's changes.
- `keys:` the Keys and buttons pages go where their keys say: each group says
  its own help; a group opens on its first chip, whatever chip the group before
  was left on; Tab switches the device on a group's page too; wide, Right opens
  a group and Left from the first chip goes back to it (the one column: neither).
- `padonly:` with pad buttons only, every pad binding is offered Start (which
  keeps it, never asks) and then cleared; Game menu still has Start, and B
  leaves Keys and buttons and A opens it again.
- `swap:` No changes nothing; Yes gives Forward J and Jetpack Forward's old
  key, with no key on two actions; swapping back puts both back; onto an empty
  chip the Yes answer says what Jetpack is left with.

It runs in one process without drawing to a window (nor drawing the
background and row shading, which the checks never read), and exits 1 when it
finds a problem. Under each problem it prints a command line that opens the
screen it was found on:

    $ build/lba2menu --selftest --size 1920x1080 --combo padnames=2 --only Sound
    1920x1080 {"padnames": 2} [Sound] text spills 15 px: 'Master volume' '100'
        again: build/lba2menu --size 1920x1080 --switch padnames=2 --switch whole=1 --do "open Sound"

- `--combo name=v,name=v` (repeatable) tests those switch settings instead of
  `padnames` 0, 2 and 3; any switch of `state` can go in.
- `--only PAGE` keeps one page (`"Keys and buttons"` for the Controls page,
  `"More options"`).
  The `esc:` and `reach:` walk needs every page, so it runs only without it.
- `--layout-only` leaves out the walks by keys.
- Add `--headless` and `; shot x.png` to an `again` line for a picture.

`tools/sweep.py` runs the self test for seven screen sizes and the seven
combinations, one process each, in parallel (about 10 seconds on the Mac), and
takes `--sizes`, `--combo`, `--only`, `--again` and `--layout-only` the same way.
It and `tools/resize-check.sh` first build `build-test/`, a RelWithDebInfo copy
(`tools/test_build.py`), so they never test a binary older than the source; the
self test runs about twice as fast there as in the Debug `build/`. `--bin` tests
another binary.

- `--scripts` walks the same screens as `--layout-only` from outside: a script
  per run, read back through `state`, checked by the same rules written in
  Python. It is slower (about 6 seconds against 2 for `--layout-only`) and must list the same problems as
  `sweep.py --layout-only`; it is the only way to take `--shots`. A rule
  change goes in both `check()` in sweep.py and `Check()` in selftest.cpp.
- `--check-again` replays every problem's `again` line and checks that the
  screen it opens has the problem; for a walk's problem, that it ends on the
  page the problem names.

`tools/resize-check.sh` checks that a size changed in place shows what a fresh
start at that size shows on the same row: seven pages (Keys and buttons and
More options among them) from four sizes to nine, compared through `state`,
and F2 round the list and back (253 runs, about 30 seconds). Both runs start at the
first size, as a headless run's native size is its launch size.

`tools/baseline.py` checks a change meant to move code without changing what a
player sees. `record DIR` runs 32 scripts under a fixed clock (seeded walks of
400 keys at four sizes, with now and then a pad button, a size change or an
`open`, the sweep's layout walk, and the play walk: the Options, Save and
Load keys in play, the Menus key, a save, a delete and Quit's screen, with a
picture after every key) and keeps a `state` after every key and a hash of a
picture every 25 keys; `diff A B` prints, per run, the first state
that differs and which pictures differ, and exits 1 if any run does.

    $ tools/baseline.py record .runs/before    # on the commit before
    $ tools/baseline.py record .runs/after     # after the change, built
    $ tools/baseline.py diff .runs/before .runs/after --context 6

- `--context N` prints the N keys before the first difference and the page
  each one left.
- `--rename old=new` compares a state field renamed on purpose (its path from
  the top, as `bindings.heads`); `--ignore` leaves fields out by name at any
  depth, `--ignore-path` by their path from the top.
- The scripts are made from the first build recorded, into
  `.runs/baseline/scripts.json`; `scripts` makes them again (needed when a
  change moves where an Esc closes the menu, which the walks avoid).

## Checked against the game

`open original` draws today's Options page as lba2cc's boot menu does. Against
lba2cc's `ui menu-main-options` capture on the same data it is identical
outside the cursor row (whose plasma starts from a random seed in the game).
That check fixed three things: the boot menu darkens through the `citabau`
table (RESS 42), the 1997 stack is centred on y 260, and a row is 51 pixels
tall.
