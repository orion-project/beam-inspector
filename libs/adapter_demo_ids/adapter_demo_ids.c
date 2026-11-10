#include "adapter.h"

#include <ids_peak_comfort_c/ids_peak_comfort_c.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ADAPTER_NAME "Demo IDS Adapter"

#define STATUS_OK IBEAM_STATUS_OK
#define STATUS_UNSUPPORTED_PIXEL_FORMAT (PEAK_STATUS_IO + 1000)

#define FRAME_TIMEOUT 5000

IBEAM_API IBeam_init(void)
{
  peak_status res = peak_Library_Init();
  if (PEAK_ERROR(res))
    return res;
  return STATUS_OK;
}

IBEAM_API IBeam_exit(void)
{
  peak_status res = peak_Library_Exit();
  if (PEAK_ERROR(res))
    return res;
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
  peak_status res = peak_CameraList_Update(count);
  if (PEAK_ERROR(res))
    return res;
  return STATUS_OK;
}

IBEAM_API IBeam_getCameraList(IBeam_CameraInfo* camList, size_t listSize)
{
  size_t camCount = listSize;
  peak_camera_descriptor* cams = malloc(sizeof(peak_camera_descriptor)*camCount);
  peak_status res = peak_CameraList_Get(cams, &camCount);
  if (PEAK_ERROR(res))
    return res;
  for (int i = 0; i < camCount; i++) {
    camList[i].cameraId = cams[i].cameraID;

    char *serial = cams[i].serialNumber;
    int serLen = strlen(serial);
    if (serLen >= 4)
      serial += (serLen - 4);

    sprintf(camList[i].displayName, "%s (*%s)", cams[i].modelName, serial);
  }
  free(cams);
  return STATUS_OK;
}

IBEAM_API IBeam_getStatusText(IBeam_Status status, char* buf, size_t bufSize)
{
  #define _(s, txt) \
    case s: \
      if (bufSize < strlen(txt)+1) \
        return IBEAM_STATUS_SMALL_BUF; \
      strcpy_s(buf, bufSize, txt); \
      return STATUS_OK;

  switch (status) {
    _(STATUS_OK, "OK")
    _(STATUS_UNSUPPORTED_PIXEL_FORMAT, "Unsupported pixel format")
    default:
      peak_status res = peak_Library_GetLastError(&status, buf, &bufSize);
      if (PEAK_ERROR(res))
        return res;
  }
  return STATUS_OK;
  #undef _
}

IBEAM_API IBeam_openCamera(IBeam_CameraId camId, IBeam_Handle *hCam)
{
  peak_camera_handle camHandle;
  peak_status res = peak_Camera_Open(camId, &camHandle);
  if (PEAK_ERROR(res)) {
    hCam = IBEAM_BAD_HANDLE;
    return res;
  }

  *hCam = (IBeam_Handle)camHandle;

  res = peak_Acquisition_Start(camHandle, PEAK_INFINITE);
  if (PEAK_ERROR(res)) {
    // Ignore possible error return the first occured error
    peak_Camera_Close(camHandle);
    hCam = IBEAM_BAD_HANDLE;
    return res;
  }
  
  return STATUS_OK;
}

IBEAM_API IBeam_closeCamera(IBeam_Handle hCam)
{
  if (peak_Acquisition_IsStarted((peak_camera_handle)hCam)) {
    // Ignore possible error and try to close anyway
    peak_Acquisition_Stop((peak_camera_handle)hCam);
  }

  peak_status res = peak_Camera_Close((peak_camera_handle)hCam);
  if (PEAK_ERROR(res))
    return res;
  return STATUS_OK;
}

IBEAM_API IBeam_getFrameInfo(IBeam_Handle hCam, IBeam_FrameInfo *info)
{
  peak_roi roi;
  peak_status res = peak_ROI_Get((peak_camera_handle)hCam, &roi);
  if (PEAK_ERROR(res))
    return res;

  info->width = roi.size.width;
  info->height = roi.size.height;

  if (PEAK_IS_READABLE(peak_GFA_Feature_GetAccessStatus(
    (peak_camera_handle)hCam, PEAK_GFA_MODULE_REMOTE_DEVICE, "SensorPixelWidth"))) {
    double pixelW;
    res = peak_GFA_Float_Get((peak_camera_handle)hCam, PEAK_GFA_MODULE_REMOTE_DEVICE, "SensorPixelWidth", &pixelW);
    if (PEAK_SUCCESS(res)) {
      info->pixelWidth = pixelW;
      info->pixelHeight = pixelW;
    }
  }

  peak_pixel_format pixelFormat;
  res = peak_PixelFormat_Get((peak_camera_handle)hCam, &pixelFormat);
  if (PEAK_ERROR(res))
    return res;

  // Support only the simplest pixel format in demo
  if (pixelFormat != PEAK_PIXEL_FORMAT_MONO8)
    return STATUS_UNSUPPORTED_PIXEL_FORMAT;

  info->bpp = 8;
  return STATUS_OK;
}

IBEAM_API IBeam_getFrame(IBeam_Handle hCam, uint8_t *buf)
{
  peak_frame_handle frame;
  peak_status res = peak_Acquisition_WaitForFrame((peak_camera_handle)hCam, FRAME_TIMEOUT, &frame);
  if (PEAK_ERROR(res))
    return res;

  peak_buffer peakBuf;
  res = peak_Frame_Buffer_Get(frame, &peakBuf);
  if (PEAK_ERROR(res))
    return res;

  memcpy_s(buf, peakBuf.memorySize, peakBuf.memoryAddress, peakBuf.memorySize);

  res = peak_Frame_Release((peak_camera_handle)hCam, frame);
  if (PEAK_ERROR(res))
    return res;

  return STATUS_OK;
}
