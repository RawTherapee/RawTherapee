/*********************************************************************
 * trackFeatures.c
 *
 *********************************************************************/

/* Standard includes */
#include <cassert>
#include <cmath>		/* fabs() */
#include <cstdlib>		/* malloc() */
#include <cstdio>		/* fflush() */

/* Our includes */
#include "base.h"
#include "error.h"
#include "convolve.h"	/* for computing pyramid */
#include "klt.h"
#include "klt_util.h"	/* _KLT_FloatImage */
#include "pyramid.h"	/* _KLT_Pyramid */

extern int KLT_verbose;

typedef float *_FloatWindow;

/*********************************************************************
 * _interpolate
 *
 * Given a point (x,y) in an image, computes the bilinear interpolated
 * gray-level value of the point in the image.
 */

static float _interpolate(
  float x,
  float y,
  _KLT_FloatImage img)
{
  int xt = (int) x;  /* coordinates of top-left corner */
  int yt = (int) y;
  float ax = x - xt;
  float ay = y - yt;
  float *ptr = img->data + (img->ncols*yt) + xt;

#ifndef _DNDEBUG
  if (xt<0 || yt<0 || xt>=img->ncols-1 || yt>=img->nrows-1) {
    fprintf(stderr, "(xt,yt)=(%d,%d)  imgsize=(%d,%d)\n"
            "(x,y)=(%f,%f)  (ax,ay)=(%f,%f)\n",
            xt, yt, img->ncols, img->nrows, x, y, ax, ay);
    fflush(stderr);
  }
#endif

  assert (xt >= 0 && yt >= 0 && xt <= img->ncols - 2 && yt <= img->nrows - 2);

  return ( (1-ax) * (1-ay) * *ptr +
           ax   * (1-ay) * *(ptr+1) +
           (1-ax) *   ay   * *(ptr+(img->ncols)) +
           ax   *   ay   * *(ptr+(img->ncols)+1) );
}


/*********************************************************************
 * _computeIntensityDifference
 *
 * Given two images and the window center in both images,
 * aligns the images wrt the window and computes the difference
 * between the two overlaid images.
 */

static void _computeIntensityDifference(
  _KLT_FloatImage img1,   /* images */
  _KLT_FloatImage img2,
  float x1, float y1,     /* center of window in 1st img */
  float x2, float y2,     /* center of window in 2nd img */
  int width, int height,  /* size of window */
  _FloatWindow imgdiff)   /* output */
{
  int hw = width/2, hh = height/2;
  float g1, g2;
  int i, j;

  /* Compute values */
  for (j = -hh ; j <= hh ; j++)
    for (i = -hw ; i <= hw ; i++)  {
      g1 = _interpolate(x1+i, y1+j, img1);
      g2 = _interpolate(x2+i, y2+j, img2);
      *imgdiff++ = g1 - g2;
    }
}


/*********************************************************************
 * _computeGradientSum
 *
 * Given two gradients and the window center in both images,
 * aligns the gradients wrt the window and computes the sum of the two
 * overlaid gradients.
 */

static void _computeGradientSum(
  _KLT_FloatImage gradx1,  /* gradient images */
  _KLT_FloatImage grady1,
  _KLT_FloatImage gradx2,
  _KLT_FloatImage grady2,
  float x1, float y1,      /* center of window in 1st img */
  float x2, float y2,      /* center of window in 2nd img */
  int width, int height,   /* size of window */
  _FloatWindow gradx,      /* output */
  _FloatWindow grady)      /*   " */
{
  int hw = width/2, hh = height/2;
  float g1, g2;
  int i, j;

  /* Compute values */
  for (j = -hh ; j <= hh ; j++)
    for (i = -hw ; i <= hw ; i++)  {
      g1 = _interpolate(x1+i, y1+j, gradx1);
      g2 = _interpolate(x2+i, y2+j, gradx2);
      *gradx++ = g1 + g2;
      g1 = _interpolate(x1+i, y1+j, grady1);
      g2 = _interpolate(x2+i, y2+j, grady2);
      *grady++ = g1 + g2;
    }
}

/*********************************************************************
 * _computeIntensityDifferenceLightingInsensitive
 *
 * Given two images and the window center in both images,
 * aligns the images wrt the window and computes the difference
 * between the two overlaid images; normalizes for overall gain and bias.
 */

static void _computeIntensityDifferenceLightingInsensitive(
  _KLT_FloatImage img1,   /* images */
  _KLT_FloatImage img2,
  float x1, float y1,     /* center of window in 1st img */
  float x2, float y2,     /* center of window in 2nd img */
  int width, int height,  /* size of window */
  _FloatWindow imgdiff)   /* output */
{
  int hw = width/2, hh = height/2;
  float g1, g2, sum1_squared = 0, sum2_squared = 0;
  int i, j;

  float sum1 = 0, sum2 = 0;
  float mean1, mean2,alpha,belta;
  /* Compute values */
  for (j = -hh ; j <= hh ; j++)
    for (i = -hw ; i <= hw ; i++)  {
      g1 = _interpolate(x1+i, y1+j, img1);
      g2 = _interpolate(x2+i, y2+j, img2);
      sum1 += g1;    sum2 += g2;
      sum1_squared += g1*g1;
      sum2_squared += g2*g2;
   }
  mean1=sum1_squared/(width*height);
  mean2=sum2_squared/(width*height);
  alpha = (float) sqrt(mean1/mean2);
  mean1=sum1/(width*height);
  mean2=sum2/(width*height);
  belta = mean1-alpha*mean2;

  for (j = -hh ; j <= hh ; j++)
    for (i = -hw ; i <= hw ; i++)  {
      g1 = _interpolate(x1+i, y1+j, img1);
      g2 = _interpolate(x2+i, y2+j, img2);
      *imgdiff++ = g1- g2*alpha-belta;
    }
}


/*********************************************************************
 * _computeGradientSumLightingInsensitive
 *
 * Given two gradients and the window center in both images,
 * aligns the gradients wrt the window and computes the sum of the two
 * overlaid gradients; normalizes for overall gain and bias.
 */

static void _computeGradientSumLightingInsensitive(
  _KLT_FloatImage gradx1,  /* gradient images */
  _KLT_FloatImage grady1,
  _KLT_FloatImage gradx2,
  _KLT_FloatImage grady2,
  _KLT_FloatImage img1,   /* images */
  _KLT_FloatImage img2,

  float x1, float y1,      /* center of window in 1st img */
  float x2, float y2,      /* center of window in 2nd img */
  int width, int height,   /* size of window */
  _FloatWindow gradx,      /* output */
  _FloatWindow grady)      /*   " */
{
  int hw = width/2, hh = height/2;
  float g1, g2, sum1_squared = 0, sum2_squared = 0;
  int i, j;

  float mean1, mean2, alpha;
  for (j = -hh ; j <= hh ; j++)
    for (i = -hw ; i <= hw ; i++)  {
      g1 = _interpolate(x1+i, y1+j, img1);
      g2 = _interpolate(x2+i, y2+j, img2);
      sum1_squared += g1;    sum2_squared += g2;
    }
  mean1 = sum1_squared/(width*height);
  mean2 = sum2_squared/(width*height);
  alpha = (float) sqrt(mean1/mean2);

  /* Compute values */
  for (j = -hh ; j <= hh ; j++)
    for (i = -hw ; i <= hw ; i++)  {
      g1 = _interpolate(x1+i, y1+j, gradx1);
      g2 = _interpolate(x2+i, y2+j, gradx2);
      *gradx++ = g1 + g2*alpha;
      g1 = _interpolate(x1+i, y1+j, grady1);
      g2 = _interpolate(x2+i, y2+j, grady2);
      *grady++ = g1+ g2*alpha;
    }
}

