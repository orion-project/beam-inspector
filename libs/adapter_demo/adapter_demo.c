/*
Demo SDK adapter library for Beam Inspector.

It doesn't really "adapt" any camera SDK. Instead, it renders a synthetic beam image 
similar to how the Virtual Camera in Beam Inspector does.
The purpose of this library is to develop and test the adapter interface
and to show the simplest example of how to implement a custom adapter library.

GCC:
gcc adapter_demo.c -shared -Wall -Werror -Wl,--exclude-all-symbols -I ../adapter -o adapter_demo.dll

MSVC:
cl /LD /Wall /WX /wd5045 /external:W1 /external:I ../adapter adapter_demo.c
*/
#include "adapter.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ADAPTER_NAME "Demo Dll Adapter"
#define CAM_COUNT 2
#define CAM_LR_ID 1
#define CAM_HR_ID 2

typedef struct {
  int w;
  int h;
  int dx;
  int dy;
  int xc;
  int yc;
  int p;
  int phi;
  int bpp; // here it's Bytes Per Pixel (not Bits)
} BeamProps;

typedef struct { double val, min, span; } BeamOffset;

// Beam Inspector opens only one camera at time.
// So for demo we don't bother with anything harder than single camera instance.
struct {
  IBeam_CameraId id;
  IBeam_Handle hCam;
  IBeam_Handle nextHandle;
  IBeam_FrameInfo img;
  BeamProps beam;
  struct {
    BeamOffset dx, dy, xc, yc, phi;
  } offset;
} CAM;

enum {
  STATUS_OK = IBEAM_STATUS_OK,
  STATUS_UNKNOWN,
  STATUS_SMALL_BUF,
  STATUS_UNKNOWN_CAM_ID,
  STATUS_ALREADY_OPENED,
  STATUS_NOT_OPENED,
  STATUS_ABORT = IBEAM_STATUS_ABORT,
};

IBEAM_API IBeam_getStatusText(IBeam_Status status, char* buf, size_t bufSize)
{
  #define _(s, txt) \
    case s: \
      if (bufSize < strlen(txt)+1) \
        return STATUS_SMALL_BUF; \
      strcpy_s(buf, bufSize, txt); \
      return STATUS_OK;
  
  switch (status) {
    _(STATUS_OK, "OK")
    _(STATUS_SMALL_BUF,
      "Buffer addressed by function argument is too small "
      "to receive all the data the function is going to put")
    _(STATUS_UNKNOWN_CAM_ID, "Unknown camera ID")
    _(STATUS_ALREADY_OPENED, "Camera is already opened")
    _(STATUS_NOT_OPENED, "Camera is not opened")
    _(STATUS_ABORT, "Camera stopped unexpectedly")
  }
  return STATUS_UNKNOWN;
  #undef _
}

IBEAM_API IBeam_init(void)
{
  CAM.id = IBEAM_BAD_CAMERA_ID;
  CAM.hCam = IBEAM_BAD_HANDLE;
  CAM.nextHandle = IBEAM_BAD_HANDLE;

  printf("%s: init\n", ADAPTER_NAME);
  fflush(stdout);
  return STATUS_OK;
}

IBEAM_API IBeam_exit(void)
{
  printf("%s: exit\n", ADAPTER_NAME);
  fflush(stdout);
  return STATUS_OK;
}

IBEAM_API IBeam_getAdapterInfo(IBeam_AdapterInfo* info)
{
  strcpy_s(info->displayName, IBEAM_NAME_LEN, ADAPTER_NAME);
  strcpy_s(info->displayVersion, IBEAM_NAME_LEN, "1.0.0");
  info->apiVersion = IBEAM_API_V1;
  return STATUS_OK;
}

IBEAM_API IBeam_getCameraCount(size_t* count)
{
  *count = CAM_COUNT;
  return STATUS_OK;
}

IBEAM_API IBeam_getCameraList(IBeam_CameraInfo* camList, size_t listSize)
{
  if (listSize < CAM_COUNT)
    return STATUS_SMALL_BUF;

  camList[0].cameraId = CAM_LR_ID;
  strcpy_s(camList[0].displayName, IBEAM_NAME_LEN, "Low-Res Cam");

  camList[1].cameraId = CAM_HR_ID;
  strcpy_s(camList[1].displayName, IBEAM_NAME_LEN, "High-Res Cam");

  return STATUS_OK;
}

