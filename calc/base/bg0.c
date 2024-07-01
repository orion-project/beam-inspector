/*

Comparison of the Approximation method for baseline offset correction (ISO 11146-3 [3.4.3])
with the Statistical method (ISO 11146-3 [3.4.2]) which is mostly the same but includes
an additional step for averaging over all unlluminated pixels.

This averaging takes additional time but probably does not increase the accuracy a lot.
ISO 11146-3 [3.4.2] says:
This method (the approximation) may not only provide guess-values for the statistical method.
In many cases the accuracy of this offset fine correction will be __sufficient__

--------------------------------------
GCC 11.2x64 Intel i7-2600 CPU 3.40GHz

Image: ../../tmp/real_beams/test_image_45.pgm
Image size: 2592x2048
Max gray: 65535
Frames: 1

lbs_calc_beam (statistical)
center=[1348,1058], diam=[968,1173], angle=-55.4, iters=1
Elapsed: 0.047s, FPS: 21.3, 47.00ms/frame

cgn_calc_beam (approximation)
center=[1348,1058], diam=[975,1174], angle=-54.4, iters=1
Elapsed: 0.030s, FPS: 33.3, 30.00ms/frame

*/

#include <math.h>
#include <time.h>
#include "../pgm.h"

#define FRAMES 30
#define FILENAME "../../beams/beam_8b_ast.pgm"
//#define FILENAME "../../beams/dot_8b_ast.pgm"
//#define FILENAME "../../tmp/real_beams/test_image_45.pgm"
//#define FILENAME "../../tmp/real_beams/test_image_35.pgm"
//#define CORNER_FRACTION 0.2
#define CORNER_FRACTION 0.035

#define sqr(s) ((s)*(s))
#define sign(s) ((s) < 0 ? -1 : ((s) > 0 ? 1 : 0))
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

typedef uint8_t pixel;
//typedef uint16_t pixel;

typedef struct {
    int w, h;
    pixel *buf;
    int iso_noise, max_iter;
    double corner_fraction, nT, mask_diam;

    double *subtracted, *subtracted_no_noise;
    int x1, x2, y1, y2, iters;

    double mean, sdev, min, max;
    double dx, dy, xc, yc, phi;
} BeamCalc;

void print_buf(pixel *buf, int w, int h) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            printf("%d\t", buf[i*w+j]);
        }
        printf("\n");
    }
}

void print_buf_d(double *buf, int w, int h) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            printf("%.2f\t", buf[i*w+j]);
        }
        printf("\n");
    }
}

void lbs_corner_background(BeamCalc *c) {
    int w = c->w, h = c->h;
    int dw = w * c->corner_fraction;
    int dh = h * c->corner_fraction;
    pixel *buf = c->buf;
    double *tmp = c->subtracted;

    int k = 0;
    double m = 0;
    for (int i = 0; i < h; i++) {
        if (i < dh || i >= h-dh) {
            int offset = i*w;
            for (int j = 0; j < w; j++) {
                if (j < dw || j >= w-dw) {
                    tmp[k] = buf[offset + j];
                    m += tmp[k];
                    k++;
                }
            }
        }
    }
    m /= (double)k;

    double s = 0;
    for (int i = 0; i < k; i++) {
        s += sqr(tmp[i] - m);
    }
    s = sqrt(s / (double)k);

    c->mean = m;
    c->sdev = s;
}

void lbs_iso_background(BeamCalc *c) {
    int w = c->w, h = c->h;
    int dw = w * c->corner_fraction;
    int dh = h * c->corner_fraction;
    pixel *buf = c->buf;
    double *tmp = c->subtracted;

    // Mean and sdev of background in corners of image
    int k = 0;
    double m = 0;
    for (int i = 0; i < h; i++) {
        if (i < dh || i >= h-dh) {
            int offset = i*w;
            for (int j = 0; j < w; j++) {
                if (j < dw || j >= w-dw) {
                    tmp[k] = buf[offset + j];
                    m += tmp[k];
                    k++;
                }
            }
        }
    }
    m /= (double)k;

    double s = 0;
    for (int i = 0; i < k; i++) {
        s += sqr(tmp[i] - m);
    }
    s = sqrt(s / (double)k);

    // Background for unilluminated pixels in an image
    double th = m + c->nT * s;

    k = 0;
    m = 0;
    for (int i = 0; i < w*h; i++) {
        if (buf[i] < th) {
            tmp[k] = buf[i];
            m += tmp[k];
            k++;
        }
    }
    m /= (double)k;

    s = 0;
    for (int i = 0; i < k; i++) {
        s += sqr(tmp[i] - m);
    }
    s = sqrt(s / (double)k);

    c->mean = m;
    c->sdev = s;
}

