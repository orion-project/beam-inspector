#ifndef IBEAM_ADAPTER_H
#define IBEAM_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#define IBEAM_API __declspec(dllexport) IBeam_Status __cdecl

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint64_t IBeam_CameraId;
typedef uint32_t IBeam_Status;

#define IBEAM_API_V1 1
#define IBEAM_BAD_CAMERA_ID 0
#define IBEAM_NAME_LEN 128
#define IBEAM_ERROR_LEN 1024
#define IBEAM_OK(status) ((status) != 0)
#define IBEAM_ERROR(status) ((status) > 0)

typedef struct
{
  /// Adapter display name.
  /// Zero-terminated string.
  char displayName[IBEAM_NAME_LEN];
  
  /// Semantic version of the adapter.
  /// Could be the same as the DLL version from its version resource, but not necessary.
  /// This is used just for information; Beam Inspector doesn't know how to deal with
  /// different versions of the same adapter; it only distinguishes API versions.
  char displayVersion[IBEAM_NAME_LEN];
  
  /// Version of API (adapter interface)
  /// API version defines available set of functions and features.
  uint32_t apiVersion;
} IBeam_AdapterInfo;

typedef struct
{
  /// SDK-agnostic unique identifier for a camera.
  /// SDK-specific ids can be of different kind - string, index, etc.
  /// Adapter library converts SDK-specific ids into simple IBeam_CameraId type 
  /// via some iternal transformations; the client app should not care about them.
  IBeam_CameraId cameraId;
  
  /// Camera display name.
  /// Zero-terminated string.
  /// Adapter library can produce the name as a combination of the model name and serial number
  /// or in a different manner. This name should be a user-friendly identifier for a camera.
  char displayName[IBEAM_NAME_LEN];
} IBeam_CameraInfo;

IBEAM_API IBeam_init();
IBEAM_API IBeam_exit();
IBEAM_API IBeam_getAdapterInfo(IBeam_AdapterInfo* info);
IBEAM_API IBeam_getCameraCount(size_t* count);
IBEAM_API IBeam_getCameraList(IBeam_CameraInfo* cameraList, size_t listSize);
IBEAM_API IBeam_getLastError(IBeam_Status status, char* buf, size_t bufSize);

#ifdef __cplusplus
}
#endif

#endif // IBEAM_ADAPTER_H