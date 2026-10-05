/*********************************************************************
 * klt_util.c
 *********************************************************************/

/* Standard includes */
#include <algorithm>
#include <cassert>
#include <cstdlib>  /* malloc() */
#include <cmath>		/* fabs() */

/* Our includes */
#include "base.h"
#include "error.h"
#include "pnmio.h"
#include "klt.h"
#include "klt_util.h"

using namespace std;

/*********************************************************************/

float _KLTComputeSmoothSigma(
  KLT_TrackingContext tc)
{
  return (tc->smooth_sigma_fact * max(tc->window_width, tc->window_height));
}


/*********************************************************************
 * _KLTCreateFloatImage
 */

_KLT_FloatImage _KLTCreateFloatImage(
  int ncols,
  int nrows)
{
  _KLT_FloatImage floatimg;
  int nbytes = sizeof(_KLT_FloatImageRec) +
    ncols * nrows * sizeof(float);

  floatimg = (_KLT_FloatImage)  malloc(nbytes);
  if (floatimg == nullptr) {
    KLTError("(_KLTCreateFloatImage)  Out of memory");
    exit(1);
  }
  floatimg->ncols = ncols;
  floatimg->nrows = nrows;
  floatimg->data = (float *)  (floatimg + 1);

  return(floatimg);
}


/*********************************************************************
 * _KLTFreeFloatImage
 */

void _KLTFreeFloatImage(
  _KLT_FloatImage floatimg)
{
  free(floatimg);
}


/*********************************************************************
 * _KLTPrintSubFloatImage
 */

void _KLTPrintSubFloatImage(
  _KLT_FloatImage floatimg,
  int x0, int y0,
  int width, int height)
{
  int ncols = floatimg->ncols;
  int offset;
  int i, j;

  assert(x0 >= 0);
  assert(y0 >= 0);
  assert(x0 + width <= ncols);
  assert(y0 + height <= floatimg->nrows);

  fprintf(stderr, "\n");
  for (j = 0 ; j < height ; j++)  {
    for (i = 0 ; i < width ; i++)  {
      offset = (j+y0)*ncols + (i+x0);
      fprintf(stderr, "%6.2f ", *(floatimg->data + offset));
    }
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n");
}
	

/*********************************************************************
 * _KLTWriteFloatImageToPGM
 */

void _KLTWriteFloatImageToPGM(
  _KLT_FloatImage img,
  const char *filename)
{
  int npixs = img->ncols * img->nrows;
  float mmax = -999999.9f, mmin = 999999.9f;
  float fact;
  float *ptr;
  uchar *byteimg, *ptrout;
  int i;

  /* Calculate minimum and maximum values of float image */
  ptr = img->data;
  for (i = 0 ; i < npixs ; i++)  {
    mmax = max(mmax, *ptr);
    mmin = min(mmin, *ptr);
    ptr++;
  }
	
  /* Allocate memory to hold converted image */
  byteimg = (uchar *) malloc(npixs * sizeof(uchar));

  /* Convert image from float to uchar */
  fact = 255.0f / (mmax-mmin);
  ptr = img->data;
  ptrout = byteimg;
  for (i = 0 ; i < npixs ; i++)  {
    *ptrout++ = (uchar) ((*ptr++ - mmin) * fact);
  }

  /* Write uchar image to PGM */
  pgmWriteFile(filename, byteimg, img->ncols, img->nrows);

  /* Free memory */
  free(byteimg);
}

/*********************************************************************
 * _KLTWriteFloatImageToPGM
 */

void _KLTWriteAbsFloatImageToPGM(
  _KLT_FloatImage img,
  const char *filename,float scale)
{
  int npixs = img->ncols * img->nrows;
  float fact;
  float *ptr;
  uchar *byteimg, *ptrout;
  int i;
  float tmp;
	
  /* Allocate memory to hold converted image */
  byteimg = (uchar *) malloc(npixs * sizeof(uchar));

  /* Convert image from float to uchar */
  fact = 255.0f / scale;
  ptr = img->data;
  ptrout = byteimg;
  for (i = 0 ; i < npixs ; i++)  {
    tmp = (float) (fabs(*ptr++) * fact);
    if(tmp > 255.0) tmp = 255.0;
    *ptrout++ =  (uchar) tmp;
  }

  /* Write uchar image to PGM */
  pgmWriteFile(filename, byteimg, img->ncols, img->nrows);

  /* Free memory */
  free(byteimg);
}