void lbs_subtract_iso_background(BeamCalc *c) {
    // The algorithm is from laserbeamsize package

    int w = c->w, h = c->h;
    int dw = w * c->corner_fraction;
    int dh = h * c->corner_fraction;
    pixel *buf = c->buf;
    double *tmp = c->subtracted;

    // ISO 11146-3 [3.4.2]:
    // Guess-values for the baseline offset and the noise,
    // may be either extracted from a non-illuminated dark image
    // or from non-illuminated areas within the measured distribution
    //
    // Mean and sdev of background in corners of image:
    int k = 0;
    double m = 0;
    for (int i = 0; i < h; i++) {
        if (i < dh || i >= h-dh) {
            int offset = i*w;
            for (int j = 0; j < w; j++) {
                if (j < dw || j >= w-dw) {
                    tmp[k] = buf[offset + j];
                    m += tmp[k];
                    k++;
                }
            }
        }
    }
    m /= (double)k;

    double s = 0;
    for (int i = 0; i < k; i++) {
        s += sqr(tmp[i] - m);
    }
    s = sqrt(s / (double)k);
    
    c->mean = m;
    c->sdev = s;

    // All pixels whose grey values exceed the threshold assumed to be illuminated
    // and which must be included in beam width calculations.
    // The other pixels are assumed to be non-illuminated,
    // their average gives the baseline offset value,
    // which has to be subtracted from the measured data:
    double th = m + c->nT * s;

    k = 0;
    m = 0;
    for (int i = 0; i < w*h; i++) {
        if (buf[i] < th) {
            tmp[k] = buf[i];
            m += tmp[k];
            k++;
        }
    }
    m /= (double)k;

    s = 0;
    for (int i = 0; i < k; i++) {
        s += sqr(tmp[i] - m);
    }
    s = sqrt(s / (double)k);
    
    c->mean = m;
    c->sdev = s;

    // Image with ISO 11146 background subtracted.
    th = c->nT * s;

    for (int i = 0; i < w*h; i++) {
        c->subtracted[i] = buf[i] - m;
        if (c->subtracted[i] < th) {
            c->subtracted_no_noise[i] = 0;
        } else {
            c->subtracted_no_noise[i] = c->subtracted[i];
        }
    }
}

void calc_beam_basic(double *buf, BeamCalc *c) {
    const int w = c->w, h = c->h;
    const int x1 = c->x1, x2 = c->x2;
    const int y1 = c->y1, y2 = c->y2;
    double p = 0;
    double xc = 0;
    double yc = 0;
    for (int i = y1; i < y2; i++) {
        const int offset = i*w;
        for (int j = x1; j < x2; j++) {
            p += buf[offset + j];
            xc += buf[offset + j] * j;
            yc += buf[offset + j] * i;
        }
    }
    xc /= p;
    yc /= p;
    double xx = 0, yy = 0, xy = 0;
    for (int i = y1; i < y2; i++) {
        const int offset = i*w;
        for (int j = x1; j < x2; j++) {
            xx += buf[offset + j] * sqr(j - xc);
            xy += buf[offset + j] * (j - xc)*(i - yc);
            yy += buf[offset + j] * sqr(i - yc);
        }
    }
    xx /= p;
    xy /= p;
    yy /= p;
    double ss = sign(xx - yy) * sqrt(sqr(xx - yy) + 4*sqr(xy));
    c->dx = 2.8284271247461903 * sqrt(xx + yy + ss);
    c->dy = 2.8284271247461903 * sqrt(xx + yy - ss);
    c->phi = 0.5 * atan(2 * xy / (xx - yy));
    c->xc = xc;
    c->yc = yc;
}

