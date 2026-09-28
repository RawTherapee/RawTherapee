/*********************************************************************
 * selectGoodFeatures.c
 *
 *********************************************************************/

/* Standard includes */
#include <cassert>
#include <cstdlib> /* malloc(), qsort() */
#include <cstdio>  /* fflush()          */
#include <cstring> /* memset()          */
#include <cmath>   /* fsqrt()           */

/* Our includes */
#include "base.h"
#include "error.h"
#include "convolve.h"
#include "klt.h"
#include "klt_util.h"
#include "pyramid.h"

int KLT_verbose = 1;

typedef enum {SELECTING_ALL, REPLACING_SOME} selectionMode;


/*********************************************************************
 * _quicksort
 * Replacement for qsort().  Computing time is decreased by taking
 * advantage of specific knowledge of our array (that there are 
 * three ints associated with each point).
 *
 * This routine generously provided by 
 *      Manolis Lourakis <lourakis@csi.forth.gr>
 *
 * NOTE: The results of this function may be slightly different from
 * those of qsort().  This is due to the fact that different sort 
 * algorithms have different behaviours when sorting numbers with the 
 * same value: Some leave them in the same relative positions in the 
 * array, while others change their relative positions. For example, 
 * if you have the array [c d b1 a b2] with b1=b2, it may be sorted as 
 * [a b1 b2 c d] or [a b2 b1 c d].
 */

#define SWAP3(list, i, j)               \
{ int *pi, *pj, tmp;            \
     pi=list+3*(i); pj=list+3*(j);      \
                                        \
     tmp=*pi;    \
     *pi++=*pj;  \
     *pj++=tmp;  \
                 \
     tmp=*pi;    \
     *pi++=*pj;  \
     *pj++=tmp;  \
                 \
     tmp=*pi;    \
     *pi=*pj;    \
     *pj=tmp;    \
}

void _quicksort(int *pointlist, int n)
{
  unsigned int i, j, ln, rn;

  while (n > 1)
  {
    SWAP3(pointlist, 0, n/2);
    for (i = 0, j = n; ; )
    {
      do
        --j;
      while (pointlist[3*j+2] < pointlist[2]);
      do
        ++i;
      while (i < j && pointlist[3*i+2] > pointlist[2]);
      if (i >= j)
        break;
      SWAP3(pointlist, i, j);
    }
    SWAP3(pointlist, j, 0);
    ln = j;
    rn = n - ++j;
    if (ln < rn)
    {
      _quicksort(pointlist, ln);
      pointlist += 3*j;
      n = rn;
    }
    else
    {
      _quicksort(pointlist + 3*j, rn);
      n = ln;
    }
  }
}
#undef SWAP3


/*********************************************************************/

static void _fillFeaturemap(
  int x, int y, 
  uchar *featuremap, 
  int mindist, 
  int ncols, 
  int nrows)
{
  int ix, iy;

  for (iy = y - mindist ; iy <= y + mindist ; iy++)
    for (ix = x - mindist ; ix <= x + mindist ; ix++)
      if (ix >= 0 && ix < ncols && iy >= 0 && iy < nrows)
        featuremap[iy*ncols+ix] = 1;
}


/*********************************************************************
 * _enforceMinimumDistance
 *
 * Removes features that are within close proximity to better features.
 *
 * INPUTS
 * featurelist:  A list of features.  The nFeatures property
 *               is used.
 *
 * OUTPUTS
 * featurelist:  Is overwritten.  Nearby "redundant" features are removed.
 *               Writes -1's into the remaining elements.
 *
 * RETURNS
 * The number of remaining features.
 */

