#include "DllCamera.h"
#include "DllAdapter.h"

#include <QSettings>

#define LOG_ID "DllCamera:"

Q_GLOBAL_STATIC(QVector<DllAdapter*>, __adapters__);

//------------------------------------------------------------------------------
//                                    Helpers
//------------------------------------------------------------------------------

static DllAdapter* findAdapter(const QString& adapterId)
{
    for (auto adapter : *__adapters__)
        if (adapter->id() == adapterId)
            return adapter;
    return nullptr;
}

static QString makeCustomId(const QString& adapterId, IBeam_CameraId cameraId)
{
    return QStringLiteral("%1:%2").arg(adapterId).arg(cameraId);
}

static void splitCustomId(const QString& customId, QString& adapterId, IBeam_CameraId& cameraId)
{
    int pos = customId.lastIndexOf(':');
    if (pos < 0) return;
    adapterId = customId.left(pos);
    cameraId = QStringView(customId).sliced(pos+1).toInt();
}

//------------------------------------------------------------------------------
//                                DllCamera
//------------------------------------------------------------------------------

void DllCamera::loadAdapters(QSettings* s)
{
    qDebug() << LOG_ID << "Loading adapters";

    s->beginGroup("DllAdapters");
    s->beginGroup("items");

    int successCount = 0;
    for (const auto& g : s->childGroups())
    {
        s->beginGroup(g);

        QString libPath = s->value("libPath").toString();
        qDebug() << LOG_ID << "Adapter" << libPath;
        auto adapter = new DllAdapter(libPath);
        if (adapter->error().isEmpty())
            successCount++;
        __adapters__->append(adapter);

        s->endGroup();
    }

    s->endGroup();
    s->endGroup();

    qDebug() << LOG_ID << "Adapters loaded | Total:" << __adapters__->size() << "| Success:" << successCount;
}

QVector<DllCamera::AdapterItem> DllCamera::getAdapters()
{
    QVector<DllCamera::AdapterItem> items;
    for (auto adapter : *__adapters__)
        items << AdapterItem {
            .id = adapter->id(),
            .name = adapter->name(),
            .error = adapter->error(),
        };
    return items;
}

QVector<CameraItem> DllCamera::getCameras(const QString& adapterId)
{
    qDebug() << LOG_ID << "Get cameras via" << adapterId;

    DllAdapter* adapter = findAdapter(adapterId);
    if (!adapter) {
        qWarning() << LOG_ID << "Adapter not found";
        return {};
    }
    
    if (!adapter->error().isEmpty()) {
        qWarning() << LOG_ID << "Adapter not loaded with error" << adapter->error();
        return {};
    }
    
    size_t camCount;
    auto res = adapter->getCameraCount(&camCount);
    if (IBEAM_ERROR(res)) {
        qWarning() << LOG_ID << "Unable to get camera count" << adapter->errorText(res);
        return {};
    }
    qDebug() << LOG_ID << "Cameras found:" << camCount;
    
    QVector<IBeam_CameraInfo> cams(camCount);
    res = adapter->getCameraList(cams.data(), camCount);
    if (IBEAM_ERROR(res)) {
        qWarning() << LOG_ID << "Unable to get camera list" << adapter->errorText(res);
        return {};
    }
    
    QVector<CameraItem> result;
    for (const auto &cam : cams) {
        qDebug() << LOG_ID << "Camera Id:" << cam.cameraId << "| Name:" << cam.displayName;
        QString customId = makeCustomId(adapter->id(), cam.cameraId);
        result << CameraItem {
            .cameraId = customId,
            .customId = customId,
            .displayName = QString::fromLatin1(cam.displayName),
        };
    }
    
    return result;
}

DllCamera::DllCamera(QVariant id, PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, QObject *parent)
    : Camera(plot, table, stabil, "IdsCamera"), QThread(parent)
{
    QString adapterId;
    IBeam_CameraId cameraId = 0;
}

DllCamera::~DllCamera()
{
    qDebug() << LOG_ID << "Deleted";
}

void DllCamera::startCapture()
{
    start();
}

void DllCamera::stopCapture()
{
    if (_dll)
        _dll.reset(nullptr);
}

