/*

Comparison between naive and gemm implementation of centroid calculation.

--------------------------------------
GCC 8.1x32 Intel i7-2600 CPU 3.40GHz

Image: ../../beams/beam_8b_ast.pgm
Image size 2592x2048
Max gray 255
Frames: 30

calc_naive
center=[1534,981], diam=[1474,1120], angle=-11.8
Elapsed: 0.663s, FPS: 45.2

calc_gemm
center=[1534,981], diam=[1474,1120], angle=-11.8
Elapsed: 0.496s, FPS: 60.5

calc_semi_gemm
center=[1534,981], diam=[1474,1120], angle=-11.8
Elapsed: 0.578s, FPS: 51.9

*/
#include <math.h>
#include <time.h>
#include "../pgm.h"
#include "cblas.h"

//#define FILENAME "../../beams/beam_8b.pgm"
#define FILENAME "../../beams/beam_8b_ast.pgm"
//#define FILENAME "../../beams/dot_8b.pgm"
//#define FILENAME "../../beams/dot_8b_ast.pgm"

#define FRAMES 30
#define PRINT

typedef float real;

#define sign(s) ((s) < 0 ? -1 : ((s) > 0 ? 1 : 0))
#define sqr(s) ((s)*(s))
#define max(a,b) ((a > b) ? a : b)

void calc_naive(uint8_t *buf, real *data, int w, int h)
{
    for (int i = 0; i < w*h; i++) {
        data[i] = buf[i];
    }
    real *d = data;

    real p = 1.0;
    real xc = 0.0;
    real yc = 0.0;
    int r;
    for (int i = 0; i < h; i++) {
        r = i * w;
        for (int j = 0; j < w; j++) {
            p += d[r + j];
            xc += d[r + j] * j;
            yc += d[r + j] * i;
        }
    }
    xc /= p;
    yc /= p;

    real xx = 0.0;
    real yy = 0.0;
    real xy = 0.0;
    for (int i = 0; i < h; i++) {
        r = i * w;
        for (int j = 0; j < w; j++) {
            xx += d[r + j] * sqr(j - xc);
            xy += d[r + j] * (j - xc)*(i - yc);
            yy += d[r + j] * sqr(i - yc);
        }
    }
    xx /= p;
    xy /= p;
    yy /= p;

    real ss = sign(xx - yy) * sqrt(sqr(xx - yy) + 4*sqr(xy));
    real dx = 2.8284271247461903 * sqrt(xx + yy + ss);
    real dy = 2.8284271247461903 * sqrt(xx + yy - ss);
    real phi = 0.5 * atan2(2 * xy, xx - yy) * 180.0/3.1416;
#ifdef PRINT
    printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", xc, yc, dx, dy, phi);
#endif
}

// https://developer.apple.com/documentation/accelerate/1513264-cblas_sgemm?changes=_8&language=objc
#define GEMM(m, n, k, a, lda, b, ldb, c, ldc) \
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, m, n, k, 1, a, lda, b, ldb, 0, c, ldc)

// https://developer.apple.com/documentation/accelerate/1513065-cblas_sgemv?changes=_8&language=objc
#define GEMV(m, n, a, x, y)  cblas_sgemv(CblasRowMajor, CblasNoTrans, m, n, 1, a, n, x, 1, 0, y, 1)
#define GEMVT(m, n, a, x, y) cblas_sgemv(CblasRowMajor, CblasTrans,   m, n, 1, a, n, x, 1, 0, y, 1)

// https://developer.apple.com/documentation/accelerate/1513280-cblas_sdot?changes=_8&language=objc
#define DOT(s, x, y) cblas_sdot(s, x, 1, y, 1)

void print_buf(uint8_t *buf, int w, int h)
{
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            printf("%d\t", buf[i*w+j]);
        }
        printf("\n");
    }
}

void calc_gemm(uint8_t *buf, real *d, real *tx, real *ty, real *x, real *y, real *u, real *s, real *t, int w, int h)
{
    cblas_sscal(w, 0, tx, 1);
    cblas_sscal(h, 0, ty, 1);
    cblas_scopy(w, s, 1, x, 1);
    cblas_scopy(h, s, 1, y, 1);

    for (int i = 0; i < w*h; i++) {
        d[i] = buf[i];
    }

    GEMVT(h, w, d, u, tx);
    GEMV (h, w, d, u, ty);
    // for (int i = 0; i < w; i++) printf("%d: imx=%f\n", i, tx[i]);
    // for (int i = 0; i < h; i++) printf("%d: imy=%f\n", i, ty[i]);

    real p = cblas_ssum(w, tx, 1);
    //printf("p=%f\n", p);

    real xc = DOT(w, tx, x) / p;
    real yc = DOT(h, ty, y) / p;

    for (int i = 0; i < w; i++) x[i] -= xc;
    for (int i = 0; i < h; i++) y[i] -= yc;

    GEMV(h, w, d, x, t);
    real xy = DOT(h, t, y) / p;

    for (int i = 0; i < w; i++) x[i] *= x[i];
    for (int i = 0; i < h; i++) y[i] *= y[i];
    real xx = DOT(w, tx, x) / p;
    real yy = DOT(h, ty, y) / p;

    //printf("xx=%f, yy=%f, xy=%f\n", xx, yy, xy);

    // compute major and minor axes as well as rotation angle
    real ss = sign(xx - yy) * sqrt(sqr(xx - yy) + 4*sqr(xy));
    real dx = 2.8284271247461903 * sqrt(xx + yy + ss);
    real dy = 2.8284271247461903 * sqrt(xx + yy - ss);
    real phi = 0.5 * atan2(2 * xy, xx - yy) * 180.0/3.1416;
#ifdef PRINT
    printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", xc, yc, dx, dy, phi);
#endif
}

