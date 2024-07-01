/*

Comparison of different naive implementation of centroid calculation.
The algorithm is in calc\.venv\Lib\site-packages\laserbeamsize\analysis.py

--------------------------------------
GCC 8.1x32 Intel i7-2600 CPU 3.40GHz

Image: ../beams/beam_8b_ast.pgm
Image size 2592x2048
Max gray 255
Frames: 30

calc_real
center=[1534,981], diam=[1474,1120], angle=-11.8
Elapsed: 0.564s, FPS: 53.2

calc_bytes
center=[1534,981], diam=[1474,1120], angle=-11.8
Elapsed: 0.668s, FPS: 44.9

calc_bytes_single_loop
center=[1533,981], diam=[1474,1120], angle=-11.8
Elapsed: 1.591s, FPS: 18.9

*/
#include <math.h>
#include <time.h>
#include "../pgm.h"

#define FRAMES 30
//#define FILENAME "../../beams/beam_8b.pgm"
#define FILENAME "../../beams/beam_8b_ast.pgm"
//#define FILENAME "../../beams/dot_8b.pgm"

// There is no notable difference between real types in this approach
typedef double real;
//typedef float real;

#define sign(s) ((s) < 0 ? -1 : ((s) > 0 ? 1 : 0))
#define sqr(s) ((s)*(s))

void calc_real(real* d, int w, int h)
{
    real p = 0.0;
    real xc = 0.0;
    real yc = 0.0;
    real pt;
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            p += d[i*w + j];
            xc += d[i*w + j] * j;
            yc += d[i*w + j] * i;
        }
    }
    xc /= p;
    yc /= p;

    real xx = 0.0;
    real yy = 0.0;
    real xy = 0.0;
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            xx += d[i*w + j] * sqr(j - xc);
            xy += d[i*w + j] * (j - xc)*(i - yc);
            yy += d[i*w + j] * sqr(i - yc);
        }
    }
    xx /= p;
    xy /= p;
    yy /= p;

    real t = sign(xx - yy) * sqrt(sqr(xx - yy) + 4*sqr(xy));
    real dx = 2.8284271247461903 * sqrt(xx + yy + t);
    real dy = 2.8284271247461903 * sqrt(xx + yy - t);
    real phi = 0.5 * atan2(2 * xy, xx - yy) * 180.0/3.1416;
    printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", xc, yc, dx, dy, phi);
}

void calc_bytes(uint8_t *d, int w, int h)
{
    real p = 1.0;
    real xc = 0.0;
    real yc = 0.0;
    real t;
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            t = d[i*w + j];
            p += t;
            xc += t * j;
            yc += t * i;
        }
    }
    xc /= p;
    yc /= p;

    real xx = 0.0;
    real yy = 0.0;
    real xy = 0.0;
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            t = d[i*w + j];
            xx += t * sqr(j - xc);
            xy += t * (j - xc)*(i - yc);
            yy += t * sqr(i - yc);
        }
    }
    xx /= p;
    xy /= p;
    yy /= p;

    real s = sign(xx - yy) * sqrt(sqr(xx - yy) + 4*sqr(xy));
    real dx = 2.8284271247461903 * sqrt(xx + yy + s);
    real dy = 2.8284271247461903 * sqrt(xx + yy - s);
    real phi = 0.5 * atan2(2 * xy, xx - yy) * 180.0/3.1416;
    printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", xc, yc, dx, dy, phi);
}

void calc_bytes_single_loop(uint8_t *d, int w, int h)
{
    int c = w * h;
    real p = 0.0;
    real xc = 0.0;
    real yc = 0.0;
    real t, i, j;
    for (int k = 0; k < c; k++) {
        t = d[k];
        i = k / w;
        j = k % w;
        p += t;
        xc += t * j;
        yc += t * i;
    }
    xc = trunc(xc / p);
    yc = trunc(yc / p);

    real xx = 0.0;
    real yy = 0.0;
    real xy = 0.0;
    for (int k = 0; k < c; k++) {
        t = d[k];
        i = k / w;
        j = k % w;
        xx += t * sqr(j - xc);
        xy += t * (j - xc)*(i - yc);
        yy += t * sqr(i - yc);
    }
    xx /= p;
    xy /= p;
    yy /= p;

    real s = sign(xx - yy) * sqrt(sqr(xx - yy) + 4*sqr(xy));
    real dx = 2.8284271247461903 * sqrt(xx + yy + s);
    real dy = 2.8284271247461903 * sqrt(xx + yy - s);
    real phi = 0.5 * atan2(2 * xy, xx - yy) * 180.0/3.1416;
    printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", xc, yc, dx, dy, phi);
}

void pixel_to_real_buf(uint8_t *b, real *d, int sz) {
    for (int i = 0; i < sz; i++) {
        d[i] = (real)b[i];
    }
}

int main() {
    int w, h, offset;
    uint8_t *buf = read_pgm(FILENAME, &w, &h, &offset);
    if (!buf) {
        exit(EXIT_FAILURE);
    }

    real *data = malloc(sizeof(real) * w * h);
    if (!data) {
        perror("Unable to allocate floats");
        free(buf);
        exit(EXIT_FAILURE);
    }

    pixel_to_real_buf(buf+offset, data, w*h);

    printf("Frames: %d\n", FRAMES);
    printf("\ncalc_real\n"); 
    {
        clock_t tm = clock();
        for (int i = 0; i < FRAMES; i++) {
            calc_real(data, w, h);
        }
        double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC;
        printf("Elapsed: %.3fs, FPS: %.1f\n", elapsed, FRAMES/elapsed);
    }
    printf("\ncalc_bytes\n");
    {
        clock_t tm = clock();
        for (int i = 0; i < FRAMES; i++) {
            calc_bytes(buf+offset, w, h);
        }
        double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC;
        printf("Elapsed: %.3fs, FPS: %.1f\n", elapsed, FRAMES/elapsed);
    }
    printf("\ncalc_bytes_single_loop\n");
    {
        clock_t tm = clock();
        for (int i = 0; i < FRAMES; i++) {
            calc_bytes_single_loop(buf+offset, w, h);
        }
        double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC;
        printf("Elapsed: %.3fs, FPS: %.1f\n", elapsed, FRAMES/elapsed);
    }
    printf("\npixel_to_real_buf\n");
    {
        clock_t tm = clock();
        for (int i = 0; i < FRAMES; i++) {
            pixel_to_real_buf(buf+offset, data, w*h);
        }
        double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC;
        printf("Elapsed: %.3fs, FPS: %.1f\n", elapsed, FRAMES/elapsed);
    }

    free(buf);
    free(data);
    return EXIT_SUCCESS;
}
