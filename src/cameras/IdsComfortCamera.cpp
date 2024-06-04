#include "IdsComfortCamera.h"

#ifdef WITH_IDS

#define LOG_ID "IdsComfortCamera:"

#include "app/AppSettings.h"
#include "cameras/CameraWorker.h"

#include "dialogs/OriConfigDlg.h"
#include "helpers/OriDialogs.h"
#include "tools/OriSettings.h"

#include <ids_peak_comfort_c/ids_peak_comfort_c.h>

#include <windows.h>

#include <QFile>

#define FRAME_TIMEOUT 5000
//#define LOG_FRAME_TIME

//------------------------------------------------------------------------------
//                              IdsComfort
//------------------------------------------------------------------------------

QString getSysError(DWORD err)
{
    const int bufSize = 1024;
    wchar_t buf[bufSize];
    auto size = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, 0, err, 0, buf, bufSize, 0);
    if (size == 0)
        return QStringLiteral("code: 0x").arg(err, 0, 16);
    return QString::fromWCharArray(buf, size).trimmed();
}

namespace IdsLib
{
    HMODULE hLib = 0;

    #define IDS_PROC(name) peak_status (__cdecl *name)
    IDS_PROC(peak_Library_Init)();
    IDS_PROC(peak_Library_Exit)();
    IDS_PROC(peak_Library_GetLastError)(peak_status*, char*, size_t*);
    IDS_PROC(peak_CameraList_Update)(size_t*);
    IDS_PROC(peak_CameraList_Get)(peak_camera_descriptor*, size_t*);
    peak_access_status (__cdecl *peak_ExposureTime_GetAccessStatus)(peak_camera_handle);
    IDS_PROC(peak_ExposureTime_GetRange)(peak_camera_handle, double*, double*, double*);
    IDS_PROC(peak_ExposureTime_Set)(peak_camera_handle, double);
    IDS_PROC(peak_ExposureTime_Get)(peak_camera_handle, double*);
    peak_access_status (__cdecl *peak_FrameRate_GetAccessStatus)(peak_camera_handle);
    IDS_PROC(peak_FrameRate_GetRange)(peak_camera_handle, double*, double*, double*);
    IDS_PROC(peak_FrameRate_Set)(peak_camera_handle, double);
    IDS_PROC(peak_FrameRate_Get)(peak_camera_handle, double*);
    peak_bool (__cdecl *peak_Acquisition_IsStarted)(peak_camera_handle);
    IDS_PROC(peak_Acquisition_Start)(peak_camera_handle, uint32_t);
    IDS_PROC(peak_Acquisition_Stop)(peak_camera_handle);
    IDS_PROC(peak_Acquisition_WaitForFrame)(peak_camera_handle, uint32_t, peak_frame_handle*);
    IDS_PROC(peak_Acquisition_GetInfo)(peak_camera_handle, peak_acquisition_info*);
    IDS_PROC(peak_Camera_GetDescriptor)(peak_camera_id, peak_camera_descriptor*);
    IDS_PROC(peak_Camera_Open)(peak_camera_id, peak_camera_handle*);
    IDS_PROC(peak_Camera_Close)(peak_camera_handle);
    IDS_PROC(peak_PixelFormat_GetList)(peak_camera_handle, peak_pixel_format*, size_t*);
    IDS_PROC(peak_PixelFormat_GetInfo)(peak_pixel_format, peak_pixel_format_info*);
    IDS_PROC(peak_PixelFormat_Get)(peak_camera_handle, peak_pixel_format*);
    IDS_PROC(peak_PixelFormat_Set)(peak_camera_handle, peak_pixel_format);
    IDS_PROC(peak_ROI_Get)(peak_camera_handle, peak_roi*);
    IDS_PROC(peak_ROI_Set)(peak_camera_handle, peak_roi);
    IDS_PROC(peak_ROI_Size_GetRange)(peak_camera_handle, peak_size*, peak_size*, peak_size*);
    IDS_PROC(peak_Frame_Buffer_Get)(peak_frame_handle, peak_buffer*);
    IDS_PROC(peak_Frame_Release)(peak_camera_handle, peak_frame_handle);
    IDS_PROC(peak_GFA_Float_Get)(peak_camera_handle, peak_gfa_module, const char*, double*);
    IDS_PROC(peak_GFA_String_Get)(peak_camera_handle, peak_gfa_module, const char*, char*, size_t*);
    peak_access_status (__cdecl *peak_GFA_Feature_GetAccessStatus)(peak_camera_handle, peak_gfa_module, const char*);

