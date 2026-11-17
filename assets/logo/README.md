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
