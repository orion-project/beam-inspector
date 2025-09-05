#include "IdsLib.h"

#ifdef WITH_IDS

#define LOG_ID "IdsLib:"

#include "app/AppSettings.h"

#include "core/OriResult.h"

#include <windows.h>

#include <QDebug>
#include <QDir>
#include <QFile>

static HMODULE __hLib = 0;

static QString getSysError(DWORD err)
{
    const int bufSize = 1024;
    wchar_t buf[bufSize];
    auto size = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, 0, err, 0, buf, bufSize, 0);
    if (size == 0)
        return QStringLiteral("code: 0x%1").arg(err, 0, 16);
    return QString::fromWCharArray(buf, size).trimmed();
}

template <typename T> QString getProc(const char *name, T *proc) {
    *proc = (T)GetProcAddress(__hLib, name);
    if (*proc == NULL) {
        auto err = GetLastError();
        qWarning() << LOG_ID << "Failed to get proc" << name << getSysError(err);
        return QString("Function '%1' not loaded: %2").arg(name, err);
    }
    return {};
}

IdsLib& IdsLib::instance()
{
    static IdsLib lib;
    return lib;
}

static QString loadLibDeps(const QString &sdkDir, const QString &libName, QStringList &deps)
{
    QString libPath = sdkDir + '/' + libName;
    qDebug() << LOG_ID << "Check deps for" << libPath;
    if (!QFile::exists(libPath)) {
        qWarning() << LOG_ID << "Library not found" << libPath;
        return "Library not found: " + libPath;
    }
    auto hLib = LoadLibraryExA(libPath.toStdString().c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!hLib) {
        auto err = getSysError(GetLastError());
        qWarning() << LOG_ID << "Failed to load" << libPath << err;
        return QString("Unable to load library %1: %2").arg(libPath, err);
    }
    
    PIMAGE_DOS_HEADER peHeader = (PIMAGE_DOS_HEADER)hLib;
    if (peHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        printf("Error: Not a valid PE file\n");
        FreeLibrary(hLib);
        return QString("Unable to load library %1: invalid PE-header").arg(libPath);
    }
    
    PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)((BYTE*)hLib + peHeader->e_lfanew);
    if (ntHeader->Signature != IMAGE_NT_SIGNATURE) {
        printf("Error: Not a valid PE file\n");
        FreeLibrary(hLib);
        return QString("Unable to load library %1: invalid NT-header").arg(libPath);
    }
    
    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hLib + 
        ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    if (ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size == 0 || 
        ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == 0) {
        FreeLibrary(hLib);
        return QString("Unable to load library %1: imports is empty").arg(libPath);
    }
    while (importDesc->Name != 0) {
        QString dllName = QString((const char*)((BYTE*)hLib + importDesc->Name));
        QString dllPath = sdkDir + '/' + dllName;
        if (QFile::exists(dllPath) && !deps.contains(dllPath)) {
            QString res = loadLibDeps(sdkDir, dllName, deps);
            if (!res.isEmpty()) {
                FreeLibrary(hLib);
                return res;
            }
        }
        importDesc++;
    }
    
    deps << libPath;
    FreeLibrary(hLib);
    return {};
}

using VerResult = Ori::Result<QString>;

static VerResult getLibVer(const QString &libPath)
{
    auto stdName = libPath.toStdString();

    DWORD dwHandle;
    DWORD dwSize = GetFileVersionInfoSizeA(stdName.c_str(), &dwHandle);
    if (dwSize == 0)
        return VerResult::fail(getSysError(GetLastError()));
    
    LPVOID lpData = malloc(dwSize);
    if (!lpData)
        return VerResult::fail("memory allocation failed");
    
    if (!GetFileVersionInfoA(stdName.c_str(), 0, dwSize, lpData)) {
        auto err = getSysError(GetLastError());
        free(lpData);
        return VerResult::fail(err);
    }
    
    VS_FIXEDFILEINFO* pFileInfo;
    UINT uLen;
    if (!VerQueryValueA(lpData, "\\", (LPVOID*)&pFileInfo, &uLen)) {
        auto err = getSysError(GetLastError());
        free(lpData);
        return VerResult::fail(err);
    }
    if (uLen == 0) {
        free(lpData);
        return VerResult::fail("No file info found");
    }
    
    DWORD dwFileVersionMS = pFileInfo->dwFileVersionMS;
    DWORD dwFileVersionLS = pFileInfo->dwFileVersionLS;
    WORD wMajor = HIWORD(dwFileVersionMS);
    WORD wMinor = LOWORD(dwFileVersionMS);
    WORD wBuild = HIWORD(dwFileVersionLS);
    WORD wRevision = LOWORD(dwFileVersionLS);

    free(lpData);

    return VerResult::ok(QStringLiteral("%1.%2.%3.%4").arg(wMajor).arg(wMinor).arg(wBuild).arg(wRevision)); 
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

    QStringList libs;
    QString sdkDir = QDir::cleanPath(AppSettings::instance().idsSdkDir);
    qDebug() << LOG_ID << "SDK version:" << getSdkVer(sdkDir);
    if (auto res = loadLibDeps(sdkDir, "ids_peak_comfort_c.dll", libs); !res.isEmpty()) {
        libError = res;
        return;
    }
    
    foreach (const auto& libPath, libs) {
        auto r = getLibVer(libPath);
        if (r.ok())
            qDebug() << LOG_ID << "Loading" << libPath << r.result();
        else
            qDebug() << LOG_ID << "Loading" << libPath << "(ver:" << r.error() << ')';

        if (!QFile::exists(libPath)) {
            qWarning() << LOG_ID << "Library not found" << libPath;
            libError = "Library not found: " + libPath;
            __hLib = 0;
            return;
        }
        __hLib = LoadLibraryW(libPath.toStdWString().c_str());
        if (__hLib == 0) {
            auto err = getSysError(GetLastError());
            qWarning() << LOG_ID << "Failed to load" << libPath << err;
            libError = QString("Unable to load library %1: %2").arg(libPath, err);
            return;
        }
    }

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
#ifdef ADJUST_PIXEL_CLOCK
    GET_PROC(peak_PixelClock_GetAccessStatus);
    GET_PROC(peak_PixelClock_HasRange);
    GET_PROC(peak_PixelClock_GetRange);
    GET_PROC(peak_PixelClock_GetList);
    GET_PROC(peak_PixelClock_Get);
    GET_PROC(peak_PixelClock_Set);
#endif

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
