#include "DllCamera.h"

#include "cameras/CameraWorker.h"
#include "cameras/DllAdapter.h"

#include "helpers/OriDialogs.h"

#include <QSettings>

#define LOG_ID "DllCamera:"

Q_GLOBAL_STATIC(QVector<DllAdapter*>, __adapters__);

//------------------------------------------------------------------------------
//                                   Helpers
//------------------------------------------------------------------------------

static DllAdapter* getAdapter(const QString& adapterId)
{
    for (auto adapter : *__adapters__)
        if (adapter->id() == adapterId)
            return adapter;
    return nullptr;
}

//------------------------------------------------------------------------------
//                                   DllIntf
//------------------------------------------------------------------------------

class DllIntf : public CameraWorker
{
public:
    IBeam_CameraId id;
    DllCamera *cam;
    DllAdapter *adapter;

    DllIntf(DllAdapter *adapter, IBeam_CameraId id, PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, DllCamera *cam)
        : CameraWorker(plot, table, stabil, cam, cam, LOG_ID), id(id), cam(cam), adapter(adapter)
    {
    }
    
    QString init()
    {
        bool camInfoFound = false;
        for (const auto& camInfo : adapter->cameras())
            if (camInfo.cameraId == id)
            {
                cam->_name = camInfo.displayName;
                cam->_customId = camInfo.customId;
                camInfoFound = true;
                break;
            }
        if (!camInfoFound)
            return "Unable to get camera descriptor";
        
        cam->_configGroup = cam->_customId;
        cam->loadConfig();
        
        // TODO: open camera
            
        return {};
    }
};

//------------------------------------------------------------------------------
//                                  DllCamera
//------------------------------------------------------------------------------

void DllCamera::loadAdapters(QSettings* s)
{
    qDebug() << LOG_ID << "Loading adapters";

    s->beginGroup("DllAdapters");
    s->beginGroup("items");

    int loadedCount = 0;
    for (const auto& g : s->childGroups())
    {
        s->beginGroup(g);

        QString libPath = s->value("libPath").toString();
        qDebug() << LOG_ID << "Adapter" << libPath;
        auto adapter = new DllAdapter(libPath);
        if (adapter->loaded())
            loadedCount++;
        __adapters__->append(adapter);

        s->endGroup();
    }

    s->endGroup();
    s->endGroup();

    qDebug() << LOG_ID << "Adapters found:" << __adapters__->size() << "| loaded:" << loadedCount;
}

void DllCamera::unloadAdapters()
{
    for (auto adapter : *__adapters__)
    {
        if (!adapter->loaded())
            continue;
        auto res = adapter->exit();
        if (IBEAM_ERROR(res))
            qWarning() << LOG_ID << "Unable to deinit adapter" << adapter->id() << adapter->errorText(res);
        else qDebug() << LOG_ID << "Adapter deinited" << adapter->id();
    }
}

QVector<DllCamera::AdapterItem> DllCamera::getAdapters()
{
    QVector<AdapterItem> items;
    for (auto adapter : *__adapters__)
        items << AdapterItem {
            .id = adapter->id(),
            .name = adapter->name(),
            .error = adapter->error(),
        };
    return items;
}

DllCamera::CamerasResult DllCamera::getCameras(const QString& adapterId)
{
    qDebug() << LOG_ID << "Get cameras via" << adapterId;

    DllAdapter* adapter = getAdapter(adapterId);
    if (!adapter) {
        auto err = qApp->tr("Unknown adapter: %1").arg(adapterId);
        qWarning().noquote() << LOG_ID << err;
        return CamerasResult::fail(err);
    }

    auto err = adapter->refreshCameras();
    if (!err.isEmpty()) {
        qWarning().noquote() << LOG_ID << err;
        return CamerasResult::fail(err);
    }
    
    return CamerasResult::ok(adapter->cameras());
}

DllCamera::DllCamera(const QString& customId, PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, QObject *parent)
    : Camera(plot, table, stabil, "IdsCamera"), QThread(parent)
{
    QString adapterId;
    IBeam_CameraId cameraId = 0;
    DllAdapter::splitCustomId(customId, adapterId, cameraId);
    auto adapter = getAdapter(adapterId);
    // TODO: move message boxes out of camera object 
    // TODO: provide a unified way of reporting camera initialization errors
    if (!adapter || !adapter->loaded()) {
        Ori::Dlg::error(qApp->tr("Invalid adapter id: %1").arg(customId));
        return;
    }
    if (cameraId == IBEAM_BAD_CAMERA_ID) {
        Ori::Dlg::error(qApp->tr("Invalid camera id: %1").arg(customId));
        return;
    }
    auto dll = new DllIntf(adapter, cameraId, plot, table, stabil, this);
    auto res = dll->init();
    if (!res.isEmpty()) {
        Ori::Dlg::error(res);
        delete dll;
        return;
    }
    _dll.reset(dll);
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

