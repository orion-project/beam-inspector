#ifndef IBEAM_ADAPTER_H
#define IBEAM_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#ifndef BEAM_INSPECTOR
#define IBEAM_API __declspec(dllexport) IBeamStatus* __cdecl
#endif // BEAM_INSPECTOR

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint64_t IBeamCamId;
typedef void* IBeamCamPtr;

/// Adapter API version
#define IBEAM_API_V1 1

#define IBEAM_BAD_CAMERA_ID 0

// Standard buffer lengths
#define IBEAM_NAME_LEN 128
#define IBEAM_ERROR_LEN 1024

/// Any adapter function returns a pointer to some preallocated status object
/// which is guaranteed to be valid until the next call of any other function.
/// Beam Inspector copies it into its internals but does not free the object.
///
typedef struct {
  /// Some predefined status codes that Beam Inspector understands and reacts.
  /// Beam Inspector doesn't know how to deal with internal error codes of the underlying SDK.
  /// The adapter should provide a helpful describing message that could be used for logging 
  /// and troubleshooting and include the low-level error code to that message if consides it useful.
  enum {
    /// Successfull status.
    /// Kind of useless, functions should return NULL in case of success.
    IBEAM_STATUS_OK = 0,

    /// When startng a camera, any error is considered as a generic failure
    /// and causes Beam Inspector to immediately break the camera initialization.
    /// While capturing, frame errors are counted and reported in the measurement file.
    IBEAM_STATUS_ERROR = 1,

    /// While capturing, this code is considered as an irrecoverable failure
    /// and stops the camera thread.
    IBEAM_STATUS_ABORT = 2,

    /// The requested feature is not implemented in the current adapter.
    /// This could be an expected result if the feature is not supposed to work on camera.
    /// E.g., when Beam Inspector initializes the control panel for the camera,
    /// it asks for the ability to change exposition time, FPS, gain, etc.
    /// When it receives the not implemented status, it silently skips the control.
    IBEAM_STATUS_NOT_IMPLEMENTED = 3,
  }
  code;

  /// A message corresponding to the given status code.
  /// In case of error, the called function should put a zero-terminated ASCII-string 
  /// into this buffer and Beam Inspector will report it in the log.
  char text[IBEAM_ERROR_LEN];
}
IBeamStatus;

/// Arguments used when initalizing the adapter (@sa IBeam_init).
///
typedef struct {
  /// Absolute path to the adapter, not including the library name.
  /// Zero-terminated string not longer than MAX_PATH.
  const char* libDir;

  /// Absolute path to SDK directory, not including any library name.
  /// If no camera SDK is used, it is the same as libDir.
  /// Zero-terminated string not longer than MAX_PATH.
  const char* sdkDir;
}
IBeamAdapterArgs;

/// A set of various boolean properties of the whole adapter.
///
typedef enum {
  // reserved
  IBEAM_ADAPTER_FLAG_RESERVER = 0,
}
IBeamAdapterFlags;

/// Meta-information about the adapter.
///
typedef struct {
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

  /// A set of various boolean properties.
  IBeamAdapterFlags flags;
}
IBeamAdapterInfo;

/// A set of various boolean properties of a camera.
///
typedef enum {
  /// Camera name is given as unicode string (wchar_t array).
  /// If the flag is not set, simple ASCII string is implied.
  IBEAM_CAM_FLAG_NAME_WCHAR = 0x01,
}
IBeamCamFlags;

/// Information about a camera returning when cameras are enumerated (not opened yet).
///
typedef struct {
  /// SDK-agnostic unique identifier for a camera.
  /// SDK-specific IDs can be of different kind - string, index, etc.
  /// Adapter library converts SDK-specific IDs the into simple IBeamCamId type 
  /// via some internal transformations; the client app should not care about them.
  /// Beam Inspector expects this ID to be persistent between OS restarts, camera replugs, etc.,
  /// because it is used for storing and loading camera settings and custom names.
  /// So a good idea is to generate this id from the camera serial number.
  IBeamCamId id;
  
  /// Camera display name.
  /// Zero-terminated string.
  /// Adapter library can produce the name as a combination of the model name and serial number
  /// or in a different manner. This name should be a user-friendly identifier for a camera.
  char displayName[IBEAM_NAME_LEN];

  /// A set of various boolean properties.
  IBeamCamFlags flags;
}
IBeamCamInfo;

typedef IBeamCamInfo* IBeamCamList;

