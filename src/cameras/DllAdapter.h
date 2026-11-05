#ifndef DLL_ADAPTER_H
#define DLL_ADAPTER_H

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
};

# endif // DLL_ADAPTER_H
