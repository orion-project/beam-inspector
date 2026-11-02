#include "DllAdapter.h"

Q_GLOBAL_STATIC(QVector<DllAdapter*>, __adapters__);

const QVector<DllAdapter*>& DllAdapter::adapters()
{
    return *__adapters__;
}

void DllAdapter::loadAdapters()
{
    auto demoAdapter = new DllAdapter;
    demoAdapter->_id = "demo_dll_adapter";
    demoAdapter->_displayName = "Demo DLL Adapter";
    __adapters__->append(demoAdapter);
}

QVector<CameraItem> DllAdapter::getCameras() const
{
    // TODO: load cameras

    QString cameraId = "1";
    QString customId = QStringLiteral("%1:%2").arg(_id, cameraId);
    return {
        CameraItem {
            .cameraId = customId,
            .customId = customId,
            .displayName = "Demo DLL Camera 1",
        }
    };
}
