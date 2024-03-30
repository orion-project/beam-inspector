#include "beam_calc.h"

#include <stdlib.h>
#include <time.h>

#include "../../calc/pgm.h"

#define FRAMES 30
//#define FILENAME "../../beams/beam_8b_ast.pgm"
#define FILENAME "../../beams/beam_16b_ast.pgm"
#define HRD

#define MEASURE(func) { \
    printf("\n" #func "\n"); \
    clock_t tm = clock(); \
    for (int i = 0; i < FRAMES; i++) func(&c, &r); \
    double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC; \
    printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", r.xc, r.yc, r.dx, r.dy, r.phi); \
    printf("Elapsed: %.3fs, FPS: %.1f, %.1fms/frame\n", elapsed, FRAMES/elapsed, elapsed/(double)FRAMES*1000); \
}

int main() {
    int w, h, offset;
    unsigned char *buf = read_pgm(FILENAME, &w, &h, &offset);
    if (!buf) {
        exit(EXIT_FAILURE);
    }
    printf("Frames: %d\n", FRAMES);
    /*{
        CgnBeamCalcBlas c;
        c.w = w;
        c.h = h;
        c.buf = (unsigned char*)(buf+offset);
        if (cgn_calc_beam_blas_init(&c)) {
            cgn_calc_beam_blas_free(&c);
            exit(EXIT_FAILURE);
        }
        CgnBeamResultBlas r;
        MEASURE(cgn_calc_beam_blas);
        cgn_calc_beam_blas_free(&c);
    }*/
    {
        CgnBeamResult r;
    #ifdef HRD
        CgnBeamCalc16 c;
        c.w = w;
        c.h = h;
        c.buf = (uint16_t*)(buf+offset);
        MEASURE(cgn_calc_beam_16_naive);
    #else
        CgnBeamCalc8 c;
        c.w = w;
        c.h = h;
        c.buf = (uint8_t*)(buf+offset);
        MEASURE(cgn_calc_beam_8_naive);
    #endif
    }
    free(buf);
    return EXIT_SUCCESS;
}