void lbs_calc_beam(BeamCalc *c) {
    int w = c->w, h = c->h;

    lbs_subtract_iso_background(c);

    // calc initial integration area
    c->x1 = 0, c->x2 = w, c->y1 = 0, c->y2 = h;
    calc_beam_basic(c->subtracted_no_noise, c);
    //printf("Initial center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", c->xc, c->yc, c->dx, c->dy, c->phi * 180/3.1416);

    double *img = (c->iso_noise > 0) 
        ? c->subtracted  // follow iso background guidelines (positive & negative bkgnd values)
        : c->subtracted_no_noise;

    for (c->iters = 0; c->iters < c->max_iter; c->iters++) {
        double xc0 = c->xc, yc0 = c->yc, dx0 = c->dx, dy0 = c->dy;
        c->x1 = xc0 - dx0/2.0 * c->mask_diam; c->x1 = max(c->x1, 0);
        c->x2 = xc0 + dx0/2.0 * c->mask_diam; c->x2 = min(c->x2, w);
        c->y1 = yc0 - dy0/2.0 * c->mask_diam; c->y1 = max(c->y1, 0);
        c->y2 = yc0 + dy0/2.0 * c->mask_diam; c->y2 = min(c->y2, h);
        calc_beam_basic(img, c);
        double th = min(dx0, dy0) * 0.05;
        if (fabs(c->xc - xc0) < th && fabs(c->yc - yc0) < th && 
            fabs(c->dx - dx0) < th && fabs(c->dy - dy0) < th) {
            c->iters++;
            break;
        }
    }
}

void cgn_subtract_background(BeamCalc *c) {
    const int w = c->w, h = c->h;
    const int dw = w * c->corner_fraction;
    const int dh = h * c->corner_fraction;
    const pixel *buf = c->buf;
    double *tmp = c->subtracted;

    // ISO 11146-3 [3.4.3]:
    // Determining the baseline offset by averaging a sample of N non-illuminated points
    // (for example arrays of n × m pixels in each of the four corners) within the image
    // directly gives the offset, which has to be subtracted from the measured distribution.
    int k = 0;
    double m = 0;
    for (int i = 0; i < h; i++) {
        if (i < dh || i >= h-dh) {
            const int offset = i*w;
            for (int j = 0; j < w; j++) {
                if (j < dw || j >= w-dw) {
                    tmp[k] = buf[offset + j];
                    m += tmp[k];
                    k++;
                }
            }
        }
    }
    m /= (double)k;

    double s = 0;
    for (int i = 0; i < k; i++) {
        s += sqr(tmp[i] - m);
    }
    s = sqrt(s / (double)k);

    c->mean = m;
    c->sdev = s;

    double th = m + c->nT * s;
    c->min = 1e10;
    c->max = -1e10;
    for (int i = 0; i < w*h; i++) {
        if (buf[i] > th) {
            tmp[i] = buf[i] - m;
        } else {
            tmp[i] = 0;
        }
        if (c->max < tmp[i]) {
            c->max = tmp[i];
        } else if (c->min > tmp[i]) {
            c->min = tmp[i];
        }
    }
}

void cgn_calc_beam(BeamCalc *c) {
    int w = c->w, h = c->h;

    cgn_subtract_background(c);

    // calc initial integration area
    c->x1 = 0, c->x2 = w, c->y1 = 0, c->y2 = h;
    calc_beam_basic(c->subtracted, c);
    //printf("Initial center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", c->xc, c->yc, c->dx, c->dy, c->phi * 180/3.1416);

    for (c->iters = 0; c->iters < c->max_iter; c->iters++) {
        double xc0 = c->xc, yc0 = c->yc, dx0 = c->dx, dy0 = c->dy;
        c->x1 = xc0 - dx0/2.0 * c->mask_diam; c->x1 = max(c->x1, 0);
        c->x2 = xc0 + dx0/2.0 * c->mask_diam; c->x2 = min(c->x2, w);
        c->y1 = yc0 - dy0/2.0 * c->mask_diam; c->y1 = max(c->y1, 0);
        c->y2 = yc0 + dy0/2.0 * c->mask_diam; c->y2 = min(c->y2, h);
        calc_beam_basic(c->subtracted, c);
        double th = min(dx0, dy0) * 0.05;
        if (fabs(c->xc - xc0) < th && fabs(c->yc - yc0) < th && 
            fabs(c->dx - dx0) < th && fabs(c->dy - dy0) < th) {
            c->iters++;
            break;
        }
    }
}

#define MEASURE(func) { \
    printf("\n" #func "\n"); \
    clock_t tm = clock(); \
    for (int i = 0; i < FRAMES; i++) func; \
    double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC; \
    printf("Elapsed: %.3fs, FPS: %.1f, %.2fms/frame\n", elapsed, FRAMES/elapsed, elapsed/(double)FRAMES*1000); \
}

