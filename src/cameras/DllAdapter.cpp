#include "DllAdapter.h"
#include "DllHelper.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

#define LOG_ID "DllAdapter:"

//------------------------------------------------------------------------------
//                                   Helpers
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

//------------------------------------------------------------------------------
//                                 DllAdapter
//------------------------------------------------------------------------------

DllAdapter::DllAdapter(const QString& libPath)
{
    QFileInfo libInfo(libPath);
    _id = libPath;
    _name = libInfo.fileName();
    
    if (!libInfo.exists()) {
        qCritical() << LOG_ID << "File not found" << libInfo.absoluteFilePath();
        _error = QString("File not found: %1").arg(libPath);
        return;
    }
    
    auto sdkResult = loadSdk(LOG_ID, libInfo.absoluteDir().absolutePath(), libInfo.fileName());
    if (!sdkResult.ok()) {
        _error = sdkResult.error();
        return;
    }
    
    auto hLib = sdkResult.result();

    #define GET_PROC(name) \
        if (auto err = getProc(hLib, "IBeam_"#name, &name); !err.isEmpty()) { \
            _error = err; \
            return; \
        }

    GET_PROC(init);
    GET_PROC(exit);
    GET_PROC(getAdapterInfo);
    GET_PROC(getCameraCount)
    GET_PROC(getCameraList)
    GET_PROC(getStatusText)
    GET_PROC(openCamera);
    GET_PROC(closeCamera);
    GET_PROC(getFrameInfo);
    GET_PROC(getFrame);

    IBeam_AdapterInfo info;
    memset(&info, 0, sizeof(IBeam_AdapterInfo));
    auto res = getAdapterInfo(&info);
    if (IBEAM_ERROR(res)) {
        _error = QString("Unable to get adapter info: %1").arg(errorText(res));
        qCritical().noquote() << LOG_ID << _error;
        return;
    }

    _name = QString::fromLatin1(info.displayName);
    
    if (info.apiVersion != IBEAM_API_V1) {
        _error = QString("Unsupported API version %1, expected %2").arg(info.apiVersion).arg(IBEAM_API_V1);
        qCritical().noquote() << LOG_ID << _error;
        return;
    }

    qDebug() << LOG_ID << "Adapter loaded" << _name
        << "| Version:" << QString::fromLatin1(info.displayVersion)
        << "| API:" << info.apiVersion;

    res = init();
    if (IBEAM_ERROR(res)) {
        _error = QString("Unable to initialize adapter: %1").arg(errorText(res));
        qCritical().noquote() << LOG_ID << _error;
        return;
    }
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

QString DllAdapter::makeCustomId(IBeam_CameraId cameraId) const
{
    return QStringLiteral("%1:%2").arg(_id).arg(cameraId);
}

void DllAdapter::splitCustomId(const QString& customId, QString& adapterId, IBeam_CameraId& cameraId)
{
    int pos = customId.lastIndexOf(':');
    if (pos < 0) return;
    adapterId = customId.left(pos);
    bool goodId = false;
    cameraId = QStringView(customId).sliced(pos+1).toInt(&goodId);
    if (!goodId) cameraId = IBEAM_BAD_CAMERA_ID;
}

QString DllAdapter::refreshCameras()
{
    _cameras.clear();

    if (!loaded())
        return QString("Adapter not loaded: %1").arg(_error);
    
    size_t camCount;
    auto res = getCameraCount(&camCount);
    if (IBEAM_ERROR(res))
        return QString("Unable to get camera count: %1").arg(errorText(res));
    
    qDebug() << LOG_ID << "Cameras found:" << camCount;
    
    QVector<IBeam_CameraInfo> cams(camCount);
    memset(cams.data(), 0, sizeof(IBeam_CameraInfo)*camCount);
    res = getCameraList(cams.data(), camCount);
    if (IBEAM_ERROR(res))
        return QString("Unable to get camera list: %1").arg(errorText(res));
    
    for (const auto &cam : cams) {
        qDebug() << LOG_ID << "Camera Id:" << cam.cameraId << "| Name:" << cam.displayName;
        _cameras << CameraItem {
            .cameraId = cam.cameraId,
            .customId = makeCustomId(cam.cameraId),
            .displayName = cam.flags & IBEAM_CAM_FLAG_NAME_WCHAR
                ? QString::fromWCharArray((const wchar_t*)cam.displayName)
                : QString::fromLatin1(cam.displayName),
        };
    }
    
    return {};
}
