/* -*- C++ -*-
 * File: dcraw_emu.cpp
 * Copyright 2008-2025 LibRaw LLC (info@libraw.org)
 * Created: Sun Mar 23,   2008
 *
 * LibRaw simple C++ API sample: almost complete dcraw emulator
 *

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).


 */
#ifdef _MSC_VER
// suppress sprintf-related warning. sprintf() is permitted in sample code
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "libraw/libraw.h"

#ifndef LIBRAW_WIN32_CALLS
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#else
#include <io.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>

#ifdef LIBRAW_WIN32_CALLS
#define snprintf _snprintf
#include <windows.h>
#else
#define O_BINARY 0
#endif

#ifdef USE_DNGSDK
#include "dng_host.h"
#include "dng_negative.h"
#include "dng_simple_image.h"
#include "dng_info.h"
#endif

void usage(const char *prog)
{
  printf("dcraw_emu: almost complete dcraw emulator\n");
  printf("Usage:  %s [OPTION]... [FILE]...\n", prog);
  printf("-c float-num       Set adjust maximum threshold (default 0.75)\n"
         "-v        Verbose: print progress messages (repeated -v will add "
         "verbosity)\n"
         "-w        Use camera white balance, if possible\n"
         "-a        Average the whole image for white balance\n"
         "-A <x y w h> Average a grey box for white balance\n"
         "-r <r g b g> Set custom white balance\n"
         "+M/-M     Use/don't use an embedded color matrix\n"
         "-C <r b>  Correct chromatic aberration\n"
         "-P <file> Fix the dead pixels listed in this file\n"
         "-K <file> Subtract dark frame (16-bit raw PGM)\n"
         "-k <num>  Set the darkness level\n"
         "-S <num>  Set the saturation level\n"
         "-R <num>  Set raw processing options to num\n"
         "-n <num>  Set threshold for wavelet denoising\n"
         "-H [0-9]  Highlight mode (0=clip, 1=unclip, 2=blend, 3+=rebuild)\n"
         "-t [0-7]  Flip image (0=none, 3=180, 5=90CCW, 6=90CW)\n"
         "-o [0-8]  Output colorspace (raw,sRGB,Adobe,Wide,ProPhoto,XYZ,ACES,\n"
         "          DCI-P3,Rec2020)\n"
#ifndef NO_LCMS
         "-o file   Output ICC profile\n"
         "-p file   Camera input profile (use \'embed\' for embedded profile)\n"
#endif
         "-j        Don't stretch or rotate raw pixels\n"
         "-W        Don't automatically brighten the image\n"
         "-b <num>  Adjust brightness (default = 1.0)\n"
         "-q N      Set the interpolation quality:\n"
         "          0 - linear, 1 - VNG, 2 - PPG, 3 - AHD, 4 - DCB\n"
         "          11 - DHT, 12 - AAHD\n"
         "-h        Half-size color image (twice as fast as \"-q 0\")\n"
         "-f        Interpolate RGGB as four colors\n"
         "-m <num>  Apply a 3x3 median filter to R-G and B-G\n"
         "-s [0..N-1] Select one raw image from input file\n"
         "-4        Linear 16-bit, same as \"-6 -W -g 1 1\n"
         "-6        Write 16-bit output\n"
         "-g pow ts Set gamma curve to gamma pow and toe slope ts (default = "
         "2.222 4.5)\n"
         "-T        Write TIFF instead of PPM\n"
         "-G        Use green_matching() filter\n"
         "-B <x y w h> use cropbox\n"
         "-F        Use FILE I/O instead of streambuf API\n"
         "-Z <suf>  Output filename generation rules\n"
         "          .suf => append .suf to input name, keeping existing suffix "
         "too\n"
         "           suf => replace input filename last extension\n"
         "          - => output to stdout\n"
         "          filename.suf => output to filename.suf\n"
         "-timing   Detailed timing report\n"
         "-fbdd N   0 - disable FBDD noise reduction (default), 1 - light "
         "FBDD, 2 - full\n"
         "-dcbi N   Number of extra DCD iterations (default - 0)\n"
         "-dcbe     DCB color enhance\n"
         "-aexpo <e p> exposure correction\n"
         "-apentax4shot enables merge of 4-shot pentax files\n"
         "-apentax4shotorder 3102 sets pentax 4-shot alignment order\n"
#ifdef USE_RAWSPEED_BITS
         "-arsbits V Set use_rawspeed to V\n"
#endif
         "-mmap     Use memory mmaped buffer instead of plain FILE I/O\n"
         "-mem	   Use memory buffer instead of FILE I/O\n"
         "-disars   Do not use RawSpeed library\n"
         "-disinterp Do not run interpolation step\n"
         "-dsrawrgb1 Disable YCbCr to RGB conversion for sRAW (Cb/Cr "
         "interpolation enabled)\n"
         "-dsrawrgb2 Disable YCbCr to RGB conversion for sRAW (Cb/Cr "
         "interpolation disabled)\n"
#ifdef USE_DNGSDK
         "-dngsdk   Use Adobe DNG SDK for DNG decode\n"
         "-dngflags N set DNG decoding options to value N\n"
#endif
         "-doutputflags N set params.output_flags to N\n"
  );
  exit(1);
}

