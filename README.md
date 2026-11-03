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

Keys: arrows, Enter, Esc; a pad's d-pad or left stick, A and B.

| Key | Switch | What it changes |
| --- | --- | --- |
| F1 | `overlay` | this list on screen |
| F2 | | the next screen size (640x480, 1280x720, 1280x800, 1920x1080, 2400x1080, 2560x1440) |
| F3 | `band` | one band (help + legend) / a help line and a prompt bar |
| F4 | `typing` | help typed on every cursor move / on entering a page only / not typed |
| F5 | `chip` | the 8x8 key font at 2x / 1x |
| F6 | `word` | the preset row's word: Settings / Preset / Rules |
| F7 | `island` | Citadel's palette, as Options opened in play would have |
| F8 | `help640` | help in the dialogue window on every one-column page |
| F9 | `gameplay` | the Gameplay category |
| F10 | `devices` | Controls: one device per view, Tab switches / Key 1, Key 2 and Gamepad side by side |
| F11 | `padnames` | the pad's names: the connected pad's / Xbox / PlayStation / Nintendo |

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
    build/lba2menu --control FILE                    # append commands to FILE while the window is up

Commands, one per line: `key <up|down|left|right|enter|esc|tab|del|f1..f11> [count]`,
`press <key>` (a raw key for rebinding: an SDL name such as `J`, `Left Alt` or
`Keypad 5`, or `pad:a`, `pad:lb`, `pad:lstick-up` ...),
`wait <frames>`, `state`, `shot <path.png> [scale]`, `switch <name> <value>`,
`set <setting> <value>`, `open <page name>`, `quit`.

- `state` prints one JSON line: layout, sizes, page, focus, cursor, every row
  drawn (label, value, fill, rectangle, dimmed, changed, change marks, where its
  text ends), the help text with how many letters are typed and whether it was
  cut off, the legend, the switches and every setting.
- On the Controls page `state` adds `controls`: the device, group, action and
  column under the cursor, whether it waits for a key, every chip drawn
  (name, rectangle, fill, conflict, whether the name fits), every binding and
  every conflict.
- `shot` writes exactly what is in the frame buffer through the current palette.
- Every key and every change is a trace line (`trace t=<ms> f=<frame> ...`) on
  stderr and in `--trace FILE`.
- Headless runs use a fixed clock and a fixed plasma seed, so a script gives
  the same pictures every time. They run with vsync off, as fast as the
  machine draws.

`tools/sweep.py` visits every row of every page and every chip of the Controls
page at several screen sizes and switch settings, and lists what does not fit:
help cut off, rows running into the help or off the screen, labels running into
their values or their key chips, key names cut in their chips.

## Checked against the game

`open original` draws today's Options page as lba2cc's boot menu does. Against
lba2cc's `ui menu-main-options` capture on the same data it is identical
outside the cursor row (whose plasma starts from a random seed in the game).
That check fixed three things: the boot menu darkens through the `citabau`
table (RESS 42), the 1997 stack is centred on y 260, and a row is 51 pixels
tall.
