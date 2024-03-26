#ifndef _CIGNUS_BEAM_CALC_H_
#define _CIGNUS_BEAM_CALC_H_

#ifdef __cplusplus
extern "C" {
#endif

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
} CgnBeamCalc;

typedef struct {
    float xc;
    float yc;
    float xx;
    float yy;
    float xy;
    float dx;
    float dy;
    float phi;
} CgnBeamResult;

int cgn_calc_beam_init(CgnBeamCalc *c);
void cgn_calc_beam_free(CgnBeamCalc *c);
void cgn_calc_beam(CgnBeamCalc *c, CgnBeamResult *r);

#ifdef __cplusplus
}
#endif

#endif // _CIGNUS_BEAM_CALC_H_
