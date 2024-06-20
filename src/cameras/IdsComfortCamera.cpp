#include "IdsComfortCamera.h"

#ifdef WITH_IDS

#include "cameras/CameraWorker.h"
#include "cameras/IdsHardConfig.h"
#include "cameras/IdsLib.h"

#include "dialogs/OriConfigDlg.h"
#include "helpers/OriDialogs.h"
#include "tools/OriSettings.h"

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
    QSet<int> supportedBpp;

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

        cam->_cfg.binningX = 1;
        cam->_cfg.binningY = 1;
        cam->_cfg.binningsX = {1, 2};
        cam->_cfg.binningsY = {1, 2};
        cam->_cfg.decimX = 1;
        cam->_cfg.decimY = 1;
        cam->_cfg.decimsX = {1, 2};
        cam->_cfg.decimsY = {1, 2};

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
                supportedBpp << 8;
                supportedFormats.insert(8, pf);
            } else if (pf == PEAK_PIXEL_FORMAT_MONO10G40_IDS) {
                supportedBpp << 10;
                supportedFormats.insert(10, pf);
            } else if (pf == PEAK_PIXEL_FORMAT_MONO12G24_IDS) {
                supportedBpp << 12;
                supportedFormats.insert(12, pf);
            }
        }
        if (supportedFormats.empty()) {
            return "Camera doesn't support any of known gray scale formats (Mono8, Mono10g40, Mono12g24)";
        }
        c.bpp = cam->_bpp;
        peak_pixel_format targetFormat;
        if (!supportedFormats.contains(c.bpp)) {
            c.bpp = supportedFormats.firstKey();
            targetFormat = supportedFormats.first();
            qWarning() << LOG_ID << "Camera does not support " << cam->_bpp << "bpp, use " << c.bpp << "bpp";
        } else {
            targetFormat = supportedFormats[c.bpp];
        }
        qDebug() << LOG_ID << "Set pixel format" << targetFormat << c.bpp << "bpp";
        res = IDS.peak_PixelFormat_Set(hCam, targetFormat);
        CHECK_ERR("Unable to set pixel format");
        cam->_bpp = c.bpp;
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

    QString gfaGetStr(const char* prop)
    {
        auto mod = PEAK_GFA_MODULE_REMOTE_DEVICE;
        if (!PEAK_IS_READABLE(IDS.peak_GFA_Feature_GetAccessStatus(hCam, mod, prop)))
            return "Is not readable";
        size_t size;
        auto res = IDS.peak_GFA_String_Get(hCam, mod, prop, nullptr, &size);
        if (PEAK_ERROR(res))
            return IDS.getPeakError(res);
        QByteArray buf(size, 0);
        res = IDS.peak_GFA_String_Get(hCam, mod, prop, buf.data(), &size);
        if (PEAK_ERROR(res))
            return IDS.getPeakError(res);
        qDebug() << "Reading" << prop << 3;
        return QString::fromLatin1(buf);
    }
};

//------------------------------------------------------------------------------
//                              IdsComfortCamera
//------------------------------------------------------------------------------

