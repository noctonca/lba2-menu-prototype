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

Needs SDL3 (Homebrew `sdl3`).

## Play

    ./play.sh                      # 640x480, one column
    ./play.sh --size 1920x1080     # the wide layout (logical 960x540 at 2x)
    ./play.sh --first              # the first-launch question

Game data: `LBA2_GAME_DIR`, or `--data DIR`; the default is
`~/code/lba-hacking/assets/lba2-activision-us-cd` (a disc image is mounted as
the engine does).

Keys: arrows, Enter, Esc; a pad's d-pad or left stick, A and B. The prototype's
own switches are on F1 to F12 and 1 to 6.

| Key | Switch | What it changes |
| --- | --- | --- |
| F1 | `overlay` | this list on screen |
| F2 | | the next screen size (640x480, 1280x720, 1280x800, 1920x1080, 2400x1080, 2560x1440, 3440x1440, 3840x2160) |
| F3 | `band` | one band (help + legend) / a help line and a prompt bar |
| F4 | `typing` | help typed on every cursor move / on entering a page only / not typed |
| F5 | `chip` | the 8x8 key font at 2x / 1x |
| F6 | `word` | the preset row's word: Settings / Preset / Rules |
| F7 | `island` | Citadel's palette, as Options opened in play would have |
| F8 | `help640` | help in the dialogue window on every one-column page |
| F9 | `gameplay` | the Gameplay category |
| F10 | `devices` | Controls: one device per view, Tab switches / Key 1, Key 2 and Gamepad side by side |
| F11 | `padnames` | the pad's names: the connected pad's / Xbox / PlayStation / Nintendo |
| F12 | `heads` | Controls, wide: the column names inside the first row, above its chips / a line of heads above the rows |
| 1 | `sheet` | wide: one 960x540 composition, centred on any larger screen / stretched to the screen's margins |
| 2 | `legend` | the band's legend stacked at its right / in a row beside the voice |
| 3 | `bg` | the background scaled evenly and cropped / stretched, as lba2cc does |
| 4 | `scale` | screens 3:2 or wider take the 960x540 composition at the largest whole step it fits, others the one column / the plan's rule (relaunches) |
| 5 | `panes` | wide: one pane at a time, centred, the page's name as a title / two panes side by side |
| 6 | `bandshade` | the band darkened to level 15 / 11 |
| 7 | `clash` | a key clash's note alone in the voice / added after the action's help |
| 8 | `short` | the shorter help, for translation, and a dimmed row saying why it is dimmed / the help as first written |
| 9 | `narrow` | 640 wide: key chips 116 wide, so "Behaviour action" clears them / 120 |

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
  asks for it. Esc cancels, and so does waiting 5 seconds. A pad view only takes
  pad input, a keyboard view only keys.
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
`set <setting> <value>`, `open <page name>`, `quit`.

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
  with how many letters are typed and whether it was cut off, the legend, the
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

## Testing

`lba2menu --selftest` visits every row of every page and every chip of the
Controls page at one screen size, for each of nine switch combinations, and
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
  changed. With Classic chosen and Show experimental settings changed, Reset
  settings asks first, starting on No; Esc and No change nothing; Yes puts the
  setting and the preset back as a fresh install has them, and a Back to my
  settings row comes right after Reset settings; Enter on it puts back the
  setting and Classic, and the row goes.
- `bind:` on the first action of each group, in each column: Enter waits and
  the voice asks for the key; Esc cancels; the other device's input is ignored;
  a key another action has goes in, both chips go red and the help names the
  other action; Del clears it. Also: Forward's Key 2 put in Key 1 leaves Key 2
  empty, and the wait gives up after 5 s.

It runs in one process without drawing to a window (nor drawing the
background and row shading, which the checks never read), and exits 1 when it
finds a problem. Under each problem it prints a command line that opens the
screen it was found on:

    $ build/lba2menu --selftest --size 1920x1080 --combo band=1,gameplay=0 --only Sound
    1920x1080 {"band": 1, "gameplay": 0} [Sound] text spills 15 px: 'Master volume' '100'
        again: build/lba2menu --size 1920x1080 --switch band=1 --switch gameplay=0 --switch typing=2 --do "open Sound"

- `--combo name=v,name=v` (repeatable) tests those switch settings instead of
  the nine; any switch of `state` can go in.
- `--only PAGE` keeps one page (`"Keys and buttons"` for the Controls page).
  The `esc:` and `reach:` walk needs every page, so it runs only without it.
- `--layout-only` leaves out the walks by keys.
- Add `--headless` and `; shot x.png` to an `again` line for a picture.

`tools/sweep.py` runs the self test for five screen sizes and the nine
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

## Checked against the game

`open original` draws today's Options page as lba2cc's boot menu does. Against
lba2cc's `ui menu-main-options` capture on the same data it is identical
outside the cursor row (whose plasma starts from a random seed in the game).
That check fixed three things: the boot menu darkens through the `citabau`
table (RESS 42), the 1997 stack is centred on y 260, and a row is 51 pixels
tall.
