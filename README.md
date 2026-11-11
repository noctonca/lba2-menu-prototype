# LBA2 menu prototype

A playable prototype of the Modern Options menu from lba2cc's menu plan
(docs/plan/MENU_PLAN.md) and its design review, drawn with the game's own
pieces: the menu palette, the LBA font, the plasma, the chamfered frames and
the darkening, read from the real game data.

Settings are fakes held in memory. Nothing here writes `lba2.cfg`.

## What it is built from

- `lba2cc/` is a submodule of [lba2-classic-community](https://github.com/LBALab/lba2-classic-community).
  Its `LIB386` libraries are built from the submodule as they are (window,
  palette, font, HQR and disc-image reading, polygon fill).
- `SOURCES/PLASMA.CPP`, `FIRE.CPP` and `ANIMTEX.CPP` are compiled straight from
  the submodule for the menu plasma.
- `src/cadre.cpp` copies the game-layer drawing helpers this needs
  (`DrawCadre`, `BackupAngles`/`RestoreAngles`, `ShadeBoxBlk`, `DrawFireBar`)
  and names where each came from.

GPL-2.0, as lba2cc.

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

## Play

    ./play.sh                      # 640x480, one column
    ./play.sh --size 1920x1080     # the wide layout (logical 960x540 at 2x)
    ./play.sh --first              # the first-launch question

Game data: `LBA2_GAME_DIR`, or `--data DIR`; the default is
`~/code/lba-hacking/assets/lba2-activision-us-cd` (a disc image is mounted as
the engine does).

### Screen sizes

The screen size changes in place, as lba2cc's own resolution switch does it:
the page, the cursor, the settings and every switch stay as they were, and a
note at the bottom right says the new size and its logical canvas.

- F2 steps to the next size in the list, Shift+F2 back to the previous one
  (640x480, 768x480, 1024x480, 1280x720, 1280x800, 1920x1080, 2400x1080,
  2560x1440, 3440x1440, 3840x2160). 768x480 and 1024x480 are lba2cc's
  widescreen modes: 480 high, they keep the one column on a wider canvas. The window is sized to suit, as at launch.
- Dragging the window's edge makes its size in pixels the screen: any size in
  between, the layout following as you drag. On a Retina display that is twice
  the window's size in points (a 1100x650 window is a 2200x1236 screen). The
  width is rounded down to a multiple of 8, as lba2cc requires, and the screen
  is at least 640x480.
- The Resolution row on Display and graphics does it as the game would: Left
  and Right step through the same list, and the row shows the screen in use
  (a dragged size too, "1512 x 904"). It is the player's screen, not a preset's
  choice, so it is never drawn as changed, and presets and Reset settings
  leave it alone.
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

To quit, Esc out of the Options, then Esc again on "Options closed" (Enter
opens them again); in fullscreen there is no close box. Cmd+Q works too.

Keys: arrows, Enter, Esc; a pad's d-pad or left stick, A and B. The prototype's
own switches are on F1 to F12 and 0 to 9.

| Key | Switch | What it changes |
| --- | --- | --- |
| F1 | `overlay` | this list on screen |
| F2 | | the next screen size; Shift+F2 the previous (see Screen sizes) |
| F3 | `band` | one band (help + legend) / a help line and a prompt bar |
| F4 | `typing` | help typed on every cursor move (decided) / on entering a page only / not typed |
| F5 | `chip` | the 8x8 key font at 2x (decided) / 1x |
| Shift+F5 | `tablefont` | the Experimental table's 8x8 font at 1x (decided) / 2x |
| F6 | `word` | the preset row: Preset: Modern, wide "Preset" with Modern where a value goes (2026-11-11) / Modern settings (decided 2026-11-03) / Settings: Modern / Rules: Modern |
| F7 | `island` | Citadel's palette, as Options opened in play would have |
| F8 | `help640` | help in the dialogue window on every one-column page (decided) / only where it must show |
| F9 | `gameplay` | the Gameplay category |
| F10 | `devices` | Controls: one device per view, Tab switches / Key 1, Key 2 and Gamepad side by side |
| F11 | `padnames` | the pad's names: the connected pad's / Xbox / PlayStation / Nintendo |
| F12 | `heads` | Controls, wide: the column names inside the first row, above its chips / a line of heads above the rows |
| 1 | `sheet` | wide: one 960x540 composition, centred on any larger screen / stretched to the screen's margins |
| 2 | `legend` | the band's legend stacked at its right / in a row beside the voice |
| 3 | `bg` | the background scaled evenly and cropped / stretched, as lba2cc does |
| 4 | `scale` | screens 3:2 or wider take the 960x540 composition at the largest whole step it fits, others the one column / the plan's rule |
| 5 | `panes` | wide: one pane at a time, centred, the page's name as a title / two panes side by side |
| 6 | `bandshade` | the band darkened to level 15 / 11 |
| 7 | `clash` | a key clash's note alone in the voice / added after the action's help |
| 8 | `short` | the shorter help, for translation, and a dimmed row saying why it is dimmed / the help as first written |
| | `keyrule` | a key another action has asks first, No (keep) or Yes (swap), as Options' questions (decided 2026-11-11) / taken, both chips red |
| 9 | `narrow` | 640 wide: key chips 116 wide, so "Behaviour action" clears them / 120 |
| 0 | `exptable` | More options: one dense table in the 8x8 font (decided) / the game-font list, a page per group |
| | `fpsrow` | the Frame rate row on Display and graphics, under Vsync (decided) / on Advanced and reset (no key: `--switch fpsrow=1`) |
| | `slider` | wide sliders: the 1997 way, the cursor row is the red plasma up to the value (2026-11-11) / a small slider at the value edge on every row |
| | `keyboard` | 1 a keyboard is there (a desktop) / 0 as a console before a key is typed: no Keyboard view or Tab, the legend names the pad |
| | `pace` | how long a window of long help is held before the next, in tenths of a second a word: 3 (decided) / 1 to 9 (no key: `--switch pace=5`) |

Keys 1 to 6 are the composition review of 2026-11-03 (wide and large screens).
All six are decided and on by default; each key turns its part off to compare.
F2 keeps every switch as it steps through the sizes. `./play-composition.sh`
plays the wide layout as it was before the review, then as decided, on the
same steps, with a trace per round.

Key 9 (decided 2026-11-03, on by default) narrows the one-column key chips.
Keys 7 and 8 are the help review of 2026-11-03, decided and on by default: one
message at a time in the voice, and help short enough that a translation half
as long again still fits two lines (the 1997 game's own translations of its
sentences run 1.1x English typically, up to 1.5x in German). Each key turns
its part off to compare. `./play-help.sh` plays the help as first written,
then as decided.

## The Controls page

Controls and camera > Keys and buttons opens the binding page of the design
review (`ControlsWideReview`). The 36 actions of lba2cc's `INPUT.H` sit in
seven groups, with the defaults of `INPUT_BINDINGS.CPP`; rebinding is held in
memory like the other settings.

- Wide: the groups on the left, the group's actions on the right with their
  chips. Right or Enter opens a group; Left from the first chip goes back.
- 640x480: the groups as a 1997 sub-menu, then a page of actions with the voice
  below.
- Tab (the pad's View button) switches Keyboard and Gamepad; the page opens on
  the one touched last.
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

## Driving it

Every run can be steered and read back without a person at the keys.

    build/lba2menu --headless --script steps.txt     # no window, fixed 16 ms clock
    build/lba2menu --do "open Sound; key down 2"     # commands inline; the window stays up on that screen
    build/lba2menu --control FILE                    # append commands to FILE while the window is up

Commands, one per line: `key <up|down|left|right|enter|esc|tab|del|f1..f12> [count]`,
`press <key>` (a raw key for rebinding: an SDL name such as `J`, `Left Alt` or
`Keypad 5`, or `pad:a`, `pad:lb`, `pad:lstick-up` ...),
`wait <frames>`, `state`, `measure`, `widths <file> [voice width]`, `shot <path.png> [scale]`, `switch <name> <value>`,
`set <setting> <value>`, `open <page name>`, `size <WxH>`, `quit`.

`measure` prints the width in the game font of every setting's label, its
widest value and its help, and of the hub's rows, for sizing panes.
`widths` measures any text the same way: each line of the file is
`tag<TAB>text` in CP850 (as TEXT.HQR holds it), and it prints the tag, the
width and the voice lines the text needs (620 wide unless given). It is how
the 1997 game's six languages were compared for the help review.
`--switch scale=N` is read before the window opens (the canvas depends on it);
`switch scale` at run time changes nothing.

- `state` prints one JSON line: layout, sizes, page, focus, cursor, every row
  drawn (label, value, fill, rectangle, dimmed, changed, change marks, whether
  it is a setting, where its text starts and ends: `text_left`, `text_right`,
  and the last pixel column of what it shows on the right, a value, a slider,
  steps or its last key chip: `value_right`, 0 for nothing), the help text
  with how many letters are typed, which window of it shows of how many
  (`page`, `pages`) and whether it was cut off, the legend, the
  switches and every setting.
- On the Controls page `state` adds `controls`: the device, group, action and
  column under the cursor, whether it waits for a key, every chip drawn
  (name, rectangle, fill, conflict, whether the name fits), the column heads
  above the actions or inside their first row (`heads`: text and rectangle),
  every binding and every conflict.
- `shot` writes exactly what is in the frame buffer through the current palette.
- Every key and every change is a trace line (`trace t=<ms> f=<frame> ...`) on
  stderr and in `--trace FILE`.
- Headless runs use a fixed clock and a fixed plasma seed, so a script gives
  the same pictures every time. They run with vsync off, as fast as the
  machine draws.

## More options

A More options row on Advanced and reset opens a page of the 41
values lba2cc's settings tables give a console name and no row in Options
(the Auto camera 22, the mouse 9, the gamepad 7, graphics 3), with their
defaults, ranges and console help as lba2cc has them, and three rows from
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
`experimental.cpp` stands in for it.

A fifth group, From 1997, holds three rows the 1997 Options had for the
machines of the day: Detail level (each step down takes away animated water,
rain, the far scenery, then shadows), Movies (at original size or fit to
screen; original size was for slow CD drives) and Reverse stereo. They came off Display and
Advanced (decided 2026-11-11); they keep their game labels, having no console
name, and the plan's 1997 Options page keeps them where they were. Movie
camera (1997's scenario cameras: off skips the outdoor shots that cut once) is
camera behaviour, not a machine's limit, so it went to Controls and camera
instead.

- Switch `exptable` (key 0): 1, the default and decided (2026-11-03), is the
  plan's dense table in the 8x8 font, as the 1997 key binding screen: a line per value under its group's
  head, the cursor a plasma strip, a status line with the default, the range and
  the lba2.cfg key when it fits. 0 is the game-font list: the groups, then a
  page of rows per group. The table's font is 1x (31 lines in view on a wide
  screen; decided 2026-11-03, as the 1997 key binding screen); Shift+F5 draws
  it at 2x (14) to compare.
- Left and Right change a value, Enter turns a switch, Del puts the default
  back, Page Up and Page Down (the pad's LB and RB) move a window at a time.
  A changed value takes the count colour.
- A changed value marks the preset changed. Choosing a preset, and Reset
  settings, put every value back to its default; Undo preset (after a
  preset) and Undo reset (after a reset) put them back and name them.
- The help is the console help, said in the voice. The game font has no `=`,
  `~` or `+`, so the voice says them as `:`, "about" and "and". Eleven console
  helps need three or four lines of the voice; they page on a window at a time
  (see the help in [docs/UX.md](docs/UX.md)) and are said as written.

`./play-experimental.sh` plays both forms at 1920x1080 and 640x480 on the
same steps, with a trace per round. The self test visits every row of it (see
Testing).

## Frame rate, the console and the version

- **Frame rate** (Display and graphics, under Vsync, decided 2026-11-03;
  `fpsrow=1` puts it on Advanced and reset to compare) shows the frame count in the bottom left corner, white,
  in the 8x8 font, as lba2cc's console `fps` and the 1997 SPEED code do. It is
  not kept in lba2.cfg: off at every start. Presets leave it alone. lba2cc
  draws it only in play; here it shows over the menu so the row can be judged.
  On Display it makes seven rows, so at 960x540 the page scrolls (Image is
  below); on Advanced it makes seven only with More options and Back
  to my settings both showing.
- **Console** (Advanced and reset, above Reset settings) is issue #679's row.
  Its value is the version, `0.13.3-dev 011394f` (lba2cc's version string and
  the commit's first 7; a release shows its number alone; an example here), in
  the dimmed grey: it reads, it does not change. Enter opens the console as
  lba2cc draws it: the top half, darkened, the 8x8 font, the messages of the
  launch (examples) with the version and the full commit in the first two
  lines. After a key it is the full console with the prompt, and Esc closes it
  (F12 in the game; F12 is a prototype switch here). After a pad button it is
  read-only, says so on its last line, and any button closes it.

`./play-console.sh` plays both places of Frame rate at 1920x1080 and 640x480 on
the same steps, with a trace per round.

## Testing

`lba2menu --selftest` visits every row of every page, every chip of the
Controls page and every row of More options at one screen size, for each of seven switch combinations, and
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
`value:` every setting row shows its value; `proximity:` a pane's last row is
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

- `esc:` from every row of every page, opened from the hub with Enter, Esc
  puts the cursor back where it was (the Controls page, its groups, the
  questions, and the wait for a key too).
- `reach:` Enter opens every page from the hub.
- `setting:` Left or Right changes the value shown and the other key puts it
  back; a dimmed row ignores Left, Right and Enter; Invert camera is dimmed
  with the Classic camera and wakes with Auto.
- `preset:` Classic asks first, starting on No, and Esc answers No; Back to my
  settings puts the preset row back; changing Fix retail bugs marks the preset
  changed. With Classic chosen and Vsync (a player's setting) changed, Reset
  settings asks first, starting on No; Esc and No change nothing; Yes puts the
  setting and the preset back as a fresh install has them, and a Back to my
  settings row comes right after Reset settings; Enter on it puts back the
  setting and Classic, and the row goes.
- `experimental:` Advanced and reset has the More options row; Enter opens the page and Esc comes back to the row;
  on every row Right and Left change the value and the other key puts it back,
  the change marks the preset row, and Del puts the default back; Page Down and
  Page Up stop at the last and first rows; Modern puts a changed value back to
  its default, Undo preset names it and puts it back; Reset settings
  and Undo reset do the same, and the cursor stays on Reset settings.
- `console:` the Console row shows the version; Enter after a key opens the
  full console, which Down and Enter leave open and Esc closes; Enter after a
  pad button opens it read-only, and a pad button, the d-pad or a key closes
  it; each time the cursor is back on Console. Turning Frame rate on leaves the
  preset row as it was.
- `bind:` on the first action of each group, in each column: Enter waits and
  the voice asks for the key; Esc cancels; the other device's input is ignored;
  a key another action has asks first, on No (the voice says "J is on
  Jetpack, ...", the answers take the actions' place), and Esc keeps both as
  they were; with
  `keyrule=1` it goes in, both chips go red and the help names the other
  action; Del clears it. On the pad, Start leaves the wait as Esc does, and
  Game menu's chip neither waits nor clears. Also: Forward's Key 2 put in
  Key 1 leaves Key 2 empty, and the wait gives up after 5 s.
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

    $ build/lba2menu --selftest --size 1920x1080 --combo band=1,gameplay=0 --only Sound
    1920x1080 {"band": 1, "gameplay": 0} [Sound] text spills 15 px: 'Master volume' '100'
        again: build/lba2menu --size 1920x1080 --switch band=1 --switch gameplay=0 --switch typing=2 --do "open Sound"

- `--combo name=v,name=v` (repeatable) tests those switch settings instead of
  the seven; any switch of `state` can go in. The seven are the chosen
  layouts and their variants; the help line and prompt bar (`band=0`) and all
  key columns side by side (`devices=0`) were not chosen and are left out.
- `--only PAGE` keeps one page (`"Keys and buttons"` for the Controls page,
  `"More options"`). `--combo exptable=0` tests the list form.
  The `esc:` and `reach:` walk needs every page, so it runs only without it.
- `--layout-only` leaves out the walks by keys.
- Add `--headless` and `; shot x.png` to an `again` line for a picture.

`tools/sweep.py` runs the self test for seven screen sizes and the seven
combinations, one process each, in parallel (about 3 seconds on the Mac), and
takes `--sizes`, `--combo`, `--only`, `--again` and `--layout-only` the same way.

- `--scripts` walks the same screens as `--layout-only` from outside: a script
  per run, read back through `state`, checked by the same rules written in
  Python. It is slower (about 10 seconds) and must list the same problems as
  `sweep.py --layout-only`; it is the only way to take `--shots`. A rule
  change goes in both `check()` in sweep.py and `Check()` in selftest.cpp.
- `--check-again` replays every problem's `again` line and checks that the
  screen it opens has the problem; for a walk's problem, that it ends on the
  page the problem names.

`tools/resize-check.sh` checks that a size changed in place shows what a fresh
start at that size shows on the same row: seven pages (Controls and
Experimental among them, and Experimental's list form) from four sizes to
nine, compared through `state`, and F2 round the list and back (289 runs,
about 50 seconds). With the layout hand-over turned off it finds 120 of them
differ.

## Checked against the game

`open original` draws today's Options page as lba2cc's boot menu does. Against
lba2cc's `ui menu-main-options` capture on the same data it is identical
outside the cursor row (whose plasma starts from a random seed in the game).
That check fixed three things: the boot menu darkens through the `citabau`
table (RESS 42), the 1997 stack is centred on y 260, and a row is 51 pixels
tall.
