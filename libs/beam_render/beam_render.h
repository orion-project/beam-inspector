#ifndef _BEAM_RENDER_H_
#define _BEAM_RENDER_H_

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
    int p0;
    unsigned char *buf;
} RenderBeamParams;

void render_beam(RenderBeamParams *b);
void copy_pixels_to_doubles(RenderBeamParams *b, double *d);

#ifdef __cplusplus
}
#endif

#endif // _BEAM_RENDER_H_