void calc_semi_gemm(uint8_t *buf, real *d, real *tx, real *ty, real *x, real *y, real *s, real *t, int w, int h)
{
    cblas_sscal(w, 0, tx, 1);
    cblas_sscal(h, 0, ty, 1);
    cblas_scopy(w, s, 1, x, 1);
    cblas_scopy(h, s, 1, y, 1);

    real p = 0;
    int r;
    for (int i = 0; i < h; i++) {
        r = i*w;
        for (int j = 0; j < w; j++) {
            d[r + j] = buf[r + j];
            p += d[r + j];
            ty[i] += d[r + j];
            tx[j] += d[r + j];
        }
    }
    //for (int i = 0; i < w; i++) printf("%d: imx=%f\n", i, tx[i]);
    //for (int i = 0; i < h; i++) printf("%d: imy=%f\n", i, ty[i]);
    //printf("p=%f\n", p);

    real xc = DOT(w, tx, x) / p;
    real yc = DOT(h, ty, y) / p;

    for (int i = 0; i < w; i++) x[i] -= xc;
    for (int i = 0; i < h; i++) y[i] -= yc;

    GEMV(h, w, d, x, t);
    real xy = DOT(h, t, y) / p;

    for (int i = 0; i < w; i++) x[i] *= x[i];
    for (int i = 0; i < h; i++) y[i] *= y[i];
    real xx = DOT(w, tx, x) / p;
    real yy = DOT(h, ty, y) / p;

    //printf("xx=%f, yy=%f, xy=%f\n", xx, yy, xy);

    real ss = sign(xx - yy) * sqrt(sqr(xx - yy) + 4*sqr(xy));
    real dx = 2.8284271247461903 * sqrt(xx + yy + ss);
    real dy = 2.8284271247461903 * sqrt(xx + yy - ss);
    real phi = 0.5 * atan2(2 * xy, xx - yy) * 180.0/3.1416;
#ifdef PRINT
    printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", xc, yc, dx, dy, phi);
#endif
}

#define ALLOC(var, typ, size) \
    typ *var = malloc(sizeof(typ) * size); \
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

    ALLOC(data, real, w * h);
    ALLOC(tx, real, w);
    ALLOC(ty, real, h);
    ALLOC(x, real, w);
    ALLOC(y, real, h);
    ALLOC(u, real, max(w,h));
    ALLOC(s, real, max(w,h));
    ALLOC(t, real, max(w,h));
    for (int i = 0; i < max(w,h); i++) u[i] = 1, s[i] = i;

    printf("Frames: %d\n", FRAMES);
    printf("\ncalc_naive\n");
    {
        clock_t tm = clock();
        for (int i = 0; i < FRAMES; i++) {
            calc_naive(buf+offset, data, w, h);
        }
        double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC;
        printf("Elapsed: %.3fs, FPS: %.1f\n", elapsed, FRAMES/elapsed);
    }
    printf("\ncalc_gemm\n");
    {
        clock_t tm = clock();
        for (int i = 0; i < FRAMES; i++) {
            calc_gemm(buf+offset, data, tx, ty, x, y, u, s, t, w, h);
        }
        double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC;
        printf("Elapsed: %.3fs, FPS: %.1f\n", elapsed, FRAMES/elapsed);
    }
    printf("\ncalc_semi_gemm\n");
    {
        clock_t tm = clock();
        for (int i = 0; i < FRAMES; i++) {
            calc_semi_gemm(buf+offset, data, tx, ty, x, y, s, t, w, h);
        }
        double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC;
        printf("Elapsed: %.3fs, FPS: %.1f\n", elapsed, FRAMES/elapsed);
    }

    free(buf);
    free(data);
    free(tx);
    free(ty);
    free(x);
    free(y);
    free(u);
    free(s);
    free(t);
    return EXIT_SUCCESS;
}