    template <typename T> bool getProc(const char *name, T *proc) {
        *proc = (T)GetProcAddress(hLib, name);
        if (*proc == NULL) {
            auto err = GetLastError();
            qWarning() << LOG_ID << "Failed to get proc" << name << getSysError(err);
            return false;
        }
        return true;
    }
}

QString getPeakError(peak_status status)
{
    size_t bufSize = 0;
    auto res = IdsLib::peak_Library_GetLastError(&status, nullptr, &bufSize);
    if (PEAK_SUCCESS(res)) {
        QByteArray buf(bufSize, 0);
        res = IdsLib::peak_Library_GetLastError(&status, buf.data(), &bufSize);
        if (PEAK_SUCCESS(res))
            return QString::fromLatin1(buf.data());
    }
    qWarning() << LOG_ID << "Unable to get text for error" << status << "because of error" << res;
    return QString("errcode=%1").arg(status);
}

IdsComfort* IdsComfort::init()
{
    if (!AppSettings::instance().idsEnabled)
        return nullptr;

    QStringList libs = {
        "GCBase_MD_VC140_v3_2_IDS.dll",
        "Log_MD_VC140_v3_2_IDS.dll",
        "MathParser_MD_VC140_v3_2_IDS.dll",
        "NodeMapData_MD_VC140_v3_2_IDS.dll",
        "XmlParser_MD_VC140_v3_2_IDS.dll",
        "GenApi_MD_VC140_v3_2_IDS.dll",
        "FirmwareUpdate_MD_VC140_v3_2_IDS.dll",
        "ids_peak_comfort_c.dll",
    };
    QString sdkPath = AppSettings::instance().idsSdkDir;
    for (const auto& lib : libs) {
        QString libPath = sdkPath + '/' + lib;
        if (!QFile::exists(libPath)) {
            qWarning() << LOG_ID << "Library not found" << libPath;
            return nullptr;
        }
        IdsLib::hLib = LoadLibraryW(libPath.toStdWString().c_str());
        if (IdsLib::hLib == NULL) {
            auto err = GetLastError();
            qWarning() << LOG_ID << "Failed to load" << libPath << getSysError(err);
            return nullptr;
        }
    }

    #define GET_PROC(name) if (!IdsLib::getProc(#name, &IdsLib::name)) return nullptr
    GET_PROC(peak_Library_Init);
    GET_PROC(peak_Library_Exit);
    GET_PROC(peak_Library_GetLastError);
    GET_PROC(peak_CameraList_Update);
    GET_PROC(peak_CameraList_Get);
    GET_PROC(peak_ExposureTime_GetAccessStatus);
    GET_PROC(peak_ExposureTime_GetRange);
    GET_PROC(peak_ExposureTime_Set);
    GET_PROC(peak_ExposureTime_Get);
    GET_PROC(peak_FrameRate_GetAccessStatus);
    GET_PROC(peak_FrameRate_GetRange);
    GET_PROC(peak_FrameRate_Set);
    GET_PROC(peak_FrameRate_Get);
    GET_PROC(peak_Acquisition_IsStarted);
    GET_PROC(peak_Acquisition_Start);
    GET_PROC(peak_Acquisition_Stop);
    GET_PROC(peak_Acquisition_WaitForFrame);
    GET_PROC(peak_Acquisition_GetInfo);
    GET_PROC(peak_Camera_GetDescriptor);
    GET_PROC(peak_Camera_Open);
    GET_PROC(peak_Camera_Close);
    GET_PROC(peak_PixelFormat_GetList);
    GET_PROC(peak_PixelFormat_GetInfo);
    GET_PROC(peak_PixelFormat_Get);
    GET_PROC(peak_PixelFormat_Set);
    GET_PROC(peak_ROI_Get);
    GET_PROC(peak_ROI_Set);
    GET_PROC(peak_ROI_Size_GetRange);
    GET_PROC(peak_Frame_Buffer_Get);
    GET_PROC(peak_Frame_Release);
    GET_PROC(peak_GFA_Float_Get);
    GET_PROC(peak_GFA_String_Get);
    GET_PROC(peak_GFA_Feature_GetAccessStatus);

    auto res = IdsLib::peak_Library_Init();
    if (PEAK_ERROR(res)) {
        qCritical() << LOG_ID << "Unable to init library" << getPeakError(res);
        return nullptr;
    }
    qDebug() << LOG_ID << "Library inited";
    return new IdsComfort();
}

