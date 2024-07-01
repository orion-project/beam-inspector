/*

Reference implementation the Approximation method for baseline offset correction (ISO 11146-3 [3.4.3])

--------------------------------------
GCC 11.2x64 Intel i7-2600 CPU 3.40GHz

Image: ../beams/beam_8b_ast.pgm
Image size: 2592x2048
Max gray: 255
Frames: 30

cgn_calc_beam
center=[1567,965], diam=[979,673], angle=-11.6, iters=1, min=0.0, max=208.4, mean=3.61, sdev=1.26
Elapsed: 0.943s, FPS: 31.8, 31.43ms/frame

*/

#include <math.h>
#include <time.h>
#include "../pgm.h"

#define FRAMES 30
#define FILENAME "../../beams/beam_8b_ast.pgm"
#define AX1 0
#define AX2 0
#define AY1 0
#define AY2 0

// #define FILENAME "../../tmp/real_beams/test_image_46_cropped.pgm"

// #define FILENAME "../../tmp/real_beams/test_image_46.pgm"
// #define AX1 902
// #define AX2 1348
// #define AY1 766
// #define AY2 1134

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
    int max_iter;
    double corner_fraction, nT, mask_diam;
    int ax1, ay1, ax2, ay2;

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

void calc_beam_basic(double *buf, BeamCalc *c) {
    const int w = c->w;
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

void cgn_subtract_background(BeamCalc *c) {
    const int w = c->w;
    const int x1 = c->ax1, x2 = c->ax2;
    const int y1 = c->ay1, y2 = c->ay2;
    const int dw = (x2 - x1) * c->corner_fraction;
    const int dh = (y2 - y1) * c->corner_fraction;
    const int bx1 = x1 + dw, bx2 = x2 - dw;
    const int by1 = y1 + dh, by2 = y2 - dh;
    const pixel *buf = c->buf;
    double *tmp = c->subtracted;

    // ISO 11146-3 [3.4.3]:
    // Determining the baseline offset by averaging a sample of N non-illuminated points
    // (for example arrays of n × m pixels in each of the four corners) within the image
    // directly gives the offset, which has to be subtracted from the measured distribution.
    int k = 0;
    double m = 0;
    for (int i = y1; i < y2; i++) {
        if (i < by1 || i >= by2) {
            const int offset = i*w;
            for (int j = x1; j < x2; j++) {
                if (j < bx1 || j >= bx2) {
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
    for (int i = y1; i < y2; i++) {
        const int offset = i*w;
        for (int j = x1; j < x2; j++) {
            const int k = offset + j;
            if (buf[k] > th) {
                tmp[k] = buf[k] - m;
            } else {
                tmp[k] = 0;
            }
            if (c->max < tmp[k]) {
                c->max = tmp[k];
            } else if (c->min > tmp[k]) {
                c->min = tmp[k];
            }
        }
    }
}

void cgn_calc_beam(BeamCalc *c) {
    cgn_subtract_background(c);

    // calc initial integration area
    c->x1 = c->ax1, c->x2 = c->ax2;
    c->y1 = c->ay1, c->y2 = c->ay2;
    calc_beam_basic(c->subtracted, c);
    //printf("Initial center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", c->xc, c->yc, c->dx, c->dy, c->phi * 180/3.1416);

    for (c->iters = 0; c->iters < c->max_iter; c->iters++) {
        double xc0 = c->xc, yc0 = c->yc, dx0 = c->dx, dy0 = c->dy;
        c->x1 = xc0 - dx0/2.0 * c->mask_diam; c->x1 = max(c->x1, c->ax1);
        c->x2 = xc0 + dx0/2.0 * c->mask_diam; c->x2 = min(c->x2, c->ax2);
        c->y1 = yc0 - dy0/2.0 * c->mask_diam; c->y1 = max(c->y1, c->ay1);
        c->y2 = yc0 + dy0/2.0 * c->mask_diam; c->y2 = min(c->y2, c->ay2);
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
    c.max_iter = 25;
    c.mask_diam = 3;
    c.subtracted = subtracted;
    c.subtracted_no_noise = subtracted_no_noise;
    c.ax1 = AX1;
    c.ax2 = AX2 > 0 ? AX2 : w; 
    c.ay1 = AY1;
    c.ay2 = AY2 > 0 ? AY2 : h;

    printf("Frames: %d\n", FRAMES);

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

    free(buf);
    free(backup);
    free(subtracted);
    free(subtracted_no_noise);
    return EXIT_SUCCESS;
}
