#include "IdsLib.h"

#ifdef WITH_IDS

#define LOG_ID "IdsLib:"

#include "app/AppSettings.h"
#include "cameras/DllHelper.h"

#include <QDebug>
#include <QDir>
#include <QFile>

static HMODULE __hLib = 0;

template <typename T> QString getProc(const char *name, T *proc)
{
    auto res = resolveProc(LOG_ID, __hLib, name);
    if (!res.ok())
        return res.error();
    *proc = (T)res.result();
    return {};
}

IdsLib& IdsLib::instance()
{
    static IdsLib lib;
    return lib;
}

static QString getSdkVer(const QString &sdkDir)
{
    QString verFile = sdkDir + "/../../../../version.txt";
    QFile f(verFile);
    if (!f.exists())
        return "Version file not found: " + verFile;
    if (!f.open(QIODeviceBase::ReadOnly|QIODeviceBase::Text))
        return f.errorString();
    return f.readAll().trimmed();
}

IdsLib::IdsLib()
{
    if (!AppSettings::instance().idsEnabled)
        return;

    QString sdkDir = QDir::cleanPath(AppSettings::instance().idsSdkDir);
    qDebug() << LOG_ID << "SDK version:" << getSdkVer(sdkDir);
    
    auto sdkResult = loadSdk(LOG_ID, sdkDir, "ids_peak_comfort_c.dll");
    if (!sdkResult.ok())
    {
        libError = sdkResult.error();
        return;
    }
    
    __hLib = sdkResult.result();
    
    #define GET_PROC(name) \
        if (auto err = getProc(#name, &name); !err.isEmpty()) { \
            libError = err; \
            __hLib = 0; \
            return; \
        }
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
    // GET_PROC(peak_IPL_Gain_GetRange);
    // GET_PROC(peak_IPL_Gain_Set);
    // GET_PROC(peak_IPL_Gain_Get);
    GET_PROC(peak_Gain_GetAccessStatus);
    GET_PROC(peak_Gain_GetChannelList);
    GET_PROC(peak_Gain_GetRange);
    GET_PROC(peak_Gain_Set);
    GET_PROC(peak_Gain_Get);
    GET_PROC(peak_Mirror_LeftRight_GetAccessStatus);
    GET_PROC(peak_Mirror_LeftRight_Enable);
    GET_PROC(peak_Mirror_LeftRight_IsEnabled);
    GET_PROC(peak_Mirror_UpDown_GetAccessStatus);
    GET_PROC(peak_Mirror_UpDown_Enable);
    GET_PROC(peak_Mirror_UpDown_IsEnabled);
    GET_PROC(peak_IPL_Mirror_UpDown_Enable);
    GET_PROC(peak_IPL_Mirror_UpDown_IsEnabled);
    GET_PROC(peak_IPL_Mirror_LeftRight_Enable);
    GET_PROC(peak_IPL_Mirror_LeftRight_IsEnabled);
    GET_PROC(peak_Binning_GetAccessStatus);
    GET_PROC(peak_Binning_FactorX_GetList);
    GET_PROC(peak_Binning_FactorY_GetList);
    GET_PROC(peak_Binning_Set);
    GET_PROC(peak_Binning_Get);
    GET_PROC(peak_Decimation_GetAccessStatus);
    GET_PROC(peak_Decimation_FactorX_GetList);
    GET_PROC(peak_Decimation_FactorY_GetList);
    GET_PROC(peak_Decimation_Set);
    GET_PROC(peak_Decimation_Get);
    GET_PROC(peak_PixelClock_GetAccessStatus);
    GET_PROC(peak_PixelClock_HasRange);
    GET_PROC(peak_PixelClock_GetRange);
    GET_PROC(peak_PixelClock_GetList);
    GET_PROC(peak_PixelClock_Get);
    GET_PROC(peak_PixelClock_Set);

    auto res = IdsLib::peak_Library_Init();
    if (PEAK_ERROR(res)) {
        auto peakErr = getPeakError(res);
        qCritical() << LOG_ID << "Unable to init library" << peakErr;
        libError = "Unable to init library: " + peakErr;
        __hLib = 0;
        return;
    }
    qDebug() << LOG_ID << "Library inited";
}

QString IdsLib::getPeakError(peak_status status)
{
    size_t bufSize = 0;
    auto res = peak_Library_GetLastError(&status, nullptr, &bufSize);
    if (PEAK_SUCCESS(res)) {
        QByteArray buf(bufSize, 0);
        res = peak_Library_GetLastError(&status, buf.data(), &bufSize);
        if (PEAK_SUCCESS(res))
            return QString::fromLatin1(buf.data());
    }
    qWarning() << LOG_ID << "Unable to get text for error" << status << "because of error" << res;
    return QString("errcode=%1").arg(status);
}

bool IdsLib::loaded()
{
    return __hLib != 0;
}

void IdsLib::unload()
{
    auto res = peak_Library_Exit();
    if (PEAK_ERROR(res))
        qWarning() << LOG_ID << "Unable to deinit library" << getPeakError(res);
    else qDebug() << LOG_ID << "Library deinited";

    // TODO: unload libs
    __hLib = 0;
}

QString IdsLib::gfaGetStr(peak_camera_handle hCam, const char* prop)
{
    auto mod = PEAK_GFA_MODULE_REMOTE_DEVICE;
    if (!PEAK_IS_READABLE(peak_GFA_Feature_GetAccessStatus(hCam, mod, prop)))
        return "Is not readable";
    size_t size;
    auto res = peak_GFA_String_Get(hCam, mod, prop, nullptr, &size);
    if (PEAK_ERROR(res))
        return getPeakError(res);
    QByteArray buf(size, 0);
    res = peak_GFA_String_Get(hCam, mod, prop, buf.data(), &size);
    if (PEAK_ERROR(res))
        return getPeakError(res);
    return QString::fromLatin1(buf);
}

#endif // WITH_IDS
