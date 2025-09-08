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
    int bpp;
    uint8_t *buf;
} CgnBeamCalc;

typedef struct {
    // Aperture bounds inside that calculations should be carried out.
    int ax1, ay1, ax2, ay2;

    // The maximum number of iterations that should be done before giving up.
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

    // Pixel values with background subtracted.
    // When subtract_bkgnd_v=1, only pixes inside aperture bonds have valid values.
    double *subtracted;

    // The latest calculation aperture inside that the beam diameter has been calculated with required precision.
    // Can not be larger and clamped to initial aperture bounds.
    int x1, x2, y1, y2;

    // The iteration number at which the required precision has been achieved.
    int iters;

    // Background and noise values.
    double mean, sdev;

    // Min and max pixel values after backgdound subtracted.
    double min, max;

    // Number of pixels about the noise threshold
    int count;

    // Version on the subtract_bkgnd function
    int subtract_bkgnd_v;
} CgnBeamBkgnd;

typedef struct {
    int x1, x2;
    int y1, y2;
    int nan;
    double p;
    double xc; // Beam center X
    double yc; // Beam center Y
    double xx;
    double yy;
    double xy;
    double dx; // Beam width along principal axis X
    double dy; // Beam width along principal axis Y
    double phi; // CCW angle between principal axis X and the horizont, in degrees
} CgnBeamResult;

typedef struct {
    int w;
    int h;
    double *data;
} CgnBeamImage;

typedef struct {
    int cnt;
    double w; // Profile width in times of beam width
    double *x_r; // Radial distance along principal axis X
    double *x_p; // Profile value along principal axis X
    double *y_r; // Radial distance along principal axis Y
    double *y_p; // Profile value along principal axis Y
} CgnBeamProfiles;

void cgn_calc_beam_naive(const CgnBeamCalc *c, CgnBeamResult *r);
void cgn_calc_beam_bkgnd(const CgnBeamCalc *c, CgnBeamBkgnd *b, CgnBeamResult *r);
void cgn_copy_to_f64(const CgnBeamCalc *c, double *dst, double *max);
void cgn_normalize_f64(double *buf, int sz, double min, double max);
void cgn_copy_normalized_f64(double *src, double *dst, int sz, double min, double max);
double cgn_calc_brightness(const CgnBeamCalc *c);
double cgn_calc_brightness_1(const CgnBeamCalc *c);
double cgn_calc_brightness_2(const CgnBeamCalc *c, int xc, int yc);
void cgn_convert_10_to_u16(uint8_t *dst, uint8_t *src, int sz);
void cgn_convert_12_to_u16(uint8_t *dst, uint8_t *src, int sz);
void cgn_convert_10g40_to_u16(uint8_t *dst, uint8_t *src, int sz);
void cgn_convert_12g24_to_u16(uint8_t *dst, uint8_t *src, int sz);
void cgn_ext_copy_to_f64(const CgnBeamCalc *c, CgnBeamBkgnd *b, double *dst, int normalize, int full_z, double *min_z, double *max_z);
double cgn_calc_overexposure(const CgnBeamCalc *c, double th);
void cgn_calc_profiles(const CgnBeamImage *img, const CgnBeamResult *res, CgnBeamProfiles *prf);

#ifdef __cplusplus
}
#endif

#endif // _CIGNUS_BEAM_CALC_H_