static int verbosity = 0;
int cnt = 0;
int my_progress_callback(void *d, enum LibRaw_progress p, int iteration,
                         int expected)
{
  char *passed = (char *)(d ? d : "default string"); // data passed to callback
                                                     // at set_callback stage

  if (verbosity > 2) // verbosity set by repeat -v switches
  {
    printf("CB: %s  pass %d of %d (data passed=%s)\n", libraw_strprogress(p),
           iteration, expected, passed);
  }
  else if (iteration == 0) // 1st iteration of each step
    printf("Starting %s (expecting %d iterations)\n", libraw_strprogress(p),
           expected);
  else if (iteration == expected - 1)
    printf("%s finished\n", libraw_strprogress(p));

  ///    if(++cnt>10) return 1; // emulate user termination on 10-th callback
  ///    call

  return 0; // always return 0 to continue processing
}

// timer
#ifndef LIBRAW_WIN32_CALLS
static struct timeval start, end;
void timerstart(void) { gettimeofday(&start, NULL); }
void timerprint(const char *msg, const char *filename)
{
  gettimeofday(&end, NULL);
  float msec = (end.tv_sec - start.tv_sec) * 1000.0f +
               (end.tv_usec - start.tv_usec) / 1000.0f;
  printf("Timing: %s/%s: %6.3f msec\n", filename, msg, msec);
}
#else
LARGE_INTEGER start;
void timerstart(void) { QueryPerformanceCounter(&start); }
void timerprint(const char *msg, const char *filename)
{
  LARGE_INTEGER unit, end;
  QueryPerformanceCounter(&end);
  QueryPerformanceFrequency(&unit);
  float msec = (float)(end.QuadPart - start.QuadPart);
  msec /= (float)unit.QuadPart / 1000.0f;
  printf("Timing: %s/%s: %6.3f msec\n", filename, msg, msec);
}

#endif

struct file_mapping
{
	void *map;
	INT64 fsize;
#ifdef LIBRAW_WIN32_CALLS
	HANDLE fd, fd_map;
	file_mapping() : map(0), fsize(0), fd(INVALID_HANDLE_VALUE), fd_map(INVALID_HANDLE_VALUE){}
#else
	int  fd;
	file_mapping() : map(0), fsize(0), fd(-1){}
#endif
};

