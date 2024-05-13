#include "beam_render.h"

#include <math.h>
#include <string.h>

#define sqr(s) ((s)*(s))
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

void cgn_render_beam(CgnBeamRender *b) {
    memset(b->buf, 0, b->w*b->h);
    double r2 = sqr(b->dx/2.0);
    double el = b->dx / (double)(b->dy);
    double p = b->p;
    int x_min = b->xc - b->dx*0.6; x_min = max(x_min, 0);
    int x_max = b->xc + b->dx*0.6; x_max = min(x_max, b->w);
    int y_min = b->yc - b->dy*0.6; y_min = max(y_min, 0);
    int y_max = b->yc + b->dy*0.6; y_max = min(y_max, b->h);
    for (int y = y_min; y < y_max; y++) {
        int offset = y * b->w;
        double y2 = sqr((y - b->yc) * el);
        for (int x = x_min; x < x_max; x++) {
            double t = 1+ (-2*(sqr(x - b->xc) + y2)/r2) /5.0;
            b->buf[offset + x] = p * t*t*t*t*t;
        }
    }
}

void cgn_render_beam_tilted(CgnBeamRender *b) {
    memset(b->buf, 0, b->w*b->h);
    double r2 = sqr(b->dx/2.0);
    double cos_phi = cos(b->phi * 3.14159265358979323846 / 180.0);
    double sin_phi = sin(b->phi * 3.14159265358979323846 / 180.0);
    double el = b->dx / (double)(b->dy);
    int x_min = -b->dx*0.6, x_max = b->dx*0.6;
    int y_min = -b->dy*0.6, y_max = b->dy*0.6;
    double p = b->p;
    for (int y = y_min; y < y_max; y++) {
        double y2 = sqr(y*el);
        for (int x = x_min; x < x_max; x++) {
            int x1 = b->xc + x*cos_phi - y*sin_phi;
            int y1 = b->yc + x*sin_phi + y*cos_phi;
            if (y1 >= 0 && y1 < b->h && x1 >= 0 && x1 < b->w) {
                double t = 1+ (-2*(sqr(x) + y2)/r2) /5.0;
                b->buf[y1*b->w + x1] = p * t*t*t*t*t;
            }
        }
    }
}

void cgn_render_beam_to_doubles(CgnBeamRender *b, double *d) {
    for (int i = 0; i < (b->w * b->h); i++) {
        d[i] = (double)b->buf[i];
    }
}

double cgn_find_max_8(const uint8_t *b, int sz) {
    double max = 0;
    for (int i = 0; i < sz; i++)
        if (b[i] > max) max = b[i];
    return max;
}

double cgn_find_max_16(const uint16_t *b, int sz) {
    double max = 0;
    for (int i = 0; i < sz; i++)
        if (b[i] > max) max = b[i];
    return max;
}

void cgn_render_beam_to_doubles_norm_8(const uint8_t *b, int sz, double *d, double max) {
    for (int i = 0; i < sz; i++) {
        d[i] = b[i] / max;
    }
}

void cgn_render_beam_to_doubles_norm_16(const uint16_t *b, int sz, double *d, double max) {
    for (int i = 0; i < sz; i++) {
        d[i] = b[i] / max;
    }
}
