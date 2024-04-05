#ifndef _CIGNUS_BEAM_CALC_H_
#define _CIGNUS_BEAM_CALC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_BLAS

typedef struct {
    int w;
    int h;
    float *d;
    float *tx;
    float *ty;
    float *x;
    float *y;
    float *u;
    float *s;
    float *t;
} CgnBeamCalcBlas;

typedef struct {
    float xc;
    float yc;
    float xx;
    float yy;
    float xy;
    float dx;
    float dy;
    float phi;
} CgnBeamResultBlas;

int cgn_calc_beam_blas_init(CgnBeamCalcBlas *c);
void cgn_calc_beam_blas_free(CgnBeamCalcBlas *c);
void cgn_calc_beam_blas(CgnBeamCalcBlas *c, CgnBeamResultBlas *r);
void cgn_calc_beam_blas_u8(const uint8_t *b, CgnBeamCalcBlas *c, CgnBeamResultBlas *r);
void cgn_calc_beam_blas_u16(const uint16_t *b, CgnBeamCalcBlas *c, CgnBeamResultBlas *r);

#endif // USE_BLAS


typedef struct {
    int w;
    int h;
    int bits;
    uint8_t *buf;
} CgnBeamCalc;

typedef struct {
    // The maximum number of iterations done before giving up.
    int max_iter;

    // Min relative difference in beam diameter between interations
    // that is enough for stopping iterating.
    double precision;

    // Determines the size of the corners.
    // ISO 11146-3 recommends values from 2-5%.
    double corner_fraction;

    // Accounts for noise in the background.
    // The background is estimated using the values in the corners of the image as `mean+nT * sdev`.
    // ISO 11146 states that `2 < nT < 4`
    double nT;

    // The size of the rectangular mask in diameters of the ellipse.
    // ISO 11146 states that it should be 3.
    double mask_diam;

    double *subtracted;
    int x1, x2, y1, y2, iters;

    double mean, sdev;
    double min, max;
} CgnBeamBkgnd;

typedef struct {
    int x1, x2;
    int y1, y2;
    double xc;
    double yc;
    double dx;
    double dy;
    double phi;
} CgnBeamResult;

void cgn_calc_beam_naive(const CgnBeamCalc *c, CgnBeamResult *r);
void cgn_calc_beam_bkgnd(const CgnBeamCalc *c, CgnBeamBkgnd *b, CgnBeamResult *r);
void cgn_copy_to_f64(const CgnBeamCalc *c, double *tgt, double *max);

#ifdef __cplusplus
}
#endif

#endif // _CIGNUS_BEAM_CALC_H_
