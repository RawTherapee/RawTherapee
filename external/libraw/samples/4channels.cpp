/* -*- C++ -*-
 * File: 4channels.cpp
 * Copyright 2008-2025 LibRaw LLC (info@libraw.org)
 * Created: Mon Feb 09, 2009
 *
 * LibRaw sample
 * Generates 4 TIFF file from RAW data, one file per channel
 *

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
#include <netinet/in.h>
#else
#include <winsock2.h>
#endif

#ifdef LIBRAW_WIN32_CALLS
#define snprintf _snprintf
#endif

int main(int ac, char *av[])
{
  int i, ret;
  int autoscale = 0, black_subtraction = 1, use_gamma = 0;
  char outfn[1024];

  LibRaw RawProcessor;
  if (ac < 2)
  {
  usage:
    printf("4channels - LibRaw %s sample. %d cameras supported\n"
           "Usage: %s [-s N] [-g] [-A] [-B] raw-files....\n"
           "\t-s N - select Nth image in file (default=0)\n"
           "\t-g - use gamma correction with gamma 2.2 (not precise,use for "
           "visual inspection only)\n"
           "\t-A - autoscaling (by integer factor)\n"
           "\t-B - no black subtraction\n",
           LibRaw::version(), LibRaw::cameraCount(), av[0]);
    return 0;
  }

#define P1 RawProcessor.imgdata.idata
#define S RawProcessor.imgdata.sizes
#define C RawProcessor.imgdata.color
#define T RawProcessor.imgdata.thumbnail
#define P2 RawProcessor.imgdata.other
#define OUT RawProcessor.imgdata.params
#define OUTR RawProcessor.imgdata.rawparams

  OUT.output_bps = 16;
  OUT.output_tiff = 1;
  OUT.user_flip = 0;
  OUT.no_auto_bright = 1;
  OUT.half_size = 1;

  for (i = 1; i < ac; i++)
  {
    if (av[i][0] == '-')
    {
      if (av[i][1] == 's' && av[i][2] == 0)
      {
        i++;
        OUTR.shot_select = av[i] ? atoi(av[i]) : 0;
      }
      else if (av[i][1] == 'g' && av[i][2] == 0)
        use_gamma = 1;
      else if (av[i][1] == 'A' && av[i][2] == 0)
        autoscale = 1;
      else if (av[i][1] == 'B' && av[i][2] == 0)
      {
        black_subtraction = 0;
      }
      else
        goto usage;
      continue;
    }
    if (!use_gamma)
      OUT.gamm[0] = OUT.gamm[1] = 1;

    int c;
    printf("Processing file %s\n", av[i]);
    if ((ret = RawProcessor.open_file(av[i])) != LIBRAW_SUCCESS)
    {
      fprintf(stderr, "Cannot open %s: %s\n", av[i], libraw_strerror(ret));
      continue; // no recycle b/c open file will recycle itself
    }
    if (P1.is_foveon)
    {
      printf("Cannot process Foveon image %s\n", av[i]);
      continue;
    }
    if ((ret = RawProcessor.unpack()) != LIBRAW_SUCCESS)
    {
      fprintf(stderr, "Cannot unpack %s: %s\n", av[i], libraw_strerror(ret));
      continue;
    }
    RawProcessor.raw2image();
    if (black_subtraction)
    {
      RawProcessor.subtract_black();
    }

    if (autoscale)
    {
      unsigned max = 0, scale = 1;
      for (int j = 0; j < S.iheight * S.iwidth; j++)
        for (int c = 0; c < 4; c++)
          if (max < RawProcessor.imgdata.image[j][c])
            max = RawProcessor.imgdata.image[j][c];
      if (max > 0 && max < 1 << 15)
      {
        scale = (1 << 16) / max;
        printf("Scaling with multiplier=%d (max=%d)\n", scale, max);
        for (int j = 0; j < S.iheight * S.iwidth; j++)
          for (c = 0; c < 4; c++)
            RawProcessor.imgdata.image[j][c] *= scale;
      }
      printf("Black level (scaled)=%d\n", C.black * scale);
    }
    else
      printf("Black level (unscaled)=%d\n", C.black);

    // hack to make dcraw tiff writer happy
    int isrgb = (P1.colors == 4 ? 0 : 1);
    P1.colors = 1;
    S.width = S.iwidth;
    S.height = S.iheight;

    for (int layer = 0; layer < 4; layer++)
    {
      if (layer > 0)
      {
        for (int rc = 0; rc < S.iheight * S.iwidth; rc++)
          RawProcessor.imgdata.image[rc][0] =
              RawProcessor.imgdata.image[rc][layer];
      }
      char lname[8];
      if (isrgb)
      {
        snprintf(lname, 7, "%c", ((char *)("RGBG"))[layer]);
        if (layer == 3)
          strcat(lname, "2");
      }
      else
        snprintf(lname, 7, "%c", ((char *)("GCMY"))[layer]);

      if (OUTR.shot_select)
        snprintf(outfn, sizeof(outfn), "%s-%d.%s.tiff", av[i], OUTR.shot_select,
                 lname);
      else
        snprintf(outfn, sizeof(outfn), "%s.%s.tiff", av[i], lname);

      printf("Writing file %s\n", outfn);
      if (LIBRAW_SUCCESS != (ret = RawProcessor.dcraw_ppm_tiff_writer(outfn)))
        fprintf(stderr, "Cannot write %s: %s\n", outfn, libraw_strerror(ret));
    }
  }
  return 0;
}
