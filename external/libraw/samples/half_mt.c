/* -*- C++ -*-
 * File: halt_mt.c
 * Copyright 2008-2025 LibRaw LLC (info@libraw.org)
 * Created: Sat Mar  8, 2008
 *
 * LibRaw  C API mutithreaded sample: emulates call to "dcraw  -h [-w] [-a]
[-v]"
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
#include <pthread.h>

#include "libraw/libraw.h"

#define HANDLE_ERRORS(ret)                                                     \
  do                                                                           \
  {                                                                            \
    if (ret)                                                                   \
    {                                                                          \
      fprintf(stderr, "%s: %s\n", fn, libraw_strerror(ret));                   \
      if (LIBRAW_FATAL_ERROR(ret))                                             \
      {                                                                        \
        libraw_close(iprc);                                                    \
        return NULL;                                                           \
      }                                                                        \
    }                                                                          \
  } while (0)

int verbose = 0, use_camera_wb = 0, use_auto_wb = 0, tiff_mode = 0;

pthread_mutex_t qm;
char **queue = NULL;
size_t qsize = 0, qptr = 0;

char *get_next_file()
{
  char *ret;
  if (!queue)
    return NULL;
  if (qptr >= qsize)
    return NULL;
  pthread_mutex_lock(&qm);
  ret = queue[qptr++];
  pthread_mutex_unlock(&qm);
  return ret;
}

void *process_files(void *q)
{
  int ret;
  int count = 0;
  char outfn[1024], *fn;
  libraw_data_t *iprc = libraw_init(0);

  if (!iprc)
  {
    fprintf(stderr, "Cannot create libraw handle\n");
    return NULL;
  }

  while ((fn = get_next_file()))
  {

    iprc->params.half_size = 1; /* dcraw -h */
    iprc->params.use_camera_wb = use_camera_wb;
    iprc->params.use_auto_wb = use_auto_wb;
    iprc->params.output_tiff = tiff_mode;

    ret = libraw_open_file(iprc, fn);
    if (verbose)
      fprintf(stderr, "%s: %s/%s\n", fn, iprc->idata.make, iprc->idata.model);
    HANDLE_ERRORS(ret);

    ret = libraw_unpack(iprc);
    HANDLE_ERRORS(ret);

    ret = libraw_dcraw_process(iprc);
    HANDLE_ERRORS(ret);

    snprintf(outfn, 1023, "%s.%s", fn, tiff_mode ? "tiff" : "ppm");

    if (verbose)
      fprintf(stderr, "Writing file %s\n", outfn);
    ret = libraw_dcraw_ppm_tiff_writer(iprc, outfn);
    HANDLE_ERRORS(ret);
    count++;
  }
  libraw_close(iprc);
  return NULL;
}

void usage(const char *p)
{
  printf("%s: Multi-threaded LibRaw sample app. Emulates dcraw -h [-w] [-a]\n",
         p);
  printf("Options:\n"
         "-J n  - set parallel job count (default 2)\n"
         "-v    - verbose\n"
         "-w    - use camera white balance\n"
         "-a    - average image for white balance\n");
  exit(1);
}

int show_files(void *q)
{
  char *p;
  int cnt = 0;
  while ((p = get_next_file()))
  {
    printf("%s\n", p);
    cnt++;
  }
  return cnt;
}

int main(int ac, char *av[])
{
  int i, max_threads = 2;
  pthread_t *threads;
  if (ac < 2)
    usage(av[0]);

  queue = calloc(ac - 1, sizeof(queue[0]));

  for (i = 1; i < ac; i++)
  {
    if (av[i][0] == '-')
    {
      if (av[i][1] == 'w')
        use_camera_wb = 1;
      if (av[i][1] == 'a')
        use_auto_wb = 1;
      if (av[i][1] == 'v')
        verbose = 1;
      if (av[i][1] == 'T')
        tiff_mode = 1;
      if (av[i][1] == 'J')
      {
        max_threads = atoi(av[++i]);
        if (max_threads < 1)
        {
          fprintf(stderr, "Job count should be at least 1\n");
          exit(1);
        }
      }
    }
    else
      queue[qsize++] = av[i];
  }
  pthread_mutex_init(&qm, NULL);
  threads = calloc(max_threads, sizeof(threads[0]));
  for (i = 0; i < max_threads; i++)
    pthread_create(&threads[i], NULL, process_files, NULL);
  for (i = 0; i < max_threads; i++)
  {
    int *iptr;
    if (threads[i])
    {
      pthread_join(threads[i], (void *)&iptr);
    }
  }

  return 0;
}
