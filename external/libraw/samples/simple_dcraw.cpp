/* -*- C++ -*-
 * File: simple_dcraw.cpp
 * Copyright 2008-2025 LibRaw LLC (info@libraw.org)
 * Created: Sat Mar  8, 2008
 *
 * LibRaw simple C++ API:  emulates call to "dcraw  [-D]  [-T] [-v] [-e] [-4]"

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

#ifndef LIBRAW_WIN32_CALLS
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#endif

#ifdef LIBRAW_WIN32_CALLS
#define snprintf _snprintf
#endif

int my_progress_callback(void *unused_data, enum LibRaw_progress state,
                         int iter, int expected)
{
  if (iter == 0)
    printf("CB: state=%x, expected %d iterations\n", state, expected);
  return 0;
}

char *customCameras[] = {
    (char *)"43704960,4080,5356, 0, 0, 0, 0,0,148,0,0, Dalsa, FTF4052C Full,0",
    (char *)"42837504,4008,5344, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF4052C 3:4",
    (char *)"32128128,4008,4008, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF4052C 1:1",
    (char *)"24096096,4008,3006, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF4052C 4:3",
    (char *)"18068064,4008,2254, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF4052C 16:9",
    (char *)"67686894,5049,6703, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF5066C Full",
    (char *)"66573312,4992,6668, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF5066C 3:4",
    (char *)"49840128,4992,4992, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF5066C 1:1",
    (char *)"37400064,4992,3746, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF5066C 4:3",
    (char *)"28035072,4992,2808, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF5066C 16:9",
    NULL};

int main(int ac, char *av[])
{
  int i, ret, verbose = 0, output_thumbs = 0, output_all_thumbs = 0;

  // don't use fixed size buffers in real apps!
  char outfn[1024], thumbfn[1024];

  LibRaw* RawProcessor = new LibRaw;
  RawProcessor->imgdata.rawparams.custom_camera_strings = customCameras;
  if (ac < 2)
  {
    printf("simple_dcraw - LibRaw %s sample. Emulates dcraw [-D] [-T] [-v] "
           "[-e] [-E]\n"
           " %d cameras supported\n"
           "Usage: %s [-D] [-T] [-v] [-e] raw-files....\n"
           "\t-4 - 16-bit mode\n"
           "\t-L - list supported cameras and exit\n"
           "\t-v - verbose output\n"
           "\t-T - output TIFF files instead of .pgm/ppm\n"
           "\t-e - extract thumbnails (same as dcraw -e in separate run)\n"
           "\t-E - extract all thumbnails\n",
           LibRaw::version(), LibRaw::cameraCount(), av[0]);
    delete RawProcessor;
    return 0;
  }

  putenv((char *)"TZ=UTC"); // dcraw compatibility, affects TIFF datestamp field

#define P1 RawProcessor->imgdata.idata
#define S RawProcessor->imgdata.sizes
#define C RawProcessor->imgdata.color
#define T RawProcessor->imgdata.thumbnail
#define P2 RawProcessor->imgdata.other
#define OUT RawProcessor->imgdata.params

  for (i = 1; i < ac; i++)
  {
    if (av[i][0] == '-')
    {
      if (av[i][1] == 'T' && av[i][2] == 0)
        OUT.output_tiff = 1;
      if (av[i][1] == 'v' && av[i][2] == 0)
        verbose++;
      if (av[i][1] == 'e' && av[i][2] == 0)
        output_thumbs++;
      if (av[i][1] == 'E' && av[i][2] == 0)
      {
        output_thumbs++;
        output_all_thumbs++;
      }
      if (av[i][1] == '4' && av[i][2] == 0)
        OUT.output_bps = 16;
      if (av[i][1] == 'C' && av[i][2] == 0)
        RawProcessor->set_progress_handler(my_progress_callback, NULL);
      if (av[i][1] == 'L' && av[i][2] == 0)
      {
        const char **clist = LibRaw::cameraList();
        const char **cc = clist;
        while (*cc)
        {
          printf("%s\n", *cc);
          cc++;
        }
        delete RawProcessor;
        exit(0);
      }
      continue;
    }

    if (verbose)
      printf("Processing file %s\n", av[i]);

	RawProcessor->imgdata.rawparams.options |= LIBRAW_RAWOPTIONS_ALLOW_JPEGXL_PREVIEWS;

    if ((ret = RawProcessor->open_file(av[i])) != LIBRAW_SUCCESS)
    {
      fprintf(stderr, "Cannot open_file %s: %s\n", av[i], libraw_strerror(ret));
      continue; // no recycle b/c open file will recycle itself
    }

    if (!output_thumbs) // No unpack for thumb extraction
      if ((ret = RawProcessor->unpack()) != LIBRAW_SUCCESS)
      {
        fprintf(stderr, "Cannot unpack %s: %s\n", av[i], libraw_strerror(ret));
        continue;
      }

    // thumbnail unpacking and output in the middle of main
    // image processing - for test purposes!
    if(output_all_thumbs)
    {
      if (verbose)
        printf("Extracting %d thumbnails\n", RawProcessor->imgdata.thumbs_list.thumbcount);
      for (int t = 0; t < RawProcessor->imgdata.thumbs_list.thumbcount; t++)
      {
        if ((ret = RawProcessor->unpack_thumb_ex(t)) != LIBRAW_SUCCESS)
        {
          fprintf(stderr, "Cannot unpack_thumb #%d from %s: %s\n", t, av[i], libraw_strerror(ret));
          if (LIBRAW_FATAL_ERROR(ret))
            break; // skip to next file
        }
        else
        {
          snprintf(thumbfn, sizeof(thumbfn), "%s.thumb.%d.%s", av[i], t,
                   T.tformat == LIBRAW_THUMBNAIL_JPEG
                       ? "jpg"
                       : (T.tformat == LIBRAW_THUMBNAIL_JPEGXL ? "jxl" : (T.tcolors == 1 ? "pgm" : "ppm")));
          if (verbose)
            printf("Writing thumbnail file %s\n", thumbfn);
          if (LIBRAW_SUCCESS != (ret = RawProcessor->dcraw_thumb_writer(thumbfn)))
          {
            fprintf(stderr, "Cannot write %s: %s\n", thumbfn, libraw_strerror(ret));
            if (LIBRAW_FATAL_ERROR(ret))
              break;
          }
        }
      }
      continue;
    }
    else if (output_thumbs)
    {
      if ((ret = RawProcessor->unpack_thumb()) != LIBRAW_SUCCESS)
      {
        fprintf(stderr, "Cannot unpack_thumb %s: %s\n", av[i],
                libraw_strerror(ret));
        if (LIBRAW_FATAL_ERROR(ret))
          continue; // skip to next file
      }
      else
      {
        snprintf(thumbfn, sizeof(thumbfn), "%s.%s", av[i],
                 T.tformat == LIBRAW_THUMBNAIL_JPEG ? "thumb.jpg"
					:  (T.tformat == LIBRAW_THUMBNAIL_JPEGXL ? "thumb.jxl"
                         : (T.tcolors == 1? "thumb.pgm" : "thumb.ppm")));
        if (verbose)
          printf("Writing thumbnail file %s\n", thumbfn);
        if (LIBRAW_SUCCESS != (ret = RawProcessor->dcraw_thumb_writer(thumbfn)))
        {
          fprintf(stderr, "Cannot write %s: %s\n", thumbfn,
                  libraw_strerror(ret));
          if (LIBRAW_FATAL_ERROR(ret))
            continue;
        }
      }
      continue;
    }

    ret = RawProcessor->dcraw_process();

    if (LIBRAW_SUCCESS != ret)
    {
      fprintf(stderr, "Cannot do postprocessing on %s: %s\n", av[i],
              libraw_strerror(ret));
      if (LIBRAW_FATAL_ERROR(ret))
        continue;
    }
    snprintf(outfn, sizeof(outfn), "%s.%s", av[i],
             OUT.output_tiff ? "tiff" : (P1.colors > 1 ? "ppm" : "pgm"));

    if (verbose)
      printf("Writing file %s\n", outfn);

    if (LIBRAW_SUCCESS != (ret = RawProcessor->dcraw_ppm_tiff_writer(outfn)))
      fprintf(stderr, "Cannot write %s: %s\n", outfn, libraw_strerror(ret));

    RawProcessor->recycle(); // just for show this call
  }
  
  delete RawProcessor;
  return 0;
}
