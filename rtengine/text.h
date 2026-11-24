// text.h
// 
// (c) Abraham Stolk
// Licensed via GPL.
//
#define FONTW		24
#define FONTXPITCH	32
#define FONTH		28

#ifdef __cplusplus
extern "C" {
#endif

extern int text_write_char(char c, float value, float* buf, int bufw, int bufh, int x, int line_stride);

extern int text_write_line(const char* t, float value, float* buf, int bufw, int bufh, int x, int line_stride);

#ifdef __cplusplus
}
#endif
