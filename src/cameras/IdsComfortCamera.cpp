#include "IdsComfortCamera.h"

#ifdef WITH_IDS

#include "cameras/CameraWorker.h"
#include "cameras/IdsCameraConfig.h"
#include "cameras/IdsHardConfig.h"
#include "cameras/IdsLib.h"

#include "helpers/OriDialogs.h"

#define LOG_ID "IdsComfortCamera:"
#define FRAME_TIMEOUT 5000
//#define LOG_FRAME_TIME

//------------------------------------------------------------------------------
//                              IdsComfort
//------------------------------------------------------------------------------

IdsComfort* IdsComfort::init()
{
    return IDS.loaded() ? new IdsComfort() : nullptr;
}

IdsComfort::~IdsComfort()
{
    if (IDS.loaded())
        IDS.unload();
}

static QString makeCameraName(const peak_camera_descriptor &cam)
{
    return QString("%1 (*%2)").arg(
        QString::fromLatin1(cam.modelName),
        QString::fromLatin1(cam.serialNumber).right(4));
}

QVector<CameraItem> IdsComfort::getCameras()
{
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

//------------------------------------------------------------------------------
//                              PeakIntf
//------------------------------------------------------------------------------

#define CHECK_ERR(msg) \
    if (PEAK_ERROR(res)) { \
        auto err = IDS.getPeakError(res); \
        qCritical() << LOG_ID << msg << err; \
        return QString(msg ": ") + err; \
    }

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

#define SHOW_CAM_PROP(prop, func, typ) {\
    typ val; \
    auto res = func(hCam, &val); \
    if (PEAK_ERROR(res)) \
        qDebug() << LOG_ID << "Unable to get" << prop << IDS.getPeakError(res); \
    else qDebug() << LOG_ID << prop << val; \
}

class PeakIntf : public CameraWorker
{
public:
    peak_camera_id id;
    IdsComfortCamera *cam;
    peak_camera_handle hCam = PEAK_INVALID_HANDLE;
    peak_status res;
    peak_buffer buf;
    peak_frame_handle frame;
    int errCount = 0;
    QByteArray hdrBuf;

    PeakIntf(peak_camera_id id, PlotIntf *plot, TableIntf *table, IdsComfortCamera *cam)
        : CameraWorker(plot, table, cam, cam, LOG_ID), id(id), cam(cam)
    {}

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

        //----------------- Init resolution reduction

        cam->_cfg->binningX = 1;
        cam->_cfg->binningY = 1;
        cam->_cfg->binningsX = {1, 2};
        cam->_cfg->binningsY = {1, 2};
        cam->_cfg->decimX = 1;
        cam->_cfg->decimY = 1;
        cam->_cfg->decimsX = {1, 2};
        cam->_cfg->decimsY = {1, 2};

        //----------------- Get image size

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

        //----------------- Init pixel format

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
        qDebug() << LOG_ID << "Set pixel format" << targetFormat << c.bpp << "bpp";
        res = IDS.peak_PixelFormat_Set(hCam, targetFormat);
        CHECK_ERR("Unable to set pixel format");
        cam->_cfg->bpp = c.bpp;
        if (c.bpp > 8) {
            hdrBuf = QByteArray(c.w*c.h*2, 0);
            c.buf = (uint8_t*)hdrBuf.data();
        }

        //----------------- Show some current props

        size_t chanCount;
        res = IDS.peak_Gain_GetChannelList(hCam, PEAK_GAIN_TYPE_ANALOG, nullptr, &chanCount);
        CHECK_ERR("Unable to get gain channel list count");
        QList<peak_gain_channel> chans(chanCount);
        res = IDS.peak_Gain_GetChannelList(hCam, PEAK_GAIN_TYPE_ANALOG, chans.data(), &chanCount);
        CHECK_ERR("Unable to get gain channel list");
        for (auto chan : chans)
            qDebug() << LOG_ID << "Gain channel" << chan;

        SHOW_CAM_PROP("FPS", IDS.peak_FrameRate_Get, double);
        SHOW_CAM_PROP("Exposure", IDS.peak_ExposureTime_Get, double);

        //----------------- Init graph and calcs

        plot->initGraph(c.w, c.h);
        graph = plot->rawGraph();

        configure();

        //----------------- Start

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
//                              IdsComfortCamera
//------------------------------------------------------------------------------

IdsComfortCamera::IdsComfortCamera(QVariant id, PlotIntf *plot, TableIntf *table, QObject *parent) :
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

IdsComfortCamera::~IdsComfortCamera()
{
    qDebug() << LOG_ID << "Deleted";
}

int IdsComfortCamera::bpp() const
{
    return _cfg->bpp;
}

PixelScale IdsComfortCamera::sensorScale() const
{
    return _pixelScale;
}

void IdsComfortCamera::startCapture()
{
    start();
}

void IdsComfortCamera::stopCapture()
{
    if (_peak)
        _peak.reset(nullptr);
}

void IdsComfortCamera::startMeasure(MeasureSaver *saver)
{
    if (_peak)
        _peak->startMeasure(saver);
}

void IdsComfortCamera::stopMeasure()
{
    if (_peak)
        _peak->stopMeasure();
}

void IdsComfortCamera::run()
{
    if (_peak)
        _peak->run();
}

void IdsComfortCamera::camConfigChanged()
{
    if (_peak)
        _peak->reconfigure();
}

void IdsComfortCamera::saveHardConfig(QSettings *s)
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

void IdsComfortCamera::requestRawImg(QObject *sender)
{
    if (_peak)
        _peak->requestRawImg(sender);
}

void IdsComfortCamera::setRawView(bool on, bool reconfig)
{
    if (_peak)
        _peak->setRawView(on, reconfig);
}

void IdsComfortCamera::initConfigMore(Ori::Dlg::ConfigDlgOpts &opts)
{
    if (_peak)
        _cfg->initDlg(_peak->hCam, opts, cfgMax);
}

void IdsComfortCamera::saveConfigMore()
{
    _cfg->save(_configGroup);
}

void IdsComfortCamera::loadConfigMore()
{
    _cfg->load(_configGroup);
}

HardConfigPanel* IdsComfortCamera::hardConfgPanel(QWidget *parent)
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
