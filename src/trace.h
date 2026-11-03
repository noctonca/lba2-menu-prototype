// One line per input and state change: "trace t=<ms> f=<frame> <what>".
#pragma once

void Trace_Open(const char *path);  // NULL: stderr only
void Trace_SetFrame(unsigned frame);
void Trace(const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
