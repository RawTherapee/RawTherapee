/* -*- C++ -*-
 * File: identify.cpp
 * Copyright 2008-2025 LibRaw LLC (info@libraw.org)
 * Created: Sat Mar  8, 2008
 *
 * LibRaw C++ demo: emulates dcraw -i [-v]
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
#include <time.h>
#include <string>
#include <list>

#include "libraw/libraw.h"

#ifdef LIBRAW_WIN32_CALLS
#define snprintf _snprintf
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

#ifndef LIBRAW_WIN32_CALLS
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#ifndef MAX_PATH
#ifdef PATH_MAX
#define MAX_PATH PATH_MAX
#else
#define MAX_PATH 4096
#endif
#endif
#endif

#ifdef _MSC_VER
#if _MSC_VER < 1800 /* below MSVC 2013 */
float roundf(float f) { return floorf(f + 0.5); }

#endif
#endif

#define P1 MyCoolRawProcessor.imgdata.idata
#define P2 MyCoolRawProcessor.imgdata.other
#define P3 MyCoolRawProcessor.imgdata.makernotes.common

#define mnLens MyCoolRawProcessor.imgdata.lens.makernotes
#define exifLens MyCoolRawProcessor.imgdata.lens
#define ShootingInfo MyCoolRawProcessor.imgdata.shootinginfo

#define S MyCoolRawProcessor.imgdata.sizes
#define O MyCoolRawProcessor.imgdata.params
#define C MyCoolRawProcessor.imgdata.color
#define T MyCoolRawProcessor.imgdata.thumbnail

void print_verbose(FILE *, LibRaw &MyCoolRawProcessor, std::string &fn);
void print_wbfun(FILE *, LibRaw &MyCoolRawProcessor, std::string &fn);
void print_szfun(FILE *, LibRaw &MyCoolRawProcessor, std::string &fn);
void print_unpackfun(FILE *, LibRaw &MyCoolRawProcessor, int print_frame, std::string &fn);

/*
table of fluorescents:
12 = FL-D; Daylight fluorescent (D 5700K – 7100K) (F1,F5)
13 = FL-N; Day white fluorescent (N 4600K – 5400K) (F7,F8)
14 = FL-W; Cool white fluorescent (W 3900K – 4500K) (F2,F6, office,
store,warehouse) 15 = FL-WW; White fluorescent (WW 3200K – 3700K) (F3,
residential) 16 = FL-L; Soft/Warm white fluorescent (L 2600K - 3250K) (F4,
kitchen, bath)
*/

