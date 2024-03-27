#ifndef _CGN_BEAM_RENDER_H_
#define _CGN_BEAM_RENDER_H_

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

#ifdef __cplusplus
}
#endif

#endif // _CGN_BEAM_RENDER_H_