/// Information about the current image mode 
/// used by Beam Inspector when it initializes a frame buffer.
///
typedef struct {
  /// Width in pixes.
  uint32_t width;

  /// Height in pixels.
  uint32_t height;

  /// Image bit depth (bits per pixel).
  /// This value is used for displaying for users and for preparing the image buffer. 
  /// In general, real cameras could have a more generic "pixel format" option
  /// instead of simple bit depth property. Pixel format defines how pixel values 
  /// are packed into machine bytes when these values are not byte-aligned.
  /// For Beam Inspector it's not possible to understand all different available pixel formats,
  /// so it expects images to be simple 8-bit or 16-bit data arrays of [width * height] size.
  /// For example, it means that when bpp=12, Beam Inspector displays it as 12 for users
  /// but prepares a 16-bit data buffer for recieving the image data.
  /// It's up to the adapter library how to decode particular pixel formats into this plain buffer.
  uint8_t bpp;

  /// Physical pixel width in micrometers. Zero if unknown.
  float pixelWidth;

  /// Physical pixel height in micrometers. Zero if unknown.
  float pixelHeight;
}
IBeamFrameInfo;

// Beam Inspector doesn't need funcion declarations because
// it resolves them in runtime from loaded DLL by symbol names
#ifndef BEAM_INSPECTOR

/// Intialize internal adapter structures.
/// Beam Inspector calls this function once after loading adapter DLL.
///
IBEAM_API IBeam_init(IBeamAdapterArgs *args);

/// Clear and free internal adapter structures.
/// Beam Inspector calls this function while closing the main window.
///
IBEAM_API IBeam_exit(void);

/// Returns basic information about the adapter library.
/// Beam Inspector calls the function with zero-filled buffer.
///
IBEAM_API IBeam_getAdapterInfo(IBeamAdapterInfo* info);

/// Returns a number of attached cameras and meta-data about them.
/// The adapter should request the information from the underlying SDK
/// and provide it as a pointer to some preallocated buffer
/// which is guaranteed to be valid until the next call of any adapter function.
/// Beam Inspector copies info into its internals but does not free the buffer.
///
IBEAM_API IBeam_getCameraList(IBeamCamList* camList, size_t* camCount);

/// Opens a camera by the given ID and assigns a handle to the opened camera object.
/// The handle should be used for all subsequent operations with the opened camera.
///
IBEAM_API IBeam_openCamera(IBeamCamId camId, IBeamCamPtr *camPtr);

/// Closes the camera with the given handle.
/// The handle is gets invalid after the camera is closed.
///
IBEAM_API IBeam_closeCamera(IBeamCamPtr camPtr);

/// Returns the currently selected image mode of the opened camera.
///
IBEAM_API IBeam_getFrameInfo(IBeamCamPtr camPtr, IBeamFrameInfo *info);

/// Acquires the next available frame from the opened camera.
/// The adapter library is expected to decode the raw frame data
/// from a pixel format it knows into the plain number array (8 or 16-bit)
/// and put the decoded data into the given buffer.
/// The Size of the target buffer is not passed because it is supposeed to match 
/// the frame size and bpp that were previously returned by IBeam_getFrameInfo.
///
IBEAM_API IBeam_getFrame(IBeamCamPtr camPtr, uint8_t *buf);

/// Set of functions for the Control panel.
/// The first time, Beam Inspector calls IBeam_get*Range() functions with null pointers,
/// asking if the camera supports the parameter control and respective widgets
/// should be shown in the control panel. If the camera does not support such control,
/// function should return the NOT_IMPLEMENTED status; otherwise, the OK status.

IBEAM_API IBeam_getExpoRange(IBeamCamPtr camPtr, double* min, double* max, double* step);
IBEAM_API IBeam_getExpo(IBeamCamPtr camPtr, double* value);
IBEAM_API IBeam_setExpo(IBeamCamPtr camPtr, double value);

IBEAM_API IBeam_getAGainRange(IBeamCamPtr camPtr, double* min, double* max, double* step);
IBEAM_API IBeam_getAGain(IBeamCamPtr camPtr, double* value);
IBEAM_API IBeam_setAGain(IBeamCamPtr camPtr, double value);

IBEAM_API IBeam_getDGainRange(IBeamCamPtr camPtr, double* min, double* max, double* step);
IBEAM_API IBeam_getDGain(IBeamCamPtr camPtr, double* value);
IBEAM_API IBeam_setDGain(IBeamCamPtr camPtr, double value);

#endif // BEAM_INSPECTOR

#ifdef __cplusplus
}
#endif

#endif // IBEAM_ADAPTER_H