static const struct
{
  const int NumId;
  const char *StrId;
  const char *hrStrId; // human-readable
  const int aux_setting;
} WBToStr[] = {
    {LIBRAW_WBI_Unknown, "WBI_Unknown", "Unknown", 0},
    {LIBRAW_WBI_Daylight, "WBI_Daylight", "Daylight", 0},
    {LIBRAW_WBI_Fluorescent, "WBI_Fluorescent", "Fluorescent", 0},
    {LIBRAW_WBI_Tungsten, "WBI_Tungsten", "Tungsten (Incandescent)", 0},
    {LIBRAW_WBI_Flash, "WBI_Flash", "Flash", 0},
    {LIBRAW_WBI_FineWeather, "WBI_FineWeather", "Fine Weather", 0},
    {LIBRAW_WBI_Cloudy, "WBI_Cloudy", "Cloudy", 0},
    {LIBRAW_WBI_Shade, "WBI_Shade", "Shade", 0},
    {LIBRAW_WBI_FL_D, "WBI_FL_D", "Daylight Fluorescent", 0},
    {LIBRAW_WBI_FL_N, "WBI_FL_N", "Day White Fluorescent", 0},
    {LIBRAW_WBI_FL_W, "WBI_FL_W", "Cool White Fluorescent", 0},
    {LIBRAW_WBI_FL_WW, "WBI_FL_WW", "White Fluorescent", 0},
    {LIBRAW_WBI_FL_L, "WBI_FL_L", "Warm White Fluorescent", 0},
    {LIBRAW_WBI_Ill_A, "WBI_Ill_A", "Illuminant A", 0},
    {LIBRAW_WBI_Ill_B, "WBI_Ill_B", "Illuminant B", 0},
    {LIBRAW_WBI_Ill_C, "WBI_Ill_C", "Illuminant C", 0},
    {LIBRAW_WBI_D55, "WBI_D55", "D55", 0},
    {LIBRAW_WBI_D65, "WBI_D65", "D65", 0},
    {LIBRAW_WBI_D75, "WBI_D75", "D75", 0},
    {LIBRAW_WBI_D50, "WBI_D50", "D50", 0},
    {LIBRAW_WBI_StudioTungsten, "WBI_StudioTungsten", "ISO Studio Tungsten", 0},
    {LIBRAW_WBI_BW, "WBI_BW", "BW", 0},
    {LIBRAW_WBI_Other, "WBI_Other", "Other", 0},
    {LIBRAW_WBI_Sunset, "WBI_Sunset", "Sunset", 1},
    {LIBRAW_WBI_Underwater, "WBI_Underwater", "Underwater", 1},
    {LIBRAW_WBI_FluorescentHigh, "WBI_FluorescentHigh", "Fluorescent High", 1},
    {LIBRAW_WBI_HT_Mercury, "WBI_HT_Mercury", "HT Mercury", 1},
    {LIBRAW_WBI_AsShot, "WBI_AsShot", "As Shot", 1},
    {LIBRAW_WBI_Measured, "WBI_Measured", "Camera Measured", 1},
    {LIBRAW_WBI_Auto, "WBI_Auto", "Camera Auto", 1},
    {LIBRAW_WBI_Auto1, "WBI_Auto1", "Camera Auto 1", 1},
    {LIBRAW_WBI_Auto2, "WBI_Auto2", "Camera Auto 2", 1},
    {LIBRAW_WBI_Auto3, "WBI_Auto3", "Camera Auto 3", 1},
    {LIBRAW_WBI_Auto4, "WBI_Auto4", "Camera Auto 4", 1},
    {LIBRAW_WBI_Custom, "WBI_Custom", "Custom", 1},
    {LIBRAW_WBI_Custom1, "WBI_Custom1", "Custom 1", 1},
    {LIBRAW_WBI_Custom2, "WBI_Custom2", "Custom 2", 1},
    {LIBRAW_WBI_Custom3, "WBI_Custom3", "Custom 3", 1},
    {LIBRAW_WBI_Custom4, "WBI_Custom4", "Custom 4", 1},
    {LIBRAW_WBI_Custom5, "WBI_Custom5", "Custom 5", 1},
    {LIBRAW_WBI_Custom6, "WBI_Custom6", "Custom 6", 1},
    {LIBRAW_WBI_PC_Set1, "WBI_PC_Set1", "PC Set 1", 1},
    {LIBRAW_WBI_PC_Set2, "WBI_PC_Set2", "PC Set 2", 1},
    {LIBRAW_WBI_PC_Set3, "WBI_PC_Set3", "PC Set 3", 1},
    {LIBRAW_WBI_PC_Set4, "WBI_PC_Set4", "PC Set 4", 1},
    {LIBRAW_WBI_PC_Set5, "WBI_PC_Set5", "PC Set 5", 1},
    {LIBRAW_WBI_Kelvin, "WBI_Kelvin", "Kelvin", 1},
};

const char *WB_idx2str(unsigned WBi)
{
  for (int i = 0; i < int(sizeof WBToStr / sizeof *WBToStr); i++)
    if (WBToStr[i].NumId == (int)WBi)
      return WBToStr[i].StrId;
  return 0;
}

const char *WB_idx2hrstr(unsigned WBi)
{
  for (int i = 0; i < int(sizeof WBToStr / sizeof *WBToStr); i++)
    if (WBToStr[i].NumId == (int)WBi)
      return WBToStr[i].hrStrId;
  return 0;
}

double _log2(double a)
{
  if(a > 0.00000000001) return log(a)/log(2.0);
  return -1000;
}

void trimSpaces(char *s)
{
  char *p = s;
  if (!strncasecmp(p, "NO=", 3))
    p = p + 3; /* fix for Nikon D70, D70s */
  int l = strlen(p);
  if (!l)
    return;
  while (isspace(p[l - 1]))
    p[--l] = 0; /* trim trailing spaces */
  while (*p && isspace(*p))
    ++p, --l; /* trim leading spaces */
  memmove(s, p, l + 1);
}

void print_usage(const char *pname)
{
  printf("Usage: %s [options] inputfiles\n", pname);
  printf("Options:\n"
         "\t-v\tverbose output\n"
         "\t-w\tprint white balance\n"
         "\t-u\tprint unpack function\n"
         "\t-f\tprint frame size (only w/ -u)\n"
         "\t-s\tprint output image size\n"
         "\t-h\tforce half-size mode (only for -s)\n"
         "\t-M\tdisable use of raw-embedded color data\n"
         "\t+M\tforce use of raw-embedded color data\n"
         "\t-L filename\tread input files list from filename\n"
         "\t-o filename\toutput to filename\n");
}

