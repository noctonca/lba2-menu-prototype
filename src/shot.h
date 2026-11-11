#pragma once

// Write Log as a PNG through `palette` (256 x RGB, 8 bits each), scaled by a whole step.
bool Shot_Write(const char *path, const unsigned char *palette, int scale);

// Write a w x h frame of 0xAARRGGBB pixels as a PNG.
bool Shot_WriteArgb(const char *path, unsigned w, unsigned h, const unsigned *argb);