IdsComfort::IdsComfort()
{
}

IdsComfort::~IdsComfort()
{
    auto res = IdsLib::peak_Library_Exit();
    if (PEAK_ERROR(res))
        qWarning() << LOG_ID << "Unable to deinit library" << getPeakError(res);
    else qDebug() << LOG_ID << "Library deinited";
}

QVector<CameraItem> IdsComfort::getCameras()
{
    size_t camCount;
    auto res = IdsLib::peak_CameraList_Update(&camCount);
    if (PEAK_ERROR(res)) {
        qWarning() << LOG_ID << "Unable to update camera list" << getPeakError(res);
        return {};
    }
    else qDebug() << LOG_ID << "Camera list updated. Cameras found:" << camCount;

    QVector<peak_camera_descriptor> cams(camCount);
    res = IdsLib::peak_CameraList_Get(cams.data(), &camCount);
    if (PEAK_ERROR(res)) {
        qWarning() << LOG_ID << "Unable to get camera list" << getPeakError(res);
        return {};
    }

    QVector<CameraItem> result;
    for (const auto &cam : cams) {
        qDebug() << LOG_ID << cam.cameraID << cam.cameraType << cam.modelName << cam.serialNumber;
        result << CameraItem {
            .id = cam.cameraID,
            .name = QString("%1 (*%2)").arg(
                QString::fromLatin1(cam.modelName),
                QString::fromLatin1(cam.serialNumber).right(4)
            )
        };
    }
    return result;
}

//------------------------------------------------------------------------------
//                              PeakIntf
//------------------------------------------------------------------------------

// Implemented in IdsComfort.cpp
QString getPeakError(peak_status status);

#define CHECK_ERR(msg) \
    if (PEAK_ERROR(res)) { \
        auto err = getPeakError(res); \
        qCritical() << LOG_ID << msg << err; \
        return QString(msg ": ") + err; \
    }

