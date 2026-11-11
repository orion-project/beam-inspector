#ifndef DLL_ADAPTER_H
#define DLL_ADAPTER_H

#include "cameras/CameraTypes.h"

#define BEAM_INSPECTOR
#include "adapter.h"

#include <QString>

class DllAdapter
{
public:
    DllAdapter(const QString& libPath, const QString& sdkPath);

    QString id() const { return _id; }
    QString name() const { return _name; }
    QString error() const { return _error; }
    bool loaded() const { return _error.isEmpty(); }
    
    QString refreshCameras();
    const QVector<CameraItem>& cameras() const { return _cameras; }

    static QString errorText(IBeamStatus* status);
    
    QString makeCustomId(IBeamCamId cameraId) const;
    static void splitCustomId(const QString& customId, QString& adapterId, IBeamCamId& cameraId);

    // Resolved functons
    #define PROC(name) IBeamStatus* (__cdecl *name)

    PROC(init)();
    PROC(exit)();
    PROC(getAdapterInfo)(IBeamAdapterInfo* info);
    PROC(getCameraList)(IBeamCamList* camList, size_t* camCount);
    PROC(openCamera)(IBeamCamId camId, IBeamCamPtr* camPtr);
    PROC(closeCamera)(IBeamCamPtr camPtr);
    PROC(getFrameInfo)(IBeamCamPtr camPtr, IBeamFrameInfo* frameInfo);
    PROC(getFrame)(IBeamCamPtr camPtr, uint8_t* frameBuf);

    #undef PROC

    static void loadAdapters();
    static void unloadAdapters();
    static const QVector<DllAdapter*>& getAdapters();
    static DllAdapter* getAdapter(const QString& adapterId);

private:
    QString _id, _name, _error;
    QVector<CameraItem> _cameras;
};

# endif // DLL_ADAPTER_H