static void _enforceMinimumDistance(
  int *pointlist,              /* featurepoints */
  int npoints,                 /* number of featurepoints */
  KLT_FeatureList featurelist, /* features */
  int ncols, int nrows,        /* size of images */
  int mindist,                 /* min. dist b/w features */
  int min_eigenvalue,          /* min. eigenvalue */
  KLT_BOOL overwriteAllFeatures)
{
  int indx;          /* Index into features */
  int x, y, val;     /* Location and trackability of pixel under consideration */
  uchar *featuremap; /* Boolean array recording proximity of features */
  int *ptr;
	
  /* Cannot add features with an eigenvalue less than one */
  if (min_eigenvalue < 1)  min_eigenvalue = 1;

  /* Allocate memory for feature map and clear it */
  featuremap = (uchar *) malloc(ncols * nrows * sizeof(uchar));
  memset(featuremap, 0, ncols*nrows);
	
  /* Necessary because code below works with (mindist-1) */
  mindist--;

  /* If we are keeping all old good features, then add them to the featuremap */
  if (!overwriteAllFeatures)
    for (indx = 0 ; indx < featurelist->nFeatures ; indx++)
      if (featurelist->feature[indx]->val >= 0)  {
        x   = (int) featurelist->feature[indx]->x;
        y   = (int) featurelist->feature[indx]->y;
        _fillFeaturemap(x, y, featuremap, mindist, ncols, nrows);
      }

  /* For each feature point, in descending order of importance, do ... */
  ptr = pointlist;
  indx = 0;
  while (1)  {

    /* If we can't add all the points, then fill in the rest
       of the featurelist with -1's */
    if (ptr >= pointlist + 3*npoints)  {
      while (indx < featurelist->nFeatures)  {	
        if (overwriteAllFeatures || 
            featurelist->feature[indx]->val < 0) {
          featurelist->feature[indx]->x   = -1;
          featurelist->feature[indx]->y   = -1;
          featurelist->feature[indx]->val = KLT_NOT_FOUND;
	  featurelist->feature[indx]->aff_img = nullptr;
	  featurelist->feature[indx]->aff_img_gradx = nullptr;
	  featurelist->feature[indx]->aff_img_grady = nullptr;
	  featurelist->feature[indx]->aff_x = -1.0;
	  featurelist->feature[indx]->aff_y = -1.0;
	  featurelist->feature[indx]->aff_Axx = 1.0;
	  featurelist->feature[indx]->aff_Ayx = 0.0;
	  featurelist->feature[indx]->aff_Axy = 0.0;
	  featurelist->feature[indx]->aff_Ayy = 1.0;
        }
        indx++;
      }
      break;
    }

    x   = *ptr++;
    y   = *ptr++;
    val = *ptr++;
		
    /* Ensure that feature is in-bounds */
    assert(x >= 0);
    assert(x < ncols);
    assert(y >= 0);
    assert(y < nrows);
	
    while (!overwriteAllFeatures && 
           indx < featurelist->nFeatures &&
           featurelist->feature[indx]->val >= 0)
      indx++;

    if (indx >= featurelist->nFeatures)  break;

    /* If no neighbor has been selected, and if the minimum
       eigenvalue is large enough, then add feature to the current list */
    if (!featuremap[y*ncols+x] && val >= min_eigenvalue)  {
      featurelist->feature[indx]->x   = (KLT_locType) x;
      featurelist->feature[indx]->y   = (KLT_locType) y;
      featurelist->feature[indx]->val = (int) val;
      featurelist->feature[indx]->aff_img = nullptr;
      featurelist->feature[indx]->aff_img_gradx = nullptr;
      featurelist->feature[indx]->aff_img_grady = nullptr;
      featurelist->feature[indx]->aff_x = -1.0;
      featurelist->feature[indx]->aff_y = -1.0;
      featurelist->feature[indx]->aff_Axx = 1.0;
      featurelist->feature[indx]->aff_Ayx = 0.0;
      featurelist->feature[indx]->aff_Axy = 0.0;
      featurelist->feature[indx]->aff_Ayy = 1.0;
      indx++;

      /* Fill in surrounding region of feature map, but
         make sure that pixels are in-bounds */
      _fillFeaturemap(x, y, featuremap, mindist, ncols, nrows);
    }
  }

  /* Free feature map  */
  free(featuremap);
}


/*********************************************************************
 * _comparePoints
 *
 * Used by qsort (in _KLTSelectGoodFeatures) to determine
 * which feature is better.
 * By switching the '>' with the '<', qsort is fooled into sorting 
 * in descending order.
 */

#ifdef KLT_USE_QSORT
static int _comparePoints(const void *a, const void *b)
{
  int v1 = *(((int *) a) + 2);
  int v2 = *(((int *) b) + 2);

  if (v1 > v2)  return(-1);
  else if (v1 < v2)  return(1);
  else return(0);
}
#endif


/*********************************************************************
 * _sortPointList
 */

static void _sortPointList(
  int *pointlist,
  int npoints)
{
#ifdef KLT_USE_QSORT
  qsort(pointlist, npoints, 3*sizeof(int), _comparePoints);
#else
  _quicksort(pointlist, npoints);
#endif
}


/*********************************************************************
 * _minEigenvalue
 *
 * Given the three distinct elements of the symmetric 2x2 matrix
 *                     [gxx gxy]
 *                     [gxy gyy],
 * Returns the minimum eigenvalue of the matrix.  
 */

static float _minEigenvalue(float gxx, float gxy, float gyy)
{
  return (float) ((gxx + gyy - sqrt((gxx - gyy)*(gxx - gyy) + 4*gxy*gxy))/2.0f);
}
	

