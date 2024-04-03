#include "beam_calc.h"

#include <stdlib.h>
#include <time.h>

#include "../../calc/pgm.h"

#define FRAMES 1
#define FILENAME_8 "../../beams/beam_8b_ast.pgm"
#define FILENAME_16 "../../beams/beam_16b_ast.pgm"

#define MEASURE(func) { \
    printf("\n" #func "\n"); \
    clock_t tm = clock(); \
    for (int i = 0; i < FRAMES; i++) func; \
    double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC; \
    printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", r.xc, r.yc, r.dx, r.dy, r.phi); \
    printf("Elapsed: %.3fs, FPS: %.1f, %.1fms/frame\n", elapsed, FRAMES/elapsed, elapsed/(double)FRAMES*1000); \
}

int main() {
    int w8, h8, offset8;
    uint8_t *buf8 = read_pgm(FILENAME_8, &w8, &h8, &offset8);
    if (!buf8) {
        exit(EXIT_FAILURE);
    }
    uint8_t max8 = 0;
    for (int i = 0; i < w8*h8; i++)
        if (max8 < buf8[offset8+i]) {
            max8 = buf8[offset8+i];
        }
    printf("Max value: %d\n", max8);
    printf("Data offset: %d\n\n", offset8);

    int w16, h16, offset16;
    uint8_t *buf16 = read_pgm(FILENAME_16, &w16, &h16, &offset16);
    if (!buf16) {
        exit(EXIT_FAILURE);
    }
    uint16_t max16 = 0;
    for (int i = 0; i < w16*h16; i++)
        if (max16 < ((uint16_t*)buf16)[offset16+i]) {
            max16 = ((uint16_t*)buf16)[offset16+i];
        }
    printf("Max value: %d\n", max16);
    printf("Data offset: %d\n\n", offset16);

    printf("Frames: %d\n", FRAMES);
    {
        CgnBeamCalcBlas c;
        c.w = w8;
        c.h = h8;
        if (cgn_calc_beam_blas_init(&c)) {
            cgn_calc_beam_blas_free(&c);
            exit(EXIT_FAILURE);
        }
        CgnBeamResultBlas r;
        MEASURE(cgn_calc_beam_blas_8((uint8_t*)(buf8+offset8), &c, &r));
        cgn_calc_beam_blas_free(&c);
    }
    {
        CgnBeamCalcBlas c;
        c.w = w16;
        c.h = h16;
        if (cgn_calc_beam_blas_init(&c)) {
            cgn_calc_beam_blas_free(&c);
            exit(EXIT_FAILURE);
        }
        CgnBeamResultBlas r;
        MEASURE(cgn_calc_beam_blas_16((uint16_t*)(buf16+offset16), &c, &r));
        cgn_calc_beam_blas_free(&c);
    }
    {
        CgnBeamResult r;
        CgnBeamCalc8 c;
        c.w = w8;
        c.h = h8;
        c.buf = (uint8_t*)(buf8+offset8);
        MEASURE(cgn_calc_beam_8_naive(&c, &r));
    }
    {
        CgnBeamResult r;
        CgnBeamCalc16 c;
        c.w = w16;
        c.h = h16;
        c.buf = (uint16_t*)(buf16+offset16);
        MEASURE(cgn_calc_beam_16_naive(&c, &r));
    }
    free(buf8);
    free(buf16);
    return EXIT_SUCCESS;
}
