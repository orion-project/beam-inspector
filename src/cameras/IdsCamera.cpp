#include "IdsCamera.h"

#ifdef WITH_IDS

#include "cameras/CameraWorker.h"
#include "cameras/IdsCameraConfig.h"
#include "cameras/IdsHardConfig.h"
#include "cameras/IdsLib.h"

#include "helpers/OriDialogs.h"

#include <QSettings>

#define LOG_ID "IdsCamera:"
#define FRAME_TIMEOUT 5000
//#define LOG_FRAME_TIME

enum CamDataRow { ROW_RENDER_TIME, ROW_CALC_TIME,
    ROW_FRAME_ERR, ROW_FRAME_UNDERRUN, ROW_FRAME_DROPPED, ROW_FRAME_INCOMPLETE,
    ROW_BRIGHTNESS, ROW_POWER };

static QString makeDisplayName(const peak_camera_descriptor &cam)
{
    return QString("%1 (*%2)").arg(
        QString::fromLatin1(cam.modelName),
        QString::fromLatin1(cam.serialNumber).right(4));
}

static QString makeCustomId(const peak_camera_descriptor &cam)
{
    return QString("IDS-%1-%2").arg(
        QString::fromLatin1(cam.modelName),
        QString::fromLatin1(cam.serialNumber));
}

//------------------------------------------------------------------------------
//                              PeakIntf
//------------------------------------------------------------------------------

#define CHECK_ERR(msg) \
    if (PEAK_ERROR(res)) { \
        auto err = IDS.getPeakError(res); \
        qCritical() << LOG_ID << msg << err; \
        return QString(msg ": ") + err; \
    }

class PeakIntf : public CameraWorker
{
public:
    peak_camera_id id;
    IdsCamera *cam;
    peak_camera_handle hCam = PEAK_INVALID_HANDLE;
    peak_status res;
    peak_buffer buf;
    peak_frame_handle frame;
    QByteArray hdrBuf;

    int framesErr = 0;
    int framesDropped = 0;
    int framesUnderrun = 0;
    int framesIncomplete = 0;

    PeakIntf(peak_camera_id id, PlotIntf *plot, TableIntf *table, IdsCamera *cam)
        : CameraWorker(plot, table, cam, cam, LOG_ID), id(id), cam(cam)
    {
        tableData = [this]{
            QMap<int, CamTableData> data = {
                { ROW_RENDER_TIME, {avgAcqTime} },
                { ROW_CALC_TIME, {avgCalcTime} },
                { ROW_FRAME_ERR, {framesErr, CamTableData::COUNT, framesErr > 0} },
                { ROW_FRAME_DROPPED, {framesDropped, CamTableData::COUNT, framesDropped > 0} },
                { ROW_FRAME_UNDERRUN, {framesUnderrun, CamTableData::COUNT, framesUnderrun > 0} },
                { ROW_FRAME_INCOMPLETE, {framesIncomplete, CamTableData::COUNT, framesIncomplete > 0} },
            };
            if (showBrightness)
                data[ROW_BRIGHTNESS] = {brightness, CamTableData::VALUE3};
            if (showPower)
                data[ROW_POWER] = {
                    QVariantList{
                        power * powerScale,
                        powerSdev * powerScale,
                        powerDecimalFactor,
                    },
                    CamTableData::POWER,
                    hasPowerWarning
                };
            return data;
        };
    }