IdsComfortCamera::IdsComfortCamera(QVariant id, PlotIntf *plot, TableIntf *table, QObject *parent) :
    Camera(plot, table, "IdsComfortCamera"), QThread(parent), _id(id)
{
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

#include <QComboBox>

#include "helpers/OriLayouts.h"

using namespace Ori::Dlg;

class ConfigEditorXY : public ConfigItemEditor
{
public:
    ConfigEditorXY(const QList<quint32> &xs, const QList<quint32> &ys, quint32 *x, quint32 *y) : ConfigItemEditor(), x(x), y(y) {
        comboX = new QComboBox;
        comboY = new QComboBox;
        fillCombo(comboX, xs, *x);
        fillCombo(comboY, ys, *y);
        Ori::Layouts::LayoutH({
            QString("X:"), comboX,
            Ori::Layouts::SpaceH(4),
            QString("Y:"), comboY, Ori::Layouts::Stretch(),
        }).useFor(this);
    }

    void collect() override {
        *x = qMax(1u, comboX->currentData().toUInt());
        *y = qMax(1u, comboY->currentData().toUInt());
    }

    void fillCombo(QComboBox *combo, const QList<quint32> &items, quint32 cur) {
        int idx = -1;
        for (int i = 0; i < items.size(); i++) {
            quint32 v = items[i];
            combo->addItem(QString::number(v), v);
            if (v == cur)
                idx = i;
        }
        if (idx >= 0)
            combo->setCurrentIndex(idx);
    }

    quint32 *x, *y;
    QComboBox *comboX, *comboY;
};

void IdsComfortCamera::initConfigMore(Ori::Dlg::ConfigDlgOpts &opts)
{
    int pageHard = cfgMax+1;
    int pageInfo = cfgMax+2;
    _cfg.bpp8 = _bpp == 8;
    _cfg.bpp10 = _bpp == 10;
    _cfg.bpp12 = _bpp == 12;
    opts.pages << ConfigPage(pageHard, tr("Hardware"), ":/toolbar/hardware");
    opts.items
        << (new ConfigItemSection(pageHard, tr("Pixel format")))
            ->withHint(tr("Reselect camera to apply"))
        << (new ConfigItemBool(pageHard, tr("8 bit"), &_cfg.bpp8))
            ->setDisabled(!_peak->supportedBpp.contains(8))->withRadioGroup("pixel_format")
        << (new ConfigItemBool(pageHard, tr("10 bit"), &_cfg.bpp10))
            ->setDisabled(!_peak->supportedBpp.contains(10))->withRadioGroup("pixel_format")
        << (new ConfigItemBool(pageHard, tr("12 bit"), &_cfg.bpp12))
            ->setDisabled(!_peak->supportedBpp.contains(12))->withRadioGroup("pixel_format")
    ;

    opts.items
        << new ConfigItemSpace(pageHard, 12)
        << (new ConfigItemSection(pageHard, tr("Resolution reduction")))
            ->withHint(tr("Reselect camera to apply"));
    if (_cfg.binningX > 0)
        opts.items << new ConfigItemCustom(pageHard, tr("Binning"), new ConfigEditorXY(
            _cfg.binningsX, _cfg.binningsY, &_cfg.binningX, &_cfg.binningY));
    else
        opts.items << (new ConfigItemEmpty(pageHard, tr("Binning")))
            ->withHint(tr("Is not configurable"));
    if (_cfg.decimX > 0)
        opts.items << new ConfigItemCustom(pageHard, tr("Decimation"), new ConfigEditorXY(
            _cfg.decimsX, _cfg.decimsY, &_cfg.decimX, &_cfg.decimY));
    else
        opts.items << (new ConfigItemEmpty(pageHard, tr("Decimation")))
            ->withHint(tr("Is not configurable"));

    if (_peak) {

        if (!_cfg.intoRequested) {
            _cfg.intoRequested = true;
            _cfg.infoModelName = _peak->gfaGetStr("DeviceModelName");
            _cfg.infoFamilyName = _peak->gfaGetStr("DeviceFamilyName");
            _cfg.infoSerialNum = _peak->gfaGetStr("DeviceSerialNumber");
            _cfg.infoVendorName = _peak->gfaGetStr("DeviceVendorName");
            _cfg.infoManufacturer = _peak->gfaGetStr("DeviceManufacturerInfo");
            _cfg.infoDeviceVer = _peak->gfaGetStr("DeviceVersion");
            _cfg.infoFirmwareVer = _peak->gfaGetStr("DeviceFirmwareVersion");
        }
        opts.pages << ConfigPage(pageInfo, tr("Info"), ":/toolbar/info");
        opts.items
            << (new ConfigItemStr(pageInfo, tr("Model name"), &_cfg.infoModelName))->withReadOnly()
            << (new ConfigItemStr(pageInfo, tr("Family name"), &_cfg.infoFamilyName))->withReadOnly()
            << (new ConfigItemStr(pageInfo, tr("Serial number"), &_cfg.infoSerialNum))->withReadOnly()
            << (new ConfigItemStr(pageInfo, tr("Vendor name"), &_cfg.infoVendorName))->withReadOnly()
            << (new ConfigItemStr(pageInfo, tr("Manufacturer info"), &_cfg.infoManufacturer))->withReadOnly()
            << (new ConfigItemStr(pageInfo, tr("Device version"), &_cfg.infoDeviceVer))->withReadOnly()
            << (new ConfigItemStr(pageInfo, tr("Firmware version"), &_cfg.infoFirmwareVer))->withReadOnly()
        ;
    }
}

void IdsComfortCamera::saveConfigMore()
{
    Ori::Settings s;
    s.beginGroup(_configGroup);
    s.setValue("hard.bpp", _cfg.bpp12 ? 12 : _cfg.bpp10 ? 10 : 8);
    s.setValue("hard.binning.x", _cfg.binningX);
    s.setValue("hard.binning.y", _cfg.binningY);
    s.setValue("hard.decim.x", _cfg.decimX);
    s.setValue("hard.decim.y", _cfg.decimY);
}

void IdsComfortCamera::loadConfigMore()
{
    Ori::Settings s;
    s.beginGroup(_configGroup);
    _bpp = s.value("hard.bpp", 8).toInt();
    _cfg.binningX = s.value("hard.binning.x").toUInt();
    _cfg.binningY = s.value("hard.binning.y").toUInt();
    _cfg.decimX = s.value("hard.decim.x").toUInt();
    _cfg.decimY = s.value("hard.decim.y").toUInt();
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
