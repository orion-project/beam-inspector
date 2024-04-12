/*

GCC 11.2x64 Intel i7-2600 CPU 3.40GHz

Image: ../../beams/beam_8b_ast.pgm
Image size: 2592x2048
Max gray: 255
Max value: 212
Data offset: 17

Image: ../../beams/beam_16b_ast.pgm
Image size: 2592x2048
Max gray: 65535
Max value: 54228
Data offset: 19

Frames: 30

*** BLAS methods give slightly better performance
*** but they seem to fight for CPU resources with other tasks.
*** So outside of synthetic tests, in the reap app with ui and plotting,
*** their performance drops significantly (8-15 -> 20-45 ms/frame)
*** while performance of "naive" methods stay almost the same

blas_8
center=[1534,981], diam=[1474,1120], angle=-11.8
Elapsed: 0.245s, FPS: 122.4, 8.2ms/frame

blas_16
center=[1534,981], diam=[1475,1120], angle=-11.8
Elapsed: 0.253s, FPS: 118.6, 8.4ms/frame

naive_8
center=[1534,981], diam=[1474,1120], angle=-11.8
Elapsed: 0.355s, FPS: 84.5, 11.8ms/frame

naive_16
center=[1534,981], diam=[1475,1120], angle=-11.8
Elapsed: 0.313s, FPS: 95.8, 10.4ms/frame

*** With background subtraction without iterating:

max_iter=0, precision=0.05, corner_fraction=0.035, nT=3.0, mask_diam=3.0

bkgnd_8
center=[1567,965], diam=[985,678], angle=-11.5
mean=3.61, sdev=1.26, min=0.00, max=208.39, iters=0
Elapsed: 0.605s, FPS: 49.6, 20.2ms/frame

bkgnd_16
center=[1567,965], diam=[985,678], angle=-11.5
mean=927.29, sdev=324.80, min=0.00, max=53555.71, iters=0
Elapsed: 0.651s, FPS: 46.1, 21.7ms/frame

*** For good quality test images having rather constant and black background,
*** without iterations it gives almost the same results but >1.5 times faster:

max_iter=25, precision=0.001, corner_fraction=0.035, nT=3.0, mask_diam=3.0

bkgnd_8
center=[1567,965], diam=[979,673], angle=-11.6
Elapsed: 1.146s, FPS: 26.2, 38.2ms/frame
mean=3.61, sdev=1.26, min=0.00, max=208.39, iters=2

bkgnd_16
center=[1567,965], diam=[979,673], angle=-11.6
Elapsed: 1.118s, FPS: 26.8, 37.3ms/frame
mean=927.29, sdev=324.80, min=0.00, max=53555.71, iters=2

*/
#include "beam_calc.h"

#include <stdlib.h>
#include <time.h>

#include "../../calc/pgm.h"

#define FRAMES 1
#define FILENAME_8 "../../beams/beam_8b_ast.pgm"
#define FILENAME_16 "../../beams/beam_16b_ast.pgm"

#define MEASURE(ident, func) { \
    printf("\n" ident "\n"); \
    clock_t tm = clock(); \
    for (int i = 0; i < FRAMES; i++) func; \
    double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC; \
    printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", r.xc, r.yc, r.dx, r.dy, r.phi); \
    printf("Elapsed: %.3fs, FPS: %.1f, %.1fms/frame\n", elapsed, FRAMES/elapsed, elapsed/(double)FRAMES*1000); \
}

int main() {
    int w, h, offset8;
    uint8_t *buf8 = read_pgm(FILENAME_8, &w, &h, &offset8);
    if (!buf8) {
        exit(EXIT_FAILURE);
    }
    uint8_t max8 = 0;
    for (int i = 0; i < w*h; i++)
        if (max8 < buf8[offset8+i]) {
            max8 = buf8[offset8+i];
        }
    printf("Max value: %d\n", max8);
    printf("Data offset: %d\n\n", offset8);

    int w16, h16, offset16;
    uint8_t *buf16 = read_pgm(FILENAME_16, &w16, &h16, &offset16);
    if (!buf16) {
        exit(EXIT_FAILURE);
    }
    if (w16 != w || h16 != h) {
        printf("Must be the same image for 8 bit and 16bit tests");
        exit(EXIT_FAILURE);
    }
    uint16_t max16 = 0;
    for (int i = 0; i < w*h; i++)
        if (max16 < ((uint16_t*)buf16)[offset16+i]) {
            max16 = ((uint16_t*)buf16)[offset16+i];
        }
    printf("Max value: %d\n", max16);
    printf("Data offset: %d\n\n", offset16);

    double *subtracted = (double*)malloc(sizeof(double)*w*h);
    if (!subtracted) {
        perror("Unable to allocate doubles");
        exit(EXIT_FAILURE);
    }

    printf("Frames: %d\n", FRAMES);
#ifdef USE_BLAS
    {
        CgnBeamCalcBlas c;
        c.w = w;
        c.h = h;
        if (cgn_calc_beam_blas_init(&c)) {
            cgn_calc_beam_blas_free(&c);
            exit(EXIT_FAILURE);
        }
        CgnBeamResultBlas r;
        MEASURE("blas_8", cgn_calc_beam_blas_u8((const uint8_t*)(buf8+offset8), &c, &r));
        MEASURE("blas_16", cgn_calc_beam_blas_u16((const uint16_t*)(buf16+offset16), &c, &r));
        cgn_calc_beam_blas_free(&c);
    }
#endif
    {
        CgnBeamResult r;
        r.x1 = 0, r.x2 = w;
        r.y1 = 0, r.y2 = h;

        CgnBeamCalc c;
        c.w = w;
        c.h = h;

        c.bits = 8;
        c.buf = buf8+offset8;
        MEASURE("naive_8", cgn_calc_beam_naive(&c, &r));

        c.bits = 16;
        c.buf = buf16+offset16;
        MEASURE("naive_16", cgn_calc_beam_naive(&c, &r));

        CgnBeamBkgnd b;
        //b.max_iter = 25;
        b.max_iter = 0;
        //b.precision = 0.001;
        b.precision = 0.05;
        b.corner_fraction = 0.035;
        b.nT = 3;
        b.mask_diam = 3;
        b.subtracted = subtracted;
        b.ax1 = 0;
        b.ay1 = 0;
        b.ax2 = w;
        b.ay2 = h;
        printf("\nmax_iter=%d, precision=%.3f, corner_fraction=%.3f, nT=%.1f, mask_diam=%.1f\n",
            b.max_iter, b.precision, b.corner_fraction, b.nT, b.mask_diam);

        c.bits = 8;
        c.buf = buf8+offset8;
        MEASURE("bkgnd_8", cgn_calc_beam_bkgnd(&c, &b, &r));
        printf("mean=%.2f, sdev=%.2f, min=%.2f, max=%.2f, iters=%d\n", b.mean, b.sdev, b.min, b.max, b.iters);

        c.bits = 16;
        c.buf = buf16+offset16;
        MEASURE("bkgnd_16", cgn_calc_beam_bkgnd(&c, &b, &r));
        printf("mean=%.2f, sdev=%.2f, min=%.2f, max=%.2f, iters=%d\n", b.mean, b.sdev, b.min, b.max, b.iters);
    }
    free(buf8);
    free(buf16);
    free(subtracted);
    return EXIT_SUCCESS;
}