#define GFA_GET_FLOAT(prop, value) \
    if (PEAK_IS_READABLE(IdsLib::peak_GFA_Feature_GetAccessStatus(hCam, PEAK_GFA_MODULE_REMOTE_DEVICE, prop))) { \
        res = IdsLib::peak_GFA_Float_Get(hCam, PEAK_GFA_MODULE_REMOTE_DEVICE, prop, &value); \
        if (PEAK_ERROR(res)) { \
            qWarning() << LOG_ID << "Failed to read" << prop << getPeakError(res); \
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
        qDebug() << LOG_ID << "Unable to get" << prop << getPeakError(res); \
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
        : CameraWorker(plot, table, cam, cam), id(id), cam(cam)
    {}

    QString init()
    {
        peak_camera_descriptor descr;
        res = IdsLib::peak_Camera_GetDescriptor(id, &descr);
        CHECK_ERR("Unable to get camera descriptor");
        cam->_name = QString::fromLatin1(descr.modelName);
        cam->_descr = cam->_name + ' ' + QString::fromLatin1(descr.serialNumber);
        cam->_configGroup = cam->_name + '-' + QString::fromLatin1(descr.serialNumber);
        cam->loadConfig();
        cam->loadConfigMore();

        res = IdsLib::peak_Camera_Open(id, &hCam);
        CHECK_ERR("Unable to open camera");
        qDebug() << LOG_ID << "Camera opened" << id;

        peak_size roiMin, roiMax, roiInc;
        res = IdsLib::peak_ROI_Size_GetRange(hCam, &roiMin, &roiMax, &roiInc);
        CHECK_ERR("Unable to get ROI range");
        qDebug() << LOG_ID << "ROI"
            << QString("min=%1x%24").arg(roiMin.width).arg(roiMin.height)
            << QString("max=%1x%24").arg(roiMax.width).arg(roiMax.height)
            << QString("inc=%1x%24").arg(roiInc.width).arg(roiInc.height);

        peak_roi roi = {0, 0, 0, 0};
        res = IdsLib::peak_ROI_Get(hCam, &roi);
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
            res = IdsLib::peak_ROI_Set(hCam, roi);
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

        c.bpp = cam->_bpp;
        peak_pixel_format targetFormat = PEAK_PIXEL_FORMAT_MONO8;
        if (c.bpp == 12) targetFormat = PEAK_PIXEL_FORMAT_MONO12G24_IDS;
        else if (c.bpp == 10) targetFormat = PEAK_PIXEL_FORMAT_MONO10G40_IDS;

        peak_pixel_format pixelFormat = PEAK_PIXEL_FORMAT_INVALID;
        res = IdsLib::peak_PixelFormat_Get(hCam, &pixelFormat);
        CHECK_ERR("Unable to get pixel format");
        qDebug() << LOG_ID << "Pixel format" << QString::number(pixelFormat, 16);

        if (targetFormat != pixelFormat) {
            size_t formatCount = 0;
            res = IdsLib::peak_PixelFormat_GetList(hCam, nullptr, &formatCount);
            CHECK_ERR("Unable to get pixel format count");
            QVector<peak_pixel_format> pixelFormats(formatCount);
            res = IdsLib::peak_PixelFormat_GetList(hCam, pixelFormats.data(), &formatCount);
            CHECK_ERR("Unable to get pixel formats");
            bool supports8 = false;
            bool supports10 = false;
            bool supports12 = false;
            for (int i = 0; i < formatCount; i++) {
                qDebug() << LOG_ID << "Supported pixel format" << QString::number(pixelFormats[i], 16);
                peak_pixel_format_info info;
                if (pixelFormats[i] == PEAK_PIXEL_FORMAT_MONO8)
                    supports8 = true;
                else if (pixelFormats[i] == PEAK_PIXEL_FORMAT_MONO10G40_IDS)
                    supports10 = true;
                else if (pixelFormats[i] == PEAK_PIXEL_FORMAT_MONO12G24_IDS)
                    supports12 = true;
            }
            if (c.bpp == 12 && !supports12) {
                qWarning() << LOG_ID << "Camera does not support 12bpp, use 8bpp";
                targetFormat = PEAK_PIXEL_FORMAT_MONO8;
                c.bpp = 8;
            }
            if (c.bpp == 10 && !supports10) {
                qWarning() << LOG_ID << "Camera does not support 10bpp, use 8bpp";
                targetFormat = PEAK_PIXEL_FORMAT_MONO8;
                c.bpp = 8;
            }
            if (c.bpp == 8 && !supports8) {
                return "Camera doesn't support gray scale format";
            }
            res = IdsLib::peak_PixelFormat_Set(hCam, targetFormat);
            CHECK_ERR("Unable to set pixel format");
            qDebug() << LOG_ID << "Pixel format set";
        }
        cam->_bpp = c.bpp;
        if (c.bpp > 8) {
            hdrBuf = QByteArray(c.w*c.h*2, 0);
            c.buf = (uint8_t*)hdrBuf.data();
        }

        SHOW_CAM_PROP("FPS", IdsLib::peak_FrameRate_Get, double);
        SHOW_CAM_PROP("Exposure", IdsLib::peak_ExposureTime_Get, double);

        res = IdsLib::peak_Acquisition_Start(hCam, PEAK_INFINITE);
        CHECK_ERR("Unable to start acquisition");
        qDebug() << LOG_ID << "Acquisition started";

        plot->initGraph(c.w, c.h);
        graph = plot->rawGraph();

        configure();

        return {};
    }

    ~PeakIntf()
    {
        if (hCam == PEAK_INVALID_HANDLE)
            return;

        if (IdsLib::peak_Acquisition_IsStarted(hCam))
        {
            auto res = IdsLib::peak_Acquisition_Stop(hCam);
            if (PEAK_ERROR(res))
                qWarning() << LOG_ID << "Unable to stop acquisition";
            else qDebug() << LOG_ID << "Acquisition stopped";
        }

        auto res = IdsLib::peak_Camera_Close(hCam);
        if (PEAK_ERROR(res))
            qWarning() << LOG_ID << "Unable to close camera" << getPeakError(res);
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
            res = IdsLib::peak_Acquisition_WaitForFrame(hCam, FRAME_TIMEOUT, &frame);
            if (PEAK_SUCCESS(res))
                res = IdsLib::peak_Frame_Buffer_Get(frame, &buf);
            if (res == PEAK_STATUS_ABORTED) {
                auto err = getPeakError(res);
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

                res = IdsLib::peak_Frame_Release(hCam, frame);
                if (PEAK_ERROR(res)) {
                    auto err = getPeakError(res);
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
                res = IdsLib::peak_Acquisition_GetInfo(hCam, &info);
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
                    << getPeakError(res);
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
        if (!PEAK_IS_READABLE(IdsLib::peak_GFA_Feature_GetAccessStatus(hCam, mod, prop)))
            return "Is not readable";
        size_t size;
        auto res = IdsLib::peak_GFA_String_Get(hCam, mod, prop, nullptr, &size);
        if (PEAK_ERROR(res))
            return getPeakError(res);
        QByteArray buf(size, 0);
        res = IdsLib::peak_GFA_String_Get(hCam, mod, prop, buf.data(), &size);
        if (PEAK_ERROR(res))
            return getPeakError(res);
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
    if (_cfgWnd)
        _cfgWnd->deleteLater();

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
    if (_cfgWnd)
        _cfgWnd->deleteLater();
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
    auto res = IdsLib::peak_ExposureTime_Get(_peak->hCam, &v);
    if (PEAK_ERROR(res))
        s->setValue("exposure", getPeakError(res));
    else s->setValue("exposure", v);
    res = IdsLib::peak_FrameRate_Get(_peak->hCam, &v);
    if (PEAK_ERROR(res))
        s->setValue("frameRate", getPeakError(res));
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

using namespace Ori::Dlg;

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
        << (new ConfigItemBool(pageHard, tr("8 bit"), &_cfg.bpp8))->withRadioGroup("pixel_format")
        << (new ConfigItemBool(pageHard, tr("10 bit"), &_cfg.bpp10))->withRadioGroup("pixel_format")
        << (new ConfigItemBool(pageHard, tr("12 bit"), &_cfg.bpp12))->withRadioGroup("pixel_format")
    ;
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
}

void IdsComfortCamera::loadConfigMore()
{
    Ori::Settings s;
    s.beginGroup(_configGroup);
    _bpp = s.value("hard.bpp", 8).toInt();
}

//------------------------------------------------------------------------------
//                             IdsHardConfigWindow
//------------------------------------------------------------------------------

#include <QApplication>
#include <QBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QStyleHints>
#include <QWheelEvent>

#include "helpers/OriLayouts.h"
#include "tools/OriSettings.h"
#include "widgets/OriValueEdit.h"

using namespace Ori::Layouts;
using namespace Ori::Widgets;

#define PROP_CONTROL(title, group, edit, label, setter) { \
    label = new QLabel; \
    label->setWordWrap(true); \
    label->setForegroundRole(qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QPalette::Light : QPalette::Mid); \
    edit = new CamPropEdit; \
    edit->scrolled = [this](bool wheel, bool inc, bool big){ setter ## Fast(wheel, inc, big); }; \
    edit->connect(edit, &ValueEdit::keyPressed, edit, [this](int key){ \
        if (key == Qt::Key_Return || key == Qt::Key_Enter) setter(); }); \
    auto btn = new QPushButton(tr("Set")); \
    btn->setFixedWidth(50); \
    btn->connect(btn, &QPushButton::pressed, btn, [this]{ setter(); }); \
    group = LayoutV({label, LayoutH({edit, btn})}).makeGroupBox(title); \
    layout->addWidget(group); \
}

#define PROP_SHOW(method, id, edit, label, getValue, getRange, showAux) \
    void method() { \
        double value, min, max, step; \
        auto res = IdsLib::getValue(hCam, &value); \
        if (PEAK_ERROR(res)) { \
            label->setText(getPeakError(res)); \
            edit->setValue(0); \
            edit->setDisabled(true); \
            props[id] = 0; \
            return; \
        } \
        edit->setValue(value); \
        edit->setDisabled(false); \
        props[id] = value; \
        res = IdsLib::getRange(hCam, &min, &max, &step); \
        if (PEAK_ERROR(res)) \
            label->setText(getPeakError(res)); \
        else { \
            label->setText(QString("<b>Min = %1, Max = %2</b>").arg(min, 0, 'f', 2).arg(max, 0, 'f', 2)); \
            props[id "_min"] = min; \
            props[id "_max"] = max; \
            props[id "_step"] = step; \
        }\
        showAux(value); \
    }

#define PROP_SET(method, id, edit, setValue) \
    void method() { \
        method ## Raw(edit->value()); \
    } \
    bool method ## Raw(double v) { \
        auto res = IdsLib::setValue(hCam, v); \
        if (PEAK_ERROR(res)) { \
            Ori::Dlg::error(getPeakError(res)); \
            return false; \
        } \
        showExp(); \
        showFps(); \
        return true; \
    } \
    void method ## Fast(bool wheel, bool inc, bool big) { \
        double change = wheel \
            ? (big ? propChangeWheelBig : propChangeWheelSm) \
            : (big ? propChangeArrowBig : propChangeArrowSm); \
        double val = props[id]; \
        double newVal; \
        if (inc) { \
            double max = props[id "_max"]; \
            if (val >= max) return; \
            newVal = val * change; \
            newVal = qMin(max, val * change); \
        } else { \
            double min = props[id "_min"]; \
            if (val <= min) return; \
            newVal = val / change; \
            newVal = qMax(min, val / change); \
        } \
        double step = props[id "_step"]; \
        if (qAbs(val - newVal) < step) \
            newVal = val + (inc ? step : -step); \
        auto res = IdsLib::setValue(hCam, newVal); \
        if (PEAK_ERROR(res)) \
            Ori::Dlg::error(getPeakError(res)); \
        showExp(); \
        showFps(); \
    }

class CamPropEdit : public ValueEdit
{
public:
    std::function<void(bool, bool, bool)> scrolled;
protected:
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Up) {
            scrolled(false, true, e->modifiers().testFlag(Qt::ControlModifier));
            e->accept();
        } else if (e->key() == Qt::Key_Down) {
            scrolled(false, false, e->modifiers().testFlag(Qt::ControlModifier));
            e->accept();
        } else {
            ValueEdit::keyPressEvent(e);
        }
    }
    void wheelEvent(QWheelEvent *e) override {
        scrolled(true, e->angleDelta().y() > 0, e->modifiers().testFlag(Qt::ControlModifier));
        e->accept();
    }
};

