/*

Comparison of single and double precision gemm implementaion of centroid calculation.

Also there is measurement of effect of copying data to an external plottings buffer.

GCC 8.1x32 Intel i7-2600 CPU 3.40GHz
--------------------------------------
FLOAT
FPS: 72.6 avg

Image: ../../beams/beam_8b_ast.pgm
Image size: 2592x2048
Max gray: 255
Frames: 30
center=[1534,981], diam=[1474,1120], angle=-11.8
Elapsed: 0.350s, FPS: 85.7
Elapsed: 0.519s, FPS: 57.8
Elapsed: 0.496s, FPS: 60.5
Elapsed: 0.509s, FPS: 58.9
Elapsed: 0.486s, FPS: 61.7
Elapsed: 0.469s, FPS: 64.0
Elapsed: 0.328s, FPS: 91.5
Elapsed: 0.334s, FPS: 89.8
Elapsed: 0.439s, FPS: 68.3
Elapsed: 0.342s, FPS: 87.7

--------------------------------------
DOUBLE
FPS: 48.2 avg (66% of floats)

Image: ../../beams/beam_8b_ast.pgm
Image size: 2592x2048
Max gray: 255
Frames: 30
center=[1534,981], diam=[1474,1120], angle=-11.8
Elapsed: 0.658s, FPS: 45.6
Elapsed: 0.524s, FPS: 57.3
Elapsed: 0.550s, FPS: 54.5
Elapsed: 0.643s, FPS: 46.7
Elapsed: 0.629s, FPS: 47.7
Elapsed: 0.690s, FPS: 43.5
Elapsed: 0.678s, FPS: 44.2
Elapsed: 0.696s, FPS: 43.1
Elapsed: 0.678s, FPS: 44.2
Elapsed: 0.629s, FPS: 47.7
Elapsed: 0.561s, FPS: 53.5

--------------------------------------
FLOAT with population of additional double buffer for plotting
FPS: 48.6 avg (66% of without aux buffer)

Image: ../../beams/beam_8b_ast.pgm
Image size: 2592x2048
Max gray: 255
Frames: 30
center=[1534,981], diam=[1474,1120], angle=-11.8
Elapsed: 0.679s, FPS: 44.2
Elapsed: 0.642s, FPS: 46.7
Elapsed: 0.661s, FPS: 45.4
Elapsed: 0.526s, FPS: 57.0
Elapsed: 0.534s, FPS: 56.2
Elapsed: 0.520s, FPS: 57.7
Elapsed: 0.686s, FPS: 43.7
Elapsed: 0.678s, FPS: 44.2
Elapsed: 0.657s, FPS: 45.7
Elapsed: 0.668s, FPS: 44.9

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
//#define DOUBLE
//#define AUX_BUF

#ifdef DOUBLE
typedef double real;
#else
typedef float real;
#endif
typedef uint8_t pixel;

#define sign(s) ((s) < 0 ? -1 : ((s) > 0 ? 1 : 0))
#define sqr(s) ((s)*(s))
#define max(a,b) ((a > b) ? a : b)

#ifdef DOUBLE
#define SCAL(m, a, b) cblas_dscal(m, a, b, 1)
#define COPY(m, a, b) cblas_dcopy(m, a, 1, b, 1)
#define SUM(m, v) cblas_dsum(m, v, 1)
#define GEMM(m, n, k, a, lda, b, ldb, c, ldc) \
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, m, n, k, 1, a, lda, b, ldb, 0, c, ldc)
#define GEMV(m, n, a, x, y)  cblas_dgemv(CblasRowMajor, CblasNoTrans, m, n, 1, a, n, x, 1, 0, y, 1)
#define GEMVT(m, n, a, x, y) cblas_dgemv(CblasRowMajor, CblasTrans,   m, n, 1, a, n, x, 1, 0, y, 1)
#define DOT(s, x, y) cblas_ddot(s, x, 1, y, 1)
#else
#define SUM(m, v) cblas_ssum(m, v, 1)
#define SCAL(m, a, b) cblas_sscal(m, a, b, 1)
#define COPY(m, a, b) cblas_scopy(m, a, 1, b, 1)
#define GEMM(m, n, k, a, lda, b, ldb, c, ldc) \
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, m, n, k, 1, a, lda, b, ldb, 0, c, ldc)
#define GEMV(m, n, a, x, y)  cblas_sgemv(CblasRowMajor, CblasNoTrans, m, n, 1, a, n, x, 1, 0, y, 1)
#define GEMVT(m, n, a, x, y) cblas_sgemv(CblasRowMajor, CblasTrans,   m, n, 1, a, n, x, 1, 0, y, 1)
#define DOT(s, x, y) cblas_sdot(s, x, 1, y, 1)
#endif

struct Result
{
    real xc;
    real yc;
    real dx;
    real dy;
    real phi;
};

struct Calc {
    int w = 0;
    int h = 0;
    pixel *p = NULL;
    real *d = NULL;
    double *dd = NULL;
    real *tx = NULL;
    real *ty = NULL;
    real *x = NULL;
    real *y = NULL;
    real *u = NULL;
    real *s = NULL;
    real *t = NULL;

    void calc(Result *r);
};

void Calc::calc(Result *r)
{
    SCAL(w, 0, tx);
    SCAL(h, 0, ty);
    COPY(w, s, x);
    COPY(h, s, y);

    for (int i = 0; i < w*h; i++) {
        d[i] = p[i];

    #ifdef AUX_BUF
        // Emulate copying frame into plotting buffer
        dd[i] = p[i];
    #endif
    }

    GEMVT(h, w, d, u, tx);
    GEMV (h, w, d, u, ty);
    real p = SUM(w, tx);

    r->xc = DOT(w, tx, x) / p;
    r->yc = DOT(h, ty, y) / p;

    for (int i = 0; i < w; i++) x[i] -= r->xc;
    for (int i = 0; i < h; i++) y[i] -= r->yc;

    GEMV(h, w, d, x, t);
    real xy = DOT(h, t, y) / p;

    for (int i = 0; i < w; i++) x[i] *= x[i];
    for (int i = 0; i < h; i++) y[i] *= y[i];
    real xx = DOT(w, tx, x) / p;
    real yy = DOT(h, ty, y) / p;

    real ss = sign(xx - yy) * sqrt(sqr(xx - yy) + 4*sqr(xy));
    r->dx = 2.8284271247461903 * sqrt(xx + yy + ss);
    r->dy = 2.8284271247461903 * sqrt(xx + yy - ss);
    r->phi = 0.5 * atan2(2 * xy, xx - yy) * 180.0/3.1416;
}

#define ALLOC(var, typ, size) \
    var = (typ*)malloc(sizeof(typ) * size); \
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

    Calc c;
    c.w = w;
    c.h = h;
    c.p = (pixel*)(buf+offset);
    ALLOC(c.d, real, w * h);
    ALLOC(c.dd, double, w * h);
    ALLOC(c.tx, real, w);
    ALLOC(c.ty, real, h);
    ALLOC(c.x, real, w);
    ALLOC(c.y, real, h);
    ALLOC(c.u, real, max(w,h));
    ALLOC(c.s, real, max(w,h));
    ALLOC(c.t, real, max(w,h));
    for (int i = 0; i < max(w,h); i++) c.u[i] = 1, c.s[i] = i;

    printf("Frames: %d\n", FRAMES);
    clock_t tm = clock();
    Result r;
    for (int i = 0; i < FRAMES; i++) {
        c.calc(&r);
    }
    double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC;
    printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", r.xc,  r.yc, r.dx, r.dy, r.phi);
    printf("Elapsed: %.3fs, FPS: %.1f\n", elapsed, FRAMES/elapsed);

    free(buf);
    free(c.d);
    free(c.dd);
    free(c.tx);
    free(c.ty);
    free(c.x);
    free(c.y);
    free(c.u);
    free(c.s);
    free(c.t);
    return EXIT_SUCCESS;
}
