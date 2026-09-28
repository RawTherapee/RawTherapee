/*********************************************************************
 * pnmio.c
 *
 * Various routines to manipulate PNM files.
 *********************************************************************/


/* Standard includes */
#include <cstdio>   /* FILE  */
#include <cstdlib>  /* malloc(), atoi() */

/* Our includes */
#include "error.h"

#define LENGTH 80


/*********************************************************************/

static void _getNextString(
  FILE *fp,
  char *line)
{
  int i;

  line[0] = '\0';

  while (line[0] == '\0')  {
    fscanf(fp, "%s", line);
    i = -1;
    do  {
      i++;
      if (line[i] == '#')  {
        line[i] = '\0';
        while (fgetc(fp) != '\n') ;
      }
    }  while (line[i] != '\0');
  }
}


/*********************************************************************
 * pnmReadHeader
 */

void pnmReadHeader(
  FILE *fp, 
  int *magic, 
  int *ncols, int *nrows, 
  int *maxval)
{
  char line[LENGTH];
	
  /* Read magic number */
  _getNextString(fp, line);
  if (line[0] != 'P') {
    KLTError("(pnmReadHeader) Magic number does not begin with 'P', "
             "but with a '%c'", line[0]);
    exit(1);
  }
  sscanf(line, "P%d", magic);
	
  /* Read size, skipping comments */
  _getNextString(fp, line);
  *ncols = atoi(line);
  _getNextString(fp, line);
  *nrows = atoi(line);
  if (*ncols < 0 || *nrows < 0 || *ncols > 10000 || *nrows > 10000) {
    KLTError("(pnmReadHeader) The dimensions %d x %d are unacceptable",
             *ncols, *nrows);
    exit(1);
  }
	
  /* Read maxval, skipping comments */
  _getNextString(fp, line);
  *maxval = atoi(line);
  fread(line, 1, 1, fp); /* Read newline which follows maxval */
	
  if (*maxval != 255)
    KLTWarning("(pnmReadHeader) Maxval is not 255, but %d", *maxval);
}


/*********************************************************************
 * pgmReadHeader
 */

void pgmReadHeader(
  FILE *fp, 
  int *magic, 
  int *ncols, int *nrows, 
  int *maxval)
{
  pnmReadHeader(fp, magic, ncols, nrows, maxval);
  if (*magic != 5) {
    KLTError("(pgmReadHeader) Magic number is not 'P5', but 'P%d'", *magic);
    exit(1);
  }
}


/*********************************************************************
 * ppmReadHeader
 */

void ppmReadHeader(
  FILE *fp, 
  int *magic, 
  int *ncols, int *nrows, 
  int *maxval)
{
  pnmReadHeader(fp, magic, ncols, nrows, maxval);
  if (*magic != 6) {
    KLTError("(ppmReadHeader) Magic number is not 'P6', but 'P%d'", *magic);
    exit(1);
  }
}


/*********************************************************************
 * pgmReadHeaderFile
 */

void pgmReadHeaderFile(
  const char *fname,
  int *magic, 
  int *ncols, int *nrows, 
  int *maxval)
{
  FILE *fp;

  /* Open file */
  if ( (fp = fopen(fname, "rb")) == nullptr) {
    KLTError("(pgmReadHeaderFile) Can't open file named '%s' for reading\n", fname);
    exit(1);
  }

  /* Read header */
  pgmReadHeader(fp, magic, ncols, nrows, maxval);

  /* Close file */
  fclose(fp);
}


/*********************************************************************
 * ppmReadHeaderFile
 */

void ppmReadHeaderFile(
  const char *fname,
  int *magic, 
  int *ncols, int *nrows, 
  int *maxval)
{
  FILE *fp;

  /* Open file */
  if ( (fp = fopen(fname, "rb")) == nullptr) {
    KLTError("(ppmReadHeaderFile) Can't open file named '%s' for reading\n", fname);
    exit(1);
  }

  /* Read header */
  ppmReadHeader(fp, magic, ncols, nrows, maxval);

  /* Close file */
  fclose(fp);
}


