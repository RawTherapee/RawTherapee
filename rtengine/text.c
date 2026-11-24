// text.c
// 
// (c) Abraham Stolk
// Licensed via GPL.

// Our libc dependencies.
#include <inttypes.h>
#include <string.h>

// Our font data is 24 * 28 pixels, fixed width, ascii only.
#include "fntdat.h"

// Our own interface.
#include "text.h"


int text_write_char(char c, float value, float* buf, int bufw, int bufh, int x, int line_stride) {
    if (x < 0)
        return 0; // clip left
    if (x + FONTW > bufw)
        return 0; // clip right
    if (c < 32 || c > 127)
        return 0; // not ascii character.

    c -= 32;
    int numpixels = 0;
    for (int row=0; row<FONTH; ++row) {
        float* writer = buf + x + row * line_stride;
        for (int byte=0; byte<3; ++byte) {
            const uint8_t pix8 = fntdat[c * (FONTH*3) + row*3 + byte];
            for (int bit=0; bit < 8; ++bit) {
                if (pix8 & (0x80>>bit)) {
                    *writer = value;
                    numpixels += 1;
                }
                writer += 1;
            }
        }
    }
    return numpixels;
}


int text_write_line(const char* t, float value, float* buf, int bufw, int bufh, int x, int line_stride) {
    const int len = (int) strlen(t);
    if (!len) return 0;

    int totalpixels = 0;
    for (int charnr=0; charnr<len; ++charnr) {
        const int xpos = x + charnr * FONTXPITCH;
        const int numpixels = text_write_char(t[charnr], value, buf, bufw, bufh, xpos, line_stride);
        totalpixels += numpixels;
    }
    return totalpixels;
}