    QString initResolution()
    {
        cam->_cfg->binning.configurable = false;
        cam->_cfg->decimation.configurable = false;

        if (IDS.peak_Binning_GetAccessStatus(hCam) != PEAK_ACCESS_READWRITE) {
            if (IDS.peak_Decimation_GetAccessStatus(hCam) != PEAK_ACCESS_READWRITE) {
                qWarning() << LOG_ID << "Resolution is not configurable";
                return {};
            }
            res = IDS.peak_Decimation_Set(hCam, 1, 1);
            CHECK_ERR("Unable to reset decimation");
            cam->_cfg->decimation.configurable = true;
            if (IDS.peak_Binning_GetAccessStatus(hCam) != PEAK_ACCESS_READWRITE) {
                qDebug() << LOG_ID << "Binning is not configurable";
            } else {
                res = IDS.peak_Binning_Set(hCam, 1, 1);
                CHECK_ERR("Unable to reset binning");
                cam->_cfg->binning.configurable = true;
            }
        } else {
            res = IDS.peak_Binning_Set(hCam, 1, 1);
            CHECK_ERR("Unable to reset binning");
            cam->_cfg->binning.configurable = true;

            if (IDS.peak_Decimation_GetAccessStatus(hCam) != PEAK_ACCESS_READWRITE) {
                qDebug() << LOG_ID << "Decimation is not configurable";
            } else {
                res = IDS.peak_Decimation_Set(hCam, 1, 1);
                CHECK_ERR("Unable to reset decimation");
                cam->_cfg->decimation.configurable = true;
            }
        }

        // Load factors when both are 1x1, before changing anything
        #define LOAD_FACTORS(method, target) { \
            size_t count; \
            res = IDS.method(hCam, nullptr, &count); \
            CHECK_ERR("Unable to get count of " #target); \
            cam->_cfg->target.resize(count); \
            res = IDS.method(hCam, cam->_cfg->target.data(), &count); \
            CHECK_ERR("Unable to get " #target); \
        }
        if (cam->_cfg->binning.configurable) {
            LOAD_FACTORS(peak_Binning_FactorX_GetList, binning.xs);
            LOAD_FACTORS(peak_Binning_FactorY_GetList, binning.ys);
        }
        if (cam->_cfg->decimation.configurable) {
            LOAD_FACTORS(peak_Decimation_FactorX_GetList, decimation.xs);
            LOAD_FACTORS(peak_Decimation_FactorY_GetList, decimation.ys);
        }
        #undef LOAD_FACTORS

        // Reset ROI to max before settings binning/decimation
        peak_size roiMin, roiMax, roiInc;
        res = IDS.peak_ROI_Size_GetRange(hCam, &roiMin, &roiMax, &roiInc);
        CHECK_ERR("Unable to get ROI range");
        qDebug() << LOG_ID << "ROI"
            << QString("min=%1x%24").arg(roiMin.width).arg(roiMin.height)
            << QString("max=%1x%24").arg(roiMax.width).arg(roiMax.height)
            << QString("inc=%1x%24").arg(roiInc.width).arg(roiInc.height);

        peak_roi roi;
        res = IDS.peak_ROI_Get(hCam, &roi);
        CHECK_ERR("Unable to get ROI");
        qDebug() << LOG_ID << "ROI"
            << QString("size=%1x%2").arg(roi.size.width).arg(roi.size.height)
            << QString("offset=%1x%2").arg(roi.offset.x).arg(roi.offset.y);
        if (roi.size.width != roiMax.width || roi.size.height != roiMax.height) {
            roi.offset.x = 0;
            roi.offset.y = 0;
            roi.size.width = roiMax.width;
            roi.size.height = roiMax.height;
            qDebug() << LOG_ID << "Set ROI"
                << QString("size=%1x%2").arg(roi.size.width).arg(roi.size.height);
            res = IDS.peak_ROI_Set(hCam, roi);
            CHECK_ERR("Unable to set ROI");
        }

        #define CLAMP_FACTOR(factor, list) \
            if (cam->_cfg->factor < 1) cam->_cfg->factor = 1; \
            else if (!cam->_cfg->list.contains(cam->_cfg->factor)) { \
                qWarning() << LOG_ID << #factor << cam->_cfg->factor << "is out of range, reset to 1"; \
                cam->_cfg->factor = 1; \
           }
        if (cam->_cfg->binning.configurable) {
            CLAMP_FACTOR(binning.x, binning.xs);
            CLAMP_FACTOR(binning.y, binning.ys);
            if (cam->_cfg->binning.on()) {
                res = IDS.peak_Binning_Set(hCam, cam->_cfg->binning.x, cam->_cfg->binning.y);
                CHECK_ERR("Unable to set binning");
                res = IDS.peak_Binning_Get(hCam, &cam->_cfg->binning.x, &cam->_cfg->binning.y);
                CHECK_ERR("Unable to get binning");
                if (cam->_cfg->decimation.configurable && cam->_cfg->decimation.on()) {
                    qWarning() << "Unable to set both binning and decimation, binning is already set, reset decimation to 1";
                    cam->_cfg->decimation.reset();
                }
            }
            qDebug() << LOG_ID << "Binning" << cam->_cfg->binning.str();
        }
        if (cam->_cfg->decimation.configurable) {
            CLAMP_FACTOR(decimation.x, decimation.xs);
            CLAMP_FACTOR(decimation.y, decimation.ys);
            if (cam->_cfg->decimation.on()) {
                res = IDS.peak_Decimation_Set(hCam, cam->_cfg->decimation.x, cam->_cfg->decimation.y);
                CHECK_ERR("Unable to set decimation");
                res = IDS.peak_Decimation_Get(hCam, &cam->_cfg->decimation.x, &cam->_cfg->decimation.y);
                CHECK_ERR("Unable to get decimation");
            }
            qDebug() << LOG_ID << "Decimation" << cam->_cfg->decimation.str();
        }
        #undef CLAMP_FACTOR

        // Get reduced image size after binning/decimation has been set
        if ((cam->_cfg->binning.configurable && cam->_cfg->binning.on()) ||
            (cam->_cfg->decimation.configurable && cam->_cfg->decimation.on())) {
            res = IDS.peak_ROI_Get(hCam, &roi);
            CHECK_ERR("Unable to get ROI");
            qDebug() << LOG_ID << "ROI" << QString("size=%1x%2").arg(roi.size.width).arg(roi.size.height);
        }
        c.w = roi.size.width;
        c.h = roi.size.height;
        cam->_width = c.w;
        cam->_height = c.h;

        #define GFA_GET_FLOAT(prop, value) \
            if (PEAK_IS_READABLE(IDS.peak_GFA_Feature_GetAccessStatus(hCam, PEAK_GFA_MODULE_REMOTE_DEVICE, prop))) { \
                res = IDS.peak_GFA_Float_Get(hCam, PEAK_GFA_MODULE_REMOTE_DEVICE, prop, &value); \
                if (PEAK_ERROR(res)) { \
                    qWarning() << LOG_ID << "Failed to read" << prop << IDS.getPeakError(res); \
                    break; \
                } \
                qDebug() << LOG_ID << prop << value; \
            } else { \
                qDebug() << LOG_ID << prop << "is not readable"; \
                break; \
            }
        while (true) {
            double pixelW, pixelH;
            GFA_GET_FLOAT("SensorPixelWidth", pixelW);
            GFA_GET_FLOAT("SensorPixelHeight", pixelH);
            if (pixelW != pixelH) {
                qWarning() << LOG_ID << "Non-square pixels are not supported";
                break;
            }
            cam->_pixelScale.on = true;
            cam->_pixelScale.factor = pixelW;
            break;
        }
        #undef GFA_GET_FLOAT

        return {};
    }

    QString initPixelFormat()
    {
        size_t formatCount = 0;
        res = IDS.peak_PixelFormat_GetList(hCam, nullptr, &formatCount);
        CHECK_ERR("Unable to get pixel format count");
        QVector<peak_pixel_format> pixelFormats(formatCount);
        res = IDS.peak_PixelFormat_GetList(hCam, pixelFormats.data(), &formatCount);
        CHECK_ERR("Unable to get pixel formats");
        QMap<int, peak_pixel_format> supportedFormats;
        for (int i = 0; i < formatCount; i++) {
            auto pf = pixelFormats.at(i);
            qDebug() << LOG_ID << "Supported pixel format" << QString::number(pf, 16);
            if (pf == PEAK_PIXEL_FORMAT_MONO8) {
                cam->_cfg->supportedBpp << 8;
                supportedFormats.insert(8, pf);
            } else if (pf == PEAK_PIXEL_FORMAT_MONO10G40_IDS) {
                cam->_cfg->supportedBpp << 10;
                supportedFormats.insert(10, pf);
            } else if (pf == PEAK_PIXEL_FORMAT_MONO12G24_IDS) {
                cam->_cfg->supportedBpp << 12;
                supportedFormats.insert(12, pf);
            }
        }
        if (supportedFormats.empty()) {
            return "Camera doesn't support any of known gray scale formats (Mono8, Mono10g40, Mono12g24)";
        }
        c.bpp = cam->_cfg->bpp;
        peak_pixel_format targetFormat;
        if (!supportedFormats.contains(c.bpp)) {
            c.bpp = supportedFormats.firstKey();
            targetFormat = supportedFormats.first();
            qWarning() << LOG_ID << "Camera does not support " << cam->_cfg->bpp << "bpp, use " << c.bpp << "bpp";
        } else {
            targetFormat = supportedFormats[c.bpp];
        }
        qDebug() << LOG_ID << "Set pixel format" << QString::number(targetFormat, 16) << c.bpp << "bpp";
        res = IDS.peak_PixelFormat_Set(hCam, targetFormat);
        CHECK_ERR("Unable to set pixel format");
        cam->_cfg->bpp = c.bpp;
        if (c.bpp > 8) {
            hdrBuf = QByteArray(c.w*c.h*2, 0);
            c.buf = (uint8_t*)hdrBuf.data();
        }
        return {};
    }

    QString showCurrProps()
    {
        size_t chanCount;
        res = IDS.peak_Gain_GetChannelList(hCam, PEAK_GAIN_TYPE_ANALOG, nullptr, &chanCount);
        CHECK_ERR("Unable to get gain channel list count");
        QList<peak_gain_channel> chans(chanCount);
        res = IDS.peak_Gain_GetChannelList(hCam, PEAK_GAIN_TYPE_ANALOG, chans.data(), &chanCount);
        CHECK_ERR("Unable to get gain channel list");
        for (auto chan : chans)
            qDebug() << LOG_ID << "Gain channel" << chan;

        #define SHOW_CAM_PROP(prop, func, typ) {\
            typ val; \
            auto res = func(hCam, &val); \
            if (PEAK_ERROR(res)) \
                qDebug() << LOG_ID << "Unable to get" << prop << IDS.getPeakError(res); \
            else qDebug() << LOG_ID << prop << val; \
        }
        SHOW_CAM_PROP("FPS", IDS.peak_FrameRate_Get, double);
        SHOW_CAM_PROP("Exposure", IDS.peak_ExposureTime_Get, double);
        #undef SHOW_CAM_PROP
        return {};
    }

