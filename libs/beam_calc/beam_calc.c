#include "beam_calc.h"

#include <math.h>
#include <string.h>

#ifdef USE_BLAS
#include "cblas.h"
#endif

#define sign(s) ((s) < 0 ? -1 : ((s) > 0 ? 1 : 0))
#define sqr(s) ((s)*(s))
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

#define _cgn_calc_beam                                             \
    double p = 0;                                                  \
    double xc = 0;                                                 \
    double yc = 0;                                                 \
    for (int i = r->y1; i < r->y2; i++) {                          \
        const int offset = i * c->w;                               \
        for (int j = r->x1; j < r->x2; j++) {                      \
            p += buf[offset + j];                                  \
            xc += buf[offset + j] * j;                             \
            yc += buf[offset + j] * i;                             \
        }                                                          \
    }                                                              \
    xc /= p;                                                       \
    yc /= p;                                                       \
    double xx = 0;                                                 \
    double yy = 0;                                                 \
    double xy = 0;                                                 \
    for (int i = r->y1; i < r->y2; i++) {                          \
        const int offset = i * c->w;                               \
        for (int j = r->x1; j < r->x2; j++) {                      \
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
    r->phi = 0.5 * atan(2 * xy / (xx - yy)) * 57.29577951308232;   \
    r->xc = xc;                                                    \
    r->yc = yc;                                                    \
    r->xx = xx;                                                    \
    r->yy = yy;                                                    \
    r->xy = xy;                                                    \
    r->p = p;

void cgn_calc_beam_u8(const uint8_t *buf, const CgnBeamCalc *c, CgnBeamResult *r) {
    _cgn_calc_beam
}

void cgn_calc_beam_u16(const uint16_t *buf, const CgnBeamCalc *c, CgnBeamResult *r) {
    _cgn_calc_beam
}

void cgn_calc_beam_f64(const double *buf, const CgnBeamCalc *c, CgnBeamResult *r) {
    _cgn_calc_beam
}

void cgn_calc_beam_naive(const CgnBeamCalc *c, CgnBeamResult *r) {
    if (c->bpp > 8) {
        cgn_calc_beam_u16((const uint16_t*)(c->buf), c, r);
    } else {
        cgn_calc_beam_u8((const uint8_t*)(c->buf), c, r);
    }
}

#define _cgn_subtract_bkgnd_v0                          \
    const int w = c->w;                                 \
    const int h = c->h;                                 \
    const int x1 = b->ax1, x2 = b->ax2;                 \
    const int y1 = b->ay1, y2 = b->ay2;                 \
    const int dw = (x2 - x1) * b->corner_fraction;      \
    const int dh = (y2 - y1) * b->corner_fraction;      \
    const int bx1 = x1 + dw, bx2 = x2 - dw;             \
    const int by1 = y1 + dh, by2 = y2 - dh;             \
    double *t = b->subtracted;                          \
                                                        \
    int k = 0;                                          \
    double m = 0;                                       \
    for (int i = y1; i < y2; i++) {                     \
        if (i < by1 || i >= by2) {                      \
            const int offset = i*w;                     \
            for (int j = x1; j < x2; j++) {             \
                if (j < bx1 || j >= bx2) {              \
                    t[k] = buf[offset + j];             \
                    m += t[k];                          \
                    k++;                                \
                }                                       \
            }                                           \
        }                                               \
    }                                                   \
    m /= (double)k;                                     \
                                                        \
    double s = 0;                                       \
    for (int i = 0; i < k; i++) {                       \
        s += sqr(t[i] - m);                             \
        t[i] = 0;                                       \
    }                                                   \
    s = sqrt(s / (double)k);                            \
                                                        \
    b->mean = m;                                        \
    b->sdev = s;                                        \
                                                        \
    const double th = m + b->nT * s;                    \
    b->min = 1e10;                                      \
    b->max = -1e10;                                     \
    b->count = 0;                                       \
    for (int i = 0; i < y1; i++) {                      \
        const int offset = i*w;                         \
        for (int j = 0; j < w; j++) {                   \
            t[offset + j] = buf[offset + j];            \
        }                                               \
    }                                                   \
    for (int i = y1; i < y2; i++) {                     \
        const int offset = i*w;                         \
        for (int j = 0; j < x1; j++) {                  \
            t[offset + j] = buf[offset + j];            \
        }                                               \
        for (int j = x2; j < w; j++) {                  \
            t[offset + j] = buf[offset + j];            \
        }                                               \
    }                                                   \
    for (int i = y2; i < h; i++) {                      \
        const int offset = i*w;                         \
        for (int j = 0; j < w; j++) {                   \
            t[offset + j] = buf[offset + j];            \
        }                                               \
    }                                                   \
    for (int i = y1; i < y2; i++) {                     \
        const int offset = i*w;                         \
        for (int j = x1; j < x2; j++) {                 \
            const int k = offset + j;                   \
            if (buf[k] > th) {                          \
                b->count++;                             \
                t[k] = buf[k] - m;                      \
            } else t[k] = 0;                            \
            if (t[k] > b->max) b->max = t[k];           \
            else if (t[k] < b->min) b->min = t[k];      \
        }                                               \
    }                                                   \

// unlinke v0, v1
// - doesn't use tmp buf when calc corners mean and sdev
// - doesn't fill target buf outside of aperture
//   (it should be filled manually before the call)
// - doesn't reset min and max before finding them
//   (reset them manually and after several calls with
//   different rois then will contain the global min and max)
//
#define _cgn_subtract_bkgnd_v1                          \
    const int w = c->w;                                 \
    const int h = c->h;                                 \
    const int x1 = b->ax1, x2 = b->ax2;                 \
    const int y1 = b->ay1, y2 = b->ay2;                 \
    const int dw = (x2 - x1) * b->corner_fraction;      \
    const int dh = (y2 - y1) * b->corner_fraction;      \
    const int bx1 = x1 + dw, bx2 = x2 - dw;             \
    const int by1 = y1 + dh, by2 = y2 - dh;             \
                                                        \
    double m = 0;                                       \
    for (int i = y1; i < y2; i++) {                     \
        if (i < by1 || i >= by2) {                      \
            const int offset = i*w;                     \
            for (int j = x1; j < x2; j++) {             \
                if (j < bx1 || j >= bx2) {              \
                    m += buf[offset + j];               \
                }                                       \
            }                                           \
        }                                               \
    }                                                   \
    m /= (double)(dw*dh*4);                             \
                                                        \
    double s = 0;                                       \
    for (int i = y1; i < y2; i++) {                     \
        if (i < by1 || i >= by2) {                      \
            const int offset = i*w;                     \
            for (int j = x1; j < x2; j++) {             \
                if (j < bx1 || j >= bx2) {              \
                    s += sqr(buf[offset + j] - m);      \
                }                                       \
            }                                           \
        }                                               \
    }                                                   \
    s = sqrt(s / (double)(dw*dh*4));                    \
                                                        \
    b->mean = m;                                        \
    b->sdev = s;                                        \
                                                        \
    const double th = m + b->nT * s;                    \
    b->count = 0;                                       \
    double *t = b->subtracted;                          \
    for (int i = y1; i < y2; i++) {                     \
        const int offset = i*w;                         \
        for (int j = x1; j < x2; j++) {                 \
            const int k = offset + j;                   \
            if (buf[k] > th) {                          \
                b->count++;                             \
                t[k] = buf[k] - m;                      \
            } else t[k] = 0;                            \
            if (t[k] > b->max) b->max = t[k];           \
            else if (t[k] < b->min) b->min = t[k];      \
        }                                               \
    }                                                   \

void cgn_subtract_bkgnd_u8(const uint8_t *buf, const CgnBeamCalc *c, CgnBeamBkgnd *b) {
    if (b->subtract_bkgnd_v == 1) {
      _cgn_subtract_bkgnd_v1
    } else {
      _cgn_subtract_bkgnd_v0
    }
}

void cgn_subtract_bkgnd_u16(const uint16_t *buf, const CgnBeamCalc *c, CgnBeamBkgnd *b) {
    if (b->subtract_bkgnd_v == 1) {
      _cgn_subtract_bkgnd_v1
    } else {
      _cgn_subtract_bkgnd_v0
    }
}

void cgn_calc_beam_bkgnd(const CgnBeamCalc *c, CgnBeamBkgnd *b, CgnBeamResult *r) {
    if (!b->subtracted) {
        cgn_calc_beam_naive(c, r);
        return;
    }

    if (c->bpp > 8) {
        cgn_subtract_bkgnd_u16((const uint16_t*)(c->buf), c, b);
    } else {
        cgn_subtract_bkgnd_u8((const uint8_t*)(c->buf), c, b);
    }

    r->x1 = b->ax1, r->x2 = b->ax2;
    r->y1 = b->ay1, r->y2 = b->ay2;
    if (b->count < 10) {
        memset(r, 0, sizeof(CgnBeamResult));
        r->nan = 1;
        return;
    }
    r->nan = 0;

    cgn_calc_beam_f64(b->subtracted, c, r);

    for (b->iters = 0; b->iters < b->max_iter; b->iters++) {
        double xc0 = r->xc, yc0 = r->yc;
        double dx0 = r->dx, dy0 = r->dy;
        r->x1 = xc0 - dx0/2.0 * b->mask_diam; r->x1 = max(r->x1, b->ax1);
        r->x2 = xc0 + dx0/2.0 * b->mask_diam; r->x2 = min(r->x2, b->ax2);
        r->y1 = yc0 - dy0/2.0 * b->mask_diam; r->y1 = max(r->y1, b->ay1);
        r->y2 = yc0 + dy0/2.0 * b->mask_diam; r->y2 = min(r->y2, b->ay2);

        cgn_calc_beam_f64(b->subtracted, c, r);

        double th = min(dx0, dy0) * b->precision;
        if (fabs(r->xc - xc0) < th && fabs(r->yc - yc0) < th &&
            fabs(r->dx - dx0) < th && fabs(r->dy - dy0) < th) {
            b->iters++;
            break;
        }
    }
}

#define _cgn_copy_to                            \
    for (int i = 0; i < sz; i++) {              \
        tgt[i] = buf[i];                        \
    }

#define _cgn_copy_to_and_find_max               \
    *max = 0;                                   \
    for (int i = 0; i < sz; i++) {              \
        tgt[i] = buf[i];                        \
        if (tgt[i] > *max) *max = tgt[i];       \
    }

void cgn_copy_u8_to_f64(const uint8_t *buf, int sz, double *tgt, double *max) {
    if (max) {
        _cgn_copy_to_and_find_max
    } else {
        _cgn_copy_to
    }
}

void cgn_copy_u16_to_f64(const uint16_t *buf, int sz, double *tgt, double *max) {
    if (max) {
        _cgn_copy_to_and_find_max
    } else {
        _cgn_copy_to
    }
}

void cgn_copy_to_f64(const CgnBeamCalc *c, double *dst, double *max) {
    if (c->bpp > 8) {
        cgn_copy_u16_to_f64((const uint16_t*)(c->buf), c->w*c->h, dst, max);
    } else {
        cgn_copy_u8_to_f64((const uint8_t*)(c->buf), c->w*c->h, dst, max);
    }
}

void cgn_normalize_f64(double *buf, int sz, double min, double max) {
    for (int i = 0; i < sz; i++) {
        buf[i] = (buf[i] - min) / max;
    }
}

void cgn_copy_normalized_f64(double *src, double *dst, int sz, double min, double max) {
    for (int i = 0; i < sz; i++) {
        dst[i] = (src[i] - min) / max;
    }
}

#define _cgn_calc_brightness               \
    double b_img = 0;                       \
    int w8 = (w / 8) * 8;                   \
    for (int i = 0; i < h; i++) {           \
        double b_row = 0;                   \
        const int offset = i*w;             \
        for (int j = 0; j < w8; j += 8) {   \
            double b0 = 0;                  \
            for (int k = 0; k < 8; k++)     \
                b0 += buf[offset + j + k];  \
            b0 /= 8.0;                      \
            if (b0 > b_row) b_row = b0;     \
        }                                   \
        if (b_row > b_img) b_img = b_row;   \
    }

double cgn_calc_brightness_u8(const uint8_t *buf, int w, int h) {
    _cgn_calc_brightness
    return b_img / 255.0;
}

double cgn_calc_brightness_u16(const uint16_t *buf, int w, int h, int bpp) {
    _cgn_calc_brightness
    return b_img / (double)((1 << bpp) - 1);
}

double cgn_calc_brightness(const CgnBeamCalc *c) {
    if (c->bpp > 8) {
        return cgn_calc_brightness_u16((const uint16_t*)(c->buf), c->w, c->h, c->bpp);
    } else {
        return cgn_calc_brightness_u8((const uint8_t*)(c->buf), c->w, c->h);
    }
}

#define _cgn_calc_brightness_1             \
    double b_img = 0;                       \
    int h8 = (h / 8) * 8;                   \
    int w8 = (w / 8) * 8;                   \
    for (int i = 0; i < h8; i += 8) {       \
        double b_row = 0;                   \
        for (int j = 0; j < w8; j += 8) {   \
            double b0 = 0;                  \
            for (int k = 0; k < 8; k++) {   \
                b0 += buf[w*(i+0) + j + k]; \
                b0 += buf[w*(i+1) + j + k]; \
                b0 += buf[w*(i+2) + j + k]; \
                b0 += buf[w*(i+3) + j + k]; \
                b0 += buf[w*(i+4) + j + k]; \
                b0 += buf[w*(i+5) + j + k]; \
                b0 += buf[w*(i+6) + j + k]; \
                b0 += buf[w*(i+7) + j + k]; \
            }                               \
            b0 /= 64.0;                     \
            if (b0 > b_row) b_row = b0;     \
        }                                   \
        if (b_row > b_img) b_img = b_row;   \
    }

double cgn_calc_brightness_1_u8(const uint8_t *buf, int w, int h) {
    _cgn_calc_brightness_1
    return b_img / 255.0;
}

double cgn_calc_brightness_1_u16(const uint16_t *buf, int w, int h, int bpp) {
    _cgn_calc_brightness_1
    return b_img / (double)((1 << bpp) - 1);
}

double cgn_calc_brightness_1(const CgnBeamCalc *c) {
    if (c->bpp > 8) {
        return cgn_calc_brightness_1_u16((const uint16_t*)(c->buf), c->w, c->h, c->bpp);
    } else {
        return cgn_calc_brightness_1_u8((const uint8_t*)(c->buf), c->w, c->h);
    }
}

#define _cgn_calc_brightness_2             \
    double b_img = 0;                       \
    int y1 = max(0, yc-4);                  \
    int x1 = max(0, xc-4);                  \
    int y2 = min(h-1, yc+4);                \
    int x2 = min(w-1, xc+4);                \
    double cnt = 0;                         \
    for (int i = y1; i <= y2; i++)          \
        for (int j = x1; j <= x2; j++)      \
            b_img += buf[i*w + j], cnt++;   \
    b_img /= cnt;

double cgn_calc_brightness_2_u8(const uint8_t *buf, int w, int h, int xc, int yc) {
    _cgn_calc_brightness_2
    return b_img / 255.0;
}

double cgn_calc_brightness_2_u16(const uint16_t *buf, int w, int h, int xc, int yc, int bpp) {
    _cgn_calc_brightness_2
    return b_img / (double)((1 << bpp) - 1);
}

double cgn_calc_brightness_2(const CgnBeamCalc *c, int xc, int yc) {
    if (c->bpp > 8) {
        return cgn_calc_brightness_2_u16((const uint16_t*)(c->buf), c->w, c->h, xc, yc, c->bpp);
    } else {
        return cgn_calc_brightness_2_u8((const uint8_t*)(c->buf), c->w, c->h, xc, yc);
    }
}

typedef struct { uint8_t b0, b1, b2, b3, b4; } PixelGroup10;
void cgn_convert_10g40_to_u16(uint8_t *dst, uint8_t *src, int sz) {
    int j = 0;
    PixelGroup10 *g = (PixelGroup10*)src;
    for (int i = 0; i < sz; i += 5) {
        dst[j++] = ((g->b4 & 0b00000011) >> 0) | (g->b0 << 2);
        dst[j++] = g->b0 >> 6;
        dst[j++] = ((g->b4 & 0b00001100) >> 2) | (g->b1 << 2);
        dst[j++] = g->b1 >> 6;
        dst[j++] = ((g->b4 & 0b00110000) >> 4) | (g->b2 << 2);
        dst[j++] = g->b2 >> 6;
        dst[j++] = ((g->b4 & 0b11000000) >> 6) | (g->b3 << 2);
        dst[j++] = g->b3 >> 6;
        g++;
    }
}

typedef struct { uint8_t b0, b1, b2; } PixelGroup12;
void cgn_convert_12g24_to_u16(uint8_t *dst, uint8_t *src, int sz) {
    int j = 0;
    PixelGroup12 *g = (PixelGroup12*)src;
    for (int i = 0; i < sz; i += 3) {
        dst[j++] = (g->b2 & 0x0F) | (g->b0 << 4);
        dst[j++] = g->b0 >> 4;
        dst[j++] = (g->b2 >> 4) | (g->b1 << 4);
        dst[j++] = g->b1 >> 4;
        g++;
    }
}

#define _cgn_find_max                   \
    for (int i = 0; i < sz; i++)        \
        if (buf[i] > max) max = buf[i]; \
    return max;

double cgn_find_max_u8(const uint8_t *buf, int sz) {
    uint8_t max = 0;
    _cgn_find_max
}

double cgn_find_max_u16(const uint16_t *buf, int sz) {
    uint16_t max = 0;
    _cgn_find_max
}

#define _cgn_copy_to_f64_norm       \
    for (int i = 0; i < sz; i++) {  \
        dst[i] = buf[i] / max;        \
    }

void cgn_copy_u8_to_f64_norm(const uint8_t *buf, int sz, double *dst, double max) {
    _cgn_copy_to_f64_norm
}

void cgn_copy_u16_to_f64_norm(const uint16_t *buf, int sz, double *dst, double max) {
    _cgn_copy_to_f64_norm
}

void cgn_ext_copy_to_f64(const CgnBeamCalc *c, CgnBeamBkgnd *b, double *dst, int normalize, int full_z, double *min_z, double *max_z)
{
    const int sz = c->w * c->h;
    const double top_z = (1 << c->bpp) - 1;
    if (b->subtracted) {
        if (normalize) {
            *min_z = 0;
            *max_z = 1;
            cgn_copy_normalized_f64(b->subtracted, dst, sz, b->min, full_z ? (top_z - b->min) : b->max);
        } else {
            *min_z = full_z ? 0 : b->min;
            *max_z = full_z ? top_z : b->max;
        #ifdef _WIN32
            memcpy_s(dst, sizeof(double)*sz, b->subtracted, sizeof(double)*sz);
        #else
            memcpy(dst, b->subtracted, sizeof(double)*sz);
        #endif
        }
    } else {
        if (normalize) {
            *min_z = 0;
            *max_z = 1;
            if (c->bpp > 8) {
                const uint16_t* buf = (const uint16_t*)c->buf;
                cgn_copy_u16_to_f64_norm(buf, sz, dst, full_z ? top_z : cgn_find_max_u16(buf, sz));
            } else {
                const uint8_t* buf = c->buf;
                cgn_copy_u8_to_f64_norm(buf, sz, dst, full_z ? top_z : cgn_find_max_u8(buf, sz));
            }
        } else {
            *min_z = 0;
            *max_z = top_z;
            cgn_copy_to_f64(c, dst, full_z ? NULL : max_z);
        }
    }
}

#define _cgn_calc_overexposure                   \
    int cnt = 0;                                 \
    for (int i = 0; i < h; i += 2)               \
        for (int j = 0; j < w; j += 2) {         \
            if (buf[i*w + j] >= top)             \
                cnt++;                           \
        }                                        \
    double p = (double)cnt * 4.0 / (double)(w*h);

double cgn_calc_overexposure_u8(const uint8_t *buf, int w, int h, double th) {
    uint8_t top = 255.0 * th;
    _cgn_calc_overexposure
    return p;
}

double cgn_calc_overexposure_u16(const uint16_t *buf, int w, int h, int bpp, double th) {
    uint16_t top = (double)((1 << bpp) - 1) * th;
    _cgn_calc_overexposure
    return p;
}

double cgn_calc_overexposure(const CgnBeamCalc *c, double th)
{
    if (c->bpp > 8) {
        return cgn_calc_overexposure_u16((const uint16_t*)(c->buf), c->w, c->h, c->bpp, th);
    } else {
        return cgn_calc_overexposure_u8((const uint8_t*)(c->buf), c->w, c->h, th);
    }
}

void cgn_calc_profiles(const CgnBeamImage *img, const CgnBeamResult *res, CgnBeamProfiles *prf)
{
    const int w = img->w;
    const int h = img->h;
    const double *data = img->data;
    const int c = prf->cnt - 1;
    const double step_x = res->dx / 2.0 * prf->w / (double)(c);
    const double step_y = res->dy / 2.0 * prf->w / (double)(c);
    const double a = res->phi / 57.29577951308232;
    const double sin_a = sin(a);
    const double cos_a = cos(a);
    int x0 = round(res->xc);
    int y0 = round(res->yc);
    int xi, yi, dx, dy;
    double r, p;
    for (int i = 0; i < prf->cnt; i++) {
        // along X-width
        r = step_x * (double)i;
        dx = round(r * cos_a);
        dy = round(r * sin_a);

        // positive half
        xi = x0 + dx;
        yi = y0 + dy;
        p = (xi >= 0 && xi < w && yi >= 0 && yi < h) ? data[w * yi + xi] : 0;
        prf->x_r[c + i] = r;
        prf->x_p[c + i] = p;

        // negative half
        xi = x0 - dx;
        yi = y0 - dy;
        p = (xi >= 0 && xi < w && yi >= 0 && yi < h) ? data[w * yi + xi] : 0;
        prf->x_r[c - i] = -r;
        prf->x_p[c - i] = p;

        // along Y-width
        r = step_y * (double)i;
        dx = round(r * sin_a);
        dy = round(r * cos_a);

        // positive half
        xi = x0 + dx;
        yi = y0 + dy;
        p = (xi >= 0 && xi < w && yi >= 0 && yi < h) ? data[w * yi + xi] : 0;
        prf->y_r[c + i] = r;
        prf->y_p[c + i] = p;

        // negative half
        r = step_y * (double)i;
        xi = x0 - dx;
        yi = y0 - dy;
        p = (xi >= 0 && xi < w && yi >= 0 && yi < h) ? data[w * yi + xi] : 0;
        prf->y_r[c - i] = -r;
        prf->y_p[c - i] = p;
    }
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

void cgn_calc_beam_blas_u8(const uint8_t *b, CgnBeamCalcBlas *c, CgnBeamResultBlas *r) {
    for (int i = 0; i < w*h; i++) {
        d[i] = b[i];
    }
    cgn_calc_beam_blas(c, r);
}

void cgn_calc_beam_blas_u16(const uint16_t *b, CgnBeamCalcBlas *c, CgnBeamResultBlas *r) {
    for (int i = 0; i < w*h; i++) {
        d[i] = b[i];
    }
    cgn_calc_beam_blas(c, r);
}

#endif // USE_BLAS
