/*
Demo SDK adapter library for Beam Inspector.

It doesn't really "adapt" any camera SDK. Instead, it renders a synthetic beam image 
similar to how the Virtual Camera in Beam Inspector does.
The purpose of this library is to develop and test the adapter interface
and to show the simplest example of how to implement a custom adapter library.

GCC:
gcc adapter_demo.c -shared -Wall -Werror -Wl,--exclude-all-symbols -I ../adapter -o adapter_demo.dll

MSVC:
cl /LD /Wall /WX /wd5045 /wd4820 /external:W1 /external:I ../adapter adapter_demo.c

Disabled warinings:
5045 - something about Spectre protection has been automatically added
4820 - X bytes padding added after data member Y
*/
#include "adapter.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ADAPTER_NAME "Demo Dll Adapter"
#define ADAPTER_VER "1.0.0"
#define CAM_COUNT 3
#define CAM_LR_ID 1
#define CAM_MR_ID 2
#define CAM_HR_ID 3

typedef struct {
  int w;
  int h;
  int dx;
  int dy;
  int xc;
  int yc;
  int p;
  int phi;
  int bpp;
} BeamProps;

typedef struct { double val, min, span; } BeamOffset;

// For demo, all info about cameras is preallocated and doesn't change.
IBeamCamInfo _cams[2];

// Beam Inspector opens only one camera at time.
// So for demo we don't bother with anything harder than single camera instance.
struct {
  IBeamCamId id;
  IBeamStatus status;
  IBeamFrameInfo frame;
  BeamProps beam;
  struct {
    BeamOffset dx, dy, xc, yc, phi;
  } offset;
} _cam;

#define STATUS_OK NULL

static IBeamStatus* errorStatus(const char* text)
{
  strcpy_s(_cam.status.text, IBEAM_ERROR_LEN, text);
  _cam.status.code = IBEAM_STATUS_ERROR;
  return &_cam.status;
}

IBEAM_API IBeam_init(void)
{
  _cams[0].id = CAM_LR_ID;
  strcpy_s(_cams[0].displayName, IBEAM_NAME_LEN, "Low-Res Cam");

  _cams[1].id = CAM_MR_ID;
  strcpy_s(_cams[1].displayName, IBEAM_NAME_LEN, "Mid-Res Cam");

  _cams[2].id = CAM_HR_ID;
  strcpy_s(_cams[2].displayName, IBEAM_NAME_LEN, "High-Res Cam");

  _cam.id = IBEAM_BAD_CAMERA_ID;

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

IBEAM_API IBeam_getAdapterInfo(IBeamAdapterInfo* info)
{
  strcpy_s(info->displayName, IBEAM_NAME_LEN, ADAPTER_NAME);
  strcpy_s(info->displayVersion, IBEAM_NAME_LEN, ADAPTER_VER);
  info->apiVersion = IBEAM_API_V1;
  return STATUS_OK;
}

IBEAM_API IBeam_getCameraList(IBeamCamList* camList, size_t* camCount)
{
  *camList = _cams;
  *camCount = CAM_COUNT;
  return STATUS_OK;
}

IBEAM_API IBeam_openCamera(IBeamCamId camId, IBeamCamPtr *camPtr)
{
  if (_cam.id != IBEAM_BAD_CAMERA_ID) {
    if (_cam.id == camId)
      return errorStatus("Camera is already opened");
    return errorStatus("Another camera is already opened");
  }

  // Beam Inspector opens only one camera at time.
  // So for demo we don't bother with anything harder than single camera object.
  switch (camId) {
    case CAM_LR_ID:
      _cam.frame.pixelWidth = 0;
      _cam.frame.pixelHeight = 0;
      _cam.frame.width = 640;
      _cam.frame.height = 480;
      _cam.frame.bpp = 8;
      _cam.beam.bpp = 8;
      _cam.beam.p = 250;
      break;
    case CAM_MR_ID:
      _cam.frame.pixelWidth = 2.5;
      _cam.frame.pixelHeight = 2.5;
      _cam.frame.width = 2592;
      _cam.frame.height = 2048;
      _cam.frame.bpp = 12;
      _cam.beam.bpp = 12;
      _cam.beam.p = 2500;
      break;
    case CAM_HR_ID:
      _cam.frame.pixelWidth = 4.5;
      _cam.frame.pixelHeight = 4.5;
      _cam.frame.width = 5120;
      _cam.frame.height = 5120;
      _cam.frame.bpp = 16;
      _cam.beam.bpp = 16;
      _cam.beam.p = 50000;
      break;
    default:
      return errorStatus("Unknown camera ID");
  }
  _cam.beam.w = _cam.frame.width;
  _cam.beam.h = _cam.frame.height;
  _cam.beam.dx = _cam.beam.w/2;
  _cam.beam.dy = _cam.beam.dx/4*3;
  _cam.beam.xc = _cam.beam.w/2;
  _cam.beam.yc = _cam.beam.h/2;
  _cam.beam.phi = 12;

  #define OFFSET(off, start, factor) \
    off.val = start; \
    off.span = off.val * factor; \
    off.min = off.val - off.span/2.0;

  OFFSET(_cam.offset.dx, _cam.beam.dx, 0.03);
  OFFSET(_cam.offset.dy, _cam.beam.dy, 0.03);
  OFFSET(_cam.offset.xc, _cam.beam.xc, 0.03);
  OFFSET(_cam.offset.yc, _cam.beam.yc, 0.03);
  OFFSET(_cam.offset.phi, _cam.beam.phi, 2);

  #undef OFFSET

  _cam.id = camId;
  *camPtr = &_cam;
  return STATUS_OK;
}

#define IF_OPEN if (&_cam != camPtr) return errorStatus("Camera is not opened");

IBEAM_API IBeam_closeCamera(IBeamCamPtr camPtr)
{
  IF_OPEN
  _cam.id = IBEAM_BAD_CAMERA_ID;
  return STATUS_OK;
}

IBEAM_API IBeam_getFrameInfo(IBeamCamPtr camPtr, IBeamFrameInfo *info)
{
  IF_OPEN
  *info = _cam.frame;
  return STATUS_OK;
}

static void renderBeam(BeamProps *b, uint8_t *buf) {
  #define sqr(s) ((s)*(s))
  memset(buf, 0, b->w * b->h * (b->bpp > 8 ? 2 : 1));
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
        if (b->bpp > 8) {
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

IBEAM_API IBeam_getFrame(IBeamCamPtr camPtr, uint8_t *buf)
{
  IF_OPEN
  _cam.beam.dx = jitter(&_cam.offset.dx);
  _cam.beam.dy = jitter(&_cam.offset.dy);
  _cam.beam.xc = jitter(&_cam.offset.xc);
  _cam.beam.yc = jitter(&_cam.offset.yc);
  _cam.beam.phi = jitter(&_cam.offset.phi);
  renderBeam(&_cam.beam, buf);
  return STATUS_OK;
}
