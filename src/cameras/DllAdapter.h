#ifndef DLL_ADAPTER_H
#define DLL_ADAPTER_H

#include "cameras/CameraTypes.h"

#define BEAM_INSPECTOR
#include "adapter.h"

#include <QString>

class DllAdapter
{
public:
    DllAdapter(const QString& libPath);

    QString id() const { return _id; }
    QString name() const { return _name; }
    QString error() const { return _error; }
    bool loaded() const { return _error.isEmpty(); }
    
    QString refreshCameras();
    const QVector<CameraItem>& cameras() const { return _cameras; }

    QString errorText(IBeam_Status status);
    
    QString makeCustomId(IBeam_CameraId cameraId) const;
    static void splitCustomId(const QString& customId, QString& adapterId, IBeam_CameraId& cameraId);

    // Resolved functons
    #define PROC(name) IBeam_Status (__cdecl *name)

    PROC(init)();
    PROC(exit)();
    PROC(getAdapterInfo)(IBeam_AdapterInfo* info);
    PROC(getCameraCount)(size_t* count);
    PROC(getCameraList)(IBeam_CameraInfo* cameraList, size_t listSize);
    PROC(getStatusText)(IBeam_Status status, char* buf, size_t bufSize);
    PROC(openCamera)(IBeam_CameraId cameraId, IBeam_Handle *hCam);
    PROC(closeCamera)(IBeam_Handle hCam);
    PROC(getFrameInfo)(IBeam_Handle hCam, IBeam_FrameInfo *info);
    PROC(getFrame)(IBeam_Handle hCam, uint8_t *buf);

    #undef PROC

private:
    QString _id, _name, _error;
    QVector<CameraItem> _cameras;
};

# endif // DLL_ADAPTER_H
