# The LBA 2 logo

`lba2-logo.svg` is the gold LBA 2 logo as the game's intro shows it once it has
settled, as two closed outlines (`lba` and `two`) with the fill's left-to-right
colour ramp.

- **Source:** the intro video of Little Big Adventure 2 (1997), `video.hqr`
  entry 16 of the European release, 320x200 at 15 frames a second. The outline
  is traced from the median of frames 204 to 214, where the logo is held still.
- **Units:** one unit is one pixel of the video. The origin is the top left of
  the logo's ink, at (33.67, 23.08) of the video frame.
- **How it was traced:** the frames' brightest channel, upsampled 16 times and
  blurred by 0.45 of a video pixel, cut at half its local level; each outline
  smoothed as a closed cubic spline (233 curve segments in all). Against the
  frames' own edge the outline's coverage differs by 0.023 on average (0 to 1).
- **The ramp's stops** are the centres of the flat colour bands the video's
  palette gives the fill.
- **Who uses it:** `src/video_logo.cpp` reads the two paths at start and builds
  its distance field from them; CMake bakes the file into the binary.

The logo is the game's own and belongs to the game's rights holders. It is here
to title the game's menu in this prototype.

# The cc

`cc.svg` is two lower-case c's for LBA2 Classic Community, in the same units and
the same place as `lba2-logo.svg`: laid over it they sit under the 2, as its
subscript, so the mark reads LBA, 2, cc.

- **Source:** this project's own. Nothing in it is traced: `make_cc.py` draws
  each c by rule (a fat round stroke round an ellipse, open on the right, a ball
  on its upper end, leaning right) and writes the file. The shape, the size and
  the place were chosen from mock-ups.
- **Why it is a separate file:** the traced logo stays as the intro has it. The
  cc marks the menu as the community build's and not the 1997 game's.
- **Who uses it:** `src/video_logo.cpp` builds a field for each c and lays them
  over the logo (switch `logocc`), in the logo's own colours, after the intro's
  build-in has played.
