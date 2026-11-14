# UX rules for the Options menu

The design system (the brand book: palette, one frame, one font, the row
pitch, the motion table) says *what* the pieces are. This file says *how
they're put together* for the Modern Options menu, and why. The why comes
from [Laws of UX](https://lawsofux.com), each law turned into rules for a
game menu driven by keys and a pad, not restated.

Three things check it:

- `lba2menu --selftest` (and `tools/sweep.py`) enforces every rule marked
  **[self test: name]**. The name is the start of the problem line it prints.
- The menu-ux-qa skill (`.claude/skills/menu-ux-qa`) measures the rest from
  screenshots and `state`, as a lead designer's pass.
- The maintainer's play test settles what only hands and eyes can: feel,
  timing, reading from a sofa.

A change that breaks a rule either fixes the rule here or doesn't land.
Rules the design review proposed and the maintainer has not accepted yet are
marked *(open call N)*, after the review's list of open calls; the prototype
builds them so they can be judged by play, and they become rules, or go, when
the call is answered.

## The players and the screens

- **Three players** (the review's): the one coming back (a 1080p or 1440p
  monitor or a television, opens Options once for resolution and volume); the
  keeper of the rules (keyboard, desktop, wants to see what changed); the
  first-timer on a sofa or a handheld (a pad, text read from a few metres).
- **Keys and a pad first.** Everything is reached with the arrows (or
  the d-pad and left stick), Enter (A), Esc (B), Tab (the pad's View) and Del
  (X). A key held repeats after 350 ms, every 90 ms (arrows only). The mouse
  and a finger point too (hover moves the cursor, a tap acts), and reach every
  row without a key.
- **The canvas.** 640x480 at 1x is the 1997 canvas and the one-column layout:
  it must look as today's Options do. Screens 3:2 or wider take the wide
  layout: one 960x540 composition (one pane at a time, centred, and a band) at
  the largest whole step that fits, centred both ways on the rest of the
  screen, as 1997 centred its 640x480 canvas. Narrower screens (4:3, 5:4) take
  the one column at the largest whole step that fits 640x480. Scale is a whole
  step only (1920x1080 is 960x540 at 2x; 2560x1440, 3440x1440 and 2560x1600
  are 2x with a border; 3840x2160 is 4x). Every number below is in logical
  pixels, measured from the composition's top left (decided 2026-11-03).
- **The picture** behind the menu is scaled evenly to cover the screen and
  cropped, never stretched. It fills the whole screen, border included
  (decided 2026-11-03).
- **Two distances.** A desk at arm's length and a sofa at a few metres. Rules
  about legibility are written for the sofa.

## The grid

| Layout | Edge | x (at logical 960) | What sits on it |
|---|---|---|---|
| One column | row box | W/2 - 275 .. W/2 + 275 (45 .. 595 at 640) | every row; labels centred, as 1997 |
| Wide | margin | 32 and W - 33 (W = 960) | the outer edge of the band |
| Wide | pane | W/2 - 320 .. W/2 + 319 (160 .. 799 at 960) | every row; the page's title centred on it |
| Wide | label edge | pane x0 + 16 | every label in the pane |
| Wide | voice edge | 48 (the band's x0 + 16) | the band's voice |
| Wide | value edge | row x1 - 24 | the right end of every value (text, slider, steps) |
| Wide, Controls | value edge | row x1 - 24 | the right end of the last key chip, as values |

- **One edge per pane.** Every row in a pane has the same x0 and x1, and in
  the wide layout every label starts at x0 + 16. **[self test: edge]**
- **Margins:** 32 at the sides, 24 at the top and bottom of the wide canvas.
  Nothing is drawn in them: rows, column heads, chips, the band.
  **[self test: margin]** *(review rule 8, open call 1)* The scroll bar sits
  beside the rows, 9 to the right: on the wide canvas inside the margins; at
  640x480 in the 1997 screen's right gap, with the rows kept centred (decided
  2026-11-03 from a side-by-side of beside, inside, none and a two-line help
  window).
- **A value never moves** when its row takes the cursor: the change marks sit
  in the row's padding, outside the value. *(review rule 4)*
- **One trailing edge:** in the wide layout, values and key chips end 24 in
  from their row's right edge on every page, measured to the last pixel drawn.
  **[self test: trailing]** The one-column Controls page keeps its chips 12
  in: at 640 wide, 24 runs action names into them. Its chips are 116 wide at
  2x (120 on the wide canvas), which holds the same 6 letters of the 8x8 font
  (and 14 in the pad view's one chip) and leaves the longest action name,
  "Behavior action", 8 clear of its first chip (decided 2026-11-03).
- **Controls column heads sit inside the first visible row**, above their
  chips, in small words at 1x: the one place 1x is used on the wide canvas,
  since 2x does not fit between a row's frame and its chip. Both panes then
  start at y 24 and their rows line up (decided 2026-11-03). With one pane at a time the group's name is
  the title and the actions start one pitch below it. The pad column's head reads "Gamepad", so the
  page names the device it shows.
- **A question takes the page's place** in the wide layout: its answers replace
  the page's rows under its title, on the label edge, and the voice asks it in
  the band. At
  640x480 it stays the centred box (decided 2026-11-03).
- **One pane at a time** (decided 2026-11-03): the hub's
  categories, then a page, as the 1997 menu goes from a page to a sub-page.
  Two panes side by side left the page 416 pixels at 960 wide, short of what
  six English labels and their values need (up to 556); one pane is 640 wide
  and holds them with 84 to spare. A page's name is its title (the brand
  book's Title, +4,+4 shadow), in the seventh row's place: a page shows six
  rows under it, the hub too (titled Options since 2026-11-13, as the column's
  hub is: rows keep their height between the hub and a page, and the corner
  Back sits beside a title, not against the first row). A longer page scrolls (Weapons,
  seven actions). The hub no longer previews a category's rows.

## Vertical rhythm

- Rows stack on the 1997 pitch: a row is 51 pixels tall (centre +-25), the
  next starts 56 below. Inside a pane, every row is on the pitch.
  **[self test: pitch]**
- The label's text cell starts at the row centre - 18 (the brand book's 7 px
  below the row's top). Measured: the cap band (22 px) of every label sits
  0.5 px above its row's centre, the nearest an even band gets. Text is never
  centred by hand.
- A centred label (one column, the question's rows) sits 1 to 1.5 px left of
  its row's centre, because the game measures a string with the spacing after
  its last letter. `open original` measures the same, so it is the 1997 look:
  keep it.
- **Proximity:** the gap between a pane's last row and the band (16 or more)
  is larger than the gap between rows (5). A column head sits closer to its
  rows than to anything above it. **[self test: proximity]**
- The band sits at the same y on every wide page (H - 110 .. H - 25: 430 to
  515 at 540), so seven rows from y 24 end at 411 and never meet it.
  *(review rule 10, open call 1)*

## Law by law

**Fitts's Law** (target size and distance) becomes *presses to reach*. There
is no pointer, so the cost of a target is the number of key presses to it.
- A page opens on its first setting. No page has a Return row (decided
  2026-11-12, EPmager: clutter): Esc is always one press back, whatever row
  has the cursor. **[self test: return]**
- **A mouse or a finger gets a Back button** on the title's line, a row's
  height (in the one column at the rows' left edge, wide in the margin left
  of the rows), shown once the pointer was used last and gone at a key or a
  pad's button, which have Esc and B. It always says Back, on a question
  too, where it does what Esc Cancel does. Every page has a title line for it, the one
  column's hub too, so nothing moves when it comes; a title too wide beside
  it swings, as a long 1997 row does (decided 2026-11-12). Back is an option:
  while the mouse is on it no row is lit and the help is quiet (a question
  still asks); it stays lit over empty space, as a row keeps the cursor. The
  first key while on Back only brings the cursor back to its row; the next
  one moves (decided 2026-11-13).
- A legend entry that takes a click lights under the mouse (its chip in the
  current plasma, its word white), so it reads as a button; a finger has no
  hover, so it lights only while pressed. No chips for the mouse's buttons:
  wide, the help shares the band with the legend, and two more chips there
  pushed ten translated help texts onto a second window (decided
  2026-11-12). **[self test: hover, pointer]**
- **Every row by the pointer alone.** A finger has no keys and no wheel: the
  scroll bar's thumb drags and its groove taps a viewful up or down, so on
  every page each row can take the cursor by pointing (2026-11-12).
  **[self test: pointer reach]**
- The cursor wraps at both ends of a list, so the last row is one press up.
- A slider crosses its range in about 16 steps; holding the key does it in
  under 2 s (350 ms, then 90 ms a step).
- **A row that throws work away asks first, and can be undone:** it is last
  on its page; its question starts on No and Esc answers No; afterwards its
  Undo row (Undo preset, Undo reset) puts back what it changed and says what. Choosing Classic and Reset settings both
  work this way (decided 2026-11-03). **[self test: preset]**

**Hick's Law, Choice Overload** (more options, slower decisions).
- A value with 2 to 4 options cycles with Left and Right in its row. A value
  with few steps shows its steps (framed steps, as Detail level had before it
  moved to More options).
  *(review rule 9)*
- A long list of the same kind (resolutions, languages beyond six) is a
  sub-page or a scroll list, never a value that takes ten presses to cycle.
- First launch asks one question with two answers.

**Miller's Law, Chunking** (working memory holds about 7 chunks).
- A page holds at most 7 rows (the brand book's "a plain menu does not
  scroll"); on the wide canvas, 6 under the page's title, the hub's too. More
  means a sub-page. Since the help and the title, a 640x480 page shows 4 rows,
  the hub too, and the rest scroll.
- The Controls page chunks 36 actions into 7 groups of 4 to 7.

**Jakob's Law** (people expect it to work like what they know). Two things
they know: the 1997 menus, and every console game's menu.
- 1997: the plasma
  cursor; the current-value plasma on the entry in use; Enter opens; Esc goes
  back. The entry in use also says "(current)" after its name ("(changed)"
  once a setting it chose is changed; wide, the row's value Changed), and the green
  plasma is off unless switch `greencurrent` (F3) is on: green beside the
  blue cursor read as a second cursor (EPmager, 2026-11-11). **[self test,
  sweep: current]**
- Consoles: A confirms and B goes back; Left and Right change a value;
  Tab (View) switches device; the prompts name the buttons of the pad in hand.
- The legend lists exactly the keys the selected row takes, by the name of
  the key or button that does it. *(review rule 10)*

**Law of Proximity** (near things read as a group). See vertical rhythm. The
voice and the legend share the band because both speak about the row under
the cursor. The legend is stacked at the band's right, one key per line, so
the voice keeps the band's width (decided 2026-11-03: a legend in a row left
the voice 248 to 312 px on the Controls page). The band holds three lines, so
four keys go in two columns read downwards, Back last at the bottom right as
everywhere else (2026-11-12: the groups page's Del Defaults pushed Back out of
the band; the voice keeps about 490 px there). **[self test: legend]** The first launch's question
sits inside the composition, in the band's place.
In the one column the legend is one line along the bottom of the 1997
dialogue window, in its third line: the 8x8 chips at 2x when the line fits,
1x (the 1997 key screen's size) when it does not, and the help keeps two
lines, as in the band; no help at 640x480 needs a third in any of the six
languages (measured 2026-11-11). Before, the one column had no legend, so a
pad player on a 4:3 or 5:4 screen saw no buttons at all (EPmager,
2026-11-11).

**Law of Similarity** (what looks alike is taken to work alike).
- One style per role: labels white in the game font; values in a column on the
  right; help in the voice; keys in the 8x8 font in chips. *(review rule 1,
  three registers)* Small words (the legend's words, the column heads, "1/2")
  are in the menu's face: Zilla Slab at the 8x8 font's size when the menu
  draws in Zilla Slab, the 8x8 font with the game font (EPmager, 2026-11-11:
  8x8 words beside Zilla Slab rows read as a second style). The 8x8 font
  stays where it means something: keys in chips, as a keycap or a pad's
  button would; the More options table, as the 1997 key screen; and
  developer text (the frame rate, the screen-size notice, the console)
  (decided 2026-11-12, after a day with every small text in Zilla Slab:
  chips in the body face read as more words, not keys).
- A chip centres its letters' ink, not their 8x8 cells (a cell leaves its
  right column and bottom row empty), and centres a capital's height, so
  chips side by side keep one baseline. A one-letter key's chip is square,
  as a keycap or a pad's button (EPmager, 2026-11-12). **[self test: chip,
  read from the frame buffer]**
- A row that opens something (a category, the preset row, Keys and buttons)
  takes Enter and shows "Open" in the legend; a setting takes Left and Right.
  A choice or a switch also steps on Enter, so its legend line shows both
  chips side by side, "<> Enter Change" ("<> A" on a pad); a slider, steps or
  a number shows the arrows alone (EPmager, 2026-11-11). **[self test, sweep:
  legend Enter]**
  The two never look the same: a setting shows a value, an opener does not.
- Every setting row shows its value. **[self test: value]**

**Law of Common Region** (a boundary makes a group). Every box is the one
frame. A pane is the rows in it; the band is one frame holding the voice and
the legend. Nothing nests a frame inside another with no gap.

**Law of Prägnanz.** One frame, chamfered 5 px, the same everywhere; no
second border, no shadow on a framed label. Joins checked at 3x. The
legend's chips take the frame too (EPmager, 2026-11-12); at 1x a chip is 12
px tall and 5 px corners close round its letters like brackets, so it is
chamfered 2 px.

**Von Restorff Effect** (the one that differs gets noticed).
- Exactly one cursor plasma on the screen. **[self test: N cursors]**
- At most one current-value plasma in a pane (the category being edited, the
  preset chosen). **[self test: current]**
- Red is for a key clash only, and a red chip under the cursor is explained in
  the voice, by the clash alone ("Cross is also Talk and search."), not added
  after the action's help (decided 2026-11-03). **[self test: red key, clash
  one way]**
- `ink-count` marks a changed value, nothing else *(review rule 5)*. The
  prototype draws it at index 158, a brighter step of the same bank than the
  token's 155, which measured 2.66:1 on a row (decided 2026-11-03).

**Serial Position Effect.** What the one coming back opens Options for goes
first: Resolution on Display, Master volume on Sound. What is rarely touched
and costly goes last: More options, Reset settings.

**Aesthetic-Usability Effect.** A row a pixel off makes the port feel
unfinished, and the review's whole aim is that nothing says "port" louder than
the game does.

**Doherty Threshold** (feedback within 400 ms).
- The cursor plasma and a changed value show on the frame the key lands on.
- Typed help is not feedback, it is reading: at one letter per 20 ms a
  40-letter line takes 800 ms. Typed again on every cursor move, a player
  scrolling past rows reads half lines. *Decided 2026-11-03: typed on every
  move, the 1997 voice kept; a player who stops on a row reads it whole.*

**Tesler's Law** (complexity has to live somewhere). The game carries it: two
presets, a fresh install that plays well, dimmed rows that only mean something
with another setting (Invert camera with the Auto camera) explained in the
voice. A setting exists only if players really disagree about it.

**Postel's Law** (be liberal in what you accept).
- Keys and pad both work everywhere, except while a binding waits: then only
  the device the view shows. Esc, Start on the pad, or 5 s, gives up the
  wait, and the voice says so: "Press a button, or <MENU> to keep it."
  In the voice a key or button is written in capitals inside angle brackets,
  as the 1997 text does ("<ESC> exit, <ENTER> Select a key"); chips, rows and
  the legend keep the key list's case (decided 2026-11-11). The game font
  draws < and > as small raised marks, so it reads ˚MENU˚ on screen.
  (decided 2026-11-11, replacing 2026-11-03's "no pad button is kept back").
- **No binding locks a pad-only player out** (decided 2026-11-11). Start
  always opens the game menu and can't be bound elsewhere, as Esc on the
  keyboard; the menu's A, B and arrows are fixed as the keyboard's Enter, Esc
  and arrows are. In lba2cc today only the arrows are: moving Start off Menus,
  or both confirm actions (Recenter, Behavior action) off the pad, leaves an
  Xbox, Switch, Vita or Android TV player no way back to Options (measured
  2026-11-11 on main, Vita and default layouts). **[self test: padonly]**
- A key already in use asks first, as choosing Classic and Reset settings do:
  the voice says who has it ("<J> is on Jetpack, in Magic and items."), and two
  answers take the actions' place, the safe one first: "No, keep it on
  Jetpack" and "Yes, Jetpack gets Up" (or "keeps Num 5", or "has no key").
  Esc answers No (decided 2026-11-11, replacing the refusal of 2026-11-03:
  "Clear C on Protection spell first." named the action but left the player to
  find it, clear it and come back; a question in the help alone was not read
  as something to answer). Still no silent side effects, and never one key on
  two actions from the page. Red stays for a
  clash that comes in from an edited lba2.cfg.

**Cognitive Load, Occam's Razor.** Every row earns its place. The voice says
what a setting does in the player's terms ("Footsteps, doors, magic."), not
the engine's.

**Paradox of the Active User** (people don't read manuals). Every setting
explains itself in the voice as the cursor lands on it; there is no help page.

- **Rows that say what they do** (decided 2026-11-14): the game menu's rows
  (Resume game, New game, Load game, Save game, Options, Quit) and a save have
  no help. Their names say what they do, so a line under them would repeat the
  label (Occam's Razor above), and neither the 1997 main menu nor a console's
  has one (Jakob's Law). They keep the legend: a pad player still needs to see
  which button opens and which goes back.
- The voice speaks there only when something happens or would be lost: "Game
  saved." after a save, until the cursor moves (1997 showed a brief overlay),
  and the questions. The game menu asks before New game, Load game or Quit
  after playing since the last save, before saving over a save and before a
  delete, each on No. 1997 asked none of them. **[self test: flow]**
- In the one column those pages take a strip one voice line tall in place of
  the dialogue window: the note at its left, the legend at its right. With the
  137 px window only 4 rows fit at 640x480, and the game menu's six would
  scroll where 1997 showed them all. A question there asks in the dialogue
  window, as everywhere. Wide, the band stays as it is.
- Before a game the game menu has nothing for Esc to go back to, so the
  legend shows no Esc and the corner Back is not drawn. Esc there puts the
  cursor on Quit, as 1997's first menu does (GAMEMENU.CPP, firstloop 2), so
  Esc then Enter leaves.
- The save lists are 1997's (Jakob's Law, for the players who know it):
  alphabetical; AUTOSAVE, the save written at each scene change, last in Load
  game, never in Save game and never deleted; Resume game plays the current
  save, written as a game starts, loads or is resumed. Each list opens on the
  save the game in play came from or was last saved as, otherwise on its last
  row, so Save game's New save is last. Checked in the retail DOS game
  (2026-11-14). **[self test: flow]**
- The save picture, 160x120 framed, is where 1997 has it: in place of the
  title on the first game menu (Resume game's), under the title on Load game
  (the save under the cursor's). It takes 128 px with its gap. The first game
  menu's five rows then fit 640x480 with the frame 4 px from the top (1997:
  9) and the last row 6 px above the strip, one more than between rows (the
  proximity rule); the wide frame keeps its 24 px top margin and takes 8 px
  from the gap above the band. In play the six rows would be
  71 px over, so the game menu shows no picture there, and the game it would
  show is behind the menu anyway (decided 2026-11-14). Load game shows 3 rows
  at a time with it, in both layouts. **[self test: flow, picture]**
- A save is named as lba2cc names one it makes, `<island> <date> <time>`,
  shorter than lba2cc's `<island> - <date> <hours-minutes-seconds>`: that
  spilled a 640x480 row by up to 33 px with the longest island, and 8 without
  the seconds (self test, 2026-11-14). The port's save list has to decide.

**Goal-Gradient, Zeigarnik.** The preset row says when the player's settings
are no longer the preset ("Preset: Modern, changed"), and the changed values
are in `ink-count`.

**Peak-End Rule.** First launch ends with the game, not with a settings
screen: one question, then play.

**Selective Attention, Banner Blindness.** The voice first says one short thing
about the row under the cursor. Help that fits one line in English *(review
rule 7)* is read while scrolling; help cut off is not. **[self test: help cut
off]**

- **Long help: a short line, then More** (2026-11-12, replaces "help never
  pages"; the capped help read as too short to be useful). A row's help is its
  short line, then a second window that says when or why to change it
  ("Everything at once." then "Turns all the sound up or down. The rows below
  set each part."). The writer ends a window with `@P`, as `TEXT.HQR` ends a
  1997 dialogue page (MESSAGE.CPP GetNextLine). Space or the pad's Y turns to
  the next window, and from the last back to the first; moving the cursor
  starts again at the short line. The legend shows More only when the help
  has another window. Rows without a second window keep their short line
  alone. **[self test: help in N windows]**
- **Every window fits its two lines.** A window too long for them goes on in
  the next, so a player still reads it all, but the self test reports more
  than two windows. The narrowest voices are the 640x480 dialogue window
  (591 px) and the Keys and buttons pages, where More widens the legend (445
  to 482 px). The console help on More options is in the table's own status
  lines, in the 8x8 font, two lines above the default and range, whole and in
  English; the voice there says the group's help, in the Text language.
  **[self test: table: row help cut off, voice text with no translation]**
- **Paged help says where it is with ellipses** (2026-11-12, replaces the "1/2"
  mark, which sat under the legend's Back at 640x480, and the dialogue's arrow,
  which asks for a key the menu never waits for). A window with more to come
  ends in "..." (its own full stop dropped), and a later window starts with
  one; its lines wrap short of the width by the ellipsis. Each window is typed
  as the cursor's help is. No window turns by itself.

**Jakob's Law (the player's own device).** The legend names what the player
holds (2026-11-11): after a pad press the pad's buttons (A Open, B Back), after
a key the keyboard's; the wait for a key keeps its own names. A keyboard is
offered only where there is one: on a console the Keyboard view and Tab come
once a key has been typed, and until then the legend names the pad.
**[self test: device]**

**The 1997 slider** (2026-11-11, after playing on the Xbox). In both layouts
the row under the cursor is the slider, the 1997 red plasma up to the value with
the label on it, and the other rows show only their label, as GAMEMENU.CPP
DrawOneChoice draws a volume. The wide layout first squeezed the plasma into a
strip at the value edge, which read badly on a TV.

- **One message at a time** (decided 2026-11-03). When there is something to
  tell about the row (a key clash, a question, the wait for a key, why a row
  is dimmed), it takes the help's place; nothing is added after the help. A
  dimmed row says why it is dimmed ("Only with the Auto camera."), not what it
  does.
- **Help leaves room for translation** (decided 2026-11-03; the 750 px cap
  dropped 2026-11-12). Each window is measured in all six Text languages
  instead of English being held to a budget: the 1997 game's own translations
  of their sentences run 1.1x English typically and up to 1.5x (German) in
  their longest tenth. Measure with `widths`.
- **The Text row changes the whole menu** (2026-11-12, after the maintainer
  changed the language and saw only the help change). Labels, values, page
  titles, answers, column heads and the legend's words are in `lang/menu.tsv`,
  translated where the game font and the legend are drawn, so every width the
  layout takes is the translated one. Key names stay as the keys read, and More
  options' tuning rows and their help stay English. Every self test walk runs
  in all six languages, and a drawn text with no translation is a problem.
  Shortened to fit the 275 px name column at 640x480: eleven action names (the
  spells, the behavior rows, turning) and three answers.
- **Text too wide for its room swings, as 1997 did** (2026-11-12). The
  original (GAMEMENU.CPP DrawOneChoice, "Shake Baby, shake...") draws a row
  wider than the menu from its left, clipped, and under the cursor slides it
  until its end shows and back. The prototype does the same for every row
  label and answer (Ui_TextInRoom, 48 px a second, restarting when the text
  changes): the room ends where the value, its marks or the first key chip
  begin. A fallback, not a fix: short text that fits stays the rule, and the
  self test still reports text that does not fit. Seen with an injected long
  French name on Keys and buttons at 640x480.
- **Every language is measured** (2026-11-12). The voice speaks the Text
  row's language (`lang/help.tsv`, stand-in translations made to measure). The self test fills each text's
  formats with long names and checks every language in both fonts: two band
  lines at 620 px, three dialogue lines at 591 px. Measured: translations run
  a median 1.08 to 1.19 times English and at most 1.6 to 2.0 times, the
  largest on short texts; every row's help fits, and the two questions and
  first launch fit after the Classic question was shortened.

## Keys

What each key does, and where the cursor goes. The walks of the self test
check the parts marked.

- **Up and Down** move the cursor on the page, wrapping.
- **Left and Right** change the value of a setting row, and the other key puts
  it back; a dimmed row ignores them. **[self test: setting]** On the wide
  hub, Right opens the category, as Enter does.
- **Enter** opens a page, or acts on the row; a choice row also cycles
  forward. Every page is reached from the hub by Enter. **[self test: reach]**
- **Esc steps back one level**: out of a wait for a key, out of a question
  (as No), out of a page back to the hub, out of a sub-page, and last out of the
  Options. It returns the cursor to the row that opened the level.
  **[self test: esc]**
- **A question starts on its safe answer** (No, keep Modern), and Esc answers
  No. **[self test: preset]**
- **Tab** switches Keyboard and Gamepad on the Controls page; the page opens
  on the device used last. **Del** clears a chip. **[self test: bind]**
- **Space** (the pad's Y) turns the help to its next window when the legend
  shows More. While a key change waits for a key, Space is taken as the key.

## Words and the voice

- The brand book's copy rules hold: sentence case, no terminal punctuation on
  rows, the game's dry register, settings words from lba2cc's glossary
  (Classic, Modern, settings backup).
- The voice speaks in the hero's dialogue colours with the dialogue shadow
  *(review rule 2)*. It never names a key the legend already shows.
- An unknown or empty value is left out, not shown as "-" mid-sentence. An
  empty key chip is a row of dots.

## Colour and contrast

- Palette indices only (brand book rule 1), checked in the menu palette.
- Minimum contrast against what is behind the text, measured from a
  screenshot: 3:1 for the game font (38 px line, large text), 4.5:1 for the
  8x8 font at 2x (16 px) and at 1x (the Experimental table: at least 5.6:1
  where the clouds show lightest through its panel, measured 2026-11-03). Disabled text stays readable and differs from
  enabled at a glance.
- A state shown by a fill, not text (a lit step against an unlit one, a dimmed
  slider against a live one), differs by 3:1 or more, on the cursor plasma
  too. One exception: the 1997 slider plasma (bank 4) reaches at most 1.7:1
  even against black, so a slider's filled part and a lit step may rely on a
  filled against an empty frame instead (decided 2026-11-03). Lit steps take
  that plasma, as the 1997 slider fill does; a dimmed slider is drawn without
  it, in the dimmed grey (5.6:1 against its empty track).
- The band is darkened to level 15, further than the 640x480 dialogue window,
  so the voice and the legend pass their contrast bars over light skies and
  the foam of the menu picture: at level 11 the voice measured 1.24:1 over
  the foam, at 15 it holds 3.15:1 or more at every screen size (decided
  2026-11-03). The 640x480 dialogue window keeps the 1997 level.

## Visual QA checklist

Every page, both layouts (640x480 and 1920x1080 at least), the pad name
families the self test runs.

1. `lba2menu --selftest` and `tools/sweep.py`: no problem against the rules
   above that isn't a known open call.
2. Every left edge on its pane's label edge; every value on the value edge;
   chips on the chip edge.
3. Labels centred on their row to 0.5 px (ink of the label colour).
4. Joins at 3x: chamfers, frames, chips, change marks, the scroll bar.
5. Contrast of every text colour on what is behind it, in both palettes.
6. One style per role across pages; one cursor; red only for clashes.
7. The legend lists exactly the keys the row takes.
8. Copy: what a setting does in the player's words; no help cut off.
9. *Play*: reading from a sofa distance, cursor feel and key repeat, the typed
   voice while scrolling, the wait for a key with a pad only (decided: the
   prompt says to wait).