    QString init()
    {
        peak_camera_descriptor descr;
        res = IDS.peak_Camera_GetDescriptor(id, &descr);
        CHECK_ERR("Unable to get camera descriptor");
        cam->_name = makeDisplayName(descr);
        cam->_customId = makeCustomId(descr);
        cam->_descr = QString("%1 (SN: %2)").arg(
            QString::fromLatin1(descr.modelName),
            QString::fromLatin1(descr.serialNumber));
        cam->_configGroup = cam->_customId;
        cam->loadConfig();

        res = IDS.peak_Camera_Open(id, &hCam);
        CHECK_ERR("Unable to open camera");
        qDebug() << LOG_ID << "Camera opened" << id;

        if (auto err = initResolution(); !err.isEmpty()) return err;
        if (auto err = initPixelFormat(); !err.isEmpty()) return err;
        if (auto err = showCurrProps(); !err.isEmpty()) return err;

        plot->initGraph(c.w, c.h);
        graph = plot->rawGraph();

        configure();

        res = IDS.peak_Acquisition_Start(hCam, PEAK_INFINITE);
        CHECK_ERR("Unable to start acquisition");
        qDebug() << LOG_ID << "Acquisition started";

        showBrightness = cam->_cfg->showBrightness;
        saveBrightness = cam->_cfg->saveBrightness;
        togglePowerMeter();

        return {};
    }

