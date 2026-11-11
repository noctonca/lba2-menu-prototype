#include "trace.h"

#include "host.h"

#include <stdarg.h>
#include <stdio.h>

static FILE *s_file;
static unsigned s_frame;
static bool s_quiet;

void Trace_Quiet(bool on) { s_quiet = on; }

void Trace_Open(const char *path) {
    if (path)
        s_file = fopen(path, "w");
}

void Trace_SetFrame(unsigned frame) { s_frame = frame; }

void Trace(const char *fmt, ...) {
    if (s_quiet && !s_file)
        return;
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    unsigned t = (unsigned)Host_TimeMs();
    if (!s_quiet)
        fprintf(stderr, "trace t=%u f=%u %s\n", t, s_frame, msg);
    if (s_file) {
        fprintf(s_file, "trace t=%u f=%u %s\n", t, s_frame, msg);
        fflush(s_file);
    }
}
