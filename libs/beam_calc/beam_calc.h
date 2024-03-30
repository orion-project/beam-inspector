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
    unsigned char *buf;
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

#endif // USE_BLAS

typedef struct {
    int w;
    int h;
    const uint8_t *buf;
} CgnBeamCalc8;

typedef struct {
    int w;
    int h;
    const uint16_t *buf;
} CgnBeamCalc16;

typedef struct {
    double xc;
    double yc;
    double dx;
    double dy;
    double phi;
} CgnBeamResult;

void cgn_calc_beam_8_naive(CgnBeamCalc8 *c, CgnBeamResult *r);
void cgn_calc_beam_16_naive(CgnBeamCalc16 *c, CgnBeamResult *r);

#ifdef __cplusplus
}
#endif

#endif // _CIGNUS_BEAM_CALC_H_
