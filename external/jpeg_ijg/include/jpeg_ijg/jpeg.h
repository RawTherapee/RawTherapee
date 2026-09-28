#pragma once

#include <csetjmp>

#ifdef __cplusplus
extern "C" {
#endif

#include <jpeglib.h>

extern GLOBAL(struct jpeg_error_mgr *)
my_jpeg_std_error (struct jpeg_error_mgr * err);

extern GLOBAL(void)
my_jpeg_stdio_src (j_decompress_ptr cinfo, FILE * infile);

GLOBAL(void)
jpeg_memory_src (j_decompress_ptr cinfo, const JOCTET * buffer, size_t bufsize);

/**
 * @brief jpeg from file and memory use this as base to managers
 */
typedef struct {
    struct jpeg_source_mgr pub; /* public fields */
    jmp_buf error_jmp_buf;      /* error handler for this instance */
} rt_jpeg_error_mgr;

#ifdef __cplusplus
}
#endif
