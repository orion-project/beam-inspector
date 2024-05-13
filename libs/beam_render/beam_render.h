#ifndef _CGN_BEAM_RENDER_H_
#define _CGN_BEAM_RENDER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int w;
    int h;
    int dx;
    int dy;
    int xc;
    int yc;
    int p;
    int phi;
    unsigned char *buf;
} CgnBeamRender;

void cgn_render_beam(CgnBeamRender *b);
void cgn_render_beam_tilted(CgnBeamRender *b);
void cgn_render_beam_to_doubles(CgnBeamRender *b, double *d);
double cgn_find_max_8(const uint8_t *b, int sz);
double cgn_find_max_16(const uint16_t *b, int sz);
void cgn_render_beam_to_doubles_norm_8(const uint8_t *b, int sz, double *d, double max);
void cgn_render_beam_to_doubles_norm_16(const uint16_t *b, int sz, double *d, double max);

#ifdef __cplusplus
}
#endif

#endif // _CGN_BEAM_RENDER_H_
