/*********************************************************************
 * klt.h
 *
 * Kanade-Lucas-Tomasi tracker
 *********************************************************************/

#ifndef _KLT_H_
#define _KLT_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef float KLT_locType;
typedef unsigned char KLT_PixelType;

#define KLT_BOOL int

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#ifndef NULL
#define NULL  0
#endif

#define KLT_TRACKED           0
#define KLT_NOT_FOUND        -1
#define KLT_SMALL_DET        -2
#define KLT_MAX_ITERATIONS   -3
#define KLT_OOB              -4
#define KLT_LARGE_RESIDUE    -5

#include "klt_util.h" /* for affine mapping */

/*******************
 * Structures
 */

typedef struct  {
  /* Available to user */
  int mindist;			/* min distance b/w features */
  int window_width, window_height;
  KLT_BOOL sequentialMode;	/* whether to save most recent image to save time */
  /* can set to TRUE manually, but don't set to */
  /* FALSE manually */
  KLT_BOOL smoothBeforeSelecting;	/* whether to smooth image before */
  /* selecting features */
  KLT_BOOL writeInternalImages;	/* whether to write internal images */
  /* tracking features */
  KLT_BOOL lighting_insensitive;  /* whether to normalize for gain and bias (not in original algorithm) */
  
  /* Available, but hopefully can ignore */
  int min_eigenvalue;		/* smallest eigenvalue allowed for selecting */
  float min_determinant;	/* th for determining lost */
  float min_displacement;	/* th for stopping tracking when pixel changes little */
  int max_iterations;		/* th for stopping tracking when too many iterations */
  float max_residue;		/* th for stopping tracking when residue is large */
  float grad_sigma;
  float smooth_sigma_fact;
  float pyramid_sigma_fact;
  float step_factor;  /* size of Newton steps; 2.0 comes from equations, 1.0 seems to avoid overshooting */
  int nSkippedPixels;		/* # of pixels skipped when finding features */
  int borderx;			/* border in which features will not be found */
  int bordery;
  int nPyramidLevels;		/* computed from search_ranges */
  int subsampling;		/* 		" */

  
  /* for affine mapping */ 
  int affine_window_width, affine_window_height;
  int affineConsistencyCheck; /* whether to evaluates the consistency of features with affine mapping 
                              -1 = don't evaluates the consistency
                              0 = evaluates the consistency of features with translation mapping
                              1 = evaluates the consistency of features with similarity mapping
                              2 = evaluates the consistency of features with affine mapping
  */
  int affine_max_iterations;  
  float affine_max_residue;
  float affine_min_displacement;        
  float affine_max_displacement_differ; /* th for the difference between the displacement calculated 
  by the affine tracker and the frame to frame tracker in pel*/

  /* User must not touch these */
  void *pyramid_last;
  void *pyramid_last_gradx;
  void *pyramid_last_grady;
}  KLT_TrackingContextRec, *KLT_TrackingContext;


typedef struct  {
  KLT_locType x;
  KLT_locType y;
  int val;	
  /* for affine mapping */
  _KLT_FloatImage aff_img; 
  _KLT_FloatImage aff_img_gradx;
  _KLT_FloatImage aff_img_grady;
  KLT_locType aff_x;
  KLT_locType aff_y;
  KLT_locType aff_Axx;
  KLT_locType aff_Ayx;
  KLT_locType aff_Axy;
  KLT_locType aff_Ayy;
}  KLT_FeatureRec, *KLT_Feature;

typedef struct  {
  int nFeatures;
  KLT_Feature *feature;
}  KLT_FeatureListRec, *KLT_FeatureList;

typedef struct  {
  int nFrames;
  KLT_Feature *feature;
}  KLT_FeatureHistoryRec, *KLT_FeatureHistory;

typedef struct  {
  int nFrames;
  int nFeatures;
  KLT_Feature **feature;
}  KLT_FeatureTableRec, *KLT_FeatureTable;



/*******************
 * Functions
 */

/* Create */
KLT_TrackingContext KLTCreateTrackingContext(void);
KLT_FeatureList KLTCreateFeatureList(
  int nFeatures);
KLT_FeatureHistory KLTCreateFeatureHistory(
  int nFrames);
KLT_FeatureTable KLTCreateFeatureTable(
  int nFrames,
  int nFeatures);

/* Free */
void KLTFreeTrackingContext(
  KLT_TrackingContext tc);
void KLTFreeFeatureList(
  KLT_FeatureList fl);
void KLTFreeFeatureHistory(
  KLT_FeatureHistory fh);
void KLTFreeFeatureTable(
  KLT_FeatureTable ft);

/* Processing */
void KLTSelectGoodFeatures(
  KLT_TrackingContext tc,
  KLT_PixelType *img,
  int ncols,
  int nrows,
  KLT_FeatureList fl);
void KLTTrackFeatures(
  KLT_TrackingContext tc,
  KLT_PixelType *img1,
  KLT_PixelType *img2,
  int ncols,
  int nrows,
  KLT_FeatureList fl);
void KLTReplaceLostFeatures(
  KLT_TrackingContext tc,
  KLT_PixelType *img,
  int ncols,
  int nrows,
  KLT_FeatureList fl);

/* Utilities */
int KLTCountRemainingFeatures(
  KLT_FeatureList fl);
void KLTPrintTrackingContext(
  KLT_TrackingContext tc);
void KLTChangeTCPyramid(
  KLT_TrackingContext tc,
  int search_range);
void KLTUpdateTCBorder(
  KLT_TrackingContext tc);
void KLTStopSequentialMode(
  KLT_TrackingContext tc);
void KLTSetVerbosity(
  int verbosity);
float _KLTComputeSmoothSigma(
  KLT_TrackingContext tc);

/* Storing/Extracting Features */
void KLTStoreFeatureList(
  KLT_FeatureList fl,
  KLT_FeatureTable ft,
  int frame);
void KLTExtractFeatureList(
  KLT_FeatureList fl,
  KLT_FeatureTable ft,
  int frame);
void KLTStoreFeatureHistory(
  KLT_FeatureHistory fh,
  KLT_FeatureTable ft,
  int feat);
void KLTExtractFeatureHistory(
  KLT_FeatureHistory fh,
  KLT_FeatureTable ft,
  int feat);

/* Writing/Reading */
void KLTWriteFeatureListToPPM(
  KLT_FeatureList fl,
  KLT_PixelType *greyimg,
  int ncols,
  int nrows,
  const char *filename);
void KLTWriteFeatureList(
  KLT_FeatureList fl,
  const char *filename,
  const char *fmt);
void KLTWriteFeatureHistory(
  KLT_FeatureHistory fh,
  const char *filename,
  const char *fmt);
void KLTWriteFeatureTable(
  KLT_FeatureTable ft,
  const char *filename,
  const char *fmt);
KLT_FeatureList KLTReadFeatureList(
  KLT_FeatureList fl,
  const char *filename);
KLT_FeatureHistory KLTReadFeatureHistory(
  KLT_FeatureHistory fh,
  const char *filename);
KLT_FeatureTable KLTReadFeatureTable(
  KLT_FeatureTable ft,
  const char *filename);
#ifdef __cplusplus
}
#endif

#endif






