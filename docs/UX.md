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
- **Keys and a pad, no pointer.** Everything is reached with the arrows (or
  the d-pad and left stick), Enter (A), Esc (B), Tab (the pad's View) and Del
  (X). There is no hover and no touch in this menu. A key held repeats after
  350 ms, every 90 ms (arrows only).
- **The canvas.** 640x480 at 1x is the 1997 canvas and the one-column layout:
  it must look as today's Options do. From a logical width of 840 the wide
  layout takes over: two panes and a band. Scale is a whole step only
  (1920x1080 is logical 960x540 at 2x). Every number below is in logical
  pixels.
- **Two distances.** A desk at arm's length and a sofa at a few metres. Rules
  about legibility are written for the sofa.

## The grid

| Layout | Edge | x (at logical 960) | What sits on it |
|---|---|---|---|
| One column | row box | W/2 - 275 .. W/2 + 275 (45 .. 595 at 640) | every row; labels centred, as 1997 |
| Wide | margin | 32 and W - 33 | the outer edge of every pane and of the band |
| Wide | label edge | pane x0 + 16 | every label in a pane, and the band's voice |
| Wide | value edge | row x1 - 24 | the right end of every value (text, slider, steps) |
| Wide, Controls | value edge | row x1 - 24 | the right end of the last key chip, as values |

- **One edge per pane.** Every row in a pane has the same x0 and x1, and in
  the wide layout every label starts at x0 + 16. **[self test: edge]**
- **Margins:** 32 at the sides, 24 at the top and bottom of the wide canvas.
  Nothing is drawn in them: rows, column heads, chips, the band.
  **[self test: margin]** *(review rule 8, open call 1)*
- **A value never moves** when its row takes the cursor: the change marks sit
  in the row's padding, outside the value. *(review rule 4)*
- **One trailing edge:** in the wide layout, values and key chips end 24 in
  from their row's right edge on every page, measured to the last pixel drawn.
  **[self test: trailing]** The one-column Controls page keeps its chips 12
  in: at 640 wide, 24 runs action names into them.
- The panes' split may differ by page: Options splits 464:416 (right pane at
  512), Controls 304:576 (right pane at 352), because Controls needs the room
  for two chip columns beside an action's name (decided 2026-11-03).

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
- A page opens on its first row (one column: the Return row, as 1997; wide:
  the first setting). Esc is always one press back, whatever row has the
  cursor.
- The cursor wraps at both ends of a list, so the last row is one press up.
- A slider crosses its range in about 16 steps; holding the key does it in
  under 2 s (350 ms, then 90 ms a step).
- **A row that throws work away asks first, and can be undone:** it is last
  on its page; its question starts on No and Esc answers No; afterwards Back to
  my settings puts everything back. Choosing Classic and Reset settings both
  work this way (decided 2026-11-03). **[self test: preset]**

**Hick's Law, Choice Overload** (more options, slower decisions).
- A value with 2 to 4 options cycles with Left and Right in its row. A value
  with few steps shows its steps (Detail level: 3 framed steps).
  *(review rule 9)*
- A long list of the same kind (resolutions, languages beyond six) is a
  sub-page or a scroll list, never a value that takes ten presses to cycle.
- First launch asks one question with two answers.

