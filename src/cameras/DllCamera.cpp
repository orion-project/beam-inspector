#include "DllCamera.h"

#include "cameras/CameraWorker.h"
#include "cameras/DllAdapter.h"
#include "cameras/DllHardConfig.h"

#include "helpers/OriDialogs.h"

#include <QFileInfo>
#include <QSettings>

#define LOG_ID "DllCamera:"

//------------------------------------------------------------------------------
//                                   Helpers
//------------------------------------------------------------------------------

enum CamDataRow { ROW_RENDER_TIME, ROW_CALC_TIME, ROW_FRAME_ERR };

#define CHECK_ERR(msg) \
    if (res) { \
        auto err = adapter->errorText(res); \
        qCritical() << LOG_ID << msg << err; \
        return QString(msg ": ") + err; \
    }

//------------------------------------------------------------------------------
//                                   DllIntf
//------------------------------------------------------------------------------

class DllIntf : public CameraWorker
{
public:
    IBeamCamId id;
    DllCamera *cam;
    DllAdapter *adapter;
    IBeamCamPtr camPtr = nullptr;
    IBeamStatus* res;
    QByteArray hdrBuf;
    
    DllIntf(DllAdapter *adapter, IBeamCamId id, PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, DllCamera *cam)
        : CameraWorker(plot, table, stabil, cam, cam, LOG_ID), id(id), cam(cam), adapter(adapter)
    {
        tableData = [this]{
            QMap<int, CamTableData> data = {
                { ROW_RENDER_TIME, {avgAcqTime} },
                { ROW_CALC_TIME, {avgCalcTime} },
                { ROW_FRAME_ERR, {errorsFrames, CamTableData::COUNT, errorsFrames > 0} },
            };
            return data;
        };
    }
    
    QString getFrameInfo()
    {
        IBeamFrameInfo info = { 0 };
        auto res = adapter->getFrameInfo(camPtr, &info);
        CHECK_ERR("Unable to get frame info");
    
        cam->_width = info.width;
        cam->_height = info.height;
        cam->_bpp = info.bpp;

        if (info.pixelWidth != info.pixelHeight)
            qWarning() << LOG_ID << "Non-square pixels are not supported";
        else if (info.pixelWidth > 0) {
            cam->_pixelScale.on = true;
            cam->_pixelScale.factor = info.pixelWidth;
        }
        
        c.w = info.width;
        c.h = info.height;
        c.bpp = info.bpp;
        int bufSize = c.w * c.h * (info.bpp > 8 ? 2 : 1);
        hdrBuf = QByteArray(bufSize, 0);
        c.buf = (uint8_t*)hdrBuf.data();
        
        return {};
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
        
        // Can't use raw customId as configGroup because it contains path.
        // QSettings splits the path and makes a group from its first item
        // and remaining items are added to key names as nested groups:
        // [..]
        // libs\adapter_demo_ids\adapter_demo_ids.dll%3A1\plot.normalize=false
        cam->_configGroup = QString::number(qHash(cam->_customId));
        cam->loadConfig();
        
        auto res = adapter->openCamera(id, &camPtr);
        CHECK_ERR("Unable to open camera");
        qDebug() << LOG_ID << " Camera opened | id:" << id << "| hCam:" << camPtr;
        
        if (auto err = getFrameInfo(); !err.isEmpty()) return err;
        
        plot->initGraph(c.w, c.h);
        graph = plot->rawGraph();
        
        configure();
        
        togglePowerMeter();
        
        return {};
    }
    
    ~DllIntf()
    {
        auto res = adapter->closeCamera(camPtr);
        if (res)
            qWarning() << LOG_ID << "Unable to close camera" << adapter->errorText(res);
        else qDebug() << LOG_ID << "Camera closed" << id;
        camPtr = nullptr;
    }
    
