/* -*- C++ -*-
 * File: mem_image.cpp
 * Copyright 2008-2025 LibRaw LLC (info@libraw.org)
 *
 * LibRaw mem_image/mem_thumb API test. Results should be same (bitwise) to
dcraw [-4] [-6] [-e]
 * Testing note: for ppm-thumbnails you should use dcraw -w -e for thumbnail
extraction

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).


 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "libraw/libraw.h"

#ifdef USE_JPEG
#include "jpeglib.h"
#endif

#ifdef LIBRAW_WIN32_CALLS
#define snprintf _snprintf
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netinet/in.h>
#endif

#ifdef USE_JPEG
void write_jpeg(libraw_processed_image_t *img, const char *basename, int quality)
{
  char fn[1024];
  if(img->colors != 1 && img->colors != 3)
  {
	printf("Only BW and 3-color images supported for JPEG output\n");
	return;
  }
  snprintf(fn, 1024, "%s.jpg", basename);
  FILE *f = fopen(fn, "wb");
  if (!f)
    return;
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1]; /* pointer to JSAMPLE row[s] */
  int row_stride;          /* physical row width in image buffer */

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, f);
  cinfo.image_width = img->width;      /* image width and height, in pixels */
  cinfo.image_height = img->height;
  cinfo.input_components = img->colors;           /* # of color components per pixel */
  cinfo.in_color_space = img->colors==3?JCS_RGB:JCS_GRAYSCALE;       /* colorspace of input image */
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, TRUE);
  jpeg_start_compress(&cinfo, TRUE);
  row_stride = img->width * img->colors; /* JSAMPLEs per row in image_buffer */
  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = &img->data[cinfo.next_scanline * row_stride];
    (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }
  jpeg_finish_compress(&cinfo);
  fclose(f);
  jpeg_destroy_compress(&cinfo);
}

#endif

// no error reporting, only params check
void write_ppm(libraw_processed_image_t *img, const char *basename)
{
  if (!img)
    return;
  // type SHOULD be LIBRAW_IMAGE_BITMAP, but we'll check
  if (img->type != LIBRAW_IMAGE_BITMAP)
    return;
  if (img->colors != 3 && img->colors != 1)
  {
    printf("Only monochrome and 3-color images supported for PPM output\n");
    return;
   }

  char fn[1024];
  snprintf(fn, 1024, "%s.p%cm", basename, img->colors==1?'g':'p');
  FILE *f = fopen(fn, "wb");
  if (!f)
    return;
  fprintf(f, "P%d\n%d %d\n%d\n", img->colors/2 + 5, img->width, img->height, (1 << img->bits) - 1);
/*
  NOTE:
  data in img->data is not converted to network byte order.
  So, we should swap values on some architectures for dcraw compatibility
  (unfortunately, xv cannot display 16-bit PPMs with network byte order data
*/
#define SWAP(a, b)                                                             \
  {                                                                            \
    a ^= b;                                                                    \
    a ^= (b ^= a);                                                             \
  }
  if (img->bits == 16 && htons(0x55aa) != 0x55aa)
    for (unsigned i = 0; i < img->data_size-1; i += 2)
      SWAP(img->data[i], img->data[i + 1]);
#undef SWAP

  fwrite(img->data, img->data_size, 1, f);
  fclose(f);
}

void write_thumb(libraw_processed_image_t *img, const char *basename)
{
  if (!img)
    return;

  if (img->type == LIBRAW_IMAGE_BITMAP)
  {
    char fnt[1024];
    snprintf(fnt, 1024, "%s.thumb", basename);
    write_ppm(img, fnt);
  }
  else if (img->type == LIBRAW_IMAGE_JPEG)
  {
    char fn[1024];
    snprintf(fn, 1024, "%s.thumb.jpg", basename);
    FILE *f = fopen(fn, "wb");
    if (!f)
      return;
    fwrite(img->data, img->data_size, 1, f);
    fclose(f);
  }
}