int main(int ac, char *av[])
{
  int ret;
  int verbose = 0, print_sz = 0, print_unpack = 0, print_frame = 0, print_wb = 0;
  LibRaw MyCoolRawProcessor;
  char *filelistfile = NULL;
  char *outputfilename = NULL;
  FILE *outfile = stdout;
  std::vector<std::string> filelist;

  filelist.reserve(ac - 1);

  for (int i = 1; i < ac; i++)
  {
    if (av[i][0] == '-')
    {
      if (!strcmp(av[i], "-v"))
        verbose++;
      if (!strcmp(av[i], "-w"))
        print_wb++;
      if (!strcmp(av[i], "-u"))
        print_unpack++;
      if (!strcmp(av[i], "-s"))
        print_sz++;
      if (!strcmp(av[i], "-h"))
        O.half_size = 1;
      if (!strcmp(av[i], "-f"))
        print_frame++;
      if (!strcmp(av[i], "-M"))
        MyCoolRawProcessor.imgdata.params.use_camera_matrix = 0;
      if (!strcmp(av[i], "-L") && i < ac - 1)
      {
        filelistfile = av[i + 1];
        i++;
      }
      if (!strcmp(av[i], "-o") && i < ac - 1)
      {
        outputfilename = av[i + 1];
        i++;
      }
      continue;
    }
    else if (!strcmp(av[i], "+M"))
    {
      MyCoolRawProcessor.imgdata.params.use_camera_matrix = 3;
      continue;
    }
    filelist.push_back(av[i]);
  }
  if (filelistfile)
  {
    char *p;
    char path[MAX_PATH + 1];
    FILE *f = fopen(filelistfile, "r");
    if (f)
    {
      while (fgets(path, MAX_PATH, f))
      {
        if ((p = strchr(path, '\n')))
          *p = 0;
        if ((p = strchr(path, '\r')))
          *p = 0;
        filelist.push_back(path);
      }
      fclose(f);
    }
  }
  if (filelist.size() < 1)
  {
    print_usage(av[0]);
    return 1;
  }
  if (outputfilename)
    outfile = fopen(outputfilename, "wt");

  for (int i = 0; i < (int)filelist.size(); i++)
  {
    if ((ret = MyCoolRawProcessor.open_file(filelist[i].c_str())) != LIBRAW_SUCCESS)
    {
      fprintf(stderr, "Cannot decode %s: %s\n", filelist[i].c_str(), libraw_strerror(ret));
      continue; // no recycle, open_file will recycle
    }

    if (print_sz)
      print_szfun(outfile, MyCoolRawProcessor, filelist[i]);
    else if (verbose)
      print_verbose(outfile, MyCoolRawProcessor, filelist[i]);
    else if (print_unpack)
      print_unpackfun(outfile, MyCoolRawProcessor, print_frame, filelist[i]);
    else if (print_wb)
      print_wbfun(outfile, MyCoolRawProcessor, filelist[i]);
    else
      fprintf(outfile, "%s is a %s %s image.\n", filelist[i].c_str(), P1.make, P1.model);

    MyCoolRawProcessor.recycle();
  } // endfor
  return 0;
}

#define PRINTMATRIX3x4(of, mat, clrs)                                                                                  \
  do                                                                                                                   \
  {                                                                                                                    \
    for (int r = 0; r < 3; r++)                                                                                        \
      if (clrs == 4)                                                                                                   \
        fprintf(of, "%6.4f\t%6.4f\t%6.4f\t%6.4f\n", mat[r][0], mat[r][1], mat[r][2], mat[r][3]);                       \
      else                                                                                                             \
        fprintf(of, "%6.4f\t%6.4f\t%6.4f\n", mat[r][0], mat[r][1], mat[r][2]);                                         \
  } while (0)

#define PRINTMATRIX4x3(of, mat, clrs)                                                                                  \
  do                                                                                                                   \
  {                                                                                                                    \
    for (int r = 0; r < clrs && r < 4; r++)                                                                            \
      fprintf(of, "%6.4f\t%6.4f\t%6.4f\n", mat[r][0], mat[r][1], mat[r][2]);                                           \
  } while (0)