class IdsHardConfigWindow : public QWidget, public IAppSettingsListener
{
    Q_DECLARE_TR_FUNCTIONS(IdsHardConfigWindow)

public:
    IdsHardConfigWindow(PeakIntf *peak) : QWidget(qApp->activeWindow()), peak(peak)
    {
        setWindowFlag(Qt::Tool, true);
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
        setAttribute(Qt::WA_DeleteOnClose, true);
        setWindowTitle(tr("Device Control"));

        hCam = peak->hCam;

        auto layout = new QVBoxLayout(this);

        PROP_CONTROL(tr("Exposure (us)"), groupExp, edExp, labExp, setExp)

        {
            auto label = new QLabel(tr("Percent of dynamic range:"));
            edAutoExp = new CamPropEdit;
            edAutoExp->connect(edAutoExp, &ValueEdit::keyPressed, edAutoExp, [this](int key){
                if (key == Qt::Key_Return || key == Qt::Key_Enter) autoExposure(); });
            butAutoExp = new QPushButton(tr("Find"));
            butAutoExp->setFixedWidth(50);
            butAutoExp->connect(butAutoExp, &QPushButton::pressed, butAutoExp, [this]{ autoExposure(); });
            layout->addWidget(LayoutV({label, LayoutH({edAutoExp, butAutoExp})}).makeGroupBox(tr("Autoexposure")));
        }

        PROP_CONTROL(tr("Frame rate"), groupFps, edFps, labFps, setFps);

        labExpFreq = new QLabel;
        groupExp->layout()->addWidget(labExpFreq);

        if (IdsLib::peak_ExposureTime_GetAccessStatus(hCam) != PEAK_ACCESS_READWRITE) {
            labExp->setText(tr("Exposure is not configurable"));
            edExp->setDisabled(true);
        } else showExp();

        if (IdsLib::peak_FrameRate_GetAccessStatus(hCam) != PEAK_ACCESS_READWRITE) {
            labFps->setText(tr("FPS is not configurable"));
            edFps->setDisabled(true);
        } else showFps();

        applySettings();
    }

