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

enum CamDataRow { ROW_RENDER_TIME, ROW_CALC_TIME, ROW_FRAME_ERR };

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

#define CHECK_ERR(msg) \
    if (IBEAM_ERROR(res)) { \
        auto err = adapter->errorText(res); \
        qCritical() << LOG_ID << msg << err; \
        return QString(msg ": ") + err; \
    }
    
class DllIntf : public CameraWorker
{
public:
    DllCamera *cam;
    DllAdapter *adapter;
    IBeam_CameraId id;
    IBeam_Handle hCam = IBEAM_BAD_HANDLE;
    IBeam_Status res;
    QByteArray hdrBuf;
    
    int framesErr = 0;

    DllIntf(DllAdapter *adapter, IBeam_CameraId id, PlotIntf *plot, TableIntf *table, StabilityIntf *stabil, DllCamera *cam)
        : CameraWorker(plot, table, stabil, cam, cam, LOG_ID), id(id), cam(cam), adapter(adapter)
    {
        tableData = [this]{
            QMap<int, CamTableData> data = {
                { ROW_RENDER_TIME, {avgAcqTime} },
                { ROW_CALC_TIME, {avgCalcTime} },
                { ROW_FRAME_ERR, {framesErr, CamTableData::COUNT, framesErr > 0} },
            };
            return data;
        };
    }
    
    QString getFrameInfo()
    {
        IBeam_FrameInfo info = { 0 };
        auto res = adapter->getFrameInfo(hCam, &info);
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
        
        cam->_configGroup = cam->_customId;
        cam->loadConfig();
        
        auto res = adapter->openCamera(id, &hCam);
        CHECK_ERR("Unable to open camera");
        qDebug() << LOG_ID << " Camera opened | id:" << id << "| hCam:" << hCam;
        
        if (auto err = getFrameInfo(); !err.isEmpty()) return err;
        
        plot->initGraph(c.w, c.h);
        graph = plot->rawGraph();
        
        configure();
        
        togglePowerMeter();
        
        return {};
    }
    
    ~DllIntf()
    {
        auto res = adapter->closeCamera(hCam);
        if (IBEAM_ERROR(res))
            qWarning() << LOG_ID << "Unable to close camera" << adapter->errorText(res);
        else qDebug() << LOG_ID << "Camera closed" << id;
        hCam = IBEAM_BAD_HANDLE;
    }
    
    void run()
    {
        startCapture();
        while (true) {
            tm = timer.elapsed();
            avgFrameCount++;
            avgFrameTime += tm - prevFrame;
            prevFrame = tm;
            
            tm = timer.elapsed();
            res = adapter->getFrame(hCam, c.buf);
            if (res == IBEAM_STATUS_ABORT) {
                auto err = adapter->errorText(res);
                qCritical() << LOG_ID << "Interrupted" << err;
                emit cam->error("Interrupted: " + err);
                return;
            }
            markAcqTime();
            
            if (res == IBEAM_STATUS_OK) {
                tm = timer.elapsed();
                calcResult();
                markCalcTime();

                if (showResults())
                    emit cam->ready();
            } else {
                framesErr++;
                stats[QStringLiteral("frameErrors")] = framesErr;
                QString errKey = QStringLiteral("frameError_") + QString::number(res, 16);
                stats[errKey] = stats[errKey].toInt() + 1;
            }
            
            if (tm - prevStat >= STAT_DELAY_MS) {
                prevStat = tm;
                
                double ft = avgFrameTime / avgFrameCount;
                avgFrameTime = 0;
                avgFrameCount = 0;
                CameraStats st {
                    .fps = 1000.0/ft,
                    .hardFps = 0, 
                    .measureTime = measureStart > 0 ? timer.elapsed() - measureStart : -1,
                };
                emit cam->stats(st);
                
                if (cam->isInterruptionRequested()) {
                    qDebug() << LOG_ID << "Interrupted by user";
                    return;
                }
                checkReconfig();
            }
        }
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
