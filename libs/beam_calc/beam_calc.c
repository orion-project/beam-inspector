#include "beam_calc.h"

#include "cblas.h"

#include <math.h>
#include <stdlib.h>

#define sign(s) ((s) < 0 ? -1 : ((s) > 0 ? 1 : 0))
#define sqr(s) ((s)*(s))
#define max(a,b) ((a > b) ? a : b)

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

#define w c->w
#define h c->h
#define buf c->buf
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

int cgn_calc_beam_init(CgnBeamCalc *c) {
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

void cgn_calc_beam_free(CgnBeamCalc *c) {
    if (d) free(d);
    if (tx) free(tx);
    if (ty) free(ty);
    if (x) free(x);
    if (y) free(y);
    if (u) free(u);
    if (s) free(s);
    if (t) free(t);
}

void cgn_calc_beam(CgnBeamCalc *c, CgnBeamResult *r) {
    SCAL(w, 0, tx);
    SCAL(h, 0, ty);
    COPY(w, s, x);
    COPY(h, s, y);

    for (int i = 0; i < w*h; i++) {
        d[i] = buf[i];
    }

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
    phi = 0.5 * atan2(2 * xy, xx - yy);
}
