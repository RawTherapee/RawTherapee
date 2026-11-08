/**********************************************************************
Finds the N_FEATURES best features in an image, and tracks these
features to the next image.  Saves the feature
locations (before and after tracking) to text files and to PPM files,
and prints the features to the screen.
**********************************************************************/

#include <pnmio.h>
#include <klt.h>
#include <cmath>
#include <cstring>

#define N_FEATURES 100
#define DELTA_1 0.05
#define DELTA_2 0.01
#define RXY_LIMIT 0.6
#define CENTER_R 0.3

//#define DEBUG_IMG

#ifdef DEBUG_IMG
void drawDotXY(unsigned char* img, int ncols, int nrows, int x, int y, int color)
{
    img[x + y * ncols] = color;
}

void drawDot(unsigned char* img, int ncols, int nrows, double r0, double r10, int color)
{
    if (r0 >= 0 && r0 < 1 && r10 >= 0.8 && r10 < 1.2) {
        drawDotXY (img, ncols, nrows, (int)(r0 * ncols), (int)((r10 - 0.8) * 2.5 * nrows), color);
    }
}
#endif

int calcDistortion(unsigned char* img1, unsigned char* img2, int ncols, int nrows, int nfactor, double &distortion)
{
    KLT_TrackingContext tc;
    KLT_FeatureList fl;
    KLT_FeatureTable ft;
    int i, n;
    double radius, wc, hc;

    double r0[N_FEATURES * nfactor];
    memset(r0, 0, N_FEATURES * nfactor * sizeof(double));
    double r10[N_FEATURES * nfactor];
    memset(r10, 0, N_FEATURES * nfactor * sizeof(double));

    tc = KLTCreateTrackingContext();
    //tc->mindist = 20;
    tc->lighting_insensitive = TRUE;
    tc->nSkippedPixels = 5;
    tc->step_factor = 2.0;
    tc->max_iterations = 20;
    //KLTPrintTrackingContext(tc);
    fl = KLTCreateFeatureList(N_FEATURES * nfactor);
    ft = KLTCreateFeatureTable(2, N_FEATURES * nfactor);

    radius = sqrt(ncols * ncols + nrows * nrows) / 2.0;
    wc = ((double)ncols) / 2.0 - 0.5;
    hc = ((double)nrows) / 2.0 - 0.5;

    KLTSelectGoodFeatures(tc, img1, ncols, nrows, fl);
    KLTStoreFeatureList(fl, ft, 0);

    KLTTrackFeatures(tc, img1, img2, ncols, nrows, fl);
    KLTStoreFeatureList(fl, ft, 1);

    // add a shade to img2, we want to draw something on top of it.
    for (i = 0; i < ncols * nrows; i++) {
        img2[i] = (img2[i] / 2) + 16;
    }

    // find the best comp and scale when assume r1 = r0*(1.0-comp+(r0*comp))*scale;
    n = 0;
    double total_r10 = 0.0, total_r0 = 0.0;

    for (i = 0; i < N_FEATURES * nfactor; i++) {
        if (ft->feature[i][1]->val >= 0) {
            double x0, y0, x1, y1;
            x0 = ft->feature[i][0]->x;
            y0 = ft->feature[i][0]->y;
            x1 = ft->feature[i][1]->x;
            y1 = ft->feature[i][1]->y;

            r0[n] = sqrt((x0 - wc) * (x0 - wc) + (y0 - hc) * (y0 - hc)) / radius;

            // dots too close to the center tends to have big deviation and create noise, extract them
            if (r0[n] < CENTER_R) {
                continue;
            }

            r10[n] = (sqrt((x1 - wc) * (x1 - wc) + (y1 - hc) * (y1 - hc)) / radius) / r0[n];
            total_r10 += r10[n];
            total_r0 += r0[n];
            n++;
        } else {
            ft->feature[i][0]->x = -1.0;
            ft->feature[i][0]->y = -1.0;
        }
    }

    if (n < 5) {
        printf ("Not sufficient features.\n");
        distortion = 0.0;
        KLTFreeFeatureTable(ft);
        KLTFreeFeatureList(fl);
        KLTFreeTrackingContext(tc);
        return -1;
    }

    double avg_r10 = total_r10 / n;
    double avg_r0  = total_r0  / n;
    double Sxx = 0.0;
    double Sxy = 0.0;
    double Syy = 0.0;

    for (i = 0; i < n; i++) {
        Sxx += (r0[i] - avg_r0) * (r0[i] - avg_r0);
        Sxy += (r0[i] - avg_r0) * (r10[i] - avg_r10);
        Syy += (r10[i] - avg_r10) * (r10[i] - avg_r10);
    }

    double u = Sxy / Sxx;
    double v = avg_r10 - u * avg_r0;
    double b = u + v;
    double a = u / b;
    double total_delta = 0.0;
    double rxy = Sxy / sqrt(Sxx * Syy);

    if (rxy < 0) {
        rxy = -rxy;
    }

    int new_n = n;

    // calculate deviation
    for (i = 0; i < n; i++) {
        double delta = r10[i] - (1.0 - a + (r0[i] * a)) * b;
        delta = delta >= 0 ? delta : -delta;

#ifdef DEBUG_IMG
        drawDot(img2, ncols, nrows, r0[i], r10[i], 255);
#endif

        if (delta >= DELTA_1) {
            total_r10 -= r10[i];
            total_r0 -= r0[i];
            r0[i] = -1.0;
            new_n--;
        }

        total_delta += delta;
    }

    printf ("distortion amount=%lf scale=%lf deviation=%lf, rxy=%lf\n", a, b, total_delta / n, rxy);

    if (new_n < 5) {
        printf ("Not sufficient features.\n");
        distortion = 0.0;
        KLTFreeFeatureTable(ft);
        KLTFreeFeatureList(fl);
        KLTFreeTrackingContext(tc);
        return -1;
    }

    printf ("Removed %d outstading data points\n", n - new_n);
    avg_r10 = total_r10 / new_n;
    avg_r0  = total_r0  / new_n;
    Sxx = 0.0;
    Sxy = 0.0;
    Syy = 0.0;

    for (i = 0; i < n; i++) {
        if (r0[i] < 0) {
            continue;
        }

        Sxx += (r0[i] - avg_r0) * (r0[i] - avg_r0);
        Sxy += (r0[i] - avg_r0) * (r10[i] - avg_r10);
        Syy += (r10[i] - avg_r10) * (r10[i] - avg_r10);
    }

    u = Sxy / Sxx;
    v = avg_r10 - u * avg_r0;
    b = u + v;
    a = u / b;
    total_delta = 0.0;
    rxy = Sxy / sqrt(Sxx * Syy);

    if (rxy < 0) {
        rxy = -rxy;
    }

#ifdef DEBUG_IMG

    // draw lines for curve and different deviation level, for debugging purpose
    for (i = 0; i < ncols; i++) {
        double val = (1.0 - a + ((i / (double)ncols) * a)) * b;

        if (val >= 0.8 && val < 1.2) {
            if (img2[i + ((int)((val - 0.8) * 2.5 * nrows))*ncols] != 255) {
                img2[i + ((int)((val - 0.8) * 2.5 * nrows))*ncols] = 0;
            }
        }

        val += DELTA_1;

        if (val >= 0.8 && val < 1.2) {
            if (img2[i + ((int)((val - 0.8) * 2.5 * nrows))*ncols] != 255) {
                img2[i + ((int)((val - 0.8) * 2.5 * nrows))*ncols] = 8;
            }
        }

        val -= DELTA_1 * 2;

        if (val >= 0.8 && val < 1.2) {
            if (img2[i + ((int)((val - 0.8) * 2.5 * nrows))*ncols] != 255) {
                img2[i + ((int)((val - 0.8) * 2.5 * nrows))*ncols] = 8;
            }
        }

        val += DELTA_1 + DELTA_2;

        if (val >= 0.8 && val < 1.2) {
            if (img2[i + ((int)((val - 0.8) * 2.5 * nrows))*ncols] != 255) {
                img2[i + ((int)((val - 0.8) * 2.5 * nrows))*ncols] = 16;
            }
        }

        val -= DELTA_2 * 2;

        if (val >= 0.8 && val < 1.2) {
            if (img2[i + ((int)((val - 0.8) * 2.5 * nrows))*ncols] != 255) {
                img2[i + ((int)((val - 0.8) * 2.5 * nrows))*ncols] = 16;
            }
        }
    }

    KLTExtractFeatureList(fl, ft, 0);
    KLTWriteFeatureListToPPM(fl, img1, ncols, nrows, "/tmp/feat0.ppm");
    KLTExtractFeatureList(fl, ft, 1);
    KLTWriteFeatureListToPPM(fl, img2, ncols, nrows, "/tmp/feat1.ppm");
#endif

    // calculate deviation
    for (i = 0; i < n; i++) {
        if (r0[i] < 0) {
            continue;
        }

        double delta = r10[i] - (1.0 - a + (r0[i] * a)) * b;
        delta = delta >= 0 ? delta : -delta;
        total_delta += delta;
    }

    printf ("distortion amount=%lf scale=%lf deviation=%lf, rxy=%lf\n", a, b, total_delta / n, rxy);

    if (total_delta / new_n > DELTA_2) {
        printf ("Deviation is too big.\n");
        distortion = 0.0;
        KLTFreeFeatureTable(ft);
        KLTFreeFeatureList(fl);
        KLTFreeTrackingContext(tc);
        return -2;
    }

    if (rxy < RXY_LIMIT) {
        printf ("Not linear enough\n");
        distortion = 0.0;
        KLTFreeFeatureTable(ft);
        KLTFreeFeatureList(fl);
        KLTFreeTrackingContext(tc);
        return -3;
    }

    printf ("distortion amount=%lf scale=%lf deviation=%lf, rxy=%lf\n", a, b, total_delta / n, rxy);
    distortion = a;

    KLTFreeFeatureTable(ft);
    KLTFreeFeatureList(fl);
    KLTFreeTrackingContext(tc);
    return 1;
}

