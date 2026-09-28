/*********************************************************************
 * writeFeatures.c
 *
 *********************************************************************
 */

/* Standard includes */
#include <cassert>
#include <cctype>		/* isdigit() */
#include <cstdio>		/* sprintf(), fprintf(), sscanf(), fscanf() */
#include <cstdlib>		/* malloc() */
#include <cstring>		/* memcpy(), strcmp() */

/* Our includes */
#include "base.h"
#include "error.h"
#include "pnmio.h"		/* ppmWriteFileRGB() */
#include "klt.h"

using namespace std;

#define BINHEADERLENGTH	6

extern int KLT_verbose;

typedef enum {FEATURE_LIST, FEATURE_HISTORY, FEATURE_TABLE} structureType;

static char warning_line[] = "!!! Warning:  This is a KLT data file.  "
                             "Do not modify below this line !!!\n";
static char binheader_fl[BINHEADERLENGTH+1] = "KLTFL1";
static char binheader_fh[BINHEADERLENGTH+1] = "KLTFH1";
static char binheader_ft[BINHEADERLENGTH+1] = "KLTFT1";

/*********************************************************************
 * KLTWriteFeatureListToPPM
 */

void KLTWriteFeatureListToPPM(
  KLT_FeatureList featurelist,
  KLT_PixelType *greyimg,
  int ncols,
  int nrows,
  const char *filename)
{
  int nbytes = ncols * nrows * sizeof(char);
  uchar *redimg, *grnimg, *bluimg;
  int offset;
  int x, y, xx, yy;
  int i;
	
  if (KLT_verbose >= 1) 
    fprintf(stderr, "(KLT) Writing %d features to PPM file: '%s'\n", 
            KLTCountRemainingFeatures(featurelist), filename);

  /* Allocate memory for component images */
  redimg = (uchar *)  malloc(nbytes);
  grnimg = (uchar *)  malloc(nbytes);
  bluimg = (uchar *)  malloc(nbytes);
  if (redimg == nullptr || grnimg == nullptr || bluimg == nullptr) {
    KLTError("(KLTWriteFeaturesToPPM)  Out of memory\n");
    exit(1);
  }

  /* Copy grey image to component images */
  if (sizeof(KLT_PixelType) != 1)
    KLTWarning("(KLTWriteFeaturesToPPM)  KLT_PixelType is not uchar");
  memcpy(redimg, greyimg, nbytes);
  memcpy(grnimg, greyimg, nbytes);
  memcpy(bluimg, greyimg, nbytes);
	
  /* Overlay features in red */
  for (i = 0 ; i < featurelist->nFeatures ; i++)
    if (featurelist->feature[i]->val >= 0)  {
      x = (int) (featurelist->feature[i]->x + 0.5);
      y = (int) (featurelist->feature[i]->y + 0.5);
      for (yy = y - 1 ; yy <= y + 1 ; yy++)
        for (xx = x - 1 ; xx <= x + 1 ; xx++)  
          if (xx >= 0 && yy >= 0 && xx < ncols && yy < nrows)  {
            offset = yy * ncols + xx;
            *(redimg + offset) = 255;
            *(grnimg + offset) = 0;
            *(bluimg + offset) = 0;
          }
    }
	
  /* Write to PPM file */
  ppmWriteFileRGB(filename, redimg, grnimg, bluimg, ncols, nrows);

  /* Free memory */
  free(redimg);
  free(grnimg);
  free(bluimg);
}


static FILE* _printSetupTxt(
  const char *fname, 	/* Input: filename, or NULL for stderr */
  const char *fmt,	/* Input: format (e.g., %5.1f or %3d) */
  char *format,	/* Output: format (e.g., (%5.1f,%5.1f)=%3d) */
  std::size_t format_size,
  char *type)	/* Output: either 'f' or 'd', based on input format */
{
  FILE *fp;
  const int val_width = 5;
  int i;

  /* Either open file or use stderr */
  if (fname == nullptr)  fp = stderr;
  else  fp = fopen(fname, "wb");
  if (fp == nullptr) {
    KLTError("(KLTWriteFeatures) "
             "Can't open file '%s' for writing\n", fname);
    exit(1);
  }

  /* Parse format */
  if (fmt[0] != '%') {
    KLTError("(KLTWriteFeatures) Bad Format: %s\n", fmt);
    exit(1);
  }
  i = 0;  while (fmt[i] != '\0') i++;  *type = fmt[i-1];
  if (*type != 'f' && *type != 'd') {
    KLTError("(KLTWriteFeatures) Format must end in 'f' or 'd'.");
    exit(1);
  }

  /* Construct feature format */
  snprintf(format, format_size, "(%s,%s)=%%%dd ", fmt, fmt, val_width);
     
  return fp;
}