/*********************************************************************
 * _compute2by2GradientMatrix
 *
 */

static void _compute2by2GradientMatrix(
  _FloatWindow gradx,
  _FloatWindow grady,
  int width,   /* size of window */
  int height,
  float *gxx,  /* return values */
  float *gxy,
  float *gyy)

{
  float gx, gy;
  int i;

  /* Compute values */
  *gxx = 0.0;  *gxy = 0.0;  *gyy = 0.0;
  for (i = 0 ; i < width * height ; i++)  {
    gx = *gradx++;
    gy = *grady++;
    *gxx += gx*gx;
    *gxy += gx*gy;
    *gyy += gy*gy;
  }
}


/*********************************************************************
 * _compute2by1ErrorVector
 *
 */

static void _compute2by1ErrorVector(
  _FloatWindow imgdiff,
  _FloatWindow gradx,
  _FloatWindow grady,
  int width,   /* size of window */
  int height,
  float step_factor, /* 2.0 comes from equations, 1.0 seems to avoid overshooting */
  float *ex,   /* return values */
  float *ey)
{
  float diff;
  int i;

  /* Compute values */
  *ex = 0;  *ey = 0;
  for (i = 0 ; i < width * height ; i++)  {
    diff = *imgdiff++;
    *ex += diff * (*gradx++);
    *ey += diff * (*grady++);
  }
  *ex *= step_factor;
  *ey *= step_factor;
}


/*********************************************************************
 * _solveEquation
 *
 * Solves the 2x2 matrix equation
 *         [gxx gxy] [dx] = [ex]
 *         [gxy gyy] [dy] = [ey]
 * for dx and dy.
 *
 * Returns KLT_TRACKED on success and KLT_SMALL_DET on failure
 */

static int _solveEquation(
  float gxx, float gxy, float gyy,
  float ex, float ey,
  float small,
  float *dx, float *dy)
{
  float det = gxx*gyy - gxy*gxy;


  if (det < small)  return KLT_SMALL_DET;

  *dx = (gyy*ex - gxy*ey)/det;
  *dy = (gxx*ey - gxy*ex)/det;
  return KLT_TRACKED;
}


/*********************************************************************
 * _allocateFloatWindow
 */

static _FloatWindow _allocateFloatWindow(
  int width,
  int height)
{
  _FloatWindow fw;

  fw = (_FloatWindow) malloc(width*height*sizeof(float));
  if (fw == nullptr) {
   KLTError("(_allocateFloatWindow) Out of memory.");
   exit(1);
  }
  return fw;
}


/*********************************************************************
 * _printFloatWindow
 * (for debugging purposes)
 */

/*
static void _printFloatWindow(
  _FloatWindow fw,
  int width,
  int height)
{
  int i, j;

  fprintf(stderr, "\n");
  for (i = 0 ; i < width ; i++)  {
    for (j = 0 ; j < height ; j++)  {
      fprintf(stderr, "%6.1f ", *fw++);
    }
    fprintf(stderr, "\n");
  }
}
*/


/*********************************************************************
 * _sumAbsFloatWindow
 */

static float _sumAbsFloatWindow(
  _FloatWindow fw,
  int width,
  int height)
{
  float sum = 0.0;
  int w;

  for ( ; height > 0 ; height--)
    for (w=0 ; w < width ; w++)
      sum += (float) fabs(*fw++);

  return sum;
}


/*********************************************************************
 * _trackFeature
 *
 * Tracks a feature point from one image to the next.
 *
 * RETURNS
 * KLT_SMALL_DET if feature is lost,
 * KLT_MAX_ITERATIONS if tracking stopped because iterations timed out,
 * KLT_TRACKED otherwise.
 */

static int _trackFeature(
  float x1,  /* location of window in first image */
  float y1,
  float *x2, /* starting location of search in second image */
  float *y2,
  _KLT_FloatImage img1,
  _KLT_FloatImage gradx1,
  _KLT_FloatImage grady1,
  _KLT_FloatImage img2,
  _KLT_FloatImage gradx2,
  _KLT_FloatImage grady2,
  int width,           /* size of window */
  int height,
  float step_factor, /* 2.0 comes from equations, 1.0 seems to avoid overshooting */
  int max_iterations,
  float small,         /* determinant threshold for declaring KLT_SMALL_DET */
  float th,            /* displacement threshold for stopping               */
  float max_residue,   /* residue threshold for declaring KLT_LARGE_RESIDUE */
  int lighting_insensitive)  /* whether to normalize for gain and bias */
{
  _FloatWindow imgdiff, gradx, grady;
  float gxx, gxy, gyy, ex, ey, dx, dy;
  int iteration = 0;
  int status;
  int hw = width/2;
  int hh = height/2;
  int nc = img1->ncols;
  int nr = img1->nrows;
  float one_plus_eps = 1.001f;   /* To prevent rounding errors */


  /* Allocate memory for windows */
  imgdiff = _allocateFloatWindow(width, height);
  gradx   = _allocateFloatWindow(width, height);
  grady   = _allocateFloatWindow(width, height);

  /* Iteratively update the window position */
  do  {

    /* If out of bounds, exit loop */
    if (  x1-hw < 0.0f || nc-( x1+hw) < one_plus_eps ||
         *x2-hw < 0.0f || nc-(*x2+hw) < one_plus_eps ||
          y1-hh < 0.0f || nr-( y1+hh) < one_plus_eps ||
         *y2-hh < 0.0f || nr-(*y2+hh) < one_plus_eps) {
      status = KLT_OOB;
      break;
    }

    /* Compute gradient and difference windows */
    if (lighting_insensitive) {
      _computeIntensityDifferenceLightingInsensitive(img1, img2, x1, y1, *x2, *y2,
                                  width, height, imgdiff);
      _computeGradientSumLightingInsensitive(gradx1, grady1, gradx2, grady2,
			  img1, img2, x1, y1, *x2, *y2, width, height, gradx, grady);
    } else {
      _computeIntensityDifference(img1, img2, x1, y1, *x2, *y2,
                                  width, height, imgdiff);
      _computeGradientSum(gradx1, grady1, gradx2, grady2,
			  x1, y1, *x2, *y2, width, height, gradx, grady);
    }


    /* Use these windows to construct matrices */
    _compute2by2GradientMatrix(gradx, grady, width, height,
                               &gxx, &gxy, &gyy);
    _compute2by1ErrorVector(imgdiff, gradx, grady, width, height, step_factor,
                            &ex, &ey);

    /* Using matrices, solve equation for new displacement */
    status = _solveEquation(gxx, gxy, gyy, ex, ey, small, &dx, &dy);
    if (status == KLT_SMALL_DET)  break;

    *x2 += dx;
    *y2 += dy;
    iteration++;

  }  while ((fabs(dx)>=th || fabs(dy)>=th) && iteration < max_iterations);

  /* Check whether window is out of bounds */
  if (*x2-hw < 0.0f || nc-(*x2+hw) < one_plus_eps ||
      *y2-hh < 0.0f || nr-(*y2+hh) < one_plus_eps || std::isnan(*x2) || std::isnan(*y2))
    status = KLT_OOB;

  /* Check whether residue is too large */
  if (status == KLT_TRACKED)  {
    if (lighting_insensitive)
      _computeIntensityDifferenceLightingInsensitive(img1, img2, x1, y1, *x2, *y2,
                                  width, height, imgdiff);
    else
      _computeIntensityDifference(img1, img2, x1, y1, *x2, *y2,
                                  width, height, imgdiff);
    if (_sumAbsFloatWindow(imgdiff, width, height)/(width*height) > max_residue)
      status = KLT_LARGE_RESIDUE;
  }

  /* Free memory */
  free(imgdiff);  free(gradx);  free(grady);

  /* Return appropriate value */
  if (status == KLT_SMALL_DET)  return KLT_SMALL_DET;
  else if (status == KLT_OOB)  return KLT_OOB;
  else if (status == KLT_LARGE_RESIDUE)  return KLT_LARGE_RESIDUE;
  else if (iteration >= max_iterations)  return KLT_MAX_ITERATIONS;
  else  return KLT_TRACKED;

}