    ~PeakIntf()
    {
        if (hCam == PEAK_INVALID_HANDLE)
            return;

        if (IDS.peak_Acquisition_IsStarted(hCam))
        {
            auto res = IDS.peak_Acquisition_Stop(hCam);
            if (PEAK_ERROR(res))
                qWarning() << LOG_ID << "Unable to stop acquisition";
            else qDebug() << LOG_ID << "Acquisition stopped";
        }

        auto res = IDS.peak_Camera_Close(hCam);
        if (PEAK_ERROR(res))
            qWarning() << LOG_ID << "Unable to close camera" << IDS.getPeakError(res);
        else qDebug() << LOG_ID << "Camera closed" << id;
        hCam = PEAK_INVALID_HANDLE;
    }

    void run()
    {
        qDebug() << LOG_ID << "Started" << QThread::currentThreadId();
        start = QDateTime::currentDateTime();
        timer.start();
        while (true) {
            tm = timer.elapsed();
            avgFrameCount++;
            avgFrameTime += tm - prevFrame;
            prevFrame = tm;

            tm = timer.elapsed();
            res = IDS.peak_Acquisition_WaitForFrame(hCam, FRAME_TIMEOUT, &frame);
            if (PEAK_SUCCESS(res))
                res = IDS.peak_Frame_Buffer_Get(frame, &buf);
            if (res == PEAK_STATUS_ABORTED) {
                auto err = IDS.getPeakError(res);
                qCritical() << LOG_ID << "Interrupted" << err;
                emit cam->error("Interrupted: " + err);
                return;
            }
            markAcqTime();

            if (res == PEAK_STATUS_SUCCESS) {
                tm = timer.elapsed();
                if (c.bpp == 12)
                    cgn_convert_12g24_to_u16(c.buf, buf.memoryAddress, buf.memorySize);
                else if (c.bpp == 10)
                    cgn_convert_10g40_to_u16(c.buf, buf.memoryAddress, buf.memorySize);
                else
                    c.buf = buf.memoryAddress;
                calcResult();
                markCalcTime();

                if (showResults())
                    emit cam->ready();

                res = IDS.peak_Frame_Release(hCam, frame);
                if (PEAK_ERROR(res)) {
                    auto err = IDS.getPeakError(res);
                    qCritical() << LOG_ID << "Unable to release frame" << err;
                    emit cam->error("Unable to release frame: " + err);
                    return;
                }
            } else {
                framesErr++;
                stats[QStringLiteral("frameErrors")] = framesErr;
                QString errKey = QStringLiteral("frameError_") + QString::number(res, 16);
                stats[errKey] = stats[errKey].toInt() + 1;
            }

            if (tm - prevStat >= STAT_DELAY_MS) {
                prevStat = tm;

                peak_acquisition_info info;
                memset(&info, 0, sizeof(info));
                res = IDS.peak_Acquisition_GetInfo(hCam, &info);
                if (PEAK_SUCCESS(res)) {
                    framesDropped = info.numDropped;
                    framesUnderrun = info.numUnderrun;
                    framesIncomplete = info.numIncomplete;
                    stats[QStringLiteral("framesDropped")] = framesDropped;
                    stats[QStringLiteral("framesUnderrun")] = framesUnderrun;
                    stats[QStringLiteral("framesIncomplete")] = framesIncomplete;
                }

                double hardFps;
                res = IDS.peak_FrameRate_Get(hCam, &hardFps);
                if (PEAK_ERROR(res)) {
                    hardFps = 0;
                }

                double ft = avgFrameTime / avgFrameCount;
                avgFrameTime = 0;
                avgFrameCount = 0;
                CameraStats st {
                    .fps = 1000.0/ft,
                    .hardFps = hardFps,
                    .measureTime = measureStart > 0 ? timer.elapsed() - measureStart : -1,
                };
                emit cam->stats(st);

#ifdef LOG_FRAME_TIME
                qDebug()
                    << "FPS:" << st.fps
                    << "avgFrameTime:" << qRound(ft)
                    << "avgAcqTime:" << qRound(avgAcqTime)
                    << "avgCalcTime:" << qRound(avgCalcTime)
                    << "errCount: " << errCount
                    << IDS.getPeakError(res);
#endif
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
//                              IdsCamera
//------------------------------------------------------------------------------

QVector<CameraItem> IdsCamera::getCameras()
{
    if (!IDS.loaded())
        return {};

    size_t camCount;
    auto res = IDS.peak_CameraList_Update(&camCount);
    if (PEAK_ERROR(res)) {
        qWarning() << LOG_ID << "Unable to update camera list" << IDS.getPeakError(res);
        return {};
    }
    else qDebug() << LOG_ID << "Camera list updated. Cameras found:" << camCount;

    QVector<peak_camera_descriptor> cams(camCount);
    res = IDS.peak_CameraList_Get(cams.data(), &camCount);
    if (PEAK_ERROR(res)) {
        qWarning() << LOG_ID << "Unable to get camera list" << IDS.getPeakError(res);
        return {};
    }

    QVector<CameraItem> result;
    for (const auto &cam : cams) {
        qDebug() << LOG_ID << cam.cameraID << cam.cameraType << cam.modelName << cam.serialNumber;
        result << CameraItem {
            .cameraId = cam.cameraID,
            .customId = makeCustomId(cam),
            .displayName = makeDisplayName(cam),
        };
    }
    return result;
}

QString IdsCamera::libError()
{
    return IDS.loaded() ? QString() : IDS.libError;
}

void IdsCamera::unloadLib()
{
    if (IDS.loaded())
        IDS.unload();
}

IdsCamera::IdsCamera(QVariant id, PlotIntf *plot, TableIntf *table, QObject *parent) :
    Camera(plot, table, "IdsCamera"), QThread(parent)
{
    _cfg.reset(new IdsCameraConfig);

    auto peak = new PeakIntf(id.value<peak_camera_id>(), plot, table, this);
    auto res = peak->init();
    if (!res.isEmpty())
    {
        Ori::Dlg::error(res);
        delete peak;
        return;
    }
    _peak.reset(peak);

    connect(parent, SIGNAL(camConfigChanged()), this, SLOT(camConfigChanged()));
}

IdsCamera::~IdsCamera()
{
    qDebug() << LOG_ID << "Deleted";
}

int IdsCamera::bpp() const
{
    return _cfg->bpp;
}

PixelScale IdsCamera::sensorScale() const
{
    return _pixelScale;
}

TableRowsSpec IdsCamera::tableRows() const
{
    auto rows = Camera::tableRows();
    rows.aux
        << qMakePair(ROW_RENDER_TIME, qApp->tr("Acq. time"))
        << qMakePair(ROW_CALC_TIME, qApp->tr("Calc time"))
        << qMakePair(ROW_FRAME_ERR, qApp->tr("Errors"))
        << qMakePair(ROW_FRAME_DROPPED, qApp->tr("Dropped"))
        << qMakePair(ROW_FRAME_UNDERRUN, qApp->tr("Underrun"))
        << qMakePair(ROW_FRAME_INCOMPLETE, qApp->tr("Incomplete"));
    if (_cfg->showBrightness)
        rows.aux << qMakePair(ROW_BRIGHTNESS, qApp->tr("Brightness"));
    if (_config.power.on)
        rows.aux << qMakePair(ROW_POWER, qApp->tr("Power"));
    return rows;
}

QList<QPair<int, QString>> IdsCamera::measurCols() const
{
    QList<QPair<int, QString>> cols;
    if (_cfg->saveBrightness)
        cols << qMakePair(COL_BRIGHTNESS, qApp->tr("Brightness"));
    if (_config.power.on)
        cols << qMakePair(COL_POWER, qApp->tr("Power"));
    return cols;
}

void IdsCamera::startCapture()
{
    start();
}

void IdsCamera::stopCapture()
{
    if (_peak)
        _peak.reset(nullptr);
}

void IdsCamera::startMeasure(MeasureSaver *saver)
{
    if (_peak)
        _peak->startMeasure(saver);
}

void IdsCamera::stopMeasure()
{
    if (_peak)
        _peak->stopMeasure();
}

void IdsCamera::run()
{
    if (_peak)
        _peak->run();
}

void IdsCamera::camConfigChanged()
{
    if (_peak)
        _peak->reconfigure();
}

void IdsCamera::saveHardConfig(QSettings *s)
{
    if (!_peak)
        return;
    double v;
    auto res = IDS.peak_ExposureTime_Get(_peak->hCam, &v);
    if (PEAK_ERROR(res))
        s->setValue("exposure", IDS.getPeakError(res));
    else {
        s->setValue("exposure", v);
        s->setValue("exposure.unit", "us");
    }
    res = IDS.peak_FrameRate_Get(_peak->hCam, &v);
    if (PEAK_ERROR(res))
        s->setValue("frameRate", IDS.getPeakError(res));
    else s->setValue("frameRate", v);
}

void IdsCamera::requestRawImg(QObject *sender)
{
    if (_peak)
        _peak->requestRawImg(sender);
}

void IdsCamera::setRawView(bool on, bool reconfig)
{
    if (_peak)
        _peak->setRawView(on, reconfig);
}

void IdsCamera::initConfigMore(Ori::Dlg::ConfigDlgOpts &opts)
{
    if (_peak)
        _cfg->initDlg(_peak->hCam, opts, cfgMax);
}

void IdsCamera::saveConfigMore(QSettings *s)
{
    _cfg->save(s);
    if (_cfg->hasPowerWarning)
        raisePowerWarning();
}

void IdsCamera::loadConfigMore(QSettings *s)
{
    _cfg->load(s);
}

HardConfigPanel* IdsCamera::hardConfgPanel(QWidget *parent)
{
    if (!_peak)
        return nullptr;
    if (!_configPanel) {
        auto requestBrightness = [this](QObject *s){ _peak->requestBrightness(s); };
        auto getCamProp = [this](IdsHardConfigPanel::CamProp prop) -> QVariant {
            switch (prop) {
            case IdsHardConfigPanel::AUTOEXP_LEVEL:
                return _cfg->autoExpLevel;
            case IdsHardConfigPanel::AUTOEXP_FRAMES_AVG:
                return _cfg->autoExpFramesAvg;
            case IdsHardConfigPanel::EXP_PRESETS:
                return QVariant::fromValue(&_cfg->expPresets);
            case IdsHardConfigPanel::FPS_LOCK:
                return _cfg->fpsLock;
            }
            return {};
        };
        auto setCamProp = [this](IdsHardConfigPanel::CamProp prop, QVariant value) {
            switch (prop) {
            case IdsHardConfigPanel::AUTOEXP_LEVEL:
                _cfg->autoExpLevel = value.toInt();
                saveConfig(true);
                break;
            case IdsHardConfigPanel::AUTOEXP_FRAMES_AVG:
                // this is not changed from hard config panel
                break;
            case IdsHardConfigPanel::EXP_PRESETS:
                // Presets already updated in the panel via pointer
                saveConfig(true);
                break;
            case IdsHardConfigPanel::FPS_LOCK:
                _cfg->fpsLock = value.toDouble();
                saveConfig(true);
                break;
            }
        };
        auto raisePowerWarning = [this]{ this->raisePowerWarning(); };
        _configPanel = new IdsHardConfigPanel(_peak->hCam, requestBrightness, getCamProp, setCamProp, raisePowerWarning, parent);
    }
    return _configPanel;
}

void IdsCamera::togglePowerMeter()
{
    if (_peak)
        _peak->togglePowerMeter();
}

void IdsCamera::raisePowerWarning()
{
    if (_peak)
        _peak->hasPowerWarning = true;
}

#endif // WITH_IDS
