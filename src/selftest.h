// The self test: every page, row and chip checked in one process (selftest.cpp).
#pragma once

#include <vector>

struct SelftestOptions {
    std::vector<const char *> combos;  // "band=1,gameplay=0"; empty: sweep.py's eight
    const char *only = nullptr;        // one page by name ("Keys and buttons", "Sound" ...)
    bool again = true;                 // print the command line that shows each problem
    bool layoutOnly = false;           // the layout walk only, as sweep.py --scripts checks it
};

// Prints each problem and a summary line; returns 0 when none, 1 otherwise, 2 on bad options.
int Selftest_Run(const SelftestOptions &opt, const char *self);
