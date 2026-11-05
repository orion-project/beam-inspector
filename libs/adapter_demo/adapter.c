/*
Demo SDK adapter library for Beam Inspector.

It doesn't really "adapt" any camera SDK. Instead, it renders a synthetic beam image 
similar to how the Virtual Camera in Beam Inspector does.
The purpose of this library is to develop and test the adapter interface
and to show the simplest example of how to implement a custom adapter library.

gcc adapter.c -o adapter.dll -shared -Wall -Werror -Wl,--exclude-all-symbols
*/
#include "../adapter/adapter.h"

#include <stdio.h>
#include <string.h>

#define ADAPTER_NAME "Demo Dll Adapter"
#define CAM_COUNT 2
#define CAM_LR_ID 1
#define CAM_LR_W 640
#define CAM_LR_H 480
#define CAM_LR_BPP 8
#define CAM_HR_ID 2
#define CAM_HR_W 1920
#define CAM_HR_H 1080
#define CAM_HR_BPP 12

enum {
  STATUS_OK,
  STATUS_UNKNOWN,
  STATUS_SMALL_BUF,
  STATUS_UNKNOWN_CAM_ID,
  STATUS_ALREADY_OPENED,
  STATUS_NOT_OPENED,
};

// Beam Inspector opens only one camera at time.
// So for demo we don't bother with anything harder than single camera instance.
struct {
  IBeam_CameraId id;
  IBeam_Handle hCam;
  IBeam_Handle nextHandle;
} C;

IBEAM_API IBeam_getAdapterInfo(IBeam_AdapterInfo* info)
{
  strcpy(info->displayName, ADAPTER_NAME);
  strcpy(info->displayVersion, "1.0.0");
  info->apiVersion = IBEAM_API_V1;
  return STATUS_OK;
}

IBEAM_API IBeam_init()
{
  C.id = IBEAM_BAD_CAMERA_ID;
  C.hCam = IBEAM_BAD_HANDLE;
  C.nextHandle = IBEAM_BAD_HANDLE;

  printf("%s: init\n", ADAPTER_NAME);
  fflush(stdout);
  return STATUS_OK;
}

IBEAM_API IBeam_exit()
{
  printf("%s: exit\n", ADAPTER_NAME);
  fflush(stdout);
  return STATUS_OK;
}

IBEAM_API IBeam_getCameraCount(size_t* count)
{
  *count = CAM_COUNT;
  return STATUS_OK;
}

IBEAM_API IBeam_getCameraList(IBeam_CameraInfo* camList, size_t camCount)
{
  if (camCount < CAM_COUNT)
    return STATUS_SMALL_BUF;

  camList[0].cameraId = CAM_LR_ID;
  strcpy(camList[0].displayName, "Low-Res Cam");

  camList[1].cameraId = CAM_HR_ID;
  strcpy(camList[1].displayName, "High-Res Cam");

  return STATUS_OK;
}

IBEAM_API IBeam_getStatusText(IBeam_Status status, char* buf, size_t bufSize)
{
  #define _(s, txt) \
    case s: \
      if (bufSize < strlen(txt)+1) \
        return STATUS_SMALL_BUF; \
      strcpy(buf, txt); \
      return STATUS_OK;
  switch (status) {
    _(STATUS_OK, "OK")
    _(STATUS_SMALL_BUF,
      "Buffer addressed by function argument is too small "
      "to receive all the data the function is going to put")
    _(STATUS_UNKNOWN_CAM_ID, "Unknown camera ID")
    _(STATUS_ALREADY_OPENED, "Camera is already opened")
    _(STATUS_NOT_OPENED, "Camera is not opened")
  }
  return STATUS_UNKNOWN;
  #undef _
}

IBEAM_API IBeam_openCamera(IBeam_CameraId camId, IBeam_Handle *hCam)
{
  if (camId != CAM_LR_ID && camId != CAM_HR_ID)
    return STATUS_UNKNOWN_CAM_ID;

  if (C.id == camId && C.hCam != IBEAM_BAD_HANDLE)
    return STATUS_ALREADY_OPENED;

  // Beam Inspector opens only one camera at time.
  // So for demo we don't bother with anything harder than single camera object.
  C.id = camId;
  C.hCam = ++C.nextHandle;
  *hCam = C.hCam;
  return STATUS_OK;
}

#define IF_OPEN if (C.hCam != hCam) return STATUS_NOT_OPENED;

IBEAM_API IBeam_closeCamera(IBeam_Handle hCam)
{
  IF_OPEN
  C.id = IBEAM_BAD_CAMERA_ID;
  C.hCam = IBEAM_BAD_HANDLE;
  return STATUS_OK;
}

IBEAM_API IBeam_getFrameInfo(IBeam_Handle hCam, IBeam_FrameInfo *info)
{
  IF_OPEN
  if (C.id == CAM_LR_ID) {
    info->width = CAM_LR_W;
    info->height = CAM_LR_H;
    info->bpp = CAM_LR_BPP;
  } else {
    info->width = CAM_HR_W;
    info->height = CAM_HR_H;
    info->bpp = CAM_HR_BPP;
    info->pixelWidth = 2.5;
    info->pixelHeight = 2.5;
  }
  return STATUS_OK;
}