void print_verbose(FILE *outfile, LibRaw &MyCoolRawProcessor, std::string &fn)
{
  int WBi;
  float denom;
  int ret;

  if ((ret = MyCoolRawProcessor.adjust_sizes_info_only()))
  {
    fprintf(outfile, "Cannot decode %s: %s\n", fn.c_str(), libraw_strerror(ret));
    return; // no recycle, open_file will recycle
  }

  fprintf(outfile, "\nFilename: %s\n", fn.c_str());
  if (C.OriginalRawFileName[0])
    fprintf(outfile, "OriginalRawFileName: =%s=\n", C.OriginalRawFileName);
  fprintf(outfile, "Timestamp: %s", ctime(&(P2.timestamp)));
  fprintf(outfile, "Camera: %s %s ID: 0x%llx\n", P1.make, P1.model, mnLens.CamID);
  fprintf(outfile, "Normalized Make/Model: =%s/%s= ", P1.normalized_make, P1.normalized_model);
  fprintf(outfile, "CamMaker ID: %d\n", P1.maker_index);

  {
    int i = 0;
    char sep[] = ", ";
    if (C.UniqueCameraModel[0])
    {
      i++;
      fprintf(outfile, "UniqueCameraModel: =%s=", C.UniqueCameraModel);
    }
    if (C.LocalizedCameraModel[0])
    {
      if (i)
      {
        fprintf(outfile, "%s", sep);
        i++;
      }
      fprintf(outfile, "LocalizedCameraModel: =%s=", C.LocalizedCameraModel);
    }
    if (i)
    {
      fprintf(outfile, "\n");
      i = 0;
    }
    if (C.ImageUniqueID[0])
    {
      if (i)
        fprintf(outfile, "%s", sep);
      i++;
      fprintf(outfile, "ImageUniqueID: =%s=", C.ImageUniqueID);
    }
    if (C.RawDataUniqueID[0])
    {
      if (i)
        fprintf(outfile, "%s", sep);
      i++;
      fprintf(outfile, "RawDataUniqueID: =%s=", C.RawDataUniqueID);
    }
    if (i)
      fprintf(outfile, "\n");
  }

  if (ShootingInfo.BodySerial[0] && strcmp(ShootingInfo.BodySerial, "0"))
  {
    trimSpaces(ShootingInfo.BodySerial);
    fprintf(outfile, "Body#: %s", ShootingInfo.BodySerial);
  }
  else if (C.model2[0] && (!strncasecmp(P1.normalized_make, "Kodak", 5)))
  {
    trimSpaces(C.model2);
    fprintf(outfile, "Body#: %s", C.model2);
  }
  if (ShootingInfo.InternalBodySerial[0])
  {
    trimSpaces(ShootingInfo.InternalBodySerial);
    fprintf(outfile, " BodyAssy#: %s", ShootingInfo.InternalBodySerial);
  }
  if (exifLens.LensSerial[0])
  {
    trimSpaces(exifLens.LensSerial);
    fprintf(outfile, " Lens#: %s", exifLens.LensSerial);
  }
  if (exifLens.InternalLensSerial[0])
  {
    trimSpaces(exifLens.InternalLensSerial);
    fprintf(outfile, " LensAssy#: %s", exifLens.InternalLensSerial);
  }
  if (P2.artist[0])
    fprintf(outfile, " Owner: %s\n", P2.artist);
  if (P1.dng_version)
  {
    fprintf(outfile, " DNG Version: ");
    for (int i = 24; i >= 0; i -= 8)
      fprintf(outfile, "%d%c", P1.dng_version >> i & 255, i ? '.' : '\n');
  }
  fprintf(outfile, "\nEXIF:\n");
  fprintf(outfile, "\tMinFocal: %0.1f mm\n", exifLens.MinFocal);
  fprintf(outfile, "\tMaxFocal: %0.1f mm\n", exifLens.MaxFocal);
  fprintf(outfile, "\tMaxAp @MinFocal: f/%0.1f\n", exifLens.MaxAp4MinFocal);
  fprintf(outfile, "\tMaxAp @MaxFocal: f/%0.1f\n", exifLens.MaxAp4MaxFocal);
  fprintf(outfile, "\tCurFocal: %0.1f mm\n", P2.focal_len);
  fprintf(outfile, "\tMaxAperture @CurFocal: f/%0.1f\n", exifLens.EXIF_MaxAp);
  fprintf(outfile, "\tFocalLengthIn35mmFormat: %d mm\n", exifLens.FocalLengthIn35mmFormat);
  fprintf(outfile, "\tLensMake: %s\n", exifLens.LensMake);
  fprintf(outfile, "\tLens: %s\n", exifLens.Lens);
  fprintf(outfile, "\n");

  fprintf(outfile, "\nMakernotes:\n");
  fprintf(outfile, "\tDriveMode: %d\n", ShootingInfo.DriveMode);
  fprintf(outfile, "\tFocusMode: %d\n", ShootingInfo.FocusMode);
  fprintf(outfile, "\tMeteringMode: %d\n", ShootingInfo.MeteringMode);
  fprintf(outfile, "\tAFPoint: %d\n", ShootingInfo.AFPoint);
  fprintf(outfile, "\tExposureMode: %d\n", ShootingInfo.ExposureMode);
  fprintf(outfile, "\tExposureProgram: %d\n", ShootingInfo.ExposureProgram);
  fprintf(outfile, "\tImageStabilization: %d\n", ShootingInfo.ImageStabilization);

  fprintf(outfile, "\tLens: %s\n", mnLens.Lens);
  fprintf(outfile, "\tLensFormat: %d, ", mnLens.LensFormat);

  fprintf(outfile, "\tLensMount: %d, ", mnLens.LensMount);
  fprintf(outfile, "\tFocalType: %d, ", mnLens.FocalType);
  switch (mnLens.FocalType)
  {
  case LIBRAW_FT_UNDEFINED:
    fprintf(outfile, "Undefined\n");
    break;
  case LIBRAW_FT_PRIME_LENS:
    fprintf(outfile, "Prime lens\n");
    break;
  case LIBRAW_FT_ZOOM_LENS:
    fprintf(outfile, "Zoom lens\n");
    break;
  default:
    fprintf(outfile, "Unknown\n");
    break;
  }
  fprintf(outfile, "\tLensFeatures_pre: %s\n", mnLens.LensFeatures_pre);
  fprintf(outfile, "\tLensFeatures_suf: %s\n", mnLens.LensFeatures_suf);
  fprintf(outfile, "\tMinFocal: %0.1f mm\n", mnLens.MinFocal);
  fprintf(outfile, "\tMaxFocal: %0.1f mm\n", mnLens.MaxFocal);
  fprintf(outfile, "\tMaxAp @MinFocal: f/%0.1f\n", mnLens.MaxAp4MinFocal);
  fprintf(outfile, "\tMaxAp @MaxFocal: f/%0.1f\n", mnLens.MaxAp4MaxFocal);
  fprintf(outfile, "\tMinAp @MinFocal: f/%0.1f\n", mnLens.MinAp4MinFocal);
  fprintf(outfile, "\tMinAp @MaxFocal: f/%0.1f\n", mnLens.MinAp4MaxFocal);
  fprintf(outfile, "\tMaxAp: f/%0.1f\n", mnLens.MaxAp);
  fprintf(outfile, "\tMinAp: f/%0.1f\n", mnLens.MinAp);
  fprintf(outfile, "\tCurFocal: %0.1f mm\n", mnLens.CurFocal);
  fprintf(outfile, "\tCurAp: f/%0.1f\n", mnLens.CurAp);
  fprintf(outfile, "\tMaxAp @CurFocal: f/%0.1f\n", mnLens.MaxAp4CurFocal);
  fprintf(outfile, "\tMinAp @CurFocal: f/%0.1f\n", mnLens.MinAp4CurFocal);

  if (exifLens.makernotes.FocalLengthIn35mmFormat > 1.0f)
    fprintf(outfile, "\tFocalLengthIn35mmFormat: %0.1f mm\n", exifLens.makernotes.FocalLengthIn35mmFormat);

  if (exifLens.nikon.EffectiveMaxAp > 0.1f)
    fprintf(outfile, "\tEffectiveMaxAp: f/%0.1f\n", exifLens.nikon.EffectiveMaxAp);

  if (exifLens.makernotes.LensFStops > 0.1f)
    fprintf(outfile, "\tLensFStops @CurFocal: %0.2f\n", exifLens.makernotes.LensFStops);

  fprintf(outfile, "\tTeleconverterID: %lld\n", mnLens.TeleconverterID);
  fprintf(outfile, "\tTeleconverter: %s\n", mnLens.Teleconverter);
  fprintf(outfile, "\tAdapterID: %lld\n", mnLens.AdapterID);
  fprintf(outfile, "\tAdapter: %s\n", mnLens.Adapter);
  fprintf(outfile, "\tAttachmentID: %lld\n", mnLens.AttachmentID);
  fprintf(outfile, "\tAttachment: %s\n", mnLens.Attachment);
  fprintf(outfile, "\n");

  fprintf(outfile, "ISO speed: %d\n", (int)P2.iso_speed);
  if (P3.real_ISO > 0.1f)
    fprintf(outfile, "real ISO speed: %d\n", (int)P3.real_ISO);
  fprintf(outfile, "Shutter: ");
  if (P2.shutter > 0 && P2.shutter < 1)
    P2.shutter = fprintf(outfile, "1/%0.1f\n", 1.0f / P2.shutter);
  else if (P2.shutter >= 1)
    fprintf(outfile, "%0.1f sec\n", P2.shutter);
  else /* negative*/
    fprintf(outfile, " negative value\n");
  fprintf(outfile, "Aperture: f/%0.1f\n", P2.aperture);
  fprintf(outfile, "Focal length: %0.1f mm\n", P2.focal_len);
  if (P3.exifAmbientTemperature > -273.15f)
    fprintf(outfile, "Ambient temperature (exif data): %6.2f° C\n", P3.exifAmbientTemperature);
  if (P3.CameraTemperature > -273.15f)
    fprintf(outfile, "Camera temperature: %6.2f° C\n", P3.CameraTemperature);
  if (P3.SensorTemperature > -273.15f)
    fprintf(outfile, "Sensor temperature: %6.2f° C\n", P3.SensorTemperature);
  if (P3.SensorTemperature2 > -273.15f)
    fprintf(outfile, "Sensor temperature2: %6.2f° C\n", P3.SensorTemperature2);
  if (P3.LensTemperature > -273.15f)
    fprintf(outfile, "Lens temperature: %6.2f° C\n", P3.LensTemperature);
  if (P3.AmbientTemperature > -273.15f)
    fprintf(outfile, "Ambient temperature: %6.2f° C\n", P3.AmbientTemperature);
  if (P3.BatteryTemperature > -273.15f)
    fprintf(outfile, "Battery temperature: %6.2f° C\n", P3.BatteryTemperature);
  if (P3.FlashGN > 1.0f)
    fprintf(outfile, "Flash Guide Number: %6.2f\n", P3.FlashGN);
  fprintf(outfile, "Flash exposure compensation: %0.2f EV\n", P3.FlashEC);
  if (C.profile)
    fprintf(outfile, "Embedded ICC profile: yes, %d bytes\n", C.profile_length);
  else
    fprintf(outfile, "Embedded ICC profile: no\n");

  if (C.dng_levels.baseline_exposure > -999.f)
    fprintf(outfile, "Baseline exposure: %04.3f\n", C.dng_levels.baseline_exposure);

  fprintf(outfile, "Number of raw images: %d\n", P1.raw_count);

  if (S.pixel_aspect != 1)
    fprintf(outfile, "Pixel Aspect Ratio: %0.6f\n", S.pixel_aspect);
  if (T.tlength)
    fprintf(outfile, "Thumb size:  %4d x %d\n", T.twidth, T.theight);
  fprintf(outfile, "Full size:   %4d x %d\n", S.raw_width, S.raw_height);

  if (S.raw_inset_crops[0].cwidth)
  {
    fprintf(outfile, "Raw inset, width x height: %4d x %d ", S.raw_inset_crops[0].cwidth, S.raw_inset_crops[0].cheight);
    if (S.raw_inset_crops[0].cleft != 0xffff)
      fprintf(outfile, "left: %d ", S.raw_inset_crops[0].cleft);
    if (S.raw_inset_crops[0].ctop != 0xffff)
      fprintf(outfile, "top: %d", S.raw_inset_crops[0].ctop);
    fprintf(outfile, "\n");
  }

  fprintf(outfile, "Image size:  %4d x %d\n", S.width, S.height);
  fprintf(outfile, "Output size: %4d x %d\n", S.iwidth, S.iheight);
  fprintf(outfile, "Image flip: %d\n", S.flip);

  fprintf(outfile, "Raw colors: %d", P1.colors);
  if (P1.filters)
  {
    fprintf(outfile, "\nFilter pattern: ");
    if (!P1.cdesc[3])
      P1.cdesc[3] = 'G';
    for (int i = 0; i < 16; i++)
      putchar(P1.cdesc[MyCoolRawProcessor.fcol(i >> 1, i & 1)]);
  }

  if (C.black)
  {
    fprintf(outfile, "\nblack: %d", C.black);
  }
  if (C.cblack[0] != 0)
  {
    fprintf(outfile, "\ncblack[0 .. 3]:");
    for (int c = 0; c < 4; c++)
      fprintf(outfile, " %d", C.cblack[c]);
  }
  if ((C.cblack[4] * C.cblack[5]) > 0)
  {
    fprintf(outfile, "\nBlackLevelRepeatDim: %d x %d\n", C.cblack[4], C.cblack[5]);
    int n = C.cblack[4] * C.cblack[5];
    fprintf(outfile, "cblack[6 .. %d]:", 6 + n - 1);
    for (int c = 6; c < 6 + n; c++)
      fprintf(outfile, " %d", C.cblack[c]);
  }

  if (C.linear_max[0] != 0)
  {
    fprintf(outfile, "\nHighlight linearity limits:");
    for (int c = 0; c < 4; c++)
      fprintf(outfile, " %u", C.linear_max[c]);
  }

  if (P1.colors > 1)
  {
    fprintf(outfile, "\nMakernotes WB data:               coeffs                  EVs");
    if ((C.cam_mul[0] > 0) && (C.cam_mul[1] > 0))
    {
      fprintf(outfile, "\n  %-23s   %g %g %g %g   %5.2f %5.2f %5.2f %5.2f", "As shot", C.cam_mul[0], C.cam_mul[1],
              C.cam_mul[2], C.cam_mul[3], roundf(_log2(C.cam_mul[0] / C.cam_mul[1]) * 100.0f) / 100.0f, 0.0f,
              roundf(_log2(C.cam_mul[2] / C.cam_mul[1]) * 100.0f) / 100.0f,
              C.cam_mul[3] ? roundf(_log2(C.cam_mul[3] / C.cam_mul[1]) * 100.0f) / 100.0f : 0.0f);
    }

    for (int cnt = 0; cnt < int(sizeof WBToStr / sizeof *WBToStr); cnt++)
    {
      WBi = WBToStr[cnt].NumId;
      if ((C.WB_Coeffs[WBi][0] > 0) && (C.WB_Coeffs[WBi][1] > 0))
      {
        denom = (float)C.WB_Coeffs[WBi][1];
        fprintf(outfile, "\n  %-23s   %4d %4d %4d %4d   %5.2f %5.2f %5.2f %5.2f", WBToStr[cnt].hrStrId,
                C.WB_Coeffs[WBi][0], C.WB_Coeffs[WBi][1], C.WB_Coeffs[WBi][2], C.WB_Coeffs[WBi][3],
                roundf(_log2((float)C.WB_Coeffs[WBi][0] / denom) * 100.0f) / 100.0f, 0.0f,
                roundf(_log2((float)C.WB_Coeffs[WBi][2] / denom) * 100.0f) / 100.0f,
                C.WB_Coeffs[3] ? roundf(_log2((float)C.WB_Coeffs[WBi][3] / denom) * 100.0f) / 100.0f : 0.0f);
      }
    }

    if (C.rgb_cam[0][0] > 0.0001)
    {
      fprintf(outfile, "\n\nCamera2RGB matrix (mode: %d):\n", MyCoolRawProcessor.imgdata.params.use_camera_matrix);
      PRINTMATRIX3x4(outfile, C.rgb_cam, P1.colors);
    }

    fprintf(outfile, "\nXYZ->CamRGB matrix:\n");
    PRINTMATRIX4x3(outfile, C.cam_xyz, P1.colors);

    for (int cnt = 0; cnt < 2; cnt++)
    {
      if (fabsf(C.P1_color[cnt].romm_cam[0]) > 0)
      {
        fprintf(outfile, "\nPhaseOne Matrix %d:\n", cnt + 1);
        for (int i = 0; i < 3; i++)
          fprintf(outfile, "%6.4f\t%6.4f\t%6.4f\n", C.P1_color[cnt].romm_cam[i * 3],
                  C.P1_color[cnt].romm_cam[i * 3 + 1], C.P1_color[cnt].romm_cam[i * 3 + 2]);
      }
    }

    if (fabsf(C.cmatrix[0][0]) > 0)
    {
      fprintf(outfile, "\ncamRGB -> sRGB Matrix:\n");
      PRINTMATRIX3x4(outfile, C.cmatrix, P1.colors);
    }

    if (fabsf(C.ccm[0][0]) > 0)
    {
      fprintf(outfile, "\nColor Correction Matrix:\n");
      PRINTMATRIX3x4(outfile, C.ccm, P1.colors);
    }

    for (int cnt = 0; cnt < 2; cnt++)
    {
      if (C.dng_color[cnt].illuminant != LIBRAW_WBI_None)
      {
        if (C.dng_color[cnt].illuminant <= LIBRAW_WBI_StudioTungsten)
        {
          fprintf(outfile, "\nDNG Illuminant %d: %s", cnt + 1, WB_idx2hrstr(C.dng_color[cnt].illuminant));
        }
        else if (C.dng_color[cnt].illuminant == LIBRAW_WBI_Other)
        {
          fprintf(outfile, "\nDNG Illuminant %d: Other", cnt + 1);
        }
        else
        {
          fprintf(outfile,
                  "\nDNG Illuminant %d is out of EXIF LightSources range "
                  "[0:24, 255]: %d",
                  cnt + 1, C.dng_color[cnt].illuminant);
        }
      }
    }

    for (int n = 0; n < 2; n++)
    {
      if (fabsf(C.dng_color[n].colormatrix[0][0]) > 0)
      {
        fprintf(outfile, "\nDNG color matrix %d:\n", n + 1);
        PRINTMATRIX4x3(outfile, C.dng_color[n].colormatrix, P1.colors);
      }
    }

    for (int n = 0; n < 2; n++)
    {
      if (fabsf(C.dng_color[n].calibration[0][0]) > 0)
      {
        fprintf(outfile, "\nDNG calibration matrix %d:\n", n + 1);
        for (int i = 0; i < P1.colors && i < 4; i++)
        {
          for (int j = 0; j < P1.colors && j < 4; j++)
            fprintf(outfile, "%6.4f\t", C.dng_color[n].calibration[j][i]);
          fprintf(outfile, "\n");
        }
      }
    }

    for (int n = 0; n < 2; n++)
    {
      if (fabsf(C.dng_color[n].forwardmatrix[0][0]) > 0)
      {
        fprintf(outfile, "\nDNG forward matrix %d:\n", n + 1);
        PRINTMATRIX3x4(outfile, C.dng_color[n].forwardmatrix, P1.colors);
      }
    }

    fprintf(outfile, "\nDerived D65 multipliers:");
    for (int c = 0; c < P1.colors; c++)
      fprintf(outfile, " %f", C.pre_mul[c]);
    fprintf(outfile, "\n");
  }
}

