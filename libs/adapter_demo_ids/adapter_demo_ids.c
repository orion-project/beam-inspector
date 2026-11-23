/*
Demo SDK adapter library for Beam Inspector.

The adapter wraps the IDS peak SDK (https://en.ids-imaging.com/ids-peak.html)
which should be installed separately and path to it provided when building the adapter.
*/

#include "adapter.h"

#include <ids_peak_comfort_c/ids_peak_comfort_c.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ADAPTER_NAME "Demo IDS Adapter"
#define ADAPTER_VER "1.0.0"

#define LOG_ID "AdapterIDS"
#define LOG(format, ...) { printf (LOG_ID ": " format "\n", __VA_ARGS__); fflush(stdout); }

#define STATUS_OK NULL

#define FRAME_TIMEOUT 5000

// Exchange buffer for adapter interface
IBeamCamList _cams;
// Internal camera meta-data returned by SDK
peak_camera_descriptor* _camsRaw;
// Size of the both above lists, they must be of the same size
size_t _camCount;

// Beam Inspector opens only one camera at time.
// So for demo we don't bother with anything harder than single camera instance.
struct {
  IBeamCamId id;
  IBeamStatus status;
  peak_camera_handle peakHandle;
  IBeamPixelFormats pixelFormats;
  peak_pixel_format pixelFormat;
  size_t pixelFormatCount;
} _cam;

static IBeamStatus* errorStatus(const char* text)
{
  strcpy_s(_cam.status.text, IBEAM_ERROR_LEN, text);
  _cam.status.code = IBEAM_STATUS_ERROR;
  return &_cam.status;
}

static IBeamStatus* notImplemented()
{
  strcpy_s(_cam.status.text, IBEAM_ERROR_LEN, "Not implemented");
  _cam.status.code = IBEAM_STATUS_NOT_IMPLEMENTED;
  return &_cam.status;
}

#define CHECK_ERR(func, res) { \
  peak_status code = (res); \
  if (PEAK_ERROR(code)) { \
    char msg[IBEAM_ERROR_LEN]; \
    size_t msgSize = IBEAM_ERROR_LEN; \
    if (PEAK_ERROR(peak_Library_GetLastError(&code, msg, &msgSize))) \
      snprintf(_cam.status.text, IBEAM_ERROR_LEN, "%s: errcode %#x", func, code); \
    else \
      snprintf(_cam.status.text, IBEAM_ERROR_LEN, "%s: %s", func, msg); \
    _cam.status.code = IBEAM_STATUS_ERROR; \
    return &_cam.status; \
  } \
}

IBEAM_API IBeam_init(const IBeamAdapterArgs *args)
{
  _cams = NULL;
  _camsRaw = NULL;

  _cam.id = IBEAM_BAD_CAMERA_ID;
  _cam.peakHandle = NULL;
  _cam.pixelFormats = NULL;
  _cam.pixelFormatCount = 0;

  printf("%s: init\n  libDir: %s\n  sdkDir: %s\n",
    ADAPTER_NAME, args->libDir, args->sdkDir);
  fflush(stdout);

  CHECK_ERR("iniLibrary", peak_Library_Init());
  return STATUS_OK;
}

IBEAM_API IBeam_exit(void)
{
  if (_cams)
    free(_cams);
  if (_camsRaw)
    free(_camsRaw);

  CHECK_ERR("exiLibrary", peak_Library_Exit());
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
  if (_cams)
    free(_cams);
  if (_camsRaw)
    free(_camsRaw);

  CHECK_ERR("updateCameraList", peak_CameraList_Update(&_camCount));

  _cams = malloc(sizeof(IBeamCamInfo) * _camCount);
  _camsRaw = malloc(sizeof(peak_camera_descriptor) * _camCount);
  CHECK_ERR("getCameraList", peak_CameraList_Get(_camsRaw, &_camCount));

  for (size_t i = 0; i < _camCount; i++) {
    // IBeamCamId is expected to be persistent for the camera,
    // while the peak_camera_id is likely not persistent.
    // But for the demo we use it directly, for simplisity.
    _cams[i].id = _camsRaw[i].cameraID;

    char *serial = _camsRaw[i].serialNumber;
    int serLen = strlen(serial);
    if (serLen >= 4)
      serial += (serLen - 4);

    sprintf(_cams[i].displayName, "%s (*%s)", _camsRaw[i].modelName, serial);
  }

  *camList = _cams;
  *camCount = _camCount;
  return STATUS_OK;
}

