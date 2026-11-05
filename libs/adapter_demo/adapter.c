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
#define CAM_ID_LR 1
#define CAM_ID_HR 2
#define CAM_COUNT 2

#define STATUS_OK 0
#define STATUS_UNKNOWN 1000
#define STATUS_SMALL_BUF 1001

IBEAM_API IBeam_getAdapterInfo(IBeam_AdapterInfo* info)
{
  strcpy(info->displayName, ADAPTER_NAME);
  strcpy(info->displayVersion, "1.0.0");
  info->apiVersion = IBEAM_API_V1;
  return STATUS_OK;
}

IBEAM_API IBeam_init()
{
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

IBEAM_API IBeam_getCameraList(IBeam_CameraInfo* cameraList, size_t cameraCount)
{
  if (cameraCount < CAM_COUNT)
    return STATUS_SMALL_BUF;

  cameraList[0].cameraId = CAM_ID_LR;
  strcpy(cameraList[0].displayName, "Low-Res Cam");

  cameraList[1].cameraId = CAM_ID_HR;
  strcpy(cameraList[1].displayName, "High-Res Cam");

  return STATUS_OK;
}

IBeam_Status copyStatusText(char* buf, size_t bufSize, const char* errorText)
{
  if (bufSize < strlen(errorText)+1)
    return STATUS_SMALL_BUF;
  strcpy(buf, errorText);
  return STATUS_OK;
}

IBEAM_API IBeam_getStatusText(IBeam_Status status, char* buf, size_t bufSize)
{
  switch (status) {
    case STATUS_OK:
      return copyStatusText(buf, bufSize, "OK");

    case STATUS_SMALL_BUF:
      return copyStatusText(buf, bufSize, 
        "Buffer addressed by function argument is too small "
        "to receive all the data the function is going to put");
  }
  return STATUS_UNKNOWN;
}
