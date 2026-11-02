#ifndef DLL_ADAPTER_H
#define DLL_ADAPTER_H

#include "CameraTypes.h"

class DllAdapter
{
public:
    static void loadAdapters();
    static const QVector<DllAdapter*>& adapters();

    QVector<CameraItem> getCameras() const;
    QString displayName() const { return _displayName; }
    QString libError() const { return _libError; }
    QString iconPath() const { return ":/toolbar/camera"; } // TODO: make different icon

private:
    QString _id;
    QString _displayName;
    QString _libError;
};

# endif // DLL_ADAPTER_H