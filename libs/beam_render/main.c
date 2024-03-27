/*

Intel i7-2600 CPU 3.40GHz
Image size: 2592x2048
Beam widths: 1474x1120
Center position: 1534x981
Max intensity: 255
Azimuth: -12
Frames: 30
Elapsed: 0.210s, FPS: 142.9

*/
#include "beam_render.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define FRAMES 30

#define MEASURE(func) { \
    printf("\n" #func "\n"); \
    clock_t tm = clock(); \
    for (int i = 0; i < FRAMES; i++) func(&b); \
    double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC; \
    printf("Elapsed: %.3fs, FPS: %.1f, %.2fms/frame\n", elapsed, FRAMES/elapsed, elapsed/(double)FRAMES*1000); \
}

int main() {
   CgnBeamRender b;
   b.w = 2592;
   b.h = 2048;
   b.dx = 1474;
   b.dy = 1120;
   b.xc = 1534;
   b.yc = 981;
   b.p = 255;
   b.buf = (unsigned char*)malloc(b.w * b.h);
   if (!b.buf) {
       perror("Unable to allocate pixels");
       return 1;
   }
   printf("Image size: %dx%d\n", b.w, b.h);
   printf("Beam widths: %dx%d\n", b.dx, b.dy);
   printf("Center position: %dx%d\n", b.xc, b.yc);
   printf("Max intensity: %d\n", b.p);
   printf("Frames: %d\n", FRAMES);
   MEASURE(cgn_render_beam)
   MEASURE(cgn_render_beam_tilted)
   free(b.buf);
   return 0;
}