/*********************************************************************/

static KLT_BOOL _outOfBounds(
  float x,
  float y,
  int ncols,
  int nrows,
  int borderx,
  int bordery)
{
  return (x < borderx || x > ncols-1-borderx ||
          y < bordery || y > nrows-1-bordery );
}




/**********************************************************************
* CONSISTENCY CHECK OF FEATURES BY AFFINE MAPPING (BEGIN)
*
* Created by: Thorsten Thormaehlen (University of Hannover) June 2004
* thormae@tnt.uni-hannover.de
*
* Permission is granted to any individual or institution to use, copy, modify,
* and distribute this part of the software, provided that this complete authorship
* and permission notice is maintained, intact, in all copies.
*
* This software is provided  "as is" without express or implied warranty.
*
*
* The following static functions are helpers for the affine mapping.
* They all start with "_am".
* There are also small changes in other files for the
* affine mapping these are all marked by "for affine mapping"
*
* Thanks to Kevin Koeser (koeser@mip.informatik.uni-kiel.de) for fixing a bug
*/

#define SWAP_ME(X,Y) {temp=(X);(X)=(Y);(Y)=temp;}

static float **_am_matrix(long nr, long nc)
{
  float **m;
  int a;
  m = (float **) malloc((size_t)(nr*sizeof(float*)));
  m[0] = (float *) malloc((size_t)((nr*nc)*sizeof(float)));
  for(a = 1; a < nr; a++) m[a] = m[a-1]+nc;
  return m;
}

static void _am_free_matrix(float **m)
{
  free(m[0]);
  free(m);
}


static int _am_gauss_jordan_elimination(float **a, int n, float **b, int m)
{
  /* re-implemented from Numerical Recipes in C */
  int *indxc,*indxr,*ipiv;
  int i,j,k,l,ll;
  float big,dum,pivinv,temp;
  int col = 0;
  int row = 0;

  indxc=(int *)malloc((size_t) (n*sizeof(int)));
  indxr=(int *)malloc((size_t) (n*sizeof(int)));
  ipiv=(int *)malloc((size_t) (n*sizeof(int)));
  for (j=0;j<n;j++) ipiv[j]=0;
  for (i=0;i<n;i++) {
    big=0.0;
    for (j=0;j<n;j++)
      if (ipiv[j] != 1)
	for (k=0;k<n;k++) {
	  if (ipiv[k] == 0) {
	    if (fabs(a[j][k]) >= big) {
	      big= (float) fabs(a[j][k]);
	      row=j;
	      col=k;
	    }
	  } else if (ipiv[k] > 1) {
           free(ipiv); free(indxr); free(indxc); return KLT_SMALL_DET;
      }
	}
    ++(ipiv[col]);
    if (row != col) {
      for (l=0;l<n;l++) SWAP_ME(a[row][l],a[col][l])
			  for (l=0;l<m;l++) SWAP_ME(b[row][l],b[col][l])
					      }
    indxr[i]=row;
    indxc[i]=col;
    if (a[col][col] == 0.0) {
      free(ipiv);
      free(indxr);
      free(indxc);
      return KLT_SMALL_DET;
    }
    pivinv=1.0f/a[col][col];
    a[col][col]=1.0;
    for (l=0;l<n;l++) a[col][l] *= pivinv;
    for (l=0;l<m;l++) b[col][l] *= pivinv;
    for (ll=0;ll<n;ll++)
      if (ll != col) {
	dum=a[ll][col];
	a[ll][col]=0.0;
	for (l=0;l<n;l++) a[ll][l] -= a[col][l]*dum;
	for (l=0;l<m;l++) b[ll][l] -= b[col][l]*dum;
      }
  }
  for (l=n-1;l>=0;l--) {
    if (indxr[l] != indxc[l])
      for (k=0;k<n;k++)
	SWAP_ME(a[k][indxr[l]],a[k][indxc[l]]);
  }
  free(ipiv);
  free(indxr);
  free(indxc);

  return KLT_TRACKED;
}

/*********************************************************************
 * _am_getGradientWinAffine
 *
 * aligns the gradients with the affine transformed window
 */

static void _am_getGradientWinAffine(
				     _KLT_FloatImage in_gradx,
				     _KLT_FloatImage in_grady,
				     float x, float y,      /* center of window*/
				     float Axx, float Ayx , float Axy, float Ayy,    /* affine mapping */
				     int width, int height,   /* size of window */
				     _FloatWindow out_gradx,      /* output */
				     _FloatWindow out_grady)      /* output */
{
  int hw = width/2, hh = height/2;
  int i, j;
  float mi, mj;

  /* Compute values */
  for (j = -hh ; j <= hh ; j++)
    for (i = -hw ; i <= hw ; i++)  {
      mi = Axx * i + Axy * j;
      mj = Ayx * i + Ayy * j;
      *out_gradx++ = _interpolate(x+mi, y+mj, in_gradx);
      *out_grady++ = _interpolate(x+mi, y+mj, in_grady);
    }

}

///*********************************************************************
// * _computeAffineMappedImage
// * used only for DEBUG output
// *
//*/
//
//static void _am_computeAffineMappedImage(
//					 _KLT_FloatImage img,   /* images */
//					 float x, float y,      /* center of window  */
//					 float Axx, float Ayx , float Axy, float Ayy,    /* affine mapping */
//					 int width, int height,  /* size of window */
//					 _FloatWindow imgdiff)   /* output */
//{
//  int hw = width/2, hh = height/2;
//  int i, j;
//  float mi, mj;
//
//  /* Compute values */
//  for (j = -hh ; j <= hh ; j++)
//    for (i = -hw ; i <= hw ; i++)  {
//      mi = Axx * i + Axy * j;
//      mj = Ayx * i + Ayy * j;
//      *imgdiff++ = _interpolate(x+mi, y+mj, img);
//    }
//}


/*********************************************************************
 * _getSubFloatImage
 */