IBEAM_API IBeam_openCamera(IBeamCamId camId, IBeamCamPtr* camPtr)
{
  if (_cam.peakHandle) {
    if (_cam.id == camId)
      return errorStatus("Camera is already opened");
    return errorStatus("Another camera is already opened");
  }

  peak_camera_id peakId = PEAK_INVALID_CAMERA_ID;
  for (size_t i = 0; i < _camCount; i++)
    if (_cams[i].id == camId) {
      peakId = _camsRaw[i].cameraID;
      break;
    }
  if (peakId == PEAK_INVALID_CAMERA_ID)
    return errorStatus("Unknown camera ID");

  peak_camera_handle peakHandle;
  CHECK_ERR("openCamera", peak_Camera_Open(peakId, &peakHandle));

  _cam.id = camId;
  _cam.peakHandle = peakHandle;
  *camPtr = &_cam;
  return STATUS_OK;
}

#define IF_OPEN if (&_cam != camPtr) return errorStatus("Camera is not opened");

IBEAM_API IBeam_closeCamera(IBeamCamPtr camPtr)
{
  IF_OPEN

  peak_camera_handle peakHandle = _cam.peakHandle; 

  // Reset opened camera even if the closing will fail (have nothing to do wth that)
  _cam.id = IBEAM_BAD_CAMERA_ID;
  _cam.peakHandle = NULL;
  if (_cam.pixelFormats) {
    free(_cam.pixelFormats);
    _cam.pixelFormats = NULL;
    _cam.pixelFormatCount = 0;
  }

  if (peak_Acquisition_IsStarted(peakHandle)) {
    // Ignore possible error and try to close anyway
    peak_Acquisition_Stop(peakHandle);
  }

  CHECK_ERR("closeCamera", peak_Camera_Close(peakHandle));
  return STATUS_OK;
}

static IBeamStatus* fillPixelFormats(IBeamCamPtr camPtr)
{
  size_t peakFormatCount = 0;
  CHECK_ERR("getPixelFormatCount", peak_PixelFormat_GetList(_cam.peakHandle, NULL, &peakFormatCount));
  peak_pixel_format* peakFormats = malloc(sizeof(peak_pixel_format) * peakFormatCount);
  CHECK_ERR("getPixelFormats", peak_PixelFormat_GetList(_cam.peakHandle, peakFormats, &peakFormatCount));
  // Return non all formats supported by the camera,
  // but only those we know how to decode
  _cam.pixelFormatCount = 0;
  for (size_t i = 0; i < peakFormatCount; i++) {
    if (peakFormats[i] == PEAK_PIXEL_FORMAT_MONO8 ||
        peakFormats[i] == PEAK_PIXEL_FORMAT_MONO10G40_IDS ||
        peakFormats[i] == PEAK_PIXEL_FORMAT_MONO12G24_IDS) {
      _cam.pixelFormatCount++;
    }
  }
  if (_cam.pixelFormatCount == 0)
    return errorStatus("Camera does not provide any of supported pixel formats");
  _cam.pixelFormats = malloc(sizeof(IBeamPixelFormat) * _cam.pixelFormatCount);
  if (!_cam.pixelFormats)
    return errorStatus("fillPixelFormats: out of memory");
  size_t fmtIdx = 0;
  for (size_t i = 0; i < peakFormatCount; i++) {
    if (peakFormats[i] == PEAK_PIXEL_FORMAT_MONO8) {
      _cam.pixelFormats[fmtIdx].id = peakFormats[i];
      _cam.pixelFormats[fmtIdx].name = "Mono8";
      _cam.pixelFormats[fmtIdx].bpp = 8;
      fmtIdx++;
    } else if (peakFormats[i] == PEAK_PIXEL_FORMAT_MONO10G40_IDS) {
      _cam.pixelFormats[fmtIdx].id = peakFormats[i];
      _cam.pixelFormats[fmtIdx].name = "Mono10G40_IDS";
      _cam.pixelFormats[fmtIdx].bpp = 10;
      fmtIdx++;
    } else if (peakFormats[i] == PEAK_PIXEL_FORMAT_MONO12G24_IDS) {
      _cam.pixelFormats[fmtIdx].id = peakFormats[i];
      _cam.pixelFormats[fmtIdx].name = "Mono12G24_IDS";
      _cam.pixelFormats[fmtIdx].bpp = 12;
      fmtIdx++;
    }
  }
  free(peakFormats);
  return STATUS_OK;
}

IBEAM_API IBeam_getPixelFormats(IBeamCamPtr camPtr, IBeamPixelFormats* pfList, size_t* pfCount)
{
  IF_OPEN

  if (!_cam.pixelFormats) {
    IBeamStatus* res = fillPixelFormats(camPtr);
    if (res)
      return res;
  }

  *pfList = _cam.pixelFormats;
  *pfCount = _cam.pixelFormatCount;
  return STATUS_OK;
}