void create_mapping(struct file_mapping& data, const std::string& fn)
{
#ifdef LIBRAW_WIN32_CALLS
	std::wstring fpath(fn.begin(), fn.end());
	if ((data.fd = CreateFileW(fpath.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)) == INVALID_HANDLE_VALUE) return;
	LARGE_INTEGER fs;
	if (!GetFileSizeEx(data.fd, &fs)) return;
	data.fsize = fs.QuadPart;
	if ((data.fd_map = ::CreateFileMapping(data.fd, 0, PAGE_READONLY, fs.HighPart, fs.LowPart, 0)) == INVALID_HANDLE_VALUE) return;
	data.map = MapViewOfFile(data.fd_map, FILE_MAP_READ, 0, 0, data.fsize);
#else
	struct stat stt;
	if ((data.fd = open(fn.c_str(), O_RDONLY)) < 0) return;
	if (fstat(data.fd, &stt) != 0) return;
	data.fsize = stt.st_size;
	data.map = mmap(0, data.fsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, data.fd, 0);
	return;
#endif
}

void close_mapping(struct file_mapping& data)
{
#ifdef LIBRAW_WIN32_CALLS
	if (data.map) UnmapViewOfFile(data.map);
	if (data.fd_map != INVALID_HANDLE_VALUE) CloseHandle(data.fd_map);
	if (data.fd != INVALID_HANDLE_VALUE) CloseHandle(data.fd);
	data.map = 0;
	data.fsize = 0;
	data.fd = data.fd_map = INVALID_HANDLE_VALUE;
#else
	if (data.map)
		munmap(data.map, data.fsize);
	if (data.fd >= 0)
		close(data.fd);
	data.map = 0;
	data.fsize = 0;
	data.fd = -1;
#endif
}


