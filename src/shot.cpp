// PNG of the logical canvas through the current palette: exactly what Log holds.
// Uncompressed deflate blocks, so no zlib.
#include "shot.h"

#include "host.h"

#include <SVGA/SCREEN.H>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long s_crcTable[256];

static void CrcInit() {
    for (unsigned n = 0; n < 256; n++) {
        unsigned long c = n;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? 0xedb88320UL ^ (c >> 1) : c >> 1;
        s_crcTable[n] = c;
    }
}

static unsigned long Crc(unsigned long crc, const unsigned char *buf, size_t len) {
    unsigned long c = crc ^ 0xffffffffUL;
    for (size_t i = 0; i < len; i++)
        c = s_crcTable[(c ^ buf[i]) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffUL;
}

static void Be32(unsigned char *p, unsigned long v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void Chunk(FILE *f, const char *type, const unsigned char *data, size_t len) {
    unsigned char hdr[8];
    Be32(hdr, (unsigned long)len);
    memcpy(hdr + 4, type, 4);
    fwrite(hdr, 1, 8, f);
    if (len)
        fwrite(data, 1, len, f);
    unsigned long crc = Crc(0, (const unsigned char *)type, 4);
    crc = Crc(crc, data, len);
    unsigned char c[4];
    Be32(c, crc);
    fwrite(c, 1, 4, f);
}

static bool WriteRows(const char *path, unsigned w, unsigned h, unsigned char *img);

bool Shot_Write(const char *path, const unsigned char *palette, int scale) {
    if (!s_crcTable[1])
        CrcInit();
    if (scale < 1)
        scale = 1;
    const unsigned w = Host_LogicalW() * scale, h = Host_LogicalH() * scale;
    const size_t rowLen = 1 + (size_t)w * 3;
    const size_t raw = rowLen * h;
    unsigned char *img = (unsigned char *)malloc(raw);
    const unsigned char *log = (const unsigned char *)Log;
    for (unsigned y = 0; y < h; y++) {
        unsigned char *row = img + y * rowLen;
        row[0] = 0;
        const unsigned char *src = log + TabOffLine[y / scale];
        for (unsigned x = 0; x < w; x++) {
            const unsigned char *rgb = palette + src[x / scale] * 3;
            row[1 + x * 3] = rgb[0];
            row[2 + x * 3] = rgb[1];
            row[3 + x * 3] = rgb[2];
        }
    }
    return WriteRows(path, w, h, img);
}

bool Shot_WriteArgb(const char *path, unsigned w, unsigned h, const unsigned *argb) {
    if (!s_crcTable[1])
        CrcInit();
    const size_t rowLen = 1 + (size_t)w * 3;
    unsigned char *img = (unsigned char *)malloc(rowLen * h);
    if (!img)
        return false;
    for (unsigned y = 0; y < h; y++) {
        unsigned char *row = img + y * rowLen;
        row[0] = 0;
        for (unsigned x = 0; x < w; x++) {
            const unsigned p = argb[(size_t)y * w + x];
            row[1 + x * 3] = (unsigned char)(p >> 16);
            row[2 + x * 3] = (unsigned char)(p >> 8);
            row[3 + x * 3] = (unsigned char)p;
        }
    }
    return WriteRows(path, w, h, img);
}

// Takes `img` (filter byte + RGB per row) and frees it.
static bool WriteRows(const char *path, unsigned w, unsigned h, unsigned char *img) {
    const size_t raw = (1 + (size_t)w * 3) * h;

    // zlib stream of stored blocks.
    size_t blocks = (raw + 65534) / 65535;
    size_t zlen = 2 + raw + blocks * 5 + 4;
    unsigned char *z = (unsigned char *)malloc(zlen);
    size_t o = 0;
    z[o++] = 0x78;
    z[o++] = 0x01;
    unsigned long a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) {
        a = (a + img[i]) % 65521;
        b = (b + a) % 65521;
    }
    for (size_t pos = 0; pos < raw; pos += 65535) {
        size_t n = raw - pos < 65535 ? raw - pos : 65535;
        z[o++] = (pos + n >= raw) ? 1 : 0;
        z[o++] = (unsigned char)(n & 0xff);
        z[o++] = (unsigned char)(n >> 8);
        z[o++] = (unsigned char)(~n & 0xff);
        z[o++] = (unsigned char)((~n >> 8) & 0xff);
        memcpy(z + o, img + pos, n);
        o += n;
    }
    Be32(z + o, (b << 16) | a);
    o += 4;

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(img);
        free(z);
        return false;
    }
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    unsigned char ihdr[13];
    Be32(ihdr, w);
    Be32(ihdr + 4, h);
    ihdr[8] = 8;   // bit depth
    ihdr[9] = 2;   // RGB
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    Chunk(f, "IHDR", ihdr, 13);
    Chunk(f, "IDAT", z, o);
    Chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(img);
    free(z);
    return true;
}