    void run()
    {
        QString lastReportedError;

        framesCaptured = false;
        startCapture();
        while (true) {
            markFrameTime();

            tm = timer.elapsed();
            res = adapter->getFrame(camPtr, c.buf);
            if (res) {
                auto err = adapter->errorText(res);

                if (res->code == IBeamStatus::IBEAM_STATUS_ABORT) {
                    qCritical() << LOG_ID << "Interrupted" << err;
                    emit cam->error("Interrupted: " + err);
                    return;
                }

                errorsFrames++;
                stats[QStringLiteral("frameErrors")] = errorsFrames;
                QString errKey = QStringLiteral("frameError/") + err;
                stats[errKey] = stats[errKey].toInt() + 1;

                if (lastReportedError != err) {
                    lastReportedError = err;
                    qWarning() << LOG_ID << err;
                }
            }
            markAcqTime();
            
            if (!res) {
                framesCaptured = true;
                tm = timer.elapsed();
                calcResult();
                markCalcTime();
            }

            if (showResults()) {
                if (framesCaptured)
                    framesCaptured = false;
            }

            if (!tick())
                break;
        }
    }
};

//------------------------------------------------------------------------------
//                                  DllCamera
//------------------------------------------------------------------------------

void DllCamera::loadAdapters() { DllAdapter::loadAdapters(); }
void DllCamera::unloadAdapters() { DllAdapter::unloadAdapters(); }

QVector<DllCamera::AdapterItem> DllCamera::getAdapters()
{
    QVector<AdapterItem> items;
    for (auto adapter : DllAdapter::getAdapters())
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

    DllAdapter* adapter = DllAdapter::getAdapter(adapterId);
    if (!adapter)
        return CamerasResult::fail(qApp->tr("Unknown adapter"));

    auto err = adapter->refreshCameras();
    if (!err.isEmpty())
        return CamerasResult::fail(err);
    
    return CamerasResult::ok(adapter->cameras());
}

DllCamera::DllCamera(const QString& customId, PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, QObject *parent)
    : QThread(parent), Camera(plot, table, stabil, "IdsCamera")
{
    QString adapterId;
    IBeamCamId cameraId = IBEAM_BAD_CAMERA_ID;
    DllAdapter::splitCustomId(customId, adapterId, cameraId);
    auto adapter = DllAdapter::getAdapter(adapterId);
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
    
    connect(parent, SIGNAL(camConfigChanged()), this, SLOT(camConfigChanged()));
}

DllCamera::~DllCamera()
{
    qDebug() << LOG_ID << "Deleted";
}

TableRowsSpec DllCamera::tableRows() const
{
    auto rows = Camera::tableRows();
    rows.aux
        << qMakePair(ROW_RENDER_TIME, qApp->tr("Acq. time"))
        << qMakePair(ROW_CALC_TIME, qApp->tr("Calc time"))
        << qMakePair(ROW_FRAME_ERR, qApp->tr("Errors"));
    return rows;
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

void DllCamera::startMeasure(MeasureSaver *saver)
{
    if (_dll)
        _dll->startMeasure(saver);
}

void DllCamera::stopMeasure()
{
    if (_dll)
        _dll->stopMeasure();
}

void DllCamera::run()
{
    if (_dll)
        _dll->run();
}

void DllCamera::camConfigChanged()
{
    if (_dll)
        _dll->reconfigure();
}

void DllCamera::requestRawImg(QObject *sender)
{
    if (_dll)
        _dll->requestRawImg(sender);
}

void DllCamera::setRawView(bool on, bool reconfig)
{
    if (_dll)
        _dll->setRawView(on, reconfig);
}

HardConfigPanel* DllCamera::hardConfgPanel(QWidget *parent)
{
    if (!_dll)
        return nullptr;
    if (!_configPanel) {
        _configPanel = new DllHardConfigPanel(_dll->camPtr, _dll->adapter, parent);
    }
    return _configPanel;
}

void DllCamera::loadConfigMore(QSettings* s)
{
    // Just for user information.
    // _configGroup is the hashed _customId, hence unreadable,
    // but it's good to know the id when looking through the config file.
    s->setValue("customId", _customId);
}

void DllCamera::saveConfigMore(QSettings* s)
{

}
