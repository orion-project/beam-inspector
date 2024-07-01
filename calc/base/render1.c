/*

Simplest and straightforward implementation for generation of ideal beam image for virtual camera.

Calculation of exponent is slow, so we use the "exp(x) ~ (1 + x/m)^m" approximation.
Here is more about fast exponents:
http://spfrnd.de/posts/2018-03-10-fast-exponential.html
https://github.com/simonpf/fastexp/tree/master

Compiler flags are also important for achieving real-time performance:
-O3 -ffast-math -funsafe-math-optimizations -msse4.2

Seems can be accelerated even more with OpenMP but more experiments required.
The performance of "no_rotation_fast_exp" mode is alredy enough for demo and test purposes.

--------------------------------------
GCC 8.1x32 Intel i7-2600 CPU 3.40GHz

Image size: 2592x2048
Beam widths: 1474x1120
Center position: 1534x981
Max intensity: 255
Azimuth: -12
Frames: 30
fill                          Elapsed: 0.109s, FPS: 275.2
render                        Elapsed: 7.221s, FPS: 4.2
render_fast_exp               Elapsed: 1.008s, FPS: 29.8
render_no_rotation            Elapsed: 6.661s, FPS: 4.5
render_no_rotation_fast_exp   Elapsed: 0.518s, FPS: 57.9

*/
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define sqr(s) ((s)*(s))
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

#define FILENAME "render1.pgm"
#define FRAMES 30

// Precision doesn't give any notable difference in speed
//typedef double real;
typedef float real;
typedef uint8_t pixel;

struct Beam {
    int w;
    int h;
    int dx;
    int dy;
    int xc;
    int yc;
    int phi;
    int p0;
    pixel *buf;

    void fill();
    void render();
    void render_fast_exp();
    void render_no_rotation();
    void render_no_rotation_fast_exp();
    void render_no_rotation_fast_exp_center();
    void write();
};

void Beam::fill() {
    // Simple fill is just for illustration that simplest per-pixel set
    // costs almost nothing even with additional memset for black background.
    memset(buf, 0, sizeof(pixel)*w*h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            buf[y*w + x] = 100;
        }
    }
}

void Beam::render() {
    memset(buf, 0, sizeof(pixel)*w*h);

    real r2 = sqr(dx/2.0);
    real cos_phi = cos(phi * 3.14159265358979323846 / 180.0);
    real sin_phi = sin(phi * 3.14159265358979323846 / 180.0);

    real el = dx / (real)dy; // elliptisity

    // ISO 11146-1 says the integration area should be 3 times larger that the beam width
    // but it's a way slower for generation and not providing notable visual difference
    int x_min = -dx, x_max = dx, y_min = -dy, y_max = dy;
    //printf("Pixels: %d\n", (x_max-x_min)*(y_max-y_min));

    real p = p0; // single conversion to real gives a bit of speed-up
    int x1, y1, y2;
    for (int y = y_min; y < y_max; y++) {
        real y2 = sqr(y*el);
        for (int x = x_min; x < x_max; x++) {
            int x1 = xc + x*cos_phi - y*sin_phi;
            int y1 = yc + x*sin_phi + y*cos_phi;
            if (y1 >= 0 && y1 < h && x1 >= 0 && x1 < w) {
                buf[y1*w + x1] = p0 * exp(-2*(sqr(x) + y2)/r2);
            }
        }
    }
}

void Beam::render_fast_exp() {
    memset(buf, 0, sizeof(pixel)*w*h);

    real r2 = sqr(dx/2.0);
    real cos_phi = cos(phi * 3.14159265358979323846 / 180.0);
    real sin_phi = sin(phi * 3.14159265358979323846 / 180.0);

    real el = dx / (real)dy; // elliptisity

    // ISO 11146-1 says the integration area should be 3 times larger that the beam width
    // but it's a way slower for generation and not providing notable visual difference
    int x_min = -dx, x_max = dx, y_min = -dy, y_max = dy;
    //printf("Pixels: %d\n", (x_max-x_min)*(y_max-y_min));

    real p = p0;
    int x1, y1, y2;
    for (int y = y_min; y < y_max; y++) {
        real y2 = sqr(y*el);
        for (int x = x_min; x < x_max; x++) {
            int x1 = xc + x*cos_phi - y*sin_phi;
            int y1 = yc + x*sin_phi + y*cos_phi;
            if (y1 >= 0 && y1 < h && x1 >= 0 && x1 < w) {
                real t = 1+ (-2*(sqr(x) + y2)/r2) /10.0;
                buf[y1*w + x1] = p * t*t*t*t*t*t*t*t*t*t;
            }
        }
    }
}