    PROP_SET(setExp, "exp", edExp, peak_ExposureTime_Set)
    PROP_SET(setFps, "fps", edFps, peak_FrameRate_Set)

    PROP_SHOW(showExp, "exp", edExp, labExp, peak_ExposureTime_Get, peak_ExposureTime_GetRange, showExpFreq)
    PROP_SHOW(showFps, "fps", edFps, labFps, peak_FrameRate_Get, peak_FrameRate_GetRange, showAux)

    void showAux(double v) {}

    void showExpFreq(double exp)
    {
        double freq = 1e6 / exp;
        QString s;
        if (freq < 1000)
            s = QStringLiteral("Corresponds to <b>%1 Hz</b>").arg(freq, 0, 'f', 1);
        else
            s = QStringLiteral("Corresponds to <b>%1 kHz</b>").arg(freq/1000.0, 0, 'f', 2);
        labExpFreq->setText(s);
    }

    void autoExposure()
    {
        if (props["exp"] == 0) return;

        butAutoExp->setDisabled(true);
        edAutoExp->setDisabled(true);

        auto level = edAutoExp->value();
        if (level <= 0) level = 1;
        else if (level > 100) level = 100;
        edAutoExp->setValue(level);
        autoExpLevel = level / 100.0;
        qDebug() << LOG_ID << "Autoexposure" << autoExpLevel;

        Ori::Settings s;
        s.beginGroup("DeviceControl");
        s.setValue("autoExposurePercent", level);

        if (!setExpRaw(props["exp_min"]))
            return;
        autoExp1 = props["exp"];
        autoExp2 = 0;
        autoExpStep = 0;
        watingBrightness = true;
        peak->requestBrightness(this);
    }