static void _am_getSubFloatImage(
				 _KLT_FloatImage img,   /* image */
				 float x, float y,     /* center of window */
				 _KLT_FloatImage window)   /* output */
{
  int hw = window->ncols/2, hh = window->nrows/2;
  int x0 = (int) x;
  int y0 = (int) y;
  float * windata = window->data;
  int offset;
  int i, j;

  assert(x0 - hw >= 0);
  assert(y0 - hh >= 0);
  assert(x0 + hw <= img->ncols);
  assert(y0 + hh <= img->nrows);

  /* copy values */
  for (j = -hh ; j <= hh ; j++)
    for (i = -hw ; i <= hw ; i++)  {
      offset = (j+y0)*img->ncols + (i+x0);
      *windata++ = *(img->data+offset);
    }
}

/*********************************************************************
 * _am_computeIntensityDifferenceAffine
 *
 * Given two images and the window center in both images,
 * aligns the images with the window and computes the difference
 * between the two overlaid images using the affine mapping.
 *       A =  [ Axx Axy]
 *            [ Ayx Ayy]
*/

static void _am_computeIntensityDifferenceAffine(
						 _KLT_FloatImage img1,   /* images */
						 _KLT_FloatImage img2,
						 float x1, float y1,     /* center of window in 1st img */
						 float x2, float y2,      /* center of window in 2nd img */
						 float Axx, float Ayx , float Axy, float Ayy,    /* affine mapping */
						 int width, int height,  /* size of window */
						 _FloatWindow imgdiff)   /* output */
{
  int hw = width/2, hh = height/2;
  float g1, g2;
  int i, j;
  float mi, mj;

  /* Compute values */
  for (j = -hh ; j <= hh ; j++)
    for (i = -hw ; i <= hw ; i++)  {
      g1 = _interpolate(x1+i, y1+j, img1);
      mi = Axx * i + Axy * j;
      mj = Ayx * i + Ayy * j;
      g2 = _interpolate(x2+mi, y2+mj, img2);
      *imgdiff++ = g1 - g2;
    }
}

/*********************************************************************
 * _am_compute6by6GradientMatrix
 *
 */

static void _am_compute6by6GradientMatrix(
					  _FloatWindow gradx,
					  _FloatWindow grady,
					  int width,   /* size of window */
					  int height,
					  float **T)  /* return values */
{
  int hw = width/2, hh = height/2;
  int i, j;
  float gx, gy, gxx, gxy, gyy,  x, y, xx, xy, yy;


  /* Set values to zero */
  for (j = 0 ; j < 6 ; j++)  {
    for (i = j ; i < 6 ; i++)  {
      T[j][i] = 0.0;
    }
  }

  for (j = -hh ; j <= hh ; j++) {
    for (i = -hw ; i <= hw ; i++)  {
      gx = *gradx++;
      gy = *grady++;
      gxx = gx * gx;
      gxy = gx * gy;
      gyy = gy * gy;
      x = (float) i;
      y = (float) j;
      xx = x * x;
      xy = x * y;
      yy = y * y;

      T[0][0] += xx * gxx;
      T[0][1] += xx * gxy;
      T[0][2] += xy * gxx;
      T[0][3] += xy * gxy;
      T[0][4] += x  * gxx;
      T[0][5] += x  * gxy;

      T[1][1] += xx * gyy;
      T[1][2] += xy * gxy;
      T[1][3] += xy * gyy;
      T[1][4] += x  * gxy;
      T[1][5] += x  * gyy;

      T[2][2] += yy * gxx;
      T[2][3] += yy * gxy;
      T[2][4] += y  * gxx;
      T[2][5] += y  * gxy;

      T[3][3] += yy * gyy;
      T[3][4] += y  * gxy;
      T[3][5] += y  * gyy;

      T[4][4] += gxx;
      T[4][5] += gxy;

      T[5][5] += gyy;
    }
  }

  for (j = 0 ; j < 5 ; j++)  {
    for (i = j+1 ; i < 6 ; i++)  {
      T[i][j] = T[j][i];
    }
  }

}



/*********************************************************************
 * _am_compute6by1ErrorVector
 *
 */

static void _am_compute6by1ErrorVector(
				       _FloatWindow imgdiff,
				       _FloatWindow gradx,
				       _FloatWindow grady,
				       int width,   /* size of window */
				       int height,
				       float **e)  /* return values */
{
  int hw = width/2, hh = height/2;
  int i, j;
  float diff,  diffgradx,  diffgrady;

  /* Set values to zero */
  for(i = 0; i < 6; i++) e[i][0] = 0.0;

  /* Compute values */
  for (j = -hh ; j <= hh ; j++) {
    for (i = -hw ; i <= hw ; i++)  {
      diff = *imgdiff++;
      diffgradx = diff * (*gradx++);
      diffgrady = diff * (*grady++);
      e[0][0] += diffgradx * i;
      e[1][0] += diffgrady * i;
      e[2][0] += diffgradx * j;
      e[3][0] += diffgrady * j;
      e[4][0] += diffgradx;
      e[5][0] += diffgrady;
    }
  }

  for(i = 0; i < 6; i++) e[i][0] *= 0.5;

}


/*********************************************************************
 * _am_compute4by4GradientMatrix
 *
 */

static void _am_compute4by4GradientMatrix(
					  _FloatWindow gradx,
					  _FloatWindow grady,
					  int width,   /* size of window */
					  int height,
					  float **T)  /* return values */
{
  int hw = width/2, hh = height/2;
  int i, j;
  float gx, gy, x, y;


  /* Set values to zero */
  for (j = 0 ; j < 4 ; j++)  {
    for (i = 0 ; i < 4 ; i++)  {
      T[j][i] = 0.0;
    }
  }

  for (j = -hh ; j <= hh ; j++) {
    for (i = -hw ; i <= hw ; i++)  {
      gx = *gradx++;
      gy = *grady++;
      x = (float) i;
      y = (float) j;
      T[0][0] += (x*gx+y*gy) * (x*gx+y*gy);
      T[0][1] += (x*gx+y*gy)*(x*gy-y*gx);
      T[0][2] += (x*gx+y*gy)*gx;
      T[0][3] += (x*gx+y*gy)*gy;

      T[1][1] += (x*gy-y*gx) * (x*gy-y*gx);
      T[1][2] += (x*gy-y*gx)*gx;
      T[1][3] += (x*gy-y*gx)*gy;

      T[2][2] += gx*gx;
      T[2][3] += gx*gy;

      T[3][3] += gy*gy;
    }
  }

  for (j = 0 ; j < 3 ; j++)  {
    for (i = j+1 ; i < 4 ; i++)  {
      T[i][j] = T[j][i];
    }
  }

}

/*********************************************************************
 * _am_compute4by1ErrorVector
 *
 */

static void _am_compute4by1ErrorVector(
				       _FloatWindow imgdiff,
				       _FloatWindow gradx,
				       _FloatWindow grady,
				       int width,   /* size of window */
				       int height,
				       float **e)  /* return values */
{
  int hw = width/2, hh = height/2;
  int i, j;
  float diff,  diffgradx,  diffgrady;

  /* Set values to zero */
  for(i = 0; i < 4; i++) e[i][0] = 0.0;

  /* Compute values */
  for (j = -hh ; j <= hh ; j++) {
    for (i = -hw ; i <= hw ; i++)  {
      diff = *imgdiff++;
      diffgradx = diff * (*gradx++);
      diffgrady = diff * (*grady++);
      e[0][0] += diffgradx * i + diffgrady * j;
      e[1][0] += diffgrady * i - diffgradx * j;
      e[2][0] += diffgradx;
      e[3][0] += diffgrady;
    }
  }

  for(i = 0; i < 4; i++) e[i][0] *= 0.5;

}