void Beam::render_no_rotation() {
    real r2 = sqr(dx/2.0);
    real el = dx / (real)dy; // elliptisity
    real p = p0;
    for (int y = 0; y < h; y++) {
        real y2 = sqr((y-yc)*el);
        for (int x = 0; x < w; x++) {
            buf[y*w + x] = p * exp(-2*(sqr(x) + y2)/r2);
        }
    }
}

void Beam::render_no_rotation_fast_exp() {
    real r2 = sqr(dx/2.0);
    real el = dx / (real)dy; // elliptisity
    real p = p0;
    for (int y = 0; y < h; y++) {
        real y2 = sqr((y-yc)*el);
        for (int x = 0; x < w; x++) {
            real t = 1+ (-2*(sqr(x-xc) + y2)/r2) /10.0;
            buf[y*w + x] = p * t*t*t*t*t*t*t*t*t*t;
        }
    }
}

void Beam::render_no_rotation_fast_exp_center() {
    memset(buf, 0, sizeof(pixel)*w*h);
    real r2 = sqr(dx/2.0);
    real el = dx / (real)dy; // elliptisity
    real p = p0;
    int x_min = xc - dx*0.6; x_min = max(x_min, 0);
    int x_max = xc + dx*0.6; x_max = min(x_max, w);
    int y_min = yc - dy*0.6; y_min = max(y_min, 0);
    int y_max = yc + dy*0.6; y_max = min(y_max, h);
    for (int y = y_min; y < y_max; y++) {
        real y2 = sqr((y-yc)*el);
        for (int x = x_min; x < x_max; x++) {
            real t = 1+ (-2*(sqr(x-xc) + y2)/r2) /5.0;
            buf[y*w + x] = p * t*t*t*t*t;
        }
    }
}

void Beam::write() {
    FILE *f = fopen(FILENAME, "wb");
    if (!f) {
        perror("Failed to open");
        return;
    }
    fprintf(f, "P5\n%d %d\n%d\n", w, h, p0);
    fwrite(buf, sizeof(pixel), w*h, f);
    fclose(f);
}

#define MEASURE(func) { \
    printf("\n" #func "\n"); \
    clock_t tm = clock(); \
    for (int i = 0; i < FRAMES; i++) b.func(); \
    double elapsed = (clock() - tm)/(double)CLOCKS_PER_SEC; \
    printf("Elapsed: %.3fs, FPS: %.1f\n", elapsed, FRAMES/elapsed); \
}

int main() {
    Beam b;

    //b.w = 600; b.h = 600;
    b.w = 2592; b.h = 2048;
    printf("Image size: %dx%d\n", b.w, b.h);

    //b.dx = 300; b.dy = 200;
    b.dx = 1474; b.dy = 1120;
    printf("Beam widths: %dx%d\n", b.dx, b.dy);

    //b.xc = b.w/2 + 100; b.yc = b.h/2 - 50;
    b.xc = 1534; b.yc = 981;
    printf("Center position: %dx%d\n", b.xc, b.yc);

    b.p0 = 255;
    printf("Max intensity: %d\n", b.p0);

    b.phi = -12;
    printf("Azimuth: %d\n", b.phi);

    b.buf = (pixel*)malloc(sizeof(pixel) * b.w * b.h);
    if (!b.buf) {
        perror("Unable to allocate pixels");
        exit(EXIT_FAILURE);
    }

    printf("Frames: %d\n", FRAMES);

    MEASURE(fill)
    MEASURE(render)
    MEASURE(render_fast_exp)
    MEASURE(render_no_rotation)
    MEASURE(render_no_rotation_fast_exp)
    MEASURE(render_no_rotation_fast_exp_center)

    b.write();

    free(b.buf);
    return EXIT_SUCCESS;
}
