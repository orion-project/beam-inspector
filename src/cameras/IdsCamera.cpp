#include "IdsCamera.h"

#ifdef WITH_IDS

#include "cameras/CameraWorker.h"
#include "cameras/IdsCameraConfig.h"
#include "cameras/IdsHardConfig.h"
#include "cameras/IdsLib.h"

#include "helpers/OriDialogs.h"

#include <QSettings>

#define LOG_ID "IdsComfortCamera:"
#define FRAME_TIMEOUT 5000
//#define LOG_FRAME_TIME

static QString makeCameraName(const peak_camera_descriptor &cam)
{
    return QString("%1 (*%2)").arg(
        QString::fromLatin1(cam.modelName),
        QString::fromLatin1(cam.serialNumber).right(4));
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
    int errCount = 0;
    QByteArray hdrBuf;

    PeakIntf(peak_camera_id id, PlotIntf *plot, TableIntf *table, IdsCamera *cam)
        : CameraWorker(plot, table, cam, cam, LOG_ID), id(id), cam(cam)
    {}

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
        return {};
    }

    QString getImageSize()
    {
        peak_size roiMin, roiMax, roiInc;
        res = IDS.peak_ROI_Size_GetRange(hCam, &roiMin, &roiMax, &roiInc);
        CHECK_ERR("Unable to get ROI range");
        qDebug() << LOG_ID << "ROI"
            << QString("min=%1x%24").arg(roiMin.width).arg(roiMin.height)
            << QString("max=%1x%24").arg(roiMax.width).arg(roiMax.height)
            << QString("inc=%1x%24").arg(roiInc.width).arg(roiInc.height);

        peak_roi roi = {0, 0, 0, 0};
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
                << QString("size=%1x%2").arg(roiMax.width).arg(roiMax.height);
            res = IDS.peak_ROI_Set(hCam, roi);
            CHECK_ERR("Unable to set ROI");
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
        cam->_name = makeCameraName(descr);
        cam->_descr = cam->_name + ' ' + QString::fromLatin1(descr.serialNumber);
        cam->_configGroup = cam->_name + '-' + QString::fromLatin1(descr.serialNumber);
        cam->loadConfig();
        cam->loadConfigMore();

        res = IDS.peak_Camera_Open(id, &hCam);
        CHECK_ERR("Unable to open camera");
        qDebug() << LOG_ID << "Camera opened" << id;

        if (auto err = initResolution(); !err.isEmpty()) return err;
        if (auto err = getImageSize(); !err.isEmpty()) return err;
        if (auto err = initPixelFormat(); !err.isEmpty()) return err;
        if (auto err = showCurrProps(); !err.isEmpty()) return err;

        plot->initGraph(c.w, c.h);
        graph = plot->rawGraph();

        configure();

        res = IDS.peak_Acquisition_Start(hCam, PEAK_INFINITE);
        CHECK_ERR("Unable to start acquisition");
        qDebug() << LOG_ID << "Acquisition started";

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
            if (waitFrame()) continue;

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
            markRenderTime();

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
                errCount++;
                stats[QStringLiteral("errorFrame")] = errCount;
                QString errKey = QStringLiteral("frameError_") + QString::number(res, 16);
                stats[errKey] = stats[errKey].toInt() + 1;
            }

            if (tm - prevStat >= STAT_DELAY_MS) {
                prevStat = tm;

                QStringList errors;
                errors << QString::number(errCount);

                // TODO: suggest a way for sending arbitrary stats and showing it in the table
                peak_acquisition_info info;
                memset(&info, 0, sizeof(info));
                res = IDS.peak_Acquisition_GetInfo(hCam, &info);
                if (PEAK_SUCCESS(res)) {
                    errors << QString::number(info.numUnderrun)
                            << QString::number(info.numDropped)
                            << QString::number(info.numIncomplete);
                    stats[QStringLiteral("underrunFrames")] = info.numUnderrun;
                    stats[QStringLiteral("droppedFrames")] = info.numDropped;
                    stats[QStringLiteral("incompleteFrames")] = info.numIncomplete;
                }

                double ft = avgFrameTime / avgFrameCount;
                avgFrameTime = 0;
                avgFrameCount = 0;
                CameraStats st {
                    .fps = qRound(1000.0/ft),
                    .measureTime = measureStart > 0 ? timer.elapsed() - measureStart : -1,
                    .errorFrames = errors.join(','),
                };
                emit cam->stats(st);

#ifdef LOG_FRAME_TIME
                qDebug()
                    << "FPS:" << st.fps
                    << "avgFrameTime:" << qRound(ft)
                    << "avgRenderTime:" << qRound(avgRenderTime)
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
            .id = cam.cameraID,
            .name = makeCameraName(cam)
        };
    }
    return result;
}

void IdsCamera::unloadLib()
{
    if (IDS.loaded())
        IDS.unload();
}

IdsCamera::IdsCamera(QVariant id, PlotIntf *plot, TableIntf *table, QObject *parent) :
    Camera(plot, table, "IdsComfortCamera"), QThread(parent), _id(id)
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
    else s->setValue("exposure", v);
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

void IdsCamera::saveConfigMore()
{
    _cfg->save(_configGroup);
}

void IdsCamera::loadConfigMore()
{
    _cfg->load(_configGroup);
}

HardConfigPanel* IdsCamera::hardConfgPanel(QWidget *parent)
{
    if (!_peak)
        return nullptr;
    if (!_configPanel) {
        auto requestBrightness = [this](QObject *s){ _peak->requestBrightness(s); };
        _configPanel = new IdsHardConfigPanel(_peak->hCam, requestBrightness, parent);
    }
    return _configPanel;
}

#endif // WITH_IDS