/*********************************************************************/

void _KLTSelectGoodFeatures(
  KLT_TrackingContext tc,
  KLT_PixelType *img, 
  int ncols, 
  int nrows,
  KLT_FeatureList featurelist,
  selectionMode mode)
{
  _KLT_FloatImage floatimg, gradx, grady;
  int window_hw, window_hh;
  int *pointlist;
  int npoints = 0;
  KLT_BOOL overwriteAllFeatures = (mode == SELECTING_ALL) ?
    TRUE : FALSE;
  KLT_BOOL floatimages_created = FALSE;

  /* Check window size (and correct if necessary) */
  if (tc->window_width % 2 != 1) {
    tc->window_width = tc->window_width+1;
    KLTWarning("Tracking context's window width must be odd.  "
               "Changing to %d.\n", tc->window_width);
  }
  if (tc->window_height % 2 != 1) {
    tc->window_height = tc->window_height+1;
    KLTWarning("Tracking context's window height must be odd.  "
               "Changing to %d.\n", tc->window_height);
  }
  if (tc->window_width < 3) {
    tc->window_width = 3;
    KLTWarning("Tracking context's window width must be at least three.  \n"
               "Changing to %d.\n", tc->window_width);
  }
  if (tc->window_height < 3) {
    tc->window_height = 3;
    KLTWarning("Tracking context's window height must be at least three.  \n"
               "Changing to %d.\n", tc->window_height);
  }
  window_hw = tc->window_width/2; 
  window_hh = tc->window_height/2;
		
  /* Create pointlist, which is a simplified version of a featurelist, */
  /* for speed.  Contains only integer locations and values. */
  pointlist = (int *) malloc(ncols * nrows * 3 * sizeof(int));

  /* Create temporary images, etc. */
  if (mode == REPLACING_SOME && 
      tc->sequentialMode && tc->pyramid_last != nullptr)  {
    floatimg = ((_KLT_Pyramid) tc->pyramid_last)->img[0];
    gradx = ((_KLT_Pyramid) tc->pyramid_last_gradx)->img[0];
    grady = ((_KLT_Pyramid) tc->pyramid_last_grady)->img[0];
    assert(gradx != nullptr);
    assert(grady != nullptr);
  } else  {
    floatimages_created = TRUE;
    floatimg = _KLTCreateFloatImage(ncols, nrows);
    gradx    = _KLTCreateFloatImage(ncols, nrows);
    grady    = _KLTCreateFloatImage(ncols, nrows);
    if (tc->smoothBeforeSelecting)  {
      _KLT_FloatImage tmpimg;
      tmpimg = _KLTCreateFloatImage(ncols, nrows);
      _KLTToFloatImage(img, ncols, nrows, tmpimg);
      _KLTComputeSmoothedImage(tmpimg, _KLTComputeSmoothSigma(tc), floatimg);
      _KLTFreeFloatImage(tmpimg);
    } else _KLTToFloatImage(img, ncols, nrows, floatimg);
 
    /* Compute gradient of image in x and y direction */
    _KLTComputeGradients(floatimg, tc->grad_sigma, gradx, grady);
  }
	
  /* Write internal images */
  if (tc->writeInternalImages)  {
    _KLTWriteFloatImageToPGM(floatimg, "kltimg_sgfrlf.pgm");
    _KLTWriteFloatImageToPGM(gradx, "kltimg_sgfrlf_gx.pgm");
    _KLTWriteFloatImageToPGM(grady, "kltimg_sgfrlf_gy.pgm");
  }

  /* Compute trackability of each image pixel as the minimum
     of the two eigenvalues of the Z matrix */
  {
    float gx, gy;
    float gxx, gxy, gyy;
    int xx, yy;
    int *ptr;
    float val;
    unsigned int limit = 1;
    int borderx = tc->borderx;	/* Must not touch cols */
    int bordery = tc->bordery;	/* lost by convolution */
    int x, y;
	
    if (borderx < window_hw)  borderx = window_hw;
    if (bordery < window_hh)  bordery = window_hh;

    /* Find largest value of an int */
    for (size_t i = 0 ; i < sizeof(int) ; i++)  limit *= 256;
    limit = limit/2 - 1;
		
    /* For most of the pixels in the image, do ... */
    ptr = pointlist;
    for (y = bordery ; y < nrows - bordery ; y += tc->nSkippedPixels + 1)
      for (x = borderx ; x < ncols - borderx ; x += tc->nSkippedPixels + 1)  {

        /* Sum the gradients in the surrounding window */
        gxx = 0;  gxy = 0;  gyy = 0;
        for (yy = y-window_hh ; yy <= y+window_hh ; yy++)
          for (xx = x-window_hw ; xx <= x+window_hw ; xx++)  {
            gx = *(gradx->data + ncols*yy+xx);
            gy = *(grady->data + ncols*yy+xx);
            gxx += gx * gx;
            gxy += gx * gy;
            gyy += gy * gy;
          }

        /* Store the trackability of the pixel as the minimum
           of the two eigenvalues */
        *ptr++ = x;
        *ptr++ = y;
        val = _minEigenvalue(gxx, gxy, gyy);
        if (val > limit)  {
          KLTWarning("(_KLTSelectGoodFeatures) minimum eigenvalue %f is "
                     "greater than the capacity of an int; setting "
                     "to maximum value", val);
          val = (float) limit;
        }
        *ptr++ = (int) val;
        npoints++;
      }
  }
			
  /* Sort the features  */
  _sortPointList(pointlist, npoints);

  /* Check tc->mindist */
  if (tc->mindist < 0)  {
    KLTWarning("(_KLTSelectGoodFeatures) Tracking context field tc->mindist "
               "is negative (%d); setting to zero", tc->mindist);
    tc->mindist = 0;
  }

  /* Enforce minimum distance between features */
  _enforceMinimumDistance(
    pointlist,
    npoints,
    featurelist,
    ncols, nrows,
    tc->mindist,
    tc->min_eigenvalue,
    overwriteAllFeatures);

  /* Free memory */
  free(pointlist);
  if (floatimages_created)  {
    _KLTFreeFloatImage(floatimg);
    _KLTFreeFloatImage(gradx);
    _KLTFreeFloatImage(grady);
  }
}