**Miller's Law, Chunking** (working memory holds about 7 chunks).
- A page holds at most 7 rows, 8 on the 640x480 canvas with its Return row
  (the brand book's "a plain menu does not scroll"). More means a sub-page.
- The Controls page chunks 36 actions into 7 groups of 4 to 7.

**Jakob's Law** (people expect it to work like what they know). Two things
they know: the 1997 menus, and every console game's menu.
- 1997: Return to previous menu on top of a one-column sub-page; the plasma
  cursor; the current-value plasma on the entry in use; Enter opens; Esc goes
  back.
- Consoles: A confirms and B goes back; Left and Right change a value;
  Tab (View) switches device; the prompts name the buttons of the pad in hand.
- The legend lists exactly the keys the selected row takes, by the name of
  the key or button that does it. *(review rule 10)*

**Law of Proximity** (near things read as a group). See vertical rhythm. The
voice and the legend share the band because both speak about the row under
the cursor.

**Law of Similarity** (what looks alike is taken to work alike).
- One style per role: labels white in the game font; values in a column on the
  right; help in the voice; keys in the 8x8 font in chips. *(review rule 1,
  three registers)*
- A row that opens something (a category, the preset row, Keys and buttons)
  takes Enter and shows "Open" in the legend; a setting takes Left and Right.
  The two never look the same: a setting shows a value, an opener does not.
- Every setting row shows its value. **[self test: value]**

**Law of Common Region** (a boundary makes a group). Every box is the one
frame. A pane is the rows in it; the band is one frame holding the voice and
the legend. Nothing nests a frame inside another with no gap.

**Law of Prägnanz.** One frame, chamfered 5 px, the same everywhere; no
second border, no shadow on a framed label. Joins checked at 3x.

**Von Restorff Effect** (the one that differs gets noticed).
- Exactly one cursor plasma on the screen. **[self test: N cursors]**
- At most one current-value plasma in a pane (the category being edited, the
  preset chosen). **[self test: current]**
- Red is for a key clash only, and a red chip under the cursor is explained in
  the voice. **[self test: red key, clash one way]**
- `ink-count` marks a changed value, nothing else *(review rule 5)*. The
  prototype draws it at index 158, a brighter step of the same bank than the
  token's 155, which measured 2.66:1 on a row (decided 2026-11-03).

**Serial Position Effect.** What the one coming back opens Options for goes
first: Resolution on Display, Master volume on Sound. What is rarely touched
and costly goes last: Reset settings, Show experimental settings.

**Aesthetic-Usability Effect.** A row a pixel off makes the port feel
unfinished, and the review's whole aim is that nothing says "port" louder than
the game does.

**Doherty Threshold** (feedback within 400 ms).
- The cursor plasma and a changed value show on the frame the key lands on.
- Typed help is not feedback, it is reading: at one letter per 20 ms a
  40-letter line takes 800 ms. Typed again on every cursor move, a player
  scrolling past rows reads half lines. *(open call 2: typed on every move, on
  page entry only, or not typed)*

**Tesler's Law** (complexity has to live somewhere). The game carries it: two
presets, a fresh install that plays well, dimmed rows that only mean something
with another setting (Invert camera with the Auto camera) explained in the
voice. A setting exists only if players really disagree about it.

**Postel's Law** (be liberal in what you accept).
- Keys and pad both work everywhere, except while a binding waits: then only
  the device the view shows. Esc, or 5 s, gives up the wait.
- A key already in use is taken and both chips go red, rather than refused
  *(open call: refused, swapped, or allowed and marked)*.

**Cognitive Load, Occam's Razor.** Every row earns its place. The voice says
what a setting does in the player's terms ("Footsteps, doors, magic."), not
the engine's.

**Paradox of the Active User** (people don't read manuals). Every row explains
itself in the voice as the cursor lands on it; there is no help page.

**Goal-Gradient, Zeigarnik.** The preset row says when the player's settings
are no longer the preset ("Settings: Modern, changed"), and the changed values
are in `ink-count`.

**Peak-End Rule.** First launch ends with the game, not with a settings
screen: one question, then play.

**Selective Attention, Banner Blindness.** The voice says one short thing about
the row under the cursor. Help that fits one line in English *(review rule 7)*
is read; help cut off is not. **[self test: help cut off]**

## Keys

What each key does, and where the cursor goes. The walks of the self test
check the parts marked.

- **Up and Down** move the cursor in the focused pane, wrapping.
- **Left and Right** change the value of a setting row, and the other key puts
  it back; a dimmed row ignores them. **[self test: setting]** In the wide
  hub's left pane, Right opens the category, as Enter does.
- **Enter** opens a page, or acts on the row; a choice row also cycles
  forward. Every page is reached from the hub by Enter. **[self test: reach]**
- **Esc steps back one level**: out of a wait for a key, out of a question
  (as No), out of the right pane, out of a sub-page, and last out of the
  Options. It returns the cursor to the row that opened the level.
  **[self test: esc]**
- **A question starts on its safe answer** (No, keep Modern), and Esc answers
  No. **[self test: preset]**
- **Tab** switches Keyboard and Gamepad on the Controls page; the page opens
  on the device used last. **Del** clears a chip. **[self test: bind]**

## Words and the voice

- The brand book's copy rules hold: sentence case, no terminal punctuation on
  rows, the game's dry register, settings words from lba2cc's glossary
  (Classic, Modern, settings backup).
- The voice speaks in the hero's dialogue colours with the dialogue shadow
  *(review rule 2)*. It never names a key the legend already shows.
- An unknown or empty value is left out, not shown as "-" mid-sentence. An
  empty key chip is a row of dots.

## Colour and contrast

- Palette indices only (brand book rule 1), checked in the menu palette and
  on Citadel Island (F7).
- Minimum contrast against what is behind the text, measured from a
  screenshot: 3:1 for the game font (38 px line, large text), 4.5:1 for the
  8x8 font at 2x (16 px). Disabled text stays readable and differs from
  enabled at a glance.
- A state shown by a fill, not text (a lit step against an unlit one, a dimmed
  slider against a live one), differs by 3:1 or more, on the cursor plasma
  too. One exception: the 1997 slider plasma (bank 4) reaches at most 1.7:1
  even against black, so a slider's filled part and a lit step may rely on a
  filled against an empty frame instead (decided 2026-11-03). Lit steps take
  that plasma, as the 1997 slider fill does; a dimmed slider is drawn without
  it, in the dimmed grey (5.6:1 against its empty track).
- The band is darkened further than the 640x480 dialogue window, so the voice
  and the legend pass their contrast bars over light skies (decided
  2026-11-03). The 640x480 dialogue window keeps the 1997
  level.

## Visual QA checklist

Every page, both layouts (640x480 and 1920x1080 at least), both palettes
(menu and island), the switch combinations the self test runs.

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
   voice while scrolling, the wait for a key with a pad only.