IBEAM_API IBeam_startCapture(IBeamCamPtr camPtr, const IBeamCaptureArgs* args, IBeamFrameInfo *info)
{
  IF_OPEN

  // Request frame size
  peak_roi roi;
  CHECK_ERR("getROI", peak_ROI_Get(_cam.peakHandle, &roi));
  info->width = roi.size.width;
  info->height = roi.size.height;

  // Request pixel size
  if (PEAK_IS_READABLE(peak_GFA_Feature_GetAccessStatus(_cam.peakHandle,
    PEAK_GFA_MODULE_REMOTE_DEVICE, "SensorPixelWidth"))) {
    double pixelW;
    peak_status res = peak_GFA_Float_Get(_cam.peakHandle, 
      PEAK_GFA_MODULE_REMOTE_DEVICE, "SensorPixelWidth", &pixelW);
    if (PEAK_SUCCESS(res)) {
      info->pixelWidth = pixelW;
      info->pixelHeight = pixelW;
    }
  }

  // Init pixel format
  if (!_cam.pixelFormats) {
    IBeamStatus* res = fillPixelFormats(camPtr);
    if (res)
      return res;
  }
  info->bpp = 0;
  for (size_t i = 0; i < _cam.pixelFormatCount; i++) {
    if (_cam.pixelFormats[i].id == args->pixelFormat) {
      info->bpp = _cam.pixelFormats[i].bpp;
      _cam.pixelFormat = args->pixelFormat;
      break;
    }
  }
  if (info->bpp == 0)
    return errorStatus("Unsupported pixel format");
  CHECK_ERR("setPixelFormat", peak_PixelFormat_Set(_cam.peakHandle, args->pixelFormat));

  // Start capturing
  CHECK_ERR("startAcquisition", peak_Acquisition_Start(_cam.peakHandle, PEAK_INFINITE));
  return STATUS_OK;
}

typedef struct { uint8_t b0, b1, b2, b3, b4; } PixelGroup10g40;
static void cgn_convert_10g40_to_u16(uint8_t *dst, uint8_t *src, int sz) {
    int j = 0;
    PixelGroup10g40 *g = (PixelGroup10g40*)src;
    for (int i = 0; i < sz; i += 5) {
        dst[j++] = ((g->b4 & 0b00000011) >> 0) | (g->b0 << 2);
        dst[j++] = g->b0 >> 6;
        dst[j++] = ((g->b4 & 0b00001100) >> 2) | (g->b1 << 2);
        dst[j++] = g->b1 >> 6;
        dst[j++] = ((g->b4 & 0b00110000) >> 4) | (g->b2 << 2);
        dst[j++] = g->b2 >> 6;
        dst[j++] = ((g->b4 & 0b11000000) >> 6) | (g->b3 << 2);
        dst[j++] = g->b3 >> 6;
        g++;
    }
}

typedef struct { uint8_t b0, b1, b2; } PixelGroup12g24;
static void cgn_convert_12g24_to_u16(uint8_t *dst, uint8_t *src, int sz) {
    int j = 0;
    PixelGroup12g24 *g = (PixelGroup12g24*)src;
    for (int i = 0; i < sz; i += 3) {
        dst[j++] = (g->b2 & 0x0F) | (g->b0 << 4);
        dst[j++] = g->b0 >> 4;
        dst[j++] = (g->b2 >> 4) | (g->b1 << 4);
        dst[j++] = g->b1 >> 4;
        g++;
    }
}

IBEAM_API IBeam_getFrame(IBeamCamPtr camPtr, uint8_t *buf)
{
  IF_OPEN

  peak_frame_handle peakFrame;
  peak_status res = peak_Acquisition_WaitForFrame(_cam.peakHandle, FRAME_TIMEOUT, &peakFrame);
  if (res == PEAK_STATUS_ABORTED) {
    _cam.status.code = IBEAM_STATUS_ABORT;
    _cam.status.text[0] = '\0';
    return &_cam.status;
  }
  CHECK_ERR("waitForFrame", res);

  peak_buffer peakBuf;
  CHECK_ERR("getFrameBuffer", peak_Frame_Buffer_Get(peakFrame, &peakBuf));

  if (_cam.pixelFormat == PEAK_PIXEL_FORMAT_MONO8)
    memcpy_s(buf, peakBuf.memorySize, peakBuf.memoryAddress, peakBuf.memorySize);
  else if (_cam.pixelFormat == PEAK_PIXEL_FORMAT_MONO10G40_IDS)
    cgn_convert_10g40_to_u16(buf, peakBuf.memoryAddress, peakBuf.memorySize);
  else if (_cam.pixelFormat == PEAK_PIXEL_FORMAT_MONO12G24_IDS)
    cgn_convert_12g24_to_u16(buf, peakBuf.memoryAddress, peakBuf.memorySize);

  CHECK_ERR("releaseFrame", peak_Frame_Release(_cam.peakHandle, peakFrame));

  return STATUS_OK;
}