/*********************************************************************
 * KLTSelectGoodFeatures
 *
 * Main routine, visible to the outside.  Finds the good features in
 * an image.  
 * 
 * INPUTS
 * tc:	Contains parameters used in computation (size of image,
 *        size of window, min distance b/w features, sigma to compute
 *        image gradients, # of features desired).
 * img:	Pointer to the data of an image (probably unsigned chars).
 * 
 * OUTPUTS
 * features:	List of features.  The member nFeatures is computed.
 */

void KLTSelectGoodFeatures(
  KLT_TrackingContext tc,
  KLT_PixelType *img, 
  int ncols, 
  int nrows,
  KLT_FeatureList fl)
{
  if (KLT_verbose >= 1)  {
    fprintf(stderr,  "(KLT) Selecting the %d best features "
            "from a %d by %d image...  ", fl->nFeatures, ncols, nrows);
    fflush(stderr);
  }

  _KLTSelectGoodFeatures(tc, img, ncols, nrows, 
                         fl, SELECTING_ALL);

  if (KLT_verbose >= 1)  {
    fprintf(stderr,  "\n\t%d features found.\n", 
            KLTCountRemainingFeatures(fl));
    if (tc->writeInternalImages)
      fprintf(stderr,  "\tWrote images to 'kltimg_sgfrlf*.pgm'.\n");
    fflush(stderr);
  }
}


/*********************************************************************
 * KLTReplaceLostFeatures
 *
 * Main routine, visible to the outside.  Replaces the lost features 
 * in an image.  
 * 
 * INPUTS
 * tc:	Contains parameters used in computation (size of image,
 *        size of window, min distance b/w features, sigma to compute
 *        image gradients, # of features desired).
 * img:	Pointer to the data of an image (probably unsigned chars).
 * 
 * OUTPUTS
 * features:	List of features.  The member nFeatures is computed.
 */

void KLTReplaceLostFeatures(
  KLT_TrackingContext tc,
  KLT_PixelType *img, 
  int ncols, 
  int nrows,
  KLT_FeatureList fl)
{
  int nLostFeatures = fl->nFeatures - KLTCountRemainingFeatures(fl);

  if (KLT_verbose >= 1)  {
    fprintf(stderr,  "(KLT) Attempting to replace %d features "
            "in a %d by %d image...  ", nLostFeatures, ncols, nrows);
    fflush(stderr);
  }

  /* If there are any lost features, replace them */
  if (nLostFeatures > 0)
    _KLTSelectGoodFeatures(tc, img, ncols, nrows, 
                           fl, REPLACING_SOME);

  if (KLT_verbose >= 1)  {
    fprintf(stderr,  "\n\t%d features replaced.\n",
            nLostFeatures - fl->nFeatures + KLTCountRemainingFeatures(fl));
    if (tc->writeInternalImages)
      fprintf(stderr,  "\tWrote images to 'kltimg_sgfrlf*.pgm'.\n");
    fflush(stderr);
  }
}