#define ALLOC(var, type, sz) \
    type *var = (type*)malloc(sizeof(type)*sz); \
    if (!var) { \
        perror("Unable to allocate " # var); \
        exit(EXIT_FAILURE); \
    }

int main() {
    int w, h, offset;
    uint8_t *buf = read_pgm(FILENAME, &w, &h, &offset);
    if (!buf) {
        exit(EXIT_FAILURE);
    }

    ALLOC(subtracted, double, w*h);
    ALLOC(subtracted_no_noise, double, w*h);
    ALLOC(backup, pixel, w*h);
    memcpy(backup, buf+offset, sizeof(pixel)*w*h);

    BeamCalc c;
    c.w = w;
    c.h = h;
    c.buf = (pixel*)(buf+offset);
    c.corner_fraction = CORNER_FRACTION;
    c.nT = 3;
    c.iso_noise = 0; // True by default in laserbeamsize
    c.max_iter = 25;
    c.mask_diam = 3;
    c.subtracted = subtracted;
    c.subtracted_no_noise = subtracted_no_noise;

    printf("Frames: %d\n", FRAMES);

// #define measure_lbs_corner_background
// #define measure_cgn_calc_background
// #define measure_lbs_iso_background
// #define measure_lbs_subtract_iso_background
// #define measure_lbs_calc_beam
#define measure_cgn_calc_beam

#ifdef measure_lbs_corner_background
    MEASURE(lbs_corner_background(&c));
    printf("mean=%f sdev=%f\n", c.mean, c.sdev);
#endif
#ifdef measure_cgn_calc_background
    MEASURE(cgn_calc_background(&c));
    printf("mean=%f sdev=%f\n", c.mean, c.sdev);
#endif
#ifdef measure_lbs_iso_background
    MEASURE(lbs_iso_background(&c));
    printf("mean=%f sdev=%f\n", c.mean, c.sdev);
#endif
#ifdef measure_lbs_subtract_iso_background
    printf("\nlbs_subtract_iso_background\n");
    {
        double elapsed = 0;
        for (int i = 0; i < FRAMES; i++) {
            clock_t tm = clock();
            lbs_subtract_iso_background(&c);
            elapsed += (clock() - tm)/(double)CLOCKS_PER_SEC;
            c.x1 = 0, c.x2 = w, c.y1 = 0, c.y2 = h;
            lbs_calc_beam_basic(c.subtracted_no_noise, &c);
            //printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", c.xc, c.yc, c.dx, c.dy, c.phi * 180/3.1416);
            memcpy(c.buf, backup, sizeof(pixel)*w*h); // restore image outside of measurement
        }
        printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", c.xc, c.yc, c.dx, c.dy, c.phi * 180/3.1416);
        printf("Elapsed: %.3fs, FPS: %.1f, %.2fms/frame\n", elapsed, FRAMES/elapsed, elapsed/(double)FRAMES*1000); \
    }
#endif
#ifdef measure_lbs_calc_beam
    printf("\nlbs_calc_beam\n");
    {
        double elapsed = 0;
        for (int i = 0; i < FRAMES; i++) {
            clock_t tm = clock();
            lbs_calc_beam(&c);
            elapsed += (clock() - tm)/(double)CLOCKS_PER_SEC;
            //printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", c.xc, c.yc, c.dx, c.dy, c.phi * 180/3.1416);
            memcpy(c.buf, backup, sizeof(pixel)*w*h); // restore image outside of measurement
        }
        printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f, iters=%d\n", c.xc, c.yc, c.dx, c.dy, c.phi * 180/3.1416, c.iters);
        printf("Elapsed: %.3fs, FPS: %.1f, %.2fms/frame\n", elapsed, FRAMES/elapsed, elapsed/(double)FRAMES*1000); \
    }
#endif
#ifdef measure_cgn_calc_beam
    printf("\ncgn_calc_beam\n");
    {
        double elapsed = 0;
        for (int i = 0; i < FRAMES; i++) {
            clock_t tm = clock();
            cgn_calc_beam(&c);
            elapsed += (clock() - tm)/(double)CLOCKS_PER_SEC;
            //printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", c.xc, c.yc, c.dx, c.dy, c.phi * 180/3.1416);
            memcpy(c.buf, backup, sizeof(pixel)*w*h); // restore image outside of measurement
        }
        printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f, iters=%d, min=%.1f, max=%.1f, mean=%.2f, sdev=%.2f\n", 
            c.xc, c.yc, c.dx, c.dy, c.phi * 180/3.1416, c.iters, c.min, c.max, c.mean, c.sdev);
        printf("Elapsed: %.3fs, FPS: %.1f, %.2fms/frame\n", elapsed, FRAMES/elapsed, elapsed/(double)FRAMES*1000); \
    }
#endif
    free(buf);
    free(backup);
    free(subtracted);
    free(subtracted_no_noise);
    return EXIT_SUCCESS;
}