IBEAM_API IBeam_openCamera(IBeam_CameraId camId, IBeam_Handle *hCam)
{
  if (camId != CAM_LR_ID && camId != CAM_HR_ID)
    return STATUS_UNKNOWN_CAM_ID;

  if (CAM.id == camId && CAM.hCam != IBEAM_BAD_HANDLE)
    return STATUS_ALREADY_OPENED;

  // Beam Inspector opens only one camera at time.
  // So for demo we don't bother with anything harder than single camera object.
  CAM.id = camId;
  CAM.hCam = ++CAM.nextHandle;
  if (CAM.id == CAM_LR_ID) {
    CAM.img.pixelWidth = 0;
    CAM.img.pixelHeight = 0;
    CAM.img.width = 640;
    CAM.img.height = 480;
    CAM.img.bpp = 8;
    CAM.beam.bpp = 1;
    CAM.beam.p = 250;
  } else {
    CAM.img.pixelWidth = 2.5;
    CAM.img.pixelHeight = 2.5;
    CAM.img.width = 2592;
    CAM.img.height = 2048;
    CAM.img.bpp = 12;
    CAM.beam.bpp = 2;
    CAM.beam.p = 2500;
  }
  CAM.beam.w = CAM.img.width;
  CAM.beam.h = CAM.img.height;
  CAM.beam.dx = CAM.beam.w/2;
  CAM.beam.dy = CAM.beam.dx/4*3;
  CAM.beam.xc = CAM.beam.w/2;
  CAM.beam.yc = CAM.beam.h/2;
  CAM.beam.phi = 12;

  #define OFFSET(off, start, factor) \
    off.val = start; \
    off.span = off.val * factor; \
    off.min = off.val - off.span/2.0;

  OFFSET(CAM.offset.dx, CAM.beam.dx, 0.03);
  OFFSET(CAM.offset.dy, CAM.beam.dy, 0.03);
  OFFSET(CAM.offset.xc, CAM.beam.xc, 0.03);
  OFFSET(CAM.offset.yc, CAM.beam.yc, 0.03);
  OFFSET(CAM.offset.phi, CAM.beam.phi, 2);

  #undef OFFSET

  *hCam = CAM.hCam;
  return STATUS_OK;
}

#define IF_OPEN if (CAM.hCam != hCam) return STATUS_NOT_OPENED;

IBEAM_API IBeam_closeCamera(IBeam_Handle hCam)
{
  IF_OPEN
  CAM.id = IBEAM_BAD_CAMERA_ID;
  CAM.hCam = IBEAM_BAD_HANDLE;
  return STATUS_OK;
}

IBEAM_API IBeam_getFrameInfo(IBeam_Handle hCam, IBeam_FrameInfo *info)
{
  IF_OPEN
  *info = CAM.img;
  return STATUS_OK;
}

static void renderBeam(BeamProps *b, uint8_t *buf) {
  #define sqr(s) ((s)*(s))
  memset(buf, 0, b->w * b->h * b->bpp);
  const double r2 = sqr(b->dx/2.0);
  const double cos_phi = cos(b->phi * 3.14159265358979323846 / 180.0);
  const double sin_phi = sin(b->phi * 3.14159265358979323846 / 180.0);
  const double el = b->dx / (double)(b->dy);
  const int x_min = -b->dx*6/10, x_max = b->dx*6/10;
  const int y_min = -b->dy*6/10, y_max = b->dy*6/10;
  const double p = b->p;
  for (int y = y_min; y < y_max; y++) {
    const double y2 = sqr(y*el);
    for (int x = x_min; x < x_max; x++) {
      const int x1 = (int)(b->xc + x*cos_phi - y*sin_phi);
      const int y1 = (int)(b->yc + x*sin_phi + y*cos_phi);
      if (y1 >= 0 && y1 < b->h && x1 >= 0 && x1 < b->w) {
        const double t = 1+ (-2*(sqr(x) + y2)/r2) /5.0;
        const double v = p * t*t*t*t*t;
        const int i = y1*b->w + x1;
        if (b->bpp > 1) {
          ((uint16_t*)buf)[i] = (uint16_t)v;
        } else {
          buf[i] = (uint8_t)v;
        }
      }
    }
  }
  #undef sqr
}

static int jitter(BeamOffset *off)
{
  #define rnd ((double)rand() / (double)RAND_MAX)
  const double bias = off->span / 4.0;
  off->val = fabs(off->val + rnd*bias - bias*0.5);
  if (off->val > off->span)
    off->val = off->span - rnd*bias;
  return (int)(off->val + off->min);
  #undef rnd
}

IBEAM_API IBeam_getFrame(IBeam_Handle hCam, uint8_t *buf)
{
  IF_OPEN
  CAM.beam.dx = jitter(&CAM.offset.dx);
  CAM.beam.dy = jitter(&CAM.offset.dy);
  CAM.beam.xc = jitter(&CAM.offset.xc);
  CAM.beam.yc = jitter(&CAM.offset.yc);
  CAM.beam.phi = jitter(&CAM.offset.phi);
  renderBeam(&CAM.beam, buf);
  return STATUS_OK;
}
