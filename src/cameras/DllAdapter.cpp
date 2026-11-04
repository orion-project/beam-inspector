#include "DllAdapter.h"
#include "DllHelper.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#define LOG_ID "DllAdapter:"

//------------------------------------------------------------------------------
//                                 DllAdapter
//------------------------------------------------------------------------------

template <typename TProc>
static QString getProc(HMODULE hLib, const char *name, TProc *proc)
{
    auto res = resolveProc(LOG_ID, hLib, name);
    if (!res.ok())
        return res.error();
    *proc = (TProc)res.result();
    return {};
}

DllAdapter::DllAdapter(const QString& libPath)
{
    QFileInfo libInfo(libPath);
    _id = libPath;
    _name = libInfo.fileName();
    
    if (!libInfo.exists()) {
        qWarning() << LOG_ID << "File not found" << libInfo.absoluteFilePath();
        _error = qApp->tr("File not found: %1").arg(libPath);
        return;
    }
    
    auto sdkResult = loadSdk(LOG_ID, libInfo.absoluteDir().absolutePath(), libInfo.fileName());
    if (!sdkResult.ok()) {
        _error = sdkResult.error();
        return;
    }
    
    _hLib = sdkResult.result();

    #define GET_PROC(name) \
        if (auto err = getProc(_hLib, "IBeam_"#name, &name); !err.isEmpty()) { \
            _error = err; \
            _hLib = 0; \
            return; \
        }

    GET_PROC(init);
    GET_PROC(exit);
    GET_PROC(getAdapterInfo);
    GET_PROC(getCameraCount)
    GET_PROC(getCameraList)
    GET_PROC(getStatusText)
    
    IBeam_AdapterInfo info;
    auto res = getAdapterInfo(&info);
    if (IBEAM_ERROR(res)) {
        _hLib = 0;
        _error = QString("Unable to get adapter info: %1").arg(errorText(res));
        qWarning().noquote() << LOG_ID << _error;
        return;
    }

    _name = QString::fromLatin1(info.displayName);
    
    if (info.apiVersion != IBEAM_API_V1) {
        _hLib = 0;
        _error = QString("Unsupported API version %1, expected %2").arg(info.apiVersion).arg(IBEAM_API_V1);
        qWarning().noquote() << LOG_ID << _error;
        return;
    }
    
    qDebug() << LOG_ID << "Adapter loaded" << _name
        << "| Version:" << QString::fromLatin1(info.displayVersion)
        << "| API:" << info.apiVersion;
}

QString DllAdapter::errorText(IBeam_Status status)
{
    QByteArray buf(IBEAM_ERROR_LEN, 0);
    auto res = getStatusText(status, buf.data(), buf.size());
    if (IBEAM_OK(res))
        return QString::fromLatin1(buf.data());

    qWarning() << LOG_ID << "Unable to get text for error" << status << "because of error" << res;
    return QString("errcode=%1").arg(status);
}
