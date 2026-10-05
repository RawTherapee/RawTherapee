/* -*- C++ -*-
 * File: multirender_test.cpp
 * Copyright 2008-2025 LibRaw LLC (info@libraw.org)
 * Created: Jul 10, 2011
 *
 * LibRaw simple C++ API:  creates 8 different renderings from 1 source file.
The 1st and 4th one should be identical

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

int process_once(LibRaw &RawProcessor, int half_mode, int camera_wb,
                 int auto_wb, int suffix, int user_flip, char *fname)
{
  char outfn[1024];
  RawProcessor.imgdata.params.half_size = half_mode;
  RawProcessor.imgdata.params.use_camera_wb = camera_wb;
  RawProcessor.imgdata.params.use_auto_wb = auto_wb;
  RawProcessor.imgdata.params.user_flip = user_flip;

  int ret = RawProcessor.dcraw_process();

  if (LIBRAW_SUCCESS != ret)
  {
    fprintf(stderr, "Cannot do postprocessing on %s: %s\n", fname,
            libraw_strerror(ret));
    return ret;
  }
  snprintf(outfn, sizeof(outfn), "%s.%d.%s", fname, suffix,
           (RawProcessor.imgdata.idata.colors > 1 ? "ppm" : "pgm"));

  printf("Writing file %s\n", outfn);

  if (LIBRAW_SUCCESS != (ret = RawProcessor.dcraw_ppm_tiff_writer(outfn)))
    fprintf(stderr, "Cannot write %s: %s\n", outfn, libraw_strerror(ret));
  return ret;
}

int main(int ac, char *av[])
{
  int i, ret;

  LibRaw RawProcessor;
  if (ac < 2)
  {
    printf("multirender_test - LibRaw %s sample. Performs 4 different "
           "renderings of one file\n"
           " %d cameras supported\n"
           "Usage: %s raw-files....\n",
           LibRaw::version(), LibRaw::cameraCount(), av[0]);
    return 0;
  }

  for (i = 1; i < ac; i++)
  {

    printf("Processing file %s\n", av[i]);

    if ((ret = RawProcessor.open_file(av[i])) != LIBRAW_SUCCESS)
    {
      fprintf(stderr, "Cannot open_file %s: %s\n", av[i], libraw_strerror(ret));
      continue; // no recycle b/c open file will recycle itself
    }

    if ((ret = RawProcessor.unpack()) != LIBRAW_SUCCESS)
    {
      fprintf(stderr, "Cannot unpack %s: %s\n", av[i], libraw_strerror(ret));
      continue;
    }
    process_once(RawProcessor, 0, 0, 0, 1, -1, av[i]); // default flip
    process_once(RawProcessor, 1, 0, 1, 2, -1, av[i]);
    process_once(RawProcessor, 1, 1, 0, 3, -1, av[i]); // default flip
    process_once(RawProcessor, 1, 1, 0, 4, 1, av[i]);  // flip 1
    process_once(RawProcessor, 1, 1, 0, 5, 3, av[i]);  // flip 3
    process_once(RawProcessor, 1, 1, 0, 6, 1, av[i]);  // 1 again same as 4
    process_once(RawProcessor, 1, 1, 0, 7, -1,
                 av[i]); // default again, same as 3
    process_once(RawProcessor, 0, 0, 0, 8, -1, av[i]); // same as 1

    RawProcessor.recycle(); // just for show this call
  }
  return 0;
}
