/* -*- C++ -*-
 * File: dcraw_half.c
 * Copyright 2008-2025 LibRaw LLC (info@libraw.org)
 * Created: Sat Mar  8 , 2008
 *
 * LibRaw  C API sample:  emulates "dcraw  -h"
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
#include <stdlib.h>
#include <math.h>

#include "libraw/libraw.h"

#define HANDLE_FATAL_ERROR(ret)                                                \
  if (ret)                                                                     \
  {                                                                            \
    fprintf(stderr, "%s: libraw  %s\n", av[i], libraw_strerror(ret));          \
    if (LIBRAW_FATAL_ERROR(ret))                                               \
      exit(1);                                                                 \
  }

#define HANDLE_ALL_ERRORS(ret)                                                 \
  if (ret)                                                                     \
  {                                                                            \
    fprintf(stderr, "%s: libraw  %s\n", av[i], libraw_strerror(ret));          \
    continue;                                                                  \
  }

int main(int ac, char *av[])
{
  int i;
  libraw_data_t *iprc = libraw_init(0);

  if (!iprc)
  {
    fprintf(stderr, "Cannot create libraw handle\n");
    exit(1);
  }

  iprc->params.half_size = 1; /* dcraw -h */

  for (i = 1; i < ac; i++)
  {
    char outfn[1024];
    int ret = libraw_open_file(iprc, av[i]);
    HANDLE_ALL_ERRORS(ret);

    printf("Processing %s (%s %s)\n", av[i], iprc->idata.make,
           iprc->idata.model);

    ret = libraw_unpack(iprc);
    HANDLE_ALL_ERRORS(ret);

    ret = libraw_dcraw_process(iprc);
    HANDLE_ALL_ERRORS(ret);

    strcpy(outfn, av[i]);
    strcat(outfn, ".ppm");
    printf("Writing to %s\n", outfn);

    ret = libraw_dcraw_ppm_tiff_writer(iprc, outfn);
    HANDLE_FATAL_ERROR(ret);
  }
  libraw_close(iprc);
  return 0;
}
