/*

Experiments and reference implementation for conversion of 
packed 10 and 12 bit IDS camera images to regular 16-bit images.
Here is the raw formats description:
https://www.1stvision.com/cameras/IDS/IDS-manuals/en/basics-monochrome-pixel-formats.html

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ids_peak_comfort_c.h"

#include "../pgm.h"

#define FRAMES 1
#define FPS 10
#define EXP 40000
#define GAIN 2
#define BINNING_X 2
#define BINNING_Y 2
#define DECIMATION_X 2
#define DECIMATION_Y 2

//#define BITS_8
//#define BITS_10
#define BITS_12

#if defined(BITS_8)
    #define BITS 8
    #define FILENAME "ids_capture_8b.pgm"
    #define PIXEL_FORMAT PEAK_PIXEL_FORMAT_MONO8
    typedef uint8_t pixel;
#elif defined(BITS_10)
    #define BITS 10
    #define FILENAME "ids_capture_10b.pgm"
    #define PIXEL_FORMAT PEAK_PIXEL_FORMAT_MONO10G40_IDS
    typedef uint16_t pixel;
    typedef struct { uint8_t b0, b1, b2, b3, b4; } PixelGroupSrc;
    typedef struct { 
        uint8_t p0_lsb, p0_msb;
        uint8_t p1_lsb, p1_msb;
        uint8_t p2_lsb, p2_msb;
        uint8_t p3_lsb, p3_msb;
    } PixelGroupDst;
#elif defined(BITS_12)
    #define BITS 12
    #define FILENAME "ids_capture_12b.pgm"
    #define PIXEL_FORMAT PEAK_PIXEL_FORMAT_MONO12G24_IDS
    typedef uint16_t pixel;
    typedef struct { uint8_t b0, b1, b2; } PixelGroupSrc;
    typedef struct { 
        uint8_t p0_lsb, p0_msb;
        uint8_t p1_lsb, p1_msb;
    } PixelGroupDst;
#endif

#define CHECK_ERR(msg) \
    if (PEAK_ERROR(res)) { \
        printf("ERR: %s (code=%#06x)\n", msg, res); \
        goto stop; \
    }

#define PRINT_ERR(msg) { \
    printf("ERR: %s\n", msg); \
    goto stop; \
}

int main() {
    peak_status res = peak_Library_Init();
    CHECK_ERR("Init library")

    res = peak_CameraList_Update(NULL);
    CHECK_ERR("Update camera list")

    peak_camera_handle hCam = PEAK_INVALID_HANDLE;
    res = peak_Camera_OpenFirstAvailable(&hCam);
    CHECK_ERR("Open first available camera")

    peak_camera_descriptor descr;
    res = peak_Camera_GetDescriptor(peak_Camera_ID_FromHandle(hCam), &descr);
    CHECK_ERR("Get camera descriptor")
    printf("Camera: %s (SN: %s, ID: %llu)\n", descr.modelName, descr.serialNumber, descr.cameraID);

    res = peak_FrameRate_Set(hCam, FPS);
    CHECK_ERR("Set frame rate")

    res = peak_PixelFormat_Set(hCam, PIXEL_FORMAT);
    CHECK_ERR("Set pixel format")

    res = peak_ExposureTime_Set(hCam, EXP);
    CHECK_ERR("Set exposition")

    res = peak_IPL_Gain_Set(hCam, PEAK_GAIN_CHANNEL_RED, GAIN);
    CHECK_ERR("Set gain R")
    res = peak_IPL_Gain_Set(hCam, PEAK_GAIN_CHANNEL_GREEN, GAIN);
    CHECK_ERR("Set gain G")
    res = peak_IPL_Gain_Set(hCam, PEAK_GAIN_CHANNEL_BLUE, GAIN);
    CHECK_ERR("Set gain B")

    peak_pixel_format_info info;
    res = peak_PixelFormat_GetInfo(PIXEL_FORMAT, &info);
    CHECK_ERR("Get pixel format info")
    printf("Pixel format:\n"
        "    numBitsPerPixel: %d\n"
        "    numSignificantBitsPerPixel: %d\n"
        "    numChannels: %d\n"
        "    numBitsPerChannel: %d\n"
        "    numSignificantBitsPerChannel: %d\n"
        "    maxValuePerChannel: %d\n", 
        info.numBitsPerPixel, info.numSignificantBitsPerPixel, info.numChannels,
        info.numBitsPerChannel, info.numSignificantBitsPerChannel, info.maxValuePerChannel);

    if (peak_Binning_GetAccessStatus(hCam) != PEAK_ACCESS_READWRITE) {
        printf("Binning is not configurable\n");
    } else {
        size_t count;
        res = peak_Binning_FactorX_GetList(hCam, NULL, &count);
        CHECK_ERR("Get binning factors count for X")
        uint32_t *factors = (uint32_t*)malloc(sizeof(uint32_t)*count);
        res = peak_Binning_FactorX_GetList(hCam, factors, &count);
        CHECK_ERR("Get binning factors for X")
        for (int i = 0; i < count; i++) {
            printf("Supported binning X: %d\n", factors[i]);
        }
        free(factors);
        res = peak_Binning_FactorY_GetList(hCam, NULL, &count);
        CHECK_ERR("Get binning factors count for Y")
        factors = (uint32_t*)malloc(sizeof(uint32_t)*count);
        res = peak_Binning_FactorY_GetList(hCam, factors, &count);
        CHECK_ERR("Get binning factors for Y")
        for (int i = 0; i < count; i++) {
            printf("Supported binning Y: %d\n", factors[i]);
        }
        free(factors);

        res = peak_Binning_Set(hCam, BINNING_X, BINNING_Y);
        CHECK_ERR("Set binning factors")

        uint32_t factor_x, factor_y;
        res = peak_Binning_Get(hCam, &factor_x, &factor_y);
        CHECK_ERR("Get binning factors")
        printf("Binning x=%d, y=%d\n", factor_x, factor_y);
    }
    {
        size_t count;
        res = peak_Decimation_FactorX_GetList(hCam, NULL, &count);
        CHECK_ERR("Get decimation factors count for X")
        uint32_t *factors = (uint32_t*)malloc(sizeof(uint32_t)*count);
        res = peak_Decimation_FactorX_GetList(hCam, factors, &count);
        CHECK_ERR("Get decimation factors for X")
        for (int i = 0; i < count; i++) {
            printf("Supported decimation X: %d\n", factors[i]);
        }
        free(factors);
        res = peak_Decimation_FactorY_GetList(hCam, NULL, &count);
        CHECK_ERR("Get decimation factors count for Y")
        factors = (uint32_t*)malloc(sizeof(uint32_t)*count);
        res = peak_Decimation_FactorY_GetList(hCam, factors, &count);
        CHECK_ERR("Get decimation factors for Y")
        for (int i = 0; i < count; i++) {
            printf("Supported decimation Y: %d\n", factors[i]);
        }
        free(factors);

        res = peak_Decimation_Set(hCam, DECIMATION_X, DECIMATION_Y);
        CHECK_ERR("Set decimation factors")

        uint32_t factor_x, factor_y;
        res = peak_Decimation_Get(hCam, &factor_x, &factor_y);
        CHECK_ERR("Get decimation factors")
        printf("Decimation x=%d, y=%d\n", factor_x, factor_y);
    }

    peak_roi roi;
    res = peak_ROI_Get(hCam, &roi);
    CHECK_ERR("Get frame size")

    int w = roi.size.width;
    int h = roi.size.height;
    printf("Size: %dx%dx%db\n", w, h, BITS);

    pixel *img = (pixel*)malloc(sizeof(pixel)*w*h);
    if (!img) {
        PRINT_ERR("Allocate image buffer");
    }

    res = peak_Acquisition_Start(hCam, FRAMES);
    CHECK_ERR("Start acquisition")

    double elapsed = 0;
            int real_max_0 = 0, real_max_1 = 0;


    peak_buffer buf;
    peak_frame_handle frame;
    for (int i = 0; i < FRAMES; i++) {
        res = peak_Acquisition_WaitForFrame(hCam, 5000, &frame);
        CHECK_ERR("Wait for frame")

        res = peak_Frame_Buffer_Get(frame, &buf);
        CHECK_ERR("Get frame buffer")

        int j = 0;
        uint8_t *b = buf.memoryAddress;
        uint8_t *p = (uint8_t*)img;
        clock_t tm = clock();
    #if defined(BITS_8)
        memcpy(p, b, buf.memorySize);
    #elif defined(BITS_10)
        PixelGroupSrc *g = (PixelGroupSrc*)b;
        PixelGroupDst *d = (PixelGroupDst*)img;
        for (int i = 0; i < buf.memorySize; i += 5) {
            // GCC 11.2x64 Intel i7-2600 CPU 3.40GHz
            // Frames: 500, Elapsed: 7.181s, FPS: 69.6, 14.36ms/frame
            d->p0_msb = g->b0 >> 6;
            d->p0_lsb = ((g->b4 & 0b00000011) >> 0) | (g->b0 << 2);
            d->p1_msb = g->b1 >> 6;
            d->p1_lsb = ((g->b4 & 0b00001100) >> 2) | (g->b1 << 2);
            d->p2_msb = g->b2 >> 6;
            d->p2_lsb = ((g->b4 & 0b00110000) >> 4) | (g->b2 << 2);
            d->p3_msb = g->b3 >> 6;
            d->p3_lsb = ((g->b4 & 0b11000000) >> 6) | (g->b3 << 2);
            g++;
            d++;

            // GCC 11.2x64 Intel i7-2600 CPU 3.40GHz
            // Frames: 500, Elapsed: 4.406s, FPS: 113.5, 8.81ms/frame
            // p[j++] = ((g->b4 & 0b00000011) >> 0) | (g->b0 << 2);
            // p[j++] = g->b0 >> 6;
            // p[j++] = ((g->b4 & 0b00001100) >> 2) | (g->b1 << 2);
            // p[j++] = g->b1 >> 6;
            // p[j++] = ((g->b4 & 0b00110000) >> 4) | (g->b2 << 2);
            // p[j++] = g->b2 >> 6;
            // p[j++] = ((g->b4 & 0b11000000) >> 6) | (g->b3 << 2);
            // p[j++] = g->b3 >> 6;
            // g++;

            // GCC 11.2x64 Intel i7-2600 CPU 3.40GHz
            // Frames: 500, Elapsed: 5.208s, FPS: 96.0, 10.42ms/frame
            // p[j++] = ((b[i+4] & 0b00000011) >> 0) | (b[i+0] << 2);
            // p[j++] = b[i+0] >> 6;
            // p[j++] = ((b[i+4] & 0b00001100) >> 2) | (b[i+1] << 2);
            // p[j++] = b[i+1] >> 6;
            // p[j++] = ((b[i+4] & 0b00110000) >> 4) | (b[i+2] << 2);
            // p[j++] = b[i+2] >> 6;
            // p[j++] = ((b[i+4] & 0b11000000) >> 6) | (b[i+3] << 2);
            // p[j++] = b[i+3] >> 6;
        }
    #elif defined(BITS_12)
        uint16_t p0, p1;
        uint8_t *t0 = (uint8_t*)&p0;
        uint8_t *t1 = (uint8_t*)&p1;
        PixelGroupSrc *g = (PixelGroupSrc*)b;
        PixelGroupDst *d = (PixelGroupDst*)img;
        for (int i = 0; i < buf.memorySize; i += 3) {
            // GCC 11.2x64 Intel i7-2600 CPU 3.40GHz
            // Frames: 500, Elapsed: 1.209s, FPS: 413.6, 2.42ms/frame
            // d->p0_lsb = (g->b2 & 0x0F) | (g->b0 << 4);
            // d->p0_msb = g->b0 >> 4;
            // d->p1_lsb = (g->b2 >> 4) | (g->b1 << 4);
            // d->p1_msb = g->b1 >> 4;
            // g++;
            // d++;

            // GCC 11.2x64 Intel i7-2600 CPU 3.40GHz
            // Frames: 500, Elapsed: 1.310s, FPS: 381.7, 2.62ms/frame
            p[j++] = (g->b2 & 0x0F) | (g->b0 << 4);
            p[j++] = g->b0 >> 4;
            p[j++] = (g->b2 >> 4) | (g->b1 << 4);
            p[j++] = g->b1 >> 4;
            g++;

            // GCC 11.2x64 Intel i7-2600 CPU 3.40GHz
            // Frames: 500, Elapsed: 1.379s, FPS: 362.6, 2.76ms/frame
            // p[j++] = (b[i+2] & 0x0F) | (b[i+0] << 4);
            // p[j++] = b[i+0] >> 4;
            // p[j++] = (b[i+2] >> 4) | (b[i+1] << 4);
            // p[j++] = b[i+1] >> 4;

            // GCC 11.2x64 Intel i7-2600 CPU 3.40GHz
            // Frames: 500, Elapsed: 7.217s, FPS: 69.3, 14.43ms/frame
            // t0[0] = (b[i+2] & 0x0F) | (b[i+0] << 4);
            // t0[1] = b[i+0] >> 4;
            // t1[0] = (b[i+2] >> 4) | (b[i+1] << 4);
            // t1[1] = b[i+1] >> 4;
            // img[j++] = p0;
            // img[j++] = p1;

            // GCC 11.2x64 Intel i7-2600 CPU 3.40GHz
            // Frames: 500, Elapsed: 6.396s, FPS: 78.2, 12.79ms/frame
            // t0[1] = b[i+0];
            // t0[0] = (b[i+2] << 4);
            // t1[1] = b[i+1];
            // t1[0] = b[i+2] & 0xF0;
            // p0 = p0 >> 4;
            // p1 = p1 >> 4;
            // img[j++] = p0;
            // img[j++] = p1;
        }
    #endif
        elapsed += (clock() - tm)/(double)CLOCKS_PER_SEC;
        res = peak_Frame_Release(hCam, frame);
        CHECK_ERR("Release frame")
    }

    int data_max = 0;
    int type_max = (1 << BITS) - 1;
    for (int i = 0; i < w*h; i++) {
        if (img[i] > data_max) data_max = img[i];
    }
    printf("type_max: %d, data_max: %d\n", type_max, data_max);

    printf("Frames: %d, Elapsed: %.3fs, FPS: %.1f, %.2fms/frame\n", 
        FRAMES, elapsed, FRAMES/elapsed, elapsed/(double)FRAMES*1000); \

    if (!write_pgm(FILENAME, (uint8_t*)img, w, h, BITS))
        PRINT_ERR("Open file for writing");

    printf("Done\n");

stop:
    free(img);

    if (hCam != PEAK_INVALID_HANDLE) {
        if (peak_Acquisition_IsStarted(hCam))
            peak_Acquisition_Stop(hCam);
        peak_Camera_Close(hCam);
    }
    peak_Library_Exit();
    return 0;
}