/*********************************************************************
 * pgmRead
 *
 * NOTE:  If img is NULL, memory is allocated.
 */

unsigned char* pgmRead(
  FILE *fp,
  unsigned char *img,
  int *ncols, int *nrows)
{
  unsigned char *ptr;
  int magic, maxval;
  int i;

  /* Read header */
  pgmReadHeader(fp, &magic, ncols, nrows, &maxval);

  /* Allocate memory, if necessary, and set pointer */
  if (img == nullptr)  {
    ptr = (unsigned char *) malloc(*ncols * *nrows * sizeof(char));
    if (ptr == nullptr) {
      KLTError("(pgmRead) Memory not allocated");
      exit(1);
    }
  }
  else
    ptr = img;

  /* Read binary image data */
  {
    unsigned char *tmpptr = ptr;
    for (i = 0 ; i < *nrows ; i++)  {
      fread(tmpptr, *ncols, 1, fp);
      tmpptr += *ncols;
    }
  }

  return ptr;
}


/*********************************************************************
 * pgmReadFile
 *
 * NOTE:  If img is NULL, memory is allocated.
 */

unsigned char* pgmReadFile(
  const char *fname,
  unsigned char *img,
  int *ncols, int *nrows)
{
  unsigned char *ptr;
  FILE *fp;

  /* Open file */
  if ( (fp = fopen(fname, "rb")) == nullptr) {
    KLTError("(pgmReadFile) Can't open file named '%s' for reading\n", fname);
    exit(1);
  }

  /* Read file */
  ptr = pgmRead(fp, img, ncols, nrows);

  /* Close file */
  fclose(fp);

  return ptr;
}


/*********************************************************************
 * pgmWrite
 */

void pgmWrite(
  FILE *fp,
  const unsigned char *img,
  int ncols, 
  int nrows)
{
  int i;

  /* Write header */
  fprintf(fp, "P5\n");
  fprintf(fp, "%d %d\n", ncols, nrows);
  fprintf(fp, "255\n");

  /* Write binary data */
  for (i = 0 ; i < nrows ; i++)  {
    fwrite(img, ncols, 1, fp);
    img += ncols;
  }
}


/*********************************************************************
 * pgmWriteFile
 */

void pgmWriteFile(
  const char *fname,
  const unsigned char *img,
  int ncols, 
  int nrows)
{
  FILE *fp;

  /* Open file */
  if ( (fp = fopen(fname, "wb")) == nullptr) {
    KLTError("(pgmWriteFile) Can't open file named '%s' for writing\n", fname);
    exit(1);
  }

  /* Write to file */
  pgmWrite(fp, img, ncols, nrows);

  /* Close file */
  fclose(fp);
}


/*********************************************************************
 * ppmWrite
 */

void ppmWrite(
  FILE *fp,
  const unsigned char *redimg,
  const unsigned char *greenimg,
  const unsigned char *blueimg,
  int ncols, 
  int nrows)
{
  int i, j;

  /* Write header */
  fprintf(fp, "P6\n");
  fprintf(fp, "%d %d\n", ncols, nrows);
  fprintf(fp, "255\n");

  /* Write binary data */
  for (j = 0 ; j < nrows ; j++)  {
    for (i = 0 ; i < ncols ; i++)  {
      fwrite(redimg, 1, 1, fp); 
      fwrite(greenimg, 1, 1, fp);
      fwrite(blueimg, 1, 1, fp);
      redimg++;  greenimg++;  blueimg++;
    }
  }
}


/*********************************************************************
 * ppmWriteFileRGB
 */

void ppmWriteFileRGB(
  const char *fname,
  const unsigned char *redimg,
  const unsigned char *greenimg,
  const unsigned char *blueimg,
  int ncols, 
  int nrows)
{
  FILE *fp;

  /* Open file */
  if ( (fp = fopen(fname, "wb")) == nullptr) {
    KLTError("(ppmWriteFileRGB) Can't open file named '%s' for writing\n", fname);
    exit(1);
  }

  /* Write to file */
  ppmWrite(fp, redimg, greenimg, blueimg, ncols, nrows);

  /* Close file */
  fclose(fp);
}


