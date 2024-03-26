#include "beam_calc.h"

#include <stdlib.h>
#include <time.h>

#include "../../calc/pgm.h"

#define FRAMES 30
#define FILENAME "../../beams/beam_8b_ast.pgm"

int main() {
    int w, h, offset;
    uint8_t *buf = read_pgm(FILENAME, &w, &h, &offset);
    if (!buf) {
        exit(EXIT_FAILURE);
    }

    CgnBeamCalc c;
    c.w = w;
    c.h = h;
    c.buf = (uint8_t*)(buf+offset);
    if (cgn_calc_beam_init(&c)) {
        cgn_calc_beam_free(&c);
        exit(EXIT_FAILURE);
    }

    printf("Frames: %d\n", FRAMES);
    clock_t tm = clock();
    CgnBeamResult r;
    for (int i = 0; i < FRAMES; i++) {
        cgn_calc_beam(&c, &r);
    }
    double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC;
    printf("center=[%.0f,%.0f], diam=[%.0f,%.0f], angle=%.1f\n", r.xc, r.yc, r.dx, r.dy, r.phi*180/3.14);
    printf("Elapsed: %.3fs, FPS: %.1f, %.2fms/frame\n", elapsed, FRAMES/elapsed, elapsed/(double)FRAMES*1000);

    free(buf);
    cgn_calc_beam_free(&c);
    return EXIT_SUCCESS;
}
