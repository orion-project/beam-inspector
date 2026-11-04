#ifndef DLL_ADAPTER_H
#define DLL_ADAPTER_H

#include <windows.h>

#include <QString>

//------------------------------------------------------------------------------
//                                 IBeam_*
//------------------------------------------------------------------------------

typedef uint64_t IBeam_CameraId;
typedef uint32_t IBeam_Status;

#define IBEAM_API_V1 1
#define IBEAM_BAD_CAMERA_ID 0
#define IBEAM_NAME_LEN 128
#define IBEAM_ERROR_LEN 1024
#define IBEAM_OK(status) ((status) == 0)
#define IBEAM_ERROR(status) ((status) != 0)

struct IBeam_AdapterInfo
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
};

struct IBeam_CameraInfo
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
};

//------------------------------------------------------------------------------

class DllAdapter
{
public:
    DllAdapter(const QString& libPath);

    QString id() const { return _id; }
    QString name() const { return _name; }
    QString error() const { return _error; }

    #define PROC(name) IBeam_Status (__cdecl *name)
    
    PROC(init)();
    PROC(exit)();
    PROC(getAdapterInfo)(IBeam_AdapterInfo* info);
    PROC(getCameraCount)(size_t* count);
    PROC(getCameraList)(IBeam_CameraInfo* cameraList, size_t listSize);
    PROC(getStatusText)(IBeam_Status status, char* buf, size_t bufSize);

    QString errorText(IBeam_Status status);

private:
    QString _id, _name, _error;
    HMODULE _hLib = 0;
};

# endif // DLL_ADAPTER_H