static FILE* _printSetupBin(
  const char *fname) 	/* Input: filename */
{
  FILE *fp;
  if (fname == nullptr) {
    KLTError("(KLTWriteFeatures) Can't write binary data to stderr");
    exit(1);
  }
  fp = fopen(fname, "wb");
  if (fp == nullptr) {
    KLTError("(KLTWriteFeatures) "
             "Can't open file '%s' for writing", fname);
    exit(1);
  }
  return fp;
}


static void _printNhyphens(
  FILE *fp,
  int n)
{
  int i;
  for (i = 0 ; i < n ; i++)
    fprintf(fp, "-");
}

static void _printInteger(
  FILE *fp,
  int integer,
  int width)
{
  char fmt[80];
  snprintf(fmt, sizeof(fmt), "%%%dd", width);
  fprintf(fp, fmt, integer);
}


static KLT_BOOL _isCharInString(
  const char c,
  const char *str)
{
  int width = strlen(str);
  int i;

  for (i = 0 ; i < width ; i++)
    if (c == str[i])  return TRUE;

  return FALSE;
}


/*********************************************************************
 * _findStringWidth
 *
 * Calculates the length of a string after expansion.  E.g., the
 * length of "(%6.1f)" is eight -- six for the floating-point number,
 * and two for the parentheses.
 */

static int _findStringWidth(
  const char *str)
{
  int width = 0;
  int add;
  int maxi = strlen(str) - 1;
  int i = 0;

	
  while (str[i] != '\0')  {
    if (str[i] == '%')  {
      if (isdigit(str[i+1]))  {
        sscanf(str+i+1, "%d", &add);
        width += add;
        i += 2;
        while (!_isCharInString(str[i], "diouxefgn"))  {
          i++;
          if (i > maxi) {
            KLTError("(_findStringWidth) Can't determine length "
                     "of string '%s'", str);
            exit(1);
          }
        }
        i++;
      } else if (str[i+1] == 'c')  {
        width++;
        i += 2;
      } else {
        KLTError("(_findStringWidth) Can't determine length "
                 "of string '%s'", str);
        exit(1);
      }
    } else  {
      i++;
      width++;
    }
  }
	
  return width;
}


