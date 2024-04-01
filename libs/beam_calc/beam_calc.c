#include "beam_calc.h"

#include <math.h>

#ifdef USE_BLAS
#include "cblas.h"

#include <stdlib.h>
#endif

#define sign(s) ((s) < 0 ? -1 : ((s) > 0 ? 1 : 0))
#define sqr(s) ((s)*(s))
#define max(a,b) ((a > b) ? a : b)

#define w c->w
#define h c->h
#define buf c->buf

#define cgn_calc_beam_naive                                        \
    double p = 0;                                                  \
    double xc = 0;                                                 \
    double yc = 0;                                                 \
    for (int i = 0; i < h; i++) {                                  \
        int offset = i*w;                                          \
        for (int j = 0; j < w; j++) {                              \
            p += buf[offset + j];                                  \
            xc += buf[offset + j] * j;                             \
            yc += buf[offset + j] * i;                             \
        }                                                          \
    }                                                              \
    xc /= p;                                                       \
    yc /= p;                                                       \
    double xx = 0, yy = 0, xy = 0;                                 \
    for (int i = 0; i < h; i++) {                                  \
        int offset = i*w;                                          \
        for (int j = 0; j < w; j++) {                              \
            xx += buf[offset + j] * sqr(j - xc);                   \
            xy += buf[offset + j] * (j - xc)*(i - yc);             \
            yy += buf[offset + j] * sqr(i - yc);                   \
        }                                                          \
    }                                                              \
    xx /= p;                                                       \
    xy /= p;                                                       \
    yy /= p;                                                       \
    double ss = sign(xx - yy) * sqrt(sqr(xx - yy) + 4*sqr(xy));    \
    r->dx = 2.8284271247461903 * sqrt(xx + yy + ss);               \
    r->dy = 2.8284271247461903 * sqrt(xx + yy - ss);               \
    r->phi = 0.5 * atan2(2 * xy, xx - yy) * 57.29577951308232;     \
    r->xc = xc;                                                    \
    r->yc = yc;                                                    \

void cgn_calc_beam_8_naive(CgnBeamCalc8 *c, CgnBeamResult *r) {
    cgn_calc_beam_naive
}

void cgn_calc_beam_16_naive(CgnBeamCalc16 *c, CgnBeamResult *r) {
    cgn_calc_beam_naive
}

#ifdef USE_BLAS

#define ALLOC(var, size) \
    var = (float*)malloc(sizeof(float) * size); \
    if (!var) { \
        perror("Unable to allocate " # var); \
        return 1; \
    }

#define SUM(m, v) cblas_ssum(m, v, 1)
#define SCAL(m, a, b) cblas_sscal(m, a, b, 1)
#define COPY(m, a, b) cblas_scopy(m, a, 1, b, 1)
#define GEMV(m, n, a, x, y)  cblas_sgemv(CblasRowMajor, CblasNoTrans, m, n, 1, a, n, x, 1, 0, y, 1)
#define GEMVT(m, n, a, x, y) cblas_sgemv(CblasRowMajor, CblasTrans,   m, n, 1, a, n, x, 1, 0, y, 1)
#define DOT(s, x, y) cblas_sdot(s, x, 1, y, 1)

#define d c->d
#define tx c->tx
#define ty c->ty
#define x c->x
#define y c->y
#define u c->u
#define s c->s
#define t c->t

#define xc r->xc
#define yc r->yc
#define xx r->xx
#define yy r->yy
#define xy r->xy
#define dx r->dx
#define dy r->dy
#define phi r->phi

int cgn_calc_beam_blas_init(CgnBeamCalcBlas *c) {
    d = NULL;
    tx = NULL;
    ty = NULL;
    x = NULL;
    y = NULL;
    u = NULL;
    s = NULL;
    t = NULL;

    ALLOC(d, w*h);
    ALLOC(tx, w);
    ALLOC(ty, h);
    ALLOC(x, w);
    ALLOC(y, h);
    ALLOC(u, max(w,h));
    ALLOC(s, max(w,h));
    ALLOC(t, max(w,h));

    for (int i = 0; i < max(w,h); i++) {
        u[i] = 1;
        s[i] = i;
    }

    return 0;
}

void cgn_calc_beam_blas_free(CgnBeamCalcBlas *c) {
    if (d) free(d);
    if (tx) free(tx);
    if (ty) free(ty);
    if (x) free(x);
    if (y) free(y);
    if (u) free(u);
    if (s) free(s);
    if (t) free(t);
}

void cgn_calc_beam_blas(CgnBeamCalcBlas *c, CgnBeamResultBlas *r) {
    SCAL(w, 0, tx);
    SCAL(h, 0, ty);
    COPY(w, s, x);
    COPY(h, s, y);

    GEMVT(h, w, d, u, tx);
    GEMV (h, w, d, u, ty);
    float p = SUM(w, tx);

    xc = DOT(w, tx, x) / p;
    yc = DOT(h, ty, y) / p;

    for (int i = 0; i < w; i++) x[i] -= xc;
    for (int i = 0; i < h; i++) y[i] -= yc;

    GEMV(h, w, d, x, t);
    xy = DOT(h, t, y) / p;

    for (int i = 0; i < w; i++) x[i] *= x[i];
    for (int i = 0; i < h; i++) y[i] *= y[i];
    xx = DOT(w, tx, x) / p;
    yy = DOT(h, ty, y) / p;

    float ss = sign(xx - yy) * sqrt(sqr(xx - yy) + 4*sqr(xy));
    dx = 2.8284271247461903 * sqrt(xx + yy + ss);
    dy = 2.8284271247461903 * sqrt(xx + yy - ss);
    phi = 0.5 * atan2(2 * xy, xx - yy) * 57.29577951308232;
}

void cgn_calc_beam_blas_8(uint8_t *b, CgnBeamCalcBlas *c, CgnBeamResultBlas *r) {
    for (int i = 0; i < w*h; i++) {
        d[i] = b[i];
    }
    cgn_calc_beam_blas(c, r);
}

void cgn_calc_beam_blas_16(uint16_t *b, CgnBeamCalcBlas *c, CgnBeamResultBlas *r) {
    for (int i = 0; i < w*h; i++) {
        d[i] = b[i];
    }
    cgn_calc_beam_blas(c, r);
}

#endif // USE_BLAS