/*********************************************************************
 * _am_trackFeatureAffine
 *
 * Tracks a feature point from the image of first occurrence to the actual image.
 *
 * RETURNS
 * KLT_SMALL_DET or KLT_LARGE_RESIDUE or KLT_OOB if feature is lost,
 * KLT_TRACKED otherwise.
 */

/* if you enable the DEBUG_AFFINE_MAPPING make sure you have created a directory "./debug" */
/* #define DEBUG_AFFINE_MAPPING */

#ifdef DEBUG_AFFINE_MAPPING
static int counter = 0;
static int glob_index = 0;
#endif

static int _am_trackFeatureAffine(
				  float x1,  /* location of window in first image */
				  float y1,
				  float *x2, /* starting location of search in second image */
				  float *y2,
				  _KLT_FloatImage img1,
				  _KLT_FloatImage gradx1,
				  _KLT_FloatImage grady1,
				  _KLT_FloatImage img2,
				  _KLT_FloatImage gradx2,
				  _KLT_FloatImage grady2,
				  int width,           /* size of window */
				  int height,
				  float step_factor, /* 2.0 comes from equations, 1.0 seems to avoid overshooting */
				  int max_iterations,
				  float small,         /* determinant threshold for declaring KLT_SMALL_DET */
				  float th,            /* displacement threshold for stopping  */
				  float th_aff,
				  float max_residue,   /* residue threshold for declaring KLT_LARGE_RESIDUE */
				  int lighting_insensitive,  /* whether to normalize for gain and bias */
				  int affine_map,      /* whether to evaluates the consistency of features with affine mapping */
				  float mdd,           /* difference between the displacements */
				  float *Axx, float *Ayx,
				  float *Axy, float *Ayy)        /* used affine mapping */
{


  _FloatWindow imgdiff, gradx, grady;
  float gxx, gxy, gyy, ex, ey, dx = 0.f, dy = 0.f;
  int iteration = 0;
  int status = 0;
  int hw = width/2;
  int hh = height/2;
  int nc1 = img1->ncols;
  int nr1 = img1->nrows;
  int nc2 = img2->ncols;
  int nr2 = img2->nrows;
  float **a;
  float **T;
  float one_plus_eps = 1.001f;   /* To prevent rounding errors */
  float old_x2 = *x2;
  float old_y2 = *y2;
  KLT_BOOL convergence = FALSE;

#ifdef DEBUG_AFFINE_MAPPING
  char fname[80];
  _KLT_FloatImage aff_diff_win = _KLTCreateFloatImage(width,height);
  printf("starting location x2=%f y2=%f\n", *x2, *y2);
#endif

  /* Allocate memory for windows */
  imgdiff = _allocateFloatWindow(width, height);
  gradx   = _allocateFloatWindow(width, height);
  grady   = _allocateFloatWindow(width, height);
  T = _am_matrix(6,6);
  a = _am_matrix(6,1);

  /* Iteratively update the window position */
  do  {
    if(!affine_map) {
      /* pure translation tracker */

      /* If out of bounds, exit loop */
      if ( x1-hw < 0.0f || nc1-( x1+hw) < one_plus_eps ||
          *x2-hw < 0.0f || nc2-(*x2+hw) < one_plus_eps ||
           y1-hh < 0.0f || nr1-( y1+hh) < one_plus_eps ||
          *y2-hh < 0.0f || nr2-(*y2+hh) < one_plus_eps) {
        status = KLT_OOB;
        break;
      }

      /* Compute gradient and difference windows */
      if (lighting_insensitive) {
        _computeIntensityDifferenceLightingInsensitive(img1, img2, x1, y1, *x2, *y2,
                                    width, height, imgdiff);
        _computeGradientSumLightingInsensitive(gradx1, grady1, gradx2, grady2,
			    img1, img2, x1, y1, *x2, *y2, width, height, gradx, grady);
      } else {
        _computeIntensityDifference(img1, img2, x1, y1, *x2, *y2,
                                    width, height, imgdiff);
        _computeGradientSum(gradx1, grady1, gradx2, grady2,
			    x1, y1, *x2, *y2, width, height, gradx, grady);
      }

#ifdef DEBUG_AFFINE_MAPPING
      aff_diff_win->data = imgdiff;
      snprintf(fname, sizeof(fname), "./debug/kltimg_trans_diff_win%03d.%03d.pgm", glob_index, counter);
      printf("%s\n", fname);
      _KLTWriteAbsFloatImageToPGM(aff_diff_win, fname,256.0);
      printf("iter = %d translation tracker res: %f\n", iteration, _sumAbsFloatWindow(imgdiff, width, height)/(width*height));
#endif

      /* Use these windows to construct matrices */
      _compute2by2GradientMatrix(gradx, grady, width, height,
				 &gxx, &gxy, &gyy);
      _compute2by1ErrorVector(imgdiff, gradx, grady, width, height, step_factor,
			      &ex, &ey);

      /* Using matrices, solve equation for new displacement */
      status = _solveEquation(gxx, gxy, gyy, ex, ey, small, &dx, &dy);

      convergence = (fabs(dx) < th && fabs(dy) < th);

      *x2 += dx;
      *y2 += dy;

    }else{
      /* affine tracker */

      float ul_x =  *Axx * (-hw) + *Axy *   hh  + *x2;  /* upper left corner */
      float ul_y =  *Ayx * (-hw) + *Ayy *   hh  + *y2;
      float ll_x =  *Axx * (-hw) + *Axy * (-hh) + *x2;  /* lower left corner */
      float ll_y =  *Ayx * (-hw) + *Ayy * (-hh) + *y2;
      float ur_x =  *Axx *   hw  + *Axy *   hh  + *x2;  /* upper right corner */
      float ur_y =  *Ayx *   hw  + *Ayy *   hh  + *y2;
      float lr_x =  *Axx *   hw  + *Axy * (-hh) + *x2;  /* lower right corner */
      float lr_y =  *Ayx *   hw  + *Ayy * (-hh) + *y2;

      /* If out of bounds, exit loop */
      if ( x1-hw < 0.0f ||  nc1-(x1+hw) < one_plus_eps ||
           y1-hh < 0.0f ||  nr1-(y1+hh) < one_plus_eps ||
           ul_x  < 0.0f ||  nc2-(ul_x ) < one_plus_eps ||
           ll_x  < 0.0f ||  nc2-(ll_x ) < one_plus_eps ||
           ur_x  < 0.0f ||  nc2-(ur_x ) < one_plus_eps ||
           lr_x  < 0.0f ||  nc2-(lr_x ) < one_plus_eps ||
           ul_y  < 0.0f ||  nr2-(ul_y ) < one_plus_eps ||
           ll_y  < 0.0f ||  nr2-(ll_y ) < one_plus_eps ||
           ur_y  < 0.0f ||  nr2-(ur_y ) < one_plus_eps ||
           lr_y  < 0.0f ||  nr2-(lr_y ) < one_plus_eps) {
        status = KLT_OOB;
        break;
      }

#ifdef DEBUG_AFFINE_MAPPING
      counter++;
      _am_computeAffineMappedImage(img1, x1, y1,  1.0, 0.0 , 0.0, 1.0, width, height, imgdiff);
      aff_diff_win->data = imgdiff;
      snprintf(fname, sizeof(fname), "./debug/kltimg_aff_diff_win%03d.%03d_1.pgm", glob_index, counter);
      printf("%s\n", fname);
      _KLTWriteAbsFloatImageToPGM(aff_diff_win, fname,256.0);

      _am_computeAffineMappedImage(img2, *x2, *y2,  *Axx, *Ayx , *Axy, *Ayy, width, height, imgdiff);
      aff_diff_win->data = imgdiff;
      snprintf(fname, sizeof(fname), "./debug/kltimg_aff_diff_win%03d.%03d_2.pgm", glob_index, counter);
      printf("%s\n", fname);
      _KLTWriteAbsFloatImageToPGM(aff_diff_win, fname,256.0);
#endif

      _am_computeIntensityDifferenceAffine(img1, img2, x1, y1, *x2, *y2,  *Axx, *Ayx , *Axy, *Ayy,
					   width, height, imgdiff);
#ifdef DEBUG_AFFINE_MAPPING
      aff_diff_win->data = imgdiff;
      snprintf(fname, sizeof(fname), "./debug/kltimg_aff_diff_win%03d.%03d_3.pgm", glob_index,counter);
      printf("%s\n", fname);
      _KLTWriteAbsFloatImageToPGM(aff_diff_win, fname,256.0);

      printf("iter = %d affine tracker res: %f\n", iteration, _sumAbsFloatWindow(imgdiff, width, height)/(width*height));
#endif

      _am_getGradientWinAffine(gradx2, grady2, *x2, *y2, *Axx, *Ayx , *Axy, *Ayy,
			       width, height, gradx, grady);

      switch(affine_map){
      case 1:
	_am_compute4by1ErrorVector(imgdiff, gradx, grady, width, height, a);
	_am_compute4by4GradientMatrix(gradx, grady, width, height, T);

	status = _am_gauss_jordan_elimination(T,4,a,1);

	*Axx += a[0][0];
	*Ayx += a[1][0];
	*Ayy = *Axx;
	*Axy = -(*Ayx);

	dx = a[2][0];
	dy = a[3][0];

	break;
      case 2:
	_am_compute6by1ErrorVector(imgdiff, gradx, grady, width, height, a);
	_am_compute6by6GradientMatrix(gradx, grady, width, height, T);

	status = _am_gauss_jordan_elimination(T,6,a,1);

	*Axx += a[0][0];
	*Ayx += a[1][0];
	*Axy += a[2][0];
	*Ayy += a[3][0];

	dx = a[4][0];
	dy = a[5][0];

	break;
      }

      *x2 += dx;
      *y2 += dy;

      /* old upper left corner - new upper left corner */
      ul_x -=  *Axx * (-hw) + *Axy *   hh  + *x2;
      ul_y -=  *Ayx * (-hw) + *Ayy *   hh  + *y2;
      /* old lower left corner - new lower left corner */
      ll_x -=  *Axx * (-hw) + *Axy * (-hh) + *x2;
      ll_y -=  *Ayx * (-hw) + *Ayy * (-hh) + *y2;
      /* old upper right corner - new upper right corner */
      ur_x -=  *Axx *   hw  + *Axy *   hh  + *x2;
      ur_y -=  *Ayx *   hw  + *Ayy *   hh  + *y2;
      /* old lower right corner - new lower right corner */
      lr_x -=  *Axx *   hw  + *Axy * (-hh) + *x2;
      lr_y -=  *Ayx *   hw  + *Ayy * (-hh) + *y2;

#ifdef DEBUG_AFFINE_MAPPING
      printf ("iter = %d, ul_x=%f ul_y=%f ll_x=%f ll_y=%f ur_x=%f ur_y=%f lr_x=%f lr_y=%f \n",
	      iteration, ul_x, ul_y, ll_x, ll_y, ur_x, ur_y, lr_x, lr_y);
#endif

      convergence = (fabs(dx) < th && fabs(dy) < th  &&
		     fabs(ul_x) < th_aff && fabs(ul_y) < th_aff &&
		     fabs(ll_x) < th_aff && fabs(ll_y) < th_aff &&
		     fabs(ur_x) < th_aff && fabs(ur_y) < th_aff &&
		     fabs(lr_x) < th_aff && fabs(lr_y) < th_aff);
    }

    if (status == KLT_SMALL_DET)  break;
    iteration++;
#ifdef DEBUG_AFFINE_MAPPING
    printf ("iter = %d, x1=%f, y1=%f, x2=%f, y2=%f,  Axx=%f, Ayx=%f , Axy=%f, Ayy=%f \n",iteration, x1, y1, *x2, *y2,  *Axx, *Ayx , *Axy, *Ayy);
#endif
    }  while ( !convergence  && iteration < max_iterations);
    /*}  while ( (fabs(dx)>=th || fabs(dy)>=th || (affine_map && iteration < 8) ) && iteration < max_iterations); */
  _am_free_matrix(T);
  _am_free_matrix(a);

  /* Check whether window is out of bounds */
  if (*x2-hw < 0.0f || nc2-(*x2+hw) < one_plus_eps ||
      *y2-hh < 0.0f || nr2-(*y2+hh) < one_plus_eps)
    status = KLT_OOB;

  /* Check whether feature point has moved to much during iteration*/
  if ( (*x2-old_x2) > mdd || (*y2-old_y2) > mdd )
    status = KLT_OOB;

  /* Check whether residue is too large */
  if (status == KLT_TRACKED)  {
    if(!affine_map){
      _computeIntensityDifference(img1, img2, x1, y1, *x2, *y2,
				  width, height, imgdiff);
    }else{
      _am_computeIntensityDifferenceAffine(img1, img2, x1, y1, *x2, *y2,  *Axx, *Ayx , *Axy, *Ayy,
					   width, height, imgdiff);
    }
#ifdef DEBUG_AFFINE_MAPPING
    printf("iter = %d final_res = %f\n", iteration, _sumAbsFloatWindow(imgdiff, width, height)/(width*height));
#endif
    if (_sumAbsFloatWindow(imgdiff, width, height)/(width*height) > max_residue)
      status = KLT_LARGE_RESIDUE;
  }

  /* Free memory */
  free(imgdiff);  free(gradx);  free(grady);

#ifdef DEBUG_AFFINE_MAPPING
  printf("iter = %d status=%d\n", iteration, status);
  _KLTFreeFloatImage( aff_diff_win );
#endif

  /* Return appropriate value */
  return status;
}

