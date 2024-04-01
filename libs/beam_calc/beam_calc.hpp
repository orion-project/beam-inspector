#ifndef _CIGNUS_BEAM_CALC_H_
#define _CIGNUS_BEAM_CALC_H_

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

template <typename TBuf>
struct CgnBeamCalc {
    int w;
    int h;
    const TBuf *buf;
};

struct CgnBeamResult {
    double xc;
    double yc;
    double dx;
    double dy;
    double phi;
} ;

template <typename TBuf>
void cgn_calc_beam_naive(CgnBeamCalc<TBuf> *c, CgnBeamResult *r);

#ifdef __cplusplus
}
#endif

#endif // _CIGNUS_BEAM_CALC_H_