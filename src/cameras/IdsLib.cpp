#include "IdsLib.h"

#ifdef WITH_IDS

#define LOG_ID "IdsLib:"

#include "app/AppSettings.h"

#include <windows.h>

#include <QDebug>
#include <QFile>

static HMODULE __hLib = 0;

static QString getSysError(DWORD err)
{
    const int bufSize = 1024;
    wchar_t buf[bufSize];
    auto size = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, 0, err, 0, buf, bufSize, 0);
    if (size == 0)
        return QStringLiteral("code: 0x").arg(err, 0, 16);
    return QString::fromWCharArray(buf, size).trimmed();
}

template <typename T> bool getProc(const char *name, T *proc) {
    *proc = (T)GetProcAddress(__hLib, name);
    if (*proc == NULL) {
        auto err = GetLastError();
        qWarning() << LOG_ID << "Failed to get proc" << name << getSysError(err);
        return false;
    }
    return true;
}

IdsLib& IdsLib::instance()
{
    static IdsLib lib;
    return lib;
}

IdsLib::IdsLib()
{
    if (!AppSettings::instance().idsEnabled)
        return;

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
            __hLib = 0;
            return;
        }
        __hLib = LoadLibraryW(libPath.toStdWString().c_str());
        if (__hLib == 0) {
            auto err = GetLastError();
            qWarning() << LOG_ID << "Failed to load" << libPath << getSysError(err);
            return;
        }
    }

    #define GET_PROC(name) if (!getProc(#name, &name)) { __hLib = 0; return; }
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

    auto res = IdsLib::peak_Library_Init();
    if (PEAK_ERROR(res)) {
        qCritical() << LOG_ID << "Unable to init library" << getPeakError(res);
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
