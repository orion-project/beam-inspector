#include "beam_render.h"

#define sqr(s) ((s)*(s))
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

void render_beam(RenderBeamParams *b) {
    double r2 = sqr(b->dx/2.0);
    double el = b->dx / (double)(b->dy);
    double p = b->p0;
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

void copy_pixels_to_doubles(RenderBeamParams *b, double *d) {
    for (int i = 0; i < (b->w * b->h); i++) {
        d[i] = (double)b->buf[i];
    }
}