int main(int ac, char *av[])
{
  int i, ret, output_thumbs = 0;
#ifdef USE_JPEG
  int output_jpeg = 0, jpgqual = 90;
#endif
  // don't use fixed size buffers in real apps!

  LibRaw RawProcessor;

  if (ac < 2)
  {
    printf("mem_image - LibRaw sample, to illustrate work for memory buffers.\n"
           "Emulates dcraw [-4] [-1] [-e] [-h]\n"
#ifdef USE_JPEG
           "Usage: %s [-D] [-j[nn]] [-T] [-v] [-e] raw-files....\n"
#else
           "Usage: %s [-D] [-T] [-v] [-e] raw-files....\n"
#endif
           "\t-6 - output 16-bit PPM\n"
           "\t-4 - linear 16-bit data\n"
           "\t-e - extract thumbnails (same as dcraw -e in separate run)\n"
#ifdef USE_JPEG
           "\t-j[qual] - output JPEG with qual quality (e.g. -j90)\n"
#endif
           "\t-h - use half_size\n", av[0]);
    return 0;
  }

  putenv((char *)"TZ=UTC"); // dcraw compatibility, affects TIFF datestamp field

#define P1 RawProcessor.imgdata.idata
#define S RawProcessor.imgdata.sizes
#define C RawProcessor.imgdata.color
#define T RawProcessor.imgdata.thumbnail
#define P2 RawProcessor.imgdata.other
#define OUT RawProcessor.imgdata.params

  for (i = 1; i < ac; i++)
  {
    if (av[i][0] == '-')
    {
      if (av[i][1] == '6' && av[i][2] == 0)
        OUT.output_bps = 16;
      if (av[i][1] == '4' && av[i][2] == 0)
      {
        OUT.output_bps = 16;
        OUT.gamm[0] = OUT.gamm[1] = OUT.no_auto_bright = 1;
      }
      if (av[i][1] == 'e' && av[i][2] == 0)
        output_thumbs++;
      if (av[i][1] == 'h' && av[i][2] == 0)
        OUT.half_size = 1;
#ifdef USE_JPEG
      if (av[i][1] == 'j')
      {
        output_jpeg = 1;
        if(av[i][2] != 0)
        jpgqual = atoi(av[i]+2);
      } 
#endif
      continue;
    }
#ifdef USE_JPEG
    if(output_jpeg && OUT.output_bps>8)
    {
      printf("JPEG is limited to 8 bit\n");
      OUT.output_bps = 8;
    }
#endif
    printf("Processing %s\n", av[i]);
    if ((ret = RawProcessor.open_file(av[i])) != LIBRAW_SUCCESS)
    {
      fprintf(stderr, "Cannot open %s: %s\n", av[i], libraw_strerror(ret));
      continue; // no recycle b/c open file will recycle itself
    }

    if ((ret = RawProcessor.unpack()) != LIBRAW_SUCCESS)
    {
      fprintf(stderr, "Cannot unpack %s: %s\n", av[i], libraw_strerror(ret));
      continue;
    }

    // we should call dcraw_process before thumbnail extraction because for
    // some cameras (i.e. Kodak ones) white balance for thumbnail should be set
    // from main image settings

    ret = RawProcessor.dcraw_process();

    if (LIBRAW_SUCCESS != ret)
    {
      fprintf(stderr, "Cannot do postprocessing on %s: %s\n", av[i],
              libraw_strerror(ret));
      if (LIBRAW_FATAL_ERROR(ret))
        continue;
    }
    libraw_processed_image_t *image = RawProcessor.dcraw_make_mem_image(&ret);
    if (image)
    {
#ifdef USE_JPEG
      if(output_jpeg)
        write_jpeg(image, av[i], jpgqual);
      else
#endif
        write_ppm(image, av[i]);
      LibRaw::dcraw_clear_mem(image);
    }
    else
      fprintf(stderr, "Cannot unpack %s to memory buffer: %s\n", av[i],
              libraw_strerror(ret));

    if (output_thumbs)
    {

      if ((ret = RawProcessor.unpack_thumb()) != LIBRAW_SUCCESS)
      {
        fprintf(stderr, "Cannot unpack_thumb %s: %s\n", av[i],
                libraw_strerror(ret));
        if (LIBRAW_FATAL_ERROR(ret))
          continue; // skip to next file
      }
      else
      {
        libraw_processed_image_t *thumb =
            RawProcessor.dcraw_make_mem_thumb(&ret);
        if (thumb)
        {
          write_thumb(thumb, av[i]);
          LibRaw::dcraw_clear_mem(thumb);
        }
        else
          fprintf(stderr,
                  "Cannot unpack thumbnail of %s to memory buffer: %s\n", av[i],
                  libraw_strerror(ret));
      }
    }

    RawProcessor.recycle(); // just for show this call
  }
  return 0;
}