int main(int argc, char *argv[])
{
  if (argc == 1)
    usage(argv[0]);

  LibRaw RawProcessor;
  int i, arg, c, ret;
  char opm, opt, *cp, *sp;
  int use_timing = 0, use_mem = 0, use_mmap = 0;
  char *outext = NULL;
#ifdef USE_DNGSDK
  dng_host *dnghost = NULL;
#endif
  struct file_mapping mapping;
  void *iobuffer = 0;
#ifdef OUT
#undef OUT
#endif
#define OUT RawProcessor.imgdata.params
#define OUTR RawProcessor.imgdata.rawparams

  argv[argc] = (char *)"";
  for (arg = 1; (((opm = argv[arg][0]) - 2) | 2) == '+';)
  {
    char *optstr = argv[arg];
    opt = argv[arg++][1];
    if ((cp = strchr(sp = (char *)"cnbrkStqmHABCgU", opt)) != 0)
      for (i = 0; i < "111411111144221"[cp - sp] - '0'; i++)
        if (!isdigit(argv[arg + i][0]) && !optstr[2])
        {
          fprintf(stderr, "Non-numeric argument to \"-%c\"\n", opt);
          return 1;
        }
    if (!strchr("ftdeam", opt) && argv[arg - 1][2]) {
      fprintf(stderr, "Unknown option \"%s\".\n", argv[arg - 1]);
      continue;
    }
    switch (opt)
    {
    case 'v':
      verbosity++;
      break;
    case 'G':
      OUT.green_matching = 1;
      break;
    case 'c':
      OUT.adjust_maximum_thr = (float)atof(argv[arg++]);
      break;
    case 'U':
      OUT.auto_bright_thr = (float)atof(argv[arg++]);
      break;
    case 'n':
      OUT.threshold = (float)atof(argv[arg++]);
      break;
    case 'b':
      OUT.bright = (float)atof(argv[arg++]);
      break;
    case 'P':
      OUT.bad_pixels = argv[arg++];
      break;
    case 'K':
      OUT.dark_frame = argv[arg++];
      break;
    case 'r':
      for (c = 0; c < 4; c++)
        OUT.user_mul[c] = (float)atof(argv[arg++]);
      break;
    case 'C':
      OUT.aber[0] = 1 / atof(argv[arg++]);
      OUT.aber[2] = 1 / atof(argv[arg++]);
      break;
    case 'g':
      OUT.gamm[0] = 1 / atof(argv[arg++]);
      OUT.gamm[1] = atof(argv[arg++]);
      break;
    case 'k':
      OUT.user_black = atoi(argv[arg++]);
      break;
    case 'S':
      OUT.user_sat = atoi(argv[arg++]);
      break;
    case 'R':
      OUTR.options = atoi(argv[arg++]);
      break;
    case 't':
      if (!strcmp(optstr, "-timing"))
        use_timing = 1;
      else if (!argv[arg - 1][2])
        OUT.user_flip = atoi(argv[arg++]);
      else
        fprintf(stderr, "Unknown option \"%s\".\n", argv[arg - 1]);
      break;
    case 'q':
      OUT.user_qual = atoi(argv[arg++]);
      break;
    case 'm':
      if (!strcmp(optstr, "-mmap"))
        use_mmap = 1;
      else
          if (!strcmp(optstr, "-mem"))
        use_mem = 1;
      else
      {
        if (!argv[arg - 1][2])
          OUT.med_passes = atoi(argv[arg++]);
        else
          fprintf(stderr, "Unknown option \"%s\".\n", argv[arg - 1]);
      }
      break;
    case 'H':
      OUT.highlight = atoi(argv[arg++]);
      break;
    case 's':
      OUTR.shot_select = abs(atoi(argv[arg++]));
      break;
    case 'o':
      if (isdigit(argv[arg][0]) && !isdigit(argv[arg][1]))
        OUT.output_color = atoi(argv[arg++]);
#ifndef NO_LCMS
      else
        OUT.output_profile = argv[arg++];
      break;
    case 'p':
      OUT.camera_profile = argv[arg++];
#endif
      break;
    case 'h':
      OUT.half_size = 1;
      break;
    case 'f':
      if (!strcmp(optstr, "-fbdd"))
        OUT.fbdd_noiserd = atoi(argv[arg++]);
      else
      {
        if (!argv[arg - 1][2])
          OUT.four_color_rgb = 1;
        else
          fprintf(stderr, "Unknown option \"%s\".\n", argv[arg - 1]);
      }
      break;
    case 'A':
      for (c = 0; c < 4; c++)
        OUT.greybox[c] = atoi(argv[arg++]);
      break;
    case 'B':
      for (c = 0; c < 4; c++)
        OUT.cropbox[c] = atoi(argv[arg++]);
      break;
    case 'a':
      if (!strcmp(optstr, "-aexpo"))
      {
        OUT.exp_correc = 1;
        OUT.exp_shift = (float)atof(argv[arg++]);
        OUT.exp_preser = (float)atof(argv[arg++]);
      }
#ifdef USE_RAWSPEED_BITS
      else if (!strcmp(optstr, "-arsbits"))
      {
	OUTR.use_rawspeed = atoi(argv[arg++]);
      }
#endif
      else if (!strcmp(optstr, "-apentax4shot"))
      {
        OUTR.options |= LIBRAW_RAWOPTIONS_PENTAX_PS_ALLFRAMES;
      }
      else if (!strcmp(optstr, "-apentax4shotorder"))
      {
        strncpy(OUTR.p4shot_order, argv[arg++], 5);
      }
      else if (!argv[arg - 1][2])
        OUT.use_auto_wb = 1;
      else
        fprintf(stderr, "Unknown option \"%s\".\n", argv[arg - 1]);
      break;
    case 'w':
      OUT.use_camera_wb = 1;
      break;
    case 'M':
      OUT.use_camera_matrix = (opm == '+')?3:0;
      break;
    case 'j':
      OUT.use_fuji_rotate = 0;
      break;
    case 'W':
      OUT.no_auto_bright = 1;
      break;
    case 'T':
      OUT.output_tiff = 1;
      break;
    case '4':
      OUT.gamm[0] = OUT.gamm[1] = OUT.no_auto_bright = 1; /* no break here! */
    case '6':
      OUT.output_bps = 16;
      break;
    case 'Z':
      outext = strdup(argv[arg++]);
      break;
    case 'd':
      if (!strcmp(optstr, "-dcbi"))
        OUT.dcb_iterations = atoi(argv[arg++]);
      else if (!strcmp(optstr, "-doutputflags"))
        OUT.output_flags = atoi(argv[arg++]);
      else if (!strcmp(optstr, "-disars"))
        OUTR.use_rawspeed = 0;
      else if (!strcmp(optstr, "-disinterp"))
        OUT.no_interpolation = 1;
      else if (!strcmp(optstr, "-dcbe"))
        OUT.dcb_enhance_fl = 1;
      else if (!strcmp(optstr, "-dsrawrgb1"))
      {
        OUTR.specials |= LIBRAW_RAWSPECIAL_SRAW_NO_RGB;
        OUTR.specials &= ~LIBRAW_RAWSPECIAL_SRAW_NO_INTERPOLATE;
      }
      else if (!strcmp(optstr, "-dsrawrgb2"))
      {
        OUTR.specials &= ~LIBRAW_RAWSPECIAL_SRAW_NO_RGB;
        OUTR.specials |= LIBRAW_RAWSPECIAL_SRAW_NO_INTERPOLATE;
      }
#ifdef USE_DNGSDK
      else if (!strcmp(optstr, "-dngsdk"))
      {
        dnghost = new dng_host;
        RawProcessor.set_dng_host(dnghost);
      }
      else if (!strcmp(optstr, "-dngflags"))
      {
        OUTR.use_dngsdk = atoi(argv[arg++]);
      }
#endif
      else
        fprintf(stderr, "Unknown option \"%s\".\n", argv[arg - 1]);
      break;
    default:
      fprintf(stderr, "Unknown option \"-%c\".\n", opt);
      break;
    }
  }
#ifndef LIBRAW_WIN32_CALLS
  putenv((char *)"TZ=UTC"); // dcraw compatibility, affects TIFF datestamp field
#else
  _putenv(
      (char *)"TZ=UTC"); // dcraw compatibility, affects TIFF datestamp field
#endif
#define P1 RawProcessor.imgdata.idata
#define S RawProcessor.imgdata.sizes
#define C RawProcessor.imgdata.color
#define T RawProcessor.imgdata.thumbnail
#define P2 RawProcessor.imgdata.other

  if (outext && !strcmp(outext, "-"))
    use_timing = verbosity = 0;

  if (verbosity > 1)
    RawProcessor.set_progress_handler(my_progress_callback,
                                      (void *)"Sample data passed");
#ifdef LIBRAW_USE_OPENMP
  if (verbosity)
    printf("Using %d threads\n", omp_get_max_threads());
#endif

  int done = 0;
  int total = argc - arg;
  for (; arg < argc; arg++)
  {
    char outfn[1024];

    if (verbosity)
      printf("Processing file %s\n", argv[arg]);

    timerstart();

	if (use_mmap)
	{
		create_mapping(mapping, argv[arg]);
		if (!mapping.map)
		{
			fprintf(stderr, "Cannot map %s\n", argv[arg]);
			close_mapping(mapping);
			continue;
		}
      if ((ret = RawProcessor.open_buffer(mapping.map,mapping.fsize) !=
                 LIBRAW_SUCCESS))
      {
        fprintf(stderr, "Cannot open_buffer %s: %s\n", argv[arg], libraw_strerror(ret));
		close_mapping(mapping);
        continue; // no recycle b/c open file will recycle itself
      }
    }
    else  if (use_mem)
    {
      int file = open(argv[arg], O_RDONLY | O_BINARY);
      struct stat st;
      if (file < 0)
      {
        fprintf(stderr, "Cannot open %s: %s\n", argv[arg], strerror(errno));
        continue;
      }
      if (fstat(file, &st))
      {
        fprintf(stderr, "Cannot stat %s: %s\n", argv[arg], strerror(errno));
        close(file);
        continue;
      }
      if (!(iobuffer = malloc(st.st_size)))
      {
        fprintf(stderr, "Cannot allocate %d kbytes for memory buffer\n",
                (int)(st.st_size / 1024));
        close(file);
        continue;
      }
      int rd;
      if (st.st_size != (rd = read(file, iobuffer, st.st_size)))
      {
        fprintf(stderr,
                "Cannot read %d bytes instead of  %d to memory buffer\n",
                (int)rd, (int)st.st_size);
        close(file);
        free(iobuffer);
        continue;
      }
      close(file);
      if ((ret = RawProcessor.open_buffer(iobuffer, st.st_size)) !=
          LIBRAW_SUCCESS)
      {
        fprintf(stderr, "Cannot open_buffer %s: %s\n", argv[arg],
                libraw_strerror(ret));
        free(iobuffer);
        continue; // no recycle b/c open file will recycle itself
      }
    }
    else
    {
        ret = RawProcessor.open_file(argv[arg]);

      if (ret != LIBRAW_SUCCESS)
      {
        fprintf(stderr, "Cannot open %s: %s\n", argv[arg],
                libraw_strerror(ret));
        continue; // no recycle b/c open_file will recycle itself
      }
    }

    if (use_timing)
      timerprint("LibRaw::open_file()", argv[arg]);

    timerstart();
    if ((ret = RawProcessor.unpack()) != LIBRAW_SUCCESS)
    {
      fprintf(stderr, "Cannot unpack %s: %s\n", argv[arg],
              libraw_strerror(ret));
      continue;
    }

    if (use_timing)
      timerprint("LibRaw::unpack()", argv[arg]);

    timerstart();
    if (LIBRAW_SUCCESS != (ret = RawProcessor.dcraw_process()))
    {
      fprintf(stderr, "Cannot do postprocessing on %s: %s\n", argv[arg],
              libraw_strerror(ret));
      if (LIBRAW_FATAL_ERROR(ret))
        continue;
    }
    if (use_timing)
      timerprint("LibRaw::dcraw_process()", argv[arg]);

    if (!outext)
      snprintf(outfn, sizeof(outfn), "%s.%s", argv[arg],
               OUT.output_tiff ? "tiff" : (P1.colors > 1 ? "ppm" : "pgm"));
    else if (!strcmp(outext, "-"))
      snprintf(outfn, sizeof(outfn), "-");
    else
    {
      if (*outext == '.') // append
        snprintf(outfn, sizeof(outfn), "%s%s", argv[arg], outext);
      else if (strchr(outext, '.') && *outext != '.') // dot is not 1st char
        strncpy(outfn, outext, sizeof(outfn));
      else
      {
        strncpy(outfn, argv[arg], sizeof(outfn));
        if (strlen(outfn) > 0)
        {
          char *lastchar = outfn + strlen(outfn); // points to term 0
          while (--lastchar > outfn)
          {
            if (*lastchar == '/' || *lastchar == '\\')
              break;
            if (*lastchar == '.')
            {
              *lastchar = 0;
              break;
            };
          }
        }
        strncat(outfn, ".", sizeof(outfn) - strlen(outfn) - 1);
        strncat(outfn, outext, sizeof(outfn) - strlen(outfn) - 1);
      }
    }

    if (verbosity)
    {
      printf("Writing file %s\n", outfn);
    }

    if (LIBRAW_SUCCESS != (ret = RawProcessor.dcraw_ppm_tiff_writer(outfn)))
      fprintf(stderr, "Cannot write %s: %s\n", outfn, libraw_strerror(ret));
    else
      done++;

	RawProcessor.recycle(); // just for show this call

	if (use_mmap && mapping.map)
		close_mapping(mapping);
    else if (use_mem && iobuffer)
    {
      free(iobuffer);
      iobuffer = 0;
    }
  }
#ifdef USE_DNGSDK
  if (dnghost)
    delete dnghost;
#endif
  if (total == 0)
    return 1;
  if (done < total)
    return 2;
  return 0;
}