    void autoExposureStep(double level)
    {
        qDebug() << LOG_ID << "Autoexposure step" << autoExpStep << "| exp" << props["exp"] << "| level" << level;

        if (qAbs(level - autoExpLevel) < 0.01) {
            qDebug() << LOG_ID << "Autoexposure: stop(0)" << props["exp"];
            goto stop;
        }

        if (level < autoExpLevel) {
            if (autoExp2 == 0) {
                if (!setExpRaw(qMin(autoExp1*2, props["exp_max"])))
                    goto stop;
                // The above does not fail when setting higher-thah-max exposure
                // It just clamps it to max and the loop never ends.
                // So need an explicit check:
                if (props["exp"] >= props["exp_max"]) {
                    qDebug() << LOG_ID << "Autoexposure: underexposed" << props["exp"];
                    Ori::Dlg::warning(tr("Underexposed"));
                    goto stop;
                }
                autoExp1 = props["exp"];
            } else {
                autoExp1 = props["exp"];
                if (!setExpRaw((autoExp1+autoExp2)/2))
                    goto stop;
                if (qAbs(autoExp1 - props["exp"]) <= props["exp_step"]) {
                    qDebug() << LOG_ID << "Autoexposure: stop(1)" << props["exp"];
                    goto stop;
                }
            }
        } else {
            if (autoExp2 == 0) {
                if (props["exp"] == props["exp_min"]) {
                    qDebug() << LOG_ID << "Autoexposure: Overexposed";
                    Ori::Dlg::warning(tr("Overexposed"));
                    goto stop;
                }
                autoExp2 = autoExp1;
                autoExp1 = autoExp2/2;
                if (!setExpRaw((autoExp1+autoExp2)/2))
                    goto stop;
            } else {
                autoExp2 = props["exp"];
                if (!setExpRaw((autoExp1+autoExp2)/2))
                    goto stop;
                if (qAbs(autoExp2 - props["exp"]) <= props["exp_step"]) {
                    qDebug() << LOG_ID << "Autoexposure: stop(2)" << props["exp"];
                    goto stop;
                }
            }
        }
        autoExpStep++;
        watingBrightness = true;
        peak->requestBrightness(this);
        return;

    stop:
        butAutoExp->setDisabled(false);
        edAutoExp->setDisabled(false);
    }

