#pragma once

// Write Log as a PNG through `palette` (256 x RGB, 8 bits each), scaled by a whole step.
bool Shot_Write(const char *path, const unsigned char *palette, int scale);