// This is a test call when BI checks if camera can change the parameter
#define CHECK_SUPPORT(func) \
  if (!max || !min || !step) { \
    peak_access_status res = func; \
    return (res != PEAK_ACCESS_READWRITE) ? notImplemented() : STATUS_OK; \
  }

IBEAM_API IBeam_getExpoRange(IBeamCamPtr camPtr, double* min, double* max, double* step)
{
  IF_OPEN
  CHECK_SUPPORT(peak_ExposureTime_GetAccessStatus(_cam.peakHandle));
  CHECK_ERR("getExpoRange", peak_ExposureTime_GetRange(_cam.peakHandle, min, max, step));
  return STATUS_OK;
}

IBEAM_API IBeam_getExpo(IBeamCamPtr camPtr, double* value)
{
  IF_OPEN
  CHECK_ERR("getExpo", peak_ExposureTime_Get(_cam.peakHandle, value));
  return STATUS_OK;
}

IBEAM_API IBeam_setExpo(IBeamCamPtr camPtr, double value)
{
  IF_OPEN
  CHECK_ERR("setExpo", peak_ExposureTime_Set(_cam.peakHandle, value));
  return STATUS_OK;
}

IBEAM_API IBeam_getFpsRange(IBeamCamPtr camPtr, double* min, double* max, double* step)
{
  IF_OPEN
  CHECK_SUPPORT(peak_FrameRate_GetAccessStatus(_cam.peakHandle));
  CHECK_ERR("getFpsRange", peak_FrameRate_GetRange(_cam.peakHandle, min, max, step));
  return STATUS_OK;
}

IBEAM_API IBeam_getFps(IBeamCamPtr camPtr, double* value)
{
  IF_OPEN
  CHECK_ERR("getFps", peak_FrameRate_Get(_cam.peakHandle, value));
  return STATUS_OK;
}

IBEAM_API IBeam_setFps(IBeamCamPtr camPtr, double value)
{
  IF_OPEN
  CHECK_ERR("setFps", peak_FrameRate_Set(_cam.peakHandle, value));
  return STATUS_OK;
}

IBEAM_API IBeam_getAGainRange(IBeamCamPtr camPtr, double* min, double* max, double* step)
{
  IF_OPEN
  CHECK_SUPPORT(peak_Gain_GetAccessStatus(_cam.peakHandle,
    PEAK_GAIN_TYPE_ANALOG, PEAK_GAIN_CHANNEL_MASTER))
  CHECK_ERR("getAGainRange", peak_Gain_GetRange(_cam.peakHandle,
    PEAK_GAIN_TYPE_ANALOG, PEAK_GAIN_CHANNEL_MASTER, min, max, step));
  return STATUS_OK;
}

IBEAM_API IBeam_getAGain(IBeamCamPtr camPtr, double* value)
{
  IF_OPEN
  CHECK_ERR("getAGain", peak_Gain_Get(_cam.peakHandle, 
    PEAK_GAIN_TYPE_ANALOG, PEAK_GAIN_CHANNEL_MASTER, value));
  return STATUS_OK;
}

IBEAM_API IBeam_setAGain(IBeamCamPtr camPtr, double value)
{
  IF_OPEN
  CHECK_ERR("setAGain", peak_Gain_Set(_cam.peakHandle, 
    PEAK_GAIN_TYPE_ANALOG, PEAK_GAIN_CHANNEL_MASTER, value));
  return STATUS_OK;
}

IBEAM_API IBeam_getDGainRange(IBeamCamPtr camPtr, double* min, double* max, double* step)
{
  IF_OPEN
  CHECK_SUPPORT(peak_Gain_GetAccessStatus(_cam.peakHandle,
    PEAK_GAIN_TYPE_DIGITAL, PEAK_GAIN_CHANNEL_MASTER))
  CHECK_ERR("getDGainRange", peak_Gain_GetRange(_cam.peakHandle,
    PEAK_GAIN_TYPE_DIGITAL, PEAK_GAIN_CHANNEL_MASTER, min, max, step));
  return STATUS_OK;
}

IBEAM_API IBeam_getDGain(IBeamCamPtr camPtr, double* value)
{
  IF_OPEN
  CHECK_ERR("getDGain", peak_Gain_Get(_cam.peakHandle, 
    PEAK_GAIN_TYPE_DIGITAL, PEAK_GAIN_CHANNEL_MASTER, value));
  return STATUS_OK;
}

IBEAM_API IBeam_setDGain(IBeamCamPtr camPtr, double value)
{
  IF_OPEN
  CHECK_ERR("setDGain", peak_Gain_Set(_cam.peakHandle, 
    PEAK_GAIN_TYPE_DIGITAL, PEAK_GAIN_CHANNEL_MASTER, value));
  return STATUS_OK;
}