    void applySettings()
    {
        auto &s = AppSettings::instance();
        propChangeWheelSm = 1 + double(s.propChangeWheelSm) / 100.0;
        propChangeWheelBig = 1 + double(s.propChangeWheelBig) / 100.0;
        propChangeArrowSm = 1 + double(s.propChangeArrowSm) / 100.0;
        propChangeArrowBig = 1 + double(s.propChangeArrowBig) / 100.0;

        Ori::Settings s1;
        s1.beginGroup("DeviceControl");
        edAutoExp->setValue(s1.value("autoExposurePercent", 80).toInt());
    }

    void settingsChanged() override
    {
        applySettings();
    }

    PeakIntf *peak;
    peak_camera_handle hCam;
    CamPropEdit *edExp, *edFps, *edAutoExp;
    QGroupBox *groupExp, *groupFps;
    QLabel *labExp, *labFps, *labExpFreq;
    QPushButton *butAutoExp;
    QMap<const char*, double> props;
    double propChangeWheelSm, propChangeWheelBig;
    double propChangeArrowSm, propChangeArrowBig;
    double autoExpLevel, autoExp1, autoExp2;
    bool watingBrightness = false;
    bool closeRequested = false;
    int autoExpStep;

protected:
    void closeEvent(QCloseEvent *e) override
    {
        QWidget::closeEvent(e);
        // Event loop will crash if there is no event receiver anymore
        // Wait for the next brightness event and close after that
        if (watingBrightness) {
            closeRequested = true;
            e->ignore();
        }
    }

    bool event(QEvent *event) override
    {
        if (auto e = dynamic_cast<BrightEvent*>(event); e) {
            watingBrightness = false;
            if (closeRequested)
                close();
            else
                autoExposureStep(e->level);
            return true;
        }
        return QWidget::event(event);
    }
};

QPointer<QWidget> IdsComfortCamera::showHardConfgWindow()
{
    if (!_cfgWnd)
        _cfgWnd = new IdsHardConfigWindow(_peak.get());
    _cfgWnd->show();
    _cfgWnd->activateWindow();
    return _cfgWnd;
}

#endif // WITH_IDS