void print_wbfun(FILE *outfile, LibRaw &MyCoolRawProcessor, std::string &fn)
{
  int WBi;
  float denom;
  fprintf(outfile, "// %s %s\n", P1.make, P1.model);
  for (int cnt = 0; cnt < int(sizeof WBToStr / sizeof *WBToStr); cnt++)
  {
    WBi = WBToStr[cnt].NumId;
    if (C.WB_Coeffs[WBi][0] && C.WB_Coeffs[WBi][1] && !WBToStr[cnt].aux_setting)
    {
      denom = (float)C.WB_Coeffs[WBi][1];
      fprintf(outfile, "{\"%s\", \"%s\", %s, {%6.5ff, 1.0f, %6.5ff, ", P1.normalized_make, P1.normalized_model,
              WBToStr[cnt].StrId, C.WB_Coeffs[WBi][0] / denom, C.WB_Coeffs[WBi][2] / denom);
      if (C.WB_Coeffs[WBi][1] == C.WB_Coeffs[WBi][3])
        fprintf(outfile, "1.0f}},\n");
      else
        fprintf(outfile, "%6.5ff}},\n", C.WB_Coeffs[WBi][3] / denom);
    }
  }

  for (int cnt = 0; cnt < 64; cnt++)
    if (C.WBCT_Coeffs[cnt][0])
    {
      fprintf(outfile, "{\"%s\", \"%s\", %d, {%6.5ff, 1.0f, %6.5ff, ", P1.normalized_make, P1.normalized_model,
              (int)C.WBCT_Coeffs[cnt][0], C.WBCT_Coeffs[cnt][1] / C.WBCT_Coeffs[cnt][2],
              C.WBCT_Coeffs[cnt][3] / C.WBCT_Coeffs[cnt][2]);
      if (C.WBCT_Coeffs[cnt][2] == C.WBCT_Coeffs[cnt][4])
        fprintf(outfile, "1.0f}},\n");
      else
        fprintf(outfile, "%6.5ff}},\n", C.WBCT_Coeffs[cnt][4] / C.WBCT_Coeffs[cnt][2]);
    }
    else
      break;
  fprintf(outfile, "\n");
}

void print_szfun(FILE *outfile, LibRaw &MyCoolRawProcessor, std::string &fn)
{
  fprintf(outfile, "%s\t%s\t%s\t%d\t%d\n", fn.c_str(), P1.make, P1.model, S.width, S.height);
}

void print_unpackfun(FILE *outfile, LibRaw &MyCoolRawProcessor, int print_frame, std::string &fn)
{
  char frame[48] = "";
  if (print_frame)
  {
    ushort right_margin = S.raw_width - S.width - S.left_margin;
    ushort bottom_margin = S.raw_height - S.height - S.top_margin;
    snprintf(frame, 48, "F=%dx%dx%dx%d RS=%dx%d", S.left_margin, S.top_margin, right_margin, bottom_margin, S.raw_width,
             S.raw_height);
  }
  fprintf(outfile, "%s\t%s\t%s\t%s/%s\n", fn.c_str(), MyCoolRawProcessor.unpack_function_name(), frame, P1.make,
          P1.model);
}
