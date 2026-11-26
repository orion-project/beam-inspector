#include "DllCamera.h"

#include "cameras/CameraWorker.h"
#include "cameras/DllAdapter.h"
#include "cameras/DllCameraConfig.h"
#include "cameras/DllControlPanel.h"

#include "helpers/OriDialogs.h"

#include <QFileInfo>
#include <QSettings>

#define LOG_ID "DllCamera:"

//------------------------------------------------------------------------------
//                                   Helpers
//------------------------------------------------------------------------------

enum CamDataRow { ROW_RENDER_TIME, ROW_CALC_TIME, ROW_FRAME_ERR, ROW_BRIGHTNESS };

#define CHECK_ERR(msg) \
    if (res) { \
        auto err = adapter->errorText(res); \
        qCritical() << LOG_ID << msg << err; \
        return QString(msg ": ") + err; \
    }

#define RETURN_ERR(msg) { \
        QString text = (msg); \
        qCritical() << LOG_ID << text; \
        return text; \
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
            if (showBrightness)
                data[ROW_BRIGHTNESS] = {brightness, CamTableData::VALUE3};
            return data;
        };
        
        getHardFps = [this]{
            double fps = 0;
            this->adapter->getFps(camPtr, &fps);
            return fps;
        };
    }
    
    QString getFrameInfo()
    {
        cam->_cfg->availablePixelFormats.clear();
        std::optional<IBeamPixelFormatId> selectedFormat;
        QString selectedFormatName;

        IBeamPixelFormats pixelFormats;
        size_t pixelFormatCount = 0;
        auto res = adapter->getPixelFormats(camPtr, &pixelFormats, &pixelFormatCount);
        // the NOT_IMPLEMENTD status means the camera supports 
        // only the single 8-bit pixel format, not an error.
        if (res && res->code != IBeamStatus::IBEAM_STATUS_NOT_IMPLEMENTED)
            CHECK_ERR("Unable to get pixel formats");
        for (size_t i = 0; i < pixelFormatCount; i++) {
            if (pixelFormats[i].bpp > 16) {
                qWarning() << LOG_ID << "Unsupported bit depth" << pixelFormats[i].bpp 
                    << "for pixel format" << pixelFormats[i].name << "skip";
            }
            int formatCode = (int)pixelFormats[i].id;
            QString formatName = QString::fromLocal8Bit(pixelFormats[i].name);
            cam->_cfg->availablePixelFormats << PixelFormat {
                .code = formatCode,
                .name = formatName,
                .descr = formatName,
                .bitsPerPixel = pixelFormats[i].bpp,
                .bytesPerPixel = (pixelFormats[i].bpp > 8) ? 2 : 1,
            };
            qDebug().noquote() << LOG_ID << "Available pixel format" << HEX(formatCode) << formatName;
            if (formatCode == cam->_cfg->pixelFormat) {
                selectedFormat = pixelFormats[i].id;
                selectedFormatName = formatName;
            }
        }
        // if pixelFormatCount is zero, this means the camera supports 
        // only the single 8-bit pixel format, not an error.
        if (pixelFormatCount > 0 && cam->_cfg->availablePixelFormats.isEmpty())
            RETURN_ERR("Camera has not returned any of known pixel formats");
            
        if (!selectedFormat && !cam->_cfg->availablePixelFormats.isEmpty()) {
            const auto& defaultFormat = cam->_cfg->availablePixelFormats.first();
            qWarning() << "Required pixel format" << HEX(cam->_cfg->pixelFormat)
                << "is not provided by the camera, use default format"
                << HEX(defaultFormat.code) << defaultFormat.name;
            selectedFormat = defaultFormat.code;
            cam->_cfg->pixelFormat = *selectedFormat;
        }

        IBeamCaptureArgs captureArgs;
        memset(&captureArgs, 0, sizeof(IBeamCaptureArgs));
        
        if (selectedFormat) {
            captureArgs.pixelFormat = *selectedFormat;
            qDebug().noquote() << LOG_ID << "Set pixel format" << HEX(*selectedFormat) << selectedFormatName;
        }

        IBeamFrameInfo frameInfo;
        memset(&frameInfo, 0, sizeof(IBeamFrameInfo));
        res = adapter->startCapture(camPtr, &captureArgs, &frameInfo);
        CHECK_ERR("Unable to start capturing");
    
        cam->_width = frameInfo.width;
        cam->_height = frameInfo.height;
        cam->_bpp = frameInfo.bpp;

        if (frameInfo.pixelWidth != frameInfo.pixelHeight)
            qWarning() << LOG_ID << "Non-square pixels are not supported";
        else if (frameInfo.pixelWidth > 0) {
            cam->_pixelScale.on = true;
            cam->_pixelScale.factor = frameInfo.pixelWidth;
        }
        
        c.w = frameInfo.width;
        c.h = frameInfo.height;
        c.bpp = frameInfo.bpp;
        int bufSize = c.w * c.h * (frameInfo.bpp > 8 ? 2 : 1);
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
        qDebug() << LOG_ID << "Camera opened | id:" << id << "| hCam:" << camPtr;
        
        if (auto err = getFrameInfo(); !err.isEmpty()) return err;
        
        plot->initGraph(c.w, c.h);
        graph = plot->rawGraph();
        
        configure();
        
        showBrightness = cam->_cfg->showBrightness;
        saveBrightness = cam->_cfg->saveBrightness;
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
    _cfg.reset(new DllCameraConfig);

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
    
    requestExpoWarning();
    
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
    if (_cfg->showBrightness)
        rows.aux << qMakePair(ROW_BRIGHTNESS, qApp->tr("Brightness"));
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

ControlPanel* DllCamera::controlPanel(QWidget *parent)
{
    if (!_dll)
        return nullptr;
    if (!_controlPanel) {
        auto loadProp = [this](DllControlPanel::StoredProp prop) -> QVariant {
            switch (prop) {
            case DllControlPanel::StoredProp::AutoExpoTargetLevel:
                _cfg->autoExpoEnabled = true;
                return _cfg->autoExpoTargetLevel;
            case DllControlPanel::StoredProp::AutoExpoAvgFrames:
                return _cfg->autoExpoAvgFrames;
            case DllControlPanel::StoredProp::ExpoPresets:
                return QVariant::fromValue(&_cfg->propPresets);
            }
            return {};
        };
        auto saveProp = [this](DllControlPanel::StoredProp prop, QVariant value) {
            switch (prop) {
            case DllControlPanel::StoredProp::AutoExpoTargetLevel:
                _cfg->autoExpoTargetLevel = value.toInt();
                saveConfig(true);
                break;
            case DllControlPanel::StoredProp::AutoExpoAvgFrames:
                // this is not changed from control panel
                break;
            case DllControlPanel::StoredProp::ExpoPresets:
                // Presets already updated in the panel via pointer
                saveConfig(true);
                break;
            }
        };
        auto requestBrightness = [this](QObject *eventTarget){
            _dll->requestBrightness(eventTarget);
        };
        auto exposureChanged = [this]{
            raisePowerWarning();
            requestExpoWarning();
        };
        _controlPanel = new DllControlPanel(_dll->camPtr, _dll->adapter,
            loadProp, saveProp, requestBrightness, exposureChanged, parent);
    }
    return _controlPanel;
}


void DllCamera::initConfigMore(Ori::Dlg::ConfigDlgOpts &opts)
{
    if (_dll)
        _cfg->initDlg(opts, cfgPageCount);
}

void DllCamera::loadConfigMore(QSettings* s)
{
    _cfg->load(s);
}

void DllCamera::saveConfigMore(QSettings* s)
{
    // Just for user information.
    // _configGroup is the hashed _customId, hence unreadable,
    // but it's good to know the id when looking through the config file.
    s->setValue("customId", _customId);
    
    _cfg->save(s);
}

void DllCamera::requestExpoWarning()
{
    if (_dll)
        _dll->requestExpoWarning(_plot->eventsTarget());
}
