#include "DllHelper.h"

#include <QDebug>
#include <QDir>
#include <QFile>

static QString getSysError(DWORD err)
{
    LPWSTR buf = NULL;
    auto size = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&buf, 0, NULL);
    if (size == 0)
        return QStringLiteral("code: 0x%1").arg(err, 0, 16);
    QString text = QString::fromWCharArray(buf, size).trimmed();
    LocalFree(buf);
    return text;
}

ProcResult resolveProc(const char* logId, HMODULE hLib, const char* name)
{
    auto proc = GetProcAddress(hLib, name);
    if (proc == NULL)
    {
        auto err = GetLastError();
        auto errText = getSysError(err);
        qWarning() << logId << "Failed to resolve proc" << name << errText;
        return ProcResult::fail(QString("Function '%1' not loaded: %2").arg(name, errText));
    }
    return ProcResult::ok(proc);
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
    
    if (!GetFileVersionInfoA(stdName.c_str(), 0, dwSize, lpData))
    {
        auto err = getSysError(GetLastError());
        free(lpData);
        return VerResult::fail(err);
    }
    
    VS_FIXEDFILEINFO* pFileInfo;
    UINT uLen;
    if (!VerQueryValueA(lpData, "\\", (LPVOID*)&pFileInfo, &uLen))
    {
        auto err = getSysError(GetLastError());
        free(lpData);
        return VerResult::fail(err);
    }
    if (uLen == 0)
    {
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

static QString findLibDeps(const char* logId, const QString &sdkDir, const QString &libName, QStringList &deps)
{
    QString libPath = QFileInfo(libName).isAbsolute() ? libName : (sdkDir + '/' + libName);
    qDebug() << logId << "Resolve deps for" << libPath;
    if (!QFile::exists(libPath))
    {
        qWarning() << logId << "Library not found" << libPath;
        return "Library not found: " + libPath;
    }
    
    auto hLib = LoadLibraryExW(libPath.toStdWString().c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!hLib)
    {
        auto err = getSysError(GetLastError());
        qWarning() << logId << "Failed to load library" << libPath << err;
        return QString("Unable to load library %1: %2").arg(libPath, err);
    }
    
    PIMAGE_DOS_HEADER peHeader = (PIMAGE_DOS_HEADER)hLib;
    if (peHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        printf("Error: Not a valid PE file\n");
        FreeLibrary(hLib);
        return QString("Unable to load library %1: invalid PE-header").arg(libPath);
    }
    
    PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)((BYTE*)hLib + peHeader->e_lfanew);
    if (ntHeader->Signature != IMAGE_NT_SIGNATURE)
    {
        printf("Error: Not a valid PE file\n");
        FreeLibrary(hLib);
        return QString("Unable to load library %1: invalid NT-header").arg(libPath);
    }
    
    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hLib + 
        ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    if (ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size == 0 || 
        ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == 0)
    {
        FreeLibrary(hLib);
        return QString("Unable to load library %1: imports is empty").arg(libPath);
    }
    
    while (importDesc->Name != 0)
    {
        QString dllName = QString((const char*)((BYTE*)hLib + importDesc->Name));
        QString dllPath = sdkDir + '/' + dllName;
        if (QFile::exists(dllPath) && !deps.contains(dllPath))
        {
            QString res = findLibDeps(logId, sdkDir, dllName, deps);
            if (!res.isEmpty()) 
            {
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

SdkResult loadSdk(const char* logId, const QString& sdkDir, const QString& mainDll)
{
    // In general, SDK is in a random directory and consist of several DLL files.
    // We load functions from the mail DLL which depends on other DLLs in the SDK dir.
    // OS doesn't know from where these additional DLLs should be loaded
    // since they are in the random dir (neither in app dir or in standrad dir).
    // So we have to resolve these additional DLL paths and load them manually
    // before the main DLL will be loaded.

    QStringList libs;
    if (auto res = findLibDeps(logId, sdkDir, mainDll, libs); !res.isEmpty())
        return SdkResult::fail(res);
    
    HMODULE hLib = 0;
    
    for (const auto& libPath : std::as_const(libs))
    {
        auto r = getLibVer(libPath);
        if (r.ok())
            qDebug() << logId << "Loading" << libPath << r.result();
        else
            qDebug() << logId << "Loading" << libPath << "(ver:" << r.error() << ')';

        if (!QFile::exists(libPath))
        {
            qWarning() << logId << "Library not found" << libPath;
            return SdkResult::fail("Library not found: " + libPath);
        }
        
        hLib = LoadLibraryW(libPath.toStdWString().c_str());
        if (hLib == 0)
        {
            auto err = getSysError(GetLastError());
            qWarning() << logId << "Unable to load library" << libPath << err;
            return SdkResult::fail(QString("Unable to load library %1: %2").arg(libPath, err));
        }
    }

    return SdkResult::ok(hLib);
}