/*
 * CONSISTENCY CHECK OF FEATURES BY AFFINE MAPPING (END)
 **********************************************************************/



/*********************************************************************
 * KLTTrackFeatures
 *
 * Tracks feature points from one image to the next.
 */

void KLTTrackFeatures(
					  KLT_TrackingContext tc,
					  KLT_PixelType *img1,
					  KLT_PixelType *img2,
					  int ncols,
					  int nrows,
					  KLT_FeatureList featurelist)
{
	_KLT_FloatImage tmpimg, floatimg1, floatimg2;
	_KLT_Pyramid pyramid1, pyramid1_gradx, pyramid1_grady,
		pyramid2, pyramid2_gradx, pyramid2_grady;
	float subsampling = (float) tc->subsampling;
	float xloc, yloc, xlocout, ylocout;
	int val = 0;
	int indx, r;
	KLT_BOOL floatimg1_created = FALSE;
	int i;

	if (KLT_verbose >= 1)  {
		fprintf(stderr,  "(KLT) Tracking %d features in a %d by %d image...  ",
			KLTCountRemainingFeatures(featurelist), ncols, nrows);
		fflush(stderr);
	}

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

	/* Create temporary image */
	tmpimg = _KLTCreateFloatImage(ncols, nrows);

	/* Process first image by converting to float, smoothing, computing */
	/* pyramid, and computing gradient pyramids */
	if (tc->sequentialMode && tc->pyramid_last != nullptr)  {
		pyramid1 = (_KLT_Pyramid) tc->pyramid_last;
		pyramid1_gradx = (_KLT_Pyramid) tc->pyramid_last_gradx;
		pyramid1_grady = (_KLT_Pyramid) tc->pyramid_last_grady;
		if (pyramid1->ncols[0] != ncols || pyramid1->nrows[0] != nrows) {
			KLTError("(KLTTrackFeatures) Size of incoming image (%d by %d) "
			"is different from size of previous image (%d by %d)\n",
			ncols, nrows, pyramid1->ncols[0], pyramid1->nrows[0]);
                        exit(1);
                }
		assert(pyramid1_gradx != nullptr);
		assert(pyramid1_grady != nullptr);
	} else  {
		floatimg1_created = TRUE;
		floatimg1 = _KLTCreateFloatImage(ncols, nrows);
		_KLTToFloatImage(img1, ncols, nrows, tmpimg);
		_KLTComputeSmoothedImage(tmpimg, _KLTComputeSmoothSigma(tc), floatimg1);
		pyramid1 = _KLTCreatePyramid(ncols, nrows, (int) subsampling, tc->nPyramidLevels);
		_KLTComputePyramid(floatimg1, pyramid1, tc->pyramid_sigma_fact);
		pyramid1_gradx = _KLTCreatePyramid(ncols, nrows, (int) subsampling, tc->nPyramidLevels);
		pyramid1_grady = _KLTCreatePyramid(ncols, nrows, (int) subsampling, tc->nPyramidLevels);
		for (i = 0 ; i < tc->nPyramidLevels ; i++)
			_KLTComputeGradients(pyramid1->img[i], tc->grad_sigma,
			pyramid1_gradx->img[i],
			pyramid1_grady->img[i]);
	}

	/* Do the same thing with second image */
	floatimg2 = _KLTCreateFloatImage(ncols, nrows);
	_KLTToFloatImage(img2, ncols, nrows, tmpimg);
	_KLTComputeSmoothedImage(tmpimg, _KLTComputeSmoothSigma(tc), floatimg2);
	pyramid2 = _KLTCreatePyramid(ncols, nrows, (int) subsampling, tc->nPyramidLevels);
	_KLTComputePyramid(floatimg2, pyramid2, tc->pyramid_sigma_fact);
	pyramid2_gradx = _KLTCreatePyramid(ncols, nrows, (int) subsampling, tc->nPyramidLevels);
	pyramid2_grady = _KLTCreatePyramid(ncols, nrows, (int) subsampling, tc->nPyramidLevels);
	for (i = 0 ; i < tc->nPyramidLevels ; i++)
		_KLTComputeGradients(pyramid2->img[i], tc->grad_sigma,
		pyramid2_gradx->img[i],
		pyramid2_grady->img[i]);

	/* Write internal images */
	if (tc->writeInternalImages)  {
		char fname[80];
		for (i = 0 ; i < tc->nPyramidLevels ; i++)  {
			snprintf(fname, sizeof(fname), "kltimg_tf_i%d.pgm", i);
			_KLTWriteFloatImageToPGM(pyramid1->img[i], fname);
			snprintf(fname, sizeof(fname), "kltimg_tf_i%d_gx.pgm", i);
			_KLTWriteFloatImageToPGM(pyramid1_gradx->img[i], fname);
			snprintf(fname, sizeof(fname), "kltimg_tf_i%d_gy.pgm", i);
			_KLTWriteFloatImageToPGM(pyramid1_grady->img[i], fname);
			snprintf(fname, sizeof(fname), "kltimg_tf_j%d.pgm", i);
			_KLTWriteFloatImageToPGM(pyramid2->img[i], fname);
			snprintf(fname, sizeof(fname), "kltimg_tf_j%d_gx.pgm", i);
			_KLTWriteFloatImageToPGM(pyramid2_gradx->img[i], fname);
			snprintf(fname, sizeof(fname), "kltimg_tf_j%d_gy.pgm", i);
			_KLTWriteFloatImageToPGM(pyramid2_grady->img[i], fname);
		}
	}

	/* For each feature, do ... */
	for (indx = 0 ; indx < featurelist->nFeatures ; indx++)  {

		/* Only track features that are not lost */
		if (featurelist->feature[indx]->val >= 0)  {

			xloc = featurelist->feature[indx]->x;
			yloc = featurelist->feature[indx]->y;

			/* Transform location to coarsest resolution */
			for (r = tc->nPyramidLevels - 1 ; r >= 0 ; r--)  {
				xloc /= subsampling;  yloc /= subsampling;
			}
			xlocout = xloc;  ylocout = yloc;

			/* Beginning with coarsest resolution, do ... */
			for (r = tc->nPyramidLevels - 1 ; r >= 0 ; r--)  {

				/* Track feature at current resolution */
				xloc *= subsampling;  yloc *= subsampling;
				xlocout *= subsampling;  ylocout *= subsampling;

				val = _trackFeature(xloc, yloc,
					&xlocout, &ylocout,
					pyramid1->img[r],
					pyramid1_gradx->img[r], pyramid1_grady->img[r],
					pyramid2->img[r],
					pyramid2_gradx->img[r], pyramid2_grady->img[r],
					tc->window_width, tc->window_height,
					tc->step_factor,
					tc->max_iterations,
					tc->min_determinant,
					tc->min_displacement,
					tc->max_residue,
					tc->lighting_insensitive);

				if (val==KLT_SMALL_DET || val==KLT_OOB)
					break;
			}

			/* Record feature */
			if (val == KLT_OOB) {
				featurelist->feature[indx]->x   = -1.0;
				featurelist->feature[indx]->y   = -1.0;
				featurelist->feature[indx]->val = KLT_OOB;
				if( featurelist->feature[indx]->aff_img ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img);
				if( featurelist->feature[indx]->aff_img_gradx ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img_gradx);
				if( featurelist->feature[indx]->aff_img_grady ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img_grady);
				featurelist->feature[indx]->aff_img = nullptr;
				featurelist->feature[indx]->aff_img_gradx = nullptr;
				featurelist->feature[indx]->aff_img_grady = nullptr;

			} else if (_outOfBounds(xlocout, ylocout, ncols, nrows, tc->borderx, tc->bordery))  {
				featurelist->feature[indx]->x   = -1.0;
				featurelist->feature[indx]->y   = -1.0;
				featurelist->feature[indx]->val = KLT_OOB;
				if( featurelist->feature[indx]->aff_img ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img);
				if( featurelist->feature[indx]->aff_img_gradx ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img_gradx);
				if( featurelist->feature[indx]->aff_img_grady ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img_grady);
				featurelist->feature[indx]->aff_img = nullptr;
				featurelist->feature[indx]->aff_img_gradx = nullptr;
				featurelist->feature[indx]->aff_img_grady = nullptr;
			} else if (val == KLT_SMALL_DET)  {
				featurelist->feature[indx]->x   = -1.0;
				featurelist->feature[indx]->y   = -1.0;
				featurelist->feature[indx]->val = KLT_SMALL_DET;
				if( featurelist->feature[indx]->aff_img ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img);
				if( featurelist->feature[indx]->aff_img_gradx ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img_gradx);
				if( featurelist->feature[indx]->aff_img_grady ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img_grady);
				featurelist->feature[indx]->aff_img = nullptr;
				featurelist->feature[indx]->aff_img_gradx = nullptr;
				featurelist->feature[indx]->aff_img_grady = nullptr;
			} else if (val == KLT_LARGE_RESIDUE)  {
				featurelist->feature[indx]->x   = -1.0;
				featurelist->feature[indx]->y   = -1.0;
				featurelist->feature[indx]->val = KLT_LARGE_RESIDUE;
				if( featurelist->feature[indx]->aff_img ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img);
				if( featurelist->feature[indx]->aff_img_gradx ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img_gradx);
				if( featurelist->feature[indx]->aff_img_grady ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img_grady);
				featurelist->feature[indx]->aff_img = nullptr;
				featurelist->feature[indx]->aff_img_gradx = nullptr;
				featurelist->feature[indx]->aff_img_grady = nullptr;
			} else if (val == KLT_MAX_ITERATIONS)  {
				featurelist->feature[indx]->x   = -1.0;
				featurelist->feature[indx]->y   = -1.0;
				featurelist->feature[indx]->val = KLT_MAX_ITERATIONS;
				if( featurelist->feature[indx]->aff_img ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img);
				if( featurelist->feature[indx]->aff_img_gradx ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img_gradx);
				if( featurelist->feature[indx]->aff_img_grady ) _KLTFreeFloatImage(featurelist->feature[indx]->aff_img_grady);
				featurelist->feature[indx]->aff_img = nullptr;
				featurelist->feature[indx]->aff_img_gradx = nullptr;
				featurelist->feature[indx]->aff_img_grady = nullptr;
			} else  {
				featurelist->feature[indx]->x = xlocout;
				featurelist->feature[indx]->y = ylocout;
				featurelist->feature[indx]->val = KLT_TRACKED;
				if (tc->affineConsistencyCheck >= 0 && val == KLT_TRACKED)  { /*for affine mapping*/
					int border = 2; /* add border for interpolation */

#ifdef DEBUG_AFFINE_MAPPING
					glob_index = indx;
#endif

					if(!featurelist->feature[indx]->aff_img){
						/* save image and gradient for each feature at finest resolution after first successful track */
						featurelist->feature[indx]->aff_img = _KLTCreateFloatImage((tc->affine_window_width+border), (tc->affine_window_height+border));
						featurelist->feature[indx]->aff_img_gradx = _KLTCreateFloatImage((tc->affine_window_width+border), (tc->affine_window_height+border));
						featurelist->feature[indx]->aff_img_grady = _KLTCreateFloatImage((tc->affine_window_width+border), (tc->affine_window_height+border));
						_am_getSubFloatImage(pyramid1->img[0],xloc,yloc,featurelist->feature[indx]->aff_img);
						_am_getSubFloatImage(pyramid1_gradx->img[0],xloc,yloc,featurelist->feature[indx]->aff_img_gradx);
						_am_getSubFloatImage(pyramid1_grady->img[0],xloc,yloc,featurelist->feature[indx]->aff_img_grady);
						featurelist->feature[indx]->aff_x = xloc - (int) xloc + (tc->affine_window_width+border)/2;
						featurelist->feature[indx]->aff_y = yloc - (int) yloc + (tc->affine_window_height+border)/2;;
					}else{
						/* affine tracking */
						val = _am_trackFeatureAffine(featurelist->feature[indx]->aff_x, featurelist->feature[indx]->aff_y,
							&xlocout, &ylocout,
							featurelist->feature[indx]->aff_img,
							featurelist->feature[indx]->aff_img_gradx,
							featurelist->feature[indx]->aff_img_grady,
							pyramid2->img[0],
							pyramid2_gradx->img[0], pyramid2_grady->img[0],
							tc->affine_window_width, tc->affine_window_height,
							tc->step_factor,
							tc->affine_max_iterations,
							tc->min_determinant,
							tc->min_displacement,
							tc->affine_min_displacement,
							tc->affine_max_residue,
							tc->lighting_insensitive,
							tc->affineConsistencyCheck,
							tc->affine_max_displacement_differ,
							&featurelist->feature[indx]->aff_Axx,
							&featurelist->feature[indx]->aff_Ayx,
							&featurelist->feature[indx]->aff_Axy,
							&featurelist->feature[indx]->aff_Ayy
							);
						featurelist->feature[indx]->val = val;
						if(val != KLT_TRACKED){
							featurelist->feature[indx]->x   = -1.0;
							featurelist->feature[indx]->y   = -1.0;
							featurelist->feature[indx]->aff_x = -1.0;
							featurelist->feature[indx]->aff_y = -1.0;
							/* free image and gradient for lost feature */
							_KLTFreeFloatImage(featurelist->feature[indx]->aff_img);
							_KLTFreeFloatImage(featurelist->feature[indx]->aff_img_gradx);
							_KLTFreeFloatImage(featurelist->feature[indx]->aff_img_grady);
							featurelist->feature[indx]->aff_img = nullptr;
							featurelist->feature[indx]->aff_img_gradx = nullptr;
							featurelist->feature[indx]->aff_img_grady = nullptr;
						}else{
							/*featurelist->feature[indx]->x = xlocout;*/
							/*featurelist->feature[indx]->y = ylocout;*/
						}
					}
				}

			}
		}
	}

	if (tc->sequentialMode)  {
		tc->pyramid_last = pyramid2;
		tc->pyramid_last_gradx = pyramid2_gradx;
		tc->pyramid_last_grady = pyramid2_grady;
	} else  {
		_KLTFreePyramid(pyramid2);
		_KLTFreePyramid(pyramid2_gradx);
		_KLTFreePyramid(pyramid2_grady);
	}

	/* Free memory */
	_KLTFreeFloatImage(tmpimg);
	if (floatimg1_created)  _KLTFreeFloatImage(floatimg1);
	_KLTFreeFloatImage(floatimg2);
	_KLTFreePyramid(pyramid1);
	_KLTFreePyramid(pyramid1_gradx);
	_KLTFreePyramid(pyramid1_grady);

	if (KLT_verbose >= 1)  {
		fprintf(stderr,  "\n\t%d features successfully tracked.\n",
			KLTCountRemainingFeatures(featurelist));
		if (tc->writeInternalImages)
			fprintf(stderr,  "\tWrote images to 'kltimg_tf*.pgm'.\n");
		fflush(stderr);
	}

}
