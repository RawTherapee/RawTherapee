/*********************************************************************
 * storeFeatures.c
 *
 *********************************************************************/

#include <cstdlib>

/* Our includes */
#include "error.h"
#include "klt.h"


/*********************************************************************
 *
 */

void KLTStoreFeatureList(
  KLT_FeatureList fl,
  KLT_FeatureTable ft,
  int frame)
{
  int feat;

  if (frame < 0 || frame >= ft->nFrames) {
    KLTError("(KLTStoreFeatures) Frame number %d is not between 0 and %d",
             frame, ft->nFrames - 1);
    exit(1);
  }

  if (fl->nFeatures != ft->nFeatures) {
    KLTError("(KLTStoreFeatures) FeatureList and FeatureTable must "
             "have the same number of features");
    exit(1);
  }

  for (feat = 0 ; feat < fl->nFeatures ; feat++)  {
    ft->feature[feat][frame]->x   = fl->feature[feat]->x;
    ft->feature[feat][frame]->y   = fl->feature[feat]->y;
    ft->feature[feat][frame]->val = fl->feature[feat]->val;
  }
}


/*********************************************************************
 *
 */

void KLTExtractFeatureList(
  KLT_FeatureList fl,
  KLT_FeatureTable ft,
  int frame)
{
  int feat;

  if (frame < 0 || frame >= ft->nFrames) {
    KLTError("(KLTExtractFeatures) Frame number %d is not between 0 and %d",
             frame, ft->nFrames - 1);
    exit(1);
  }

  if (fl->nFeatures != ft->nFeatures) {
    KLTError("(KLTExtractFeatures) FeatureList and FeatureTable must "
             "have the same number of features");
    exit(1);
  }

  for (feat = 0 ; feat < fl->nFeatures ; feat++)  {
    fl->feature[feat]->x   = ft->feature[feat][frame]->x;
    fl->feature[feat]->y   = ft->feature[feat][frame]->y;
    fl->feature[feat]->val = ft->feature[feat][frame]->val;
  }
}
 

/*********************************************************************
 *
 */

void KLTStoreFeatureHistory(
  KLT_FeatureHistory fh,
  KLT_FeatureTable ft,
  int feat)
{
  int frame;

  if (feat < 0 || feat >= ft->nFeatures) {
    KLTError("(KLTStoreFeatureHistory) Feature number %d is not between 0 and %d",
             feat, ft->nFeatures - 1);
    exit(1);
  }

  if (fh->nFrames != ft->nFrames) {
    KLTError("(KLTStoreFeatureHistory) FeatureHistory and FeatureTable must "
             "have the same number of frames");
    exit(1);
  }

  for (frame = 0 ; frame < fh->nFrames ; frame++)  {
    ft->feature[feat][frame]->x   = fh->feature[frame]->x;
    ft->feature[feat][frame]->y   = fh->feature[frame]->y;
    ft->feature[feat][frame]->val = fh->feature[frame]->val;
  }
}


/*********************************************************************
 *
 */

void KLTExtractFeatureHistory(
  KLT_FeatureHistory fh,
  KLT_FeatureTable ft,
  int feat)
{
  int frame;

  if (feat < 0 || feat >= ft->nFeatures) {
    KLTError("(KLTExtractFeatureHistory) Feature number %d is not between 0 and %d",
             feat, ft->nFeatures - 1);
    exit(1);
  }

  if (fh->nFrames != ft->nFrames) {
    KLTError("(KLTExtractFeatureHistory) FeatureHistory and FeatureTable must "
             "have the same number of frames");
    exit(1);
  }

  for (frame = 0 ; frame < fh->nFrames ; frame++)  {
    fh->feature[frame]->x   = ft->feature[feat][frame]->x;
    fh->feature[frame]->y   = ft->feature[feat][frame]->y;
    fh->feature[frame]->val = ft->feature[feat][frame]->val;
  }
}

