#ifndef IBEAM_ADAPTER_H
#define IBEAM_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#ifndef BEAM_INSPECTOR
#define IBEAM_API __declspec(dllexport) IBeam_Status __cdecl
#endif // BEAM_INSPECTOR

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint64_t IBeam_CameraId;
typedef uint32_t IBeam_Status;

/// Adapter API version
#define IBEAM_API_V1 1

#define IBEAM_BAD_CAMERA_ID 0

// Standard buffer lengths
#define IBEAM_NAME_LEN 128
#define IBEAM_ERROR_LEN 1024

// Status checks
#define IBEAM_OK(status) ((status) == 0)
#define IBEAM_ERROR(status) ((status) != 0)

/// Camera name is given as unicode string (wchar_t array).
/// If the flag is not set, simple ASCII string is implied.
#define IBEAM_CAM_FLAG_NAME_WCHAR 0x01

typedef struct
{
  /**
    Adapter display name.
    Zero-terminated string.
  */
  char displayName[IBEAM_NAME_LEN];
  
  /**
    Semantic version of the adapter.
    Could be the same as the DLL version from its version resource, but not necessary.
    This is used just for information; Beam Inspector doesn't know how to deal with
    different versions of the same adapter; it only distinguishes API versions.
  */
  char displayVersion[IBEAM_NAME_LEN];
  
  /**
    Version of API (adapter interface)
    API version defines available set of functions and features.
  */
  uint32_t apiVersion;

  /**
    A set of various boolean properties.
  */
  uint32_t flags;
} IBeam_AdapterInfo;

typedef struct
{
  /**
    SDK-agnostic unique identifier for a camera.
    SDK-specific IDs can be of different kind - string, index, etc.
    Adapter library converts SDK-specific IDs into the into simple IBeam_CameraId type 
    via some internal transformations; the client app should not care about them.
  */
  IBeam_CameraId cameraId;
  
  /**
    Camera display name.
    Zero-terminated string.
    Adapter library can produce the name as a combination of the model name and serial number
    or in a different manner. This name should be a user-friendly identifier for a camera.
  */
  char displayName[IBEAM_NAME_LEN];

  /**
    A set of various boolean properties.
  */
  uint32_t flags;
} IBeam_CameraInfo;

// Beam Inspector doesn't need funcion declarations because
// it resolves them in runtime from loaded DLL by symbol names
#ifndef BEAM_INSPECTOR

/**
  Intialize internal adapter structures.
  Beam Inspector calls this function once after loading adapter DLL.
*/
IBEAM_API IBeam_init();

/**
  Clear and free internal adapter structures.
  Beam Inspector calls this function while closing the main window.
*/
IBEAM_API IBeam_exit();

/**
  Returns basic information about the adapter library.
  Beam Inspector calls the function with zero-filled buffer.
*/
IBEAM_API IBeam_getAdapterInfo(IBeam_AdapterInfo* info);

/**
  Returns a number of attached cameras.
  This is a companion function for IBeam_getCameraList,
  use it to prepare a buffer for information about cameras.
*/
IBEAM_API IBeam_getCameraCount(size_t* count);

/**
  Fills the given buffer with information about attached cameras.
  The size of the given buffer should match the number of cameras 
  previously returned by the IBeam_getCameraCount function.
  Beam Inspector calls the function with zero-filled buffer.
*/
IBEAM_API IBeam_getCameraList(IBeam_CameraInfo* cameraList, size_t cameraCount);

/**
  Fills the given buffer with a message text corresponding to the given status.
  Should return an error status if the buffer is too small for the message.
  The message must be zero-terminated ASCII string.
*/
IBEAM_API IBeam_getStatusText(IBeam_Status status, char* buf, size_t bufSize);

#endif // BEAM_INSPECTOR

#ifdef __cplusplus
}
#endif

#endif // IBEAM_ADAPTER_H