static void _printHeader(
  FILE *fp,
  const char *format,
  structureType id,
  int nFrames,
  int nFeatures)
{
  int width = _findStringWidth(format);
  int i;
	
  assert(id == FEATURE_LIST || id == FEATURE_HISTORY || id == FEATURE_TABLE);

  if (fp != stderr)  {
    fprintf(fp, "Feel free to place comments here.\n\n\n");
    fprintf(fp, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    fprintf(fp, "%s", warning_line);
    fprintf(fp, "\n");
  }
  fprintf(fp, "------------------------------\n");
  switch (id)  {
     case FEATURE_LIST: fprintf(fp, "KLT Feature List\n");    break;
     case FEATURE_HISTORY: fprintf(fp, "KLT Feature History\n"); break;
     case FEATURE_TABLE: fprintf(fp, "KLT Feature Table\n");   break;
  }

  fprintf(fp, "------------------------------\n\n");
  switch (id)  {
     case FEATURE_LIST: fprintf(fp, "nFeatures = %d\n\n", nFeatures);    break;
     case FEATURE_HISTORY: fprintf(fp, "nFrames = %d\n\n", nFrames); break;
     case FEATURE_TABLE: fprintf(fp, "nFrames = %d, nFeatures = %d\n\n",
                                 nFrames, nFeatures);   break;
  }

  switch (id)  {
     case FEATURE_LIST: fprintf(fp, "feature | (x,y)=val\n");
       fprintf(fp, "--------+-");
       _printNhyphens(fp, width);
       fprintf(fp, "\n");   
       break;
     case FEATURE_HISTORY: fprintf(fp, "frame | (x,y)=val\n");
       fprintf(fp, "------+-");
       _printNhyphens(fp, width);
       fprintf(fp, "\n");   
       break;
     case FEATURE_TABLE: fprintf(fp, "feature |          frame\n");   
       fprintf(fp, "        |");   
       for (i = 0 ; i < nFrames ; i++) _printInteger(fp, i, width);
       fprintf(fp, "\n--------+-");   
       for (i = 0 ; i < nFrames ; i++) _printNhyphens(fp, width);
       fprintf(fp, "\n");   
       break;
  }
}


static void _printFeatureTxt(
  FILE *fp,
  KLT_Feature feat,
  const char *format,
  const char type)
{
  assert(type == 'f' || type == 'd');

  if (type == 'f')
    fprintf(fp, format, (float) feat->x, (float) feat->y, feat->val);
  else if (type == 'd')  {
    /* Round x & y to nearest integer, unless negative */
    float x = feat->x;
    float y = feat->y;
    if (x >= 0.0) x += 0.5;
    if (y >= 0.0) y += 0.5;
    fprintf(fp, format, 
            (int) x, (int) y, feat->val);
  }
}


static void _printFeatureBin(
  FILE *fp,
  KLT_Feature feat)
{
  fwrite(&(feat->x), sizeof(KLT_locType), 1, fp);
  fwrite(&(feat->y), sizeof(KLT_locType), 1, fp);
  fwrite(&(feat->val), sizeof(int), 1, fp);
}


static void _printShutdown(
  FILE *fp)
{
  /* Close file, if necessary */
  if (fp != stderr)
    fclose(fp);
}


/*********************************************************************
 * KLTWriteFeatureList()
 * KLTWriteFeatureHistory()
 * KLTWriteFeatureTable()
 * 
 * Writes features to file or to screen.
 *
 * INPUTS
 * fname: name of file to write data; if NULL, then print to stderr
 * fmt:   format for printing (e.g., "%5.1f" or "%3d");
 *        if NULL, and if fname is not NULL, then write to binary file.
 */

void KLTWriteFeatureList(
  KLT_FeatureList fl,
  const char *fname,
  const char *fmt)
{
  FILE *fp;
  char format[100];
  char type;
  int i;

  if (KLT_verbose >= 1 && fname != nullptr)  {
    fprintf(stderr,  
            "(KLT) Writing feature list to %s file: '%s'\n", 
            (fmt == nullptr ? "binary" : "text"), fname);
  }

  if (fmt != nullptr) {  /* text file or stderr */
    fp = _printSetupTxt(fname, fmt, format, sizeof(format), &type);
    _printHeader(fp, format, FEATURE_LIST, 0, fl->nFeatures);
	
    for (i = 0 ; i < fl->nFeatures ; i++)  {
      fprintf(fp, "%7d | ", i);
      _printFeatureTxt(fp, fl->feature[i], format, type);
      fprintf(fp, "\n");
    }
    _printShutdown(fp);
  } else {  /* binary file */
    fp = _printSetupBin(fname);
    fwrite(binheader_fl, sizeof(char), BINHEADERLENGTH, fp); 
    fwrite(&(fl->nFeatures), sizeof(int), 1, fp);
    for (i = 0 ; i < fl->nFeatures ; i++)  {
      _printFeatureBin(fp, fl->feature[i]);
    }
    fclose(fp);
  }
}


void KLTWriteFeatureHistory(
  KLT_FeatureHistory fh,
  const char *fname,
  const char *fmt)
{
  FILE *fp;
  char format[100];
  char type;
  int i;

  if (KLT_verbose >= 1 && fname != nullptr)  {
    fprintf(stderr,  
            "(KLT) Writing feature history to %s file: '%s'\n", 
            (fmt == nullptr ? "binary" : "text"), fname);
  }

  if (fmt != nullptr) {  /* text file or stderr */
    fp = _printSetupTxt(fname, fmt, format, sizeof(format), &type);
    _printHeader(fp, format, FEATURE_HISTORY, fh->nFrames, 0);
	
    for (i = 0 ; i < fh->nFrames ; i++)  {
      fprintf(fp, "%5d | ", i);
      _printFeatureTxt(fp, fh->feature[i], format, type);
      fprintf(fp, "\n");
    }
    _printShutdown(fp);
  } else {  /* binary file */
    fp = _printSetupBin(fname);
    fwrite(binheader_fh, sizeof(char), BINHEADERLENGTH, fp); 
    fwrite(&(fh->nFrames), sizeof(int), 1, fp);
    for (i = 0 ; i < fh->nFrames ; i++)  {
      _printFeatureBin(fp, fh->feature[i]);
    }
    fclose(fp);
  }
}



void KLTWriteFeatureTable(
  KLT_FeatureTable ft,
  const char *fname,
  const char *fmt)
{
  FILE *fp;
  char format[100];
  char type;
  int i, j;

  if (KLT_verbose >= 1 && fname != nullptr)  {
    fprintf(stderr,  
            "(KLT) Writing feature table to %s file: '%s'\n", 
            (fmt == nullptr ? "binary" : "text"), fname);
  }

  if (fmt != nullptr) {  /* text file or stderr */
    fp = _printSetupTxt(fname, fmt, format, sizeof(format), &type);
    _printHeader(fp, format, FEATURE_TABLE, ft->nFrames, ft->nFeatures);

    for (j = 0 ; j < ft->nFeatures ; j++)  {
      fprintf(fp, "%7d | ", j);
      for (i = 0 ; i < ft->nFrames ; i++)
        _printFeatureTxt(fp, ft->feature[j][i], format, type);
      fprintf(fp, "\n");
    }
    _printShutdown(fp);
  } else {  /* binary file */
    fp = _printSetupBin(fname);
    fwrite(binheader_ft, sizeof(char), BINHEADERLENGTH, fp); 
    fwrite(&(ft->nFrames), sizeof(int), 1, fp);
    fwrite(&(ft->nFeatures), sizeof(int), 1, fp);
    for (j = 0 ; j < ft->nFeatures ; j++)  {
      for (i = 0 ; i < ft->nFrames ; i++)  {
        _printFeatureBin(fp, ft->feature[j][i]);
      }
    }
    fclose(fp);
  }
}



static structureType _readHeader(
  FILE *fp,
  int *nFrames,
  int *nFeatures,
  KLT_BOOL *binary)
{
#define LINELENGTH 100
  char line[LINELENGTH];
  structureType id;
	
  /* If file is binary, then read data and return */
  fread(line, sizeof(char), BINHEADERLENGTH, fp);
  line[BINHEADERLENGTH] = 0;
  if (strcmp(line, binheader_fl) == 0)  {
    assert(nFeatures != nullptr);
    fread(nFeatures, sizeof(int), 1, fp);
    *binary = TRUE;
    return FEATURE_LIST;
  } else if (strcmp(line, binheader_fh) == 0)  {
    assert(nFrames != nullptr);
    fread(nFrames, sizeof(int), 1, fp);
    *binary = TRUE;
    return FEATURE_HISTORY;
  } else if (strcmp(line, binheader_ft) == 0)  {
    assert(nFrames != nullptr);
    assert(nFeatures != nullptr);
    fread(nFrames, sizeof(int), 1, fp);
    fread(nFeatures, sizeof(int), 1, fp);
    *binary = TRUE;
    return FEATURE_TABLE;

    /* If file is NOT binary, then continue.*/
  } else {
    rewind(fp);
    *binary = FALSE;
  }

  /* Skip comments until warning line */
  while (strcmp(line, warning_line) != 0)  {
    fgets(line, LINELENGTH, fp);
    if (feof(fp)) {
      KLTError("(_readFeatures) File is corrupted -- Couldn't find line:\n"
               "\t%s\n", warning_line);
      exit(1);
    }
  }

  /* Read 'Feature List', 'Feature History', or 'Feature Table' */
  while (fgetc(fp) != '-');
  while (fgetc(fp) != '\n');
  fgets(line, LINELENGTH, fp);
  if (strcmp(line, "KLT Feature List\n") == 0) id = FEATURE_LIST;
  else if (strcmp(line, "KLT Feature History\n") == 0) id = FEATURE_HISTORY;
  else if (strcmp(line, "KLT Feature Table\n") == 0) id = FEATURE_TABLE;
  else {
    KLTError("(_readFeatures) File is corrupted -- (Not 'KLT Feature List', "
             "'KLT Feature History', or 'KLT Feature Table')");
    exit(1);
  }

  /* If there's an incompatibility between the type of file */
  /* and the parameters passed, exit now before we attempt */
  /* to write to non-allocated memory.  Higher routine should */
  /* detect and handle this error. */
  if ((id == FEATURE_LIST && nFeatures == nullptr) ||
      (id == FEATURE_HISTORY && nFrames == nullptr) ||
      (id == FEATURE_TABLE && (nFeatures == nullptr || nFrames == nullptr)))
    return id;

  /* Read nFeatures and nFrames */
  while (fgetc(fp) != '-');
  while (fgetc(fp) != '\n');
  fscanf(fp, "%s", line);
  if (id == FEATURE_LIST)  {
    if (strcmp(line, "nFeatures") != 0) {
      KLTError("(_readFeatures) File is corrupted -- "
               "(Expected 'nFeatures', found '%s' instead)", line);
      exit(1);
    }
  } else if (strcmp(line, "nFrames") != 0) {
    KLTError("(_readFeatures) File is corrupted -- "
             "(Expected 'nFrames', found '%s' instead)", line);
    exit(1);
  }
  fscanf(fp, "%s", line);
  if (strcmp(line, "=") != 0) {
    KLTError("(_readFeatures) File is corrupted -- "
             "(Expected '=', found '%s' instead)", line);
    exit(1);
  }
  if (id == FEATURE_LIST) fscanf(fp, "%d", nFeatures);
  else fscanf(fp, "%d", nFrames);

  /* If 'Feature Table', then also get nFeatures */
  if (id == FEATURE_TABLE)  {
    fscanf(fp, "%s", line);
    if (strcmp(line, ",") != 0) {
      KLTError("(_readFeatures) File is corrupted -- "
               "(Expected 'comma', found '%s' instead)", line);
      exit(1);
    }
    fscanf(fp, "%s", line);
    if (strcmp(line, "nFeatures") != 0) {
      KLTError("(_readFeatures) File is corrupted -- "
               "(2 Expected 'nFeatures ', found '%s' instead)", line);
      exit(1);
    }
    fscanf(fp, "%s", line);
    if (strcmp(line, "=") != 0) {
      KLTError("(_readFeatures) File is corrupted -- "
               "(2 Expected '= ', found '%s' instead)", line);
      exit(1);
    }
    fscanf(fp, "%d", nFeatures);
  }

  /* Skip junk before data */
  while (fgetc(fp) != '-');
  while (fgetc(fp) != '\n');

  return id;
#undef LINELENGTH
}


static void _readFeatureTxt(
  FILE *fp,
  KLT_Feature feat)
{
  while (fgetc(fp) != '(');
  fscanf(fp, "%f,%f)=%d", &(feat->x), &(feat->y), &(feat->val));
}


static void _readFeatureBin(
  FILE *fp,
  KLT_Feature feat)
{
  fread(&(feat->x), sizeof(KLT_locType), 1, fp);
  fread(&(feat->y), sizeof(KLT_locType), 1, fp);
  fread(&(feat->val), sizeof(int), 1, fp);
}


/*********************************************************************
 * KLTReadFeatureList
 * KLTReadFeatureHistory
 * KLTReadFeatureTable
 *
 * If the first parameter (fl, fh, or ft) is NULL, then the 
 * corresponding structure is created.
 */

KLT_FeatureList KLTReadFeatureList(
  KLT_FeatureList fl_in,
  const char *fname)
{
  FILE *fp;
  KLT_FeatureList fl;
  int nFeatures;
  structureType id;
  int indx;
  KLT_BOOL binary; 		/* whether file is binary or text */
  int i;

  fp = fopen(fname, "rb");
  if (fp == nullptr) {
        KLTError("(KLTReadFeatureList) Can't open file '%s' "
                            "for reading", fname);
        exit(1);
  }
  if (KLT_verbose >= 1) 
    fprintf(stderr,  "(KLT) Reading feature list from '%s'\n", fname);
  id = _readHeader(fp, nullptr, &nFeatures, &binary);
  if (id != FEATURE_LIST) {
    KLTError("(KLTReadFeatureList) File '%s' does not contain "
             "a FeatureList", fname);
    exit(1);
  }

  if (fl_in == nullptr)  {
    fl = KLTCreateFeatureList(nFeatures);
    fl->nFeatures = nFeatures;
  }
  else  {
    fl = fl_in;
    if (fl->nFeatures != nFeatures) {
      KLTError("(KLTReadFeatureList) The feature list passed "
               "does not contain the same number of features as "
               "the feature list in file '%s' ", fname);
      exit(1);
    }
  }

  if (!binary) {  /* text file */
    for (i = 0 ; i < fl->nFeatures ; i++)  {
      fscanf(fp, "%d |", &indx);
      if (indx != i) {
        KLTError("(KLTReadFeatureList) Bad index at i = %d"
                              "-- %d", i, indx);
        exit(1);
      }
      _readFeatureTxt(fp, fl->feature[i]);
    }
  } else {  /* binary file */
    for (i = 0 ; i < fl->nFeatures ; i++)  {
      _readFeatureBin(fp, fl->feature[i]);
    }
  }

  fclose(fp);

  return fl;
}


KLT_FeatureHistory KLTReadFeatureHistory(
  KLT_FeatureHistory fh_in,
  const char *fname)
{
  FILE *fp;
  KLT_FeatureHistory fh;
  int nFrames;
  structureType id;
  int indx;
  KLT_BOOL binary; 		/* whether file is binary or text */
  int i;

  fp = fopen(fname, "rb");
  if (fp == nullptr) {
        KLTError("(KLTReadFeatureHistory) Can't open file '%s' "
                            "for reading", fname);
        exit(1);
  }
  if (KLT_verbose >= 1) fprintf(stderr,  "(KLT) Reading feature history from '%s'\n", fname);
  id = _readHeader(fp, &nFrames, nullptr, &binary);
  if (id != FEATURE_HISTORY) {
        KLTError("(KLTReadFeatureHistory) File '%s' does not contain "
                                      "a FeatureHistory", fname);
        exit(1);
  }

  if (fh_in == nullptr)  {
    fh = KLTCreateFeatureHistory(nFrames);
    fh->nFrames = nFrames;
  }
  else  {
    fh = fh_in;
    if (fh->nFrames != nFrames) {
      KLTError("(KLTReadFeatureHistory) The feature history passed "
               "does not contain the same number of frames as "
               "the feature history in file '%s' ", fname);
      exit(1);
    }
  }

  if (!binary) {  /* text file */
    for (i = 0 ; i < fh->nFrames ; i++)  {
      fscanf(fp, "%d |", &indx);
      if (indx != i) {
        KLTError("(KLTReadFeatureHistory) Bad index at i = %d"
                 "-- %d", i, indx);
        exit(1);
      }
      _readFeatureTxt(fp, fh->feature[i]);
    }
  } else {  /* binary file */
    for (i = 0 ; i < fh->nFrames ; i++)  {
      _readFeatureBin(fp, fh->feature[i]);
    }
  }

  fclose(fp);

  return fh;
}


KLT_FeatureTable KLTReadFeatureTable(
  KLT_FeatureTable ft_in,
  const char *fname)
{
  FILE *fp;
  KLT_FeatureTable ft;
  int nFrames;
  int nFeatures;
  structureType id;
  int indx;
  KLT_BOOL binary; 		/* whether file is binary or text */
  int i, j;

  fp = fopen(fname, "rb");
  if (fp == nullptr) {
        KLTError("(KLTReadFeatureTable) Can't open file '%s' "
                            "for reading", fname);
        exit(1);
  }
  if (KLT_verbose >= 1) fprintf(stderr,  "(KLT) Reading feature table from '%s'\n", fname);
  id = _readHeader(fp, &nFrames, &nFeatures, &binary);
  if (id != FEATURE_TABLE) {
        KLTError("(KLTReadFeatureTable) File '%s' does not contain "
                                    "a FeatureTable", fname);
        exit(1);
  }

  if (ft_in == nullptr)  {
    ft = KLTCreateFeatureTable(nFrames, nFeatures);
    ft->nFrames = nFrames;
    ft->nFeatures = nFeatures;
  }
  else  {
    ft = ft_in;
					
    if (ft->nFrames != nFrames || ft->nFeatures != nFeatures) {
      KLTError("(KLTReadFeatureTable) The feature table passed "
               "does not contain the same number of frames and "
               "features as the feature table in file '%s' ", fname);
      exit(1);
    }
  }

  if (!binary) {  /* text file */
    for (j = 0 ; j < ft->nFeatures ; j++)  {
      fscanf(fp, "%d |", &indx);
      if (indx != j) {
        KLTError("(KLTReadFeatureTable) Bad index at j = %d"
                 "-- %d", j, indx);
        exit(1);
      }
      for (i = 0 ; i < ft->nFrames ; i++)
        _readFeatureTxt(fp, ft->feature[j][i]);
    }
  } else {  /* binary file */
    for (j = 0 ; j < ft->nFeatures ; j++)  {
      for (i = 0 ; i < ft->nFrames ; i++)
        _readFeatureBin(fp, ft->feature[j][i]);
    }
  }

  fclose(fp);

  return ft;
}

