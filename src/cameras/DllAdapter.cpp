#include "DllAdapter.h"
#include "DllHelper.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#define LOG_ID "DllAdapter:"

Q_GLOBAL_STATIC(QVector<DllAdapter*>, __adapters__);

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

static void loadAdaptersFromJson(int& loadedCount)
{
    QFile jsonConfig(qApp->applicationDirPath() + QStringLiteral("/adapters.json"));
    if (!jsonConfig.exists())
        return;

    qDebug() << LOG_ID << "Loading adapters from json config";

    if (!jsonConfig.open(QIODevice::ReadOnly)) {
        qCritical() << LOG_ID << "Unable to open json config" << jsonConfig.errorString();
        return;
    }

    QJsonParseError jErr;
    auto jDoc = QJsonDocument::fromJson(jsonConfig.readAll(), &jErr);
    if (jErr.error != QJsonParseError::NoError) {
        qCritical() << LOG_ID << "Unable to parse json config" << jErr.errorString();
        return;
    }

    if (!jDoc.isArray()) {
        qWarning() << LOG_ID << "Invalid json format, array expected";
        return;
    }

    auto jArr = jDoc.array();
    for (auto it = jArr.cbegin(); it != jArr.cend(); it++) {
        auto jObj = it->toObject();
        QString libPath = jObj["libPath"].toString();
        QString sdkPath = jObj["sdkPath"].toString();
        if (libPath.isEmpty())
            continue;
        qDebug() << LOG_ID << "Adapter" << libPath << sdkPath;
        auto adapter = new DllAdapter(libPath, sdkPath);
        if (adapter->loaded())
            loadedCount++;
        __adapters__->append(adapter);
    }
}

static void loadAdaptersFromSubdir(int& loadedCount)
{
    QString adaptersDir = qApp->applicationDirPath() + QStringLiteral("/adapters/");
    if (!QDir(adaptersDir).exists())
        return;

    qDebug() << LOG_ID << "Loading adapters from subdirs";

    QStringList nameFilter { QStringLiteral("*adapter*.dll") };
    for (const auto& subdir : QDir(adaptersDir).entryList(QDir::Dirs|QDir::NoDotAndDotDot))
    {
        QString adapterDir = adaptersDir + subdir;
        qDebug() << LOG_ID << "Loading adapters from subdir" << adapterDir;
        for (const auto& dll : QDir(adapterDir).entryList(nameFilter, QDir::Files))
        {
            QString libPath = QStringLiteral("./adapters/") + subdir + '/' + dll;
            qDebug() << LOG_ID << "Adapter" << libPath;
            auto adapter = new DllAdapter(libPath, {});
            if (adapter->loaded())
                loadedCount++;
            __adapters__->append(adapter);
        }
    }
}

void DllAdapter::loadAdapters()
{
    int loadedCount = 0;
    loadAdaptersFromJson(loadedCount);
    loadAdaptersFromSubdir(loadedCount);
    qDebug() << LOG_ID << "Adapters found:" << __adapters__->size() << "| loaded:" << loadedCount;
}

void DllAdapter::unloadAdapters()
{
    for (auto adapter : *__adapters__)
    {
        if (!adapter->loaded())
            continue;
        auto res = adapter->exit();
        if (res)
            qWarning() << LOG_ID << "Unable to deinit adapter" << adapter->id() << errorText(res);
        else qDebug() << LOG_ID << "Adapter deinited" << adapter->id();
    }
}

const QVector<DllAdapter*>& DllAdapter::getAdapters()
{
    return *__adapters__;
}

DllAdapter* DllAdapter::getAdapter(const QString& adapterId)
{
    for (auto adapter : *__adapters__)
        if (adapter->id() == adapterId)
            return adapter;
    qWarning() << "Unknown adapter" << adapterId;
    return nullptr;
}

DllAdapter::DllAdapter(const QString& libPath, const QString& sdkPath)
{
    // Adapter ID is always its libPath as it given in the config (either relative or not).
    // Using relative paths (like `../libs/adapter_demo/adapter_demo.dll`) could be useful
    // as this allows to move/reinstall the application into a different directory
    // while not spoiling the camera settings saved in the user profile (`AppData\Roaming`).
    // Their settings key is the relative path and it's still valid after reinstalling.
    _id = libPath;

    QFileInfo libInfo(libPath);
    
    // Default adapter display name is it's library file name (without path).
    // Will be overridden (see below) if the adapter is loaded successfully
    // and provides some user-friendly name.
    _name = libInfo.fileName();

    if (!libInfo.isAbsolute())
        libInfo = QFileInfo(qApp->applicationDirPath() + '/' + libPath);

    if (!libInfo.exists()) {
        qCritical() << LOG_ID << "File not found" << libInfo.absoluteFilePath();
        _error = QString("File not found: %1").arg(libPath);
        return;
    }
    
    QString mainDll = libInfo.absoluteFilePath();

    QString sdkDir = sdkPath.isEmpty()
        ? libInfo.absoluteDir().absolutePath()
        : !QDir(sdkPath).isAbsolute()
            ? QDir(qApp->applicationDirPath() + '/' + sdkPath).absolutePath()
            : sdkPath;
    
    if (!QDir(sdkDir).exists()) {
        qCritical() << LOG_ID << "Directory not found" << sdkDir;
        _error = QString("Directory not found: %1").arg(sdkDir);
        return;
    }
    
    auto sdkResult = loadSdk(LOG_ID, sdkDir, mainDll);
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
    GET_PROC(getCameraList)
    GET_PROC(openCamera);
    GET_PROC(closeCamera);
    GET_PROC(getFrameInfo);
    GET_PROC(getFrame);

    IBeamAdapterInfo info = { 0 };
    auto res = getAdapterInfo(&info);
    if (res) {
        _error = QString("Unable to get adapter info: %1").arg(errorText(res));
        qCritical().noquote() << LOG_ID << _error;
        return;
    }

    if (auto name = QString::fromLatin1(info.displayName).trimmed(); !name.isEmpty())
        _name = name;
    
    if (info.apiVersion != IBEAM_API_V1) {
        _error = QString("Unsupported API version %1, expected %2").arg(info.apiVersion).arg(IBEAM_API_V1);
        qCritical().noquote() << LOG_ID << _error;
        return;
    }

    qDebug() << LOG_ID << "Adapter loaded" << _name
        << "| Version:" << QString::fromLatin1(info.displayVersion)
        << "| API:" << info.apiVersion;

    res = init();
    if (res) {
        _error = QString("Unable to initialize adapter: %1").arg(errorText(res));
        qCritical().noquote() << LOG_ID << _error;
        return;
    }
}

QString DllAdapter::errorText(IBeamStatus* status)
{
    QString text = QString::fromLatin1(status->text).trimmed();
    if (text.isEmpty())
        return QString("errcode=%1").arg(status->code);
    return text;
}

QString DllAdapter::makeCustomId(IBeamCamId cameraId) const
{
    return QStringLiteral("%1:%2").arg(_id).arg(cameraId);
}

void DllAdapter::splitCustomId(const QString& customId, QString& adapterId, IBeamCamId& cameraId)
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

    if (!loaded()) {
        qWarning() << "Adapter not loaded" << _error;
        return QString("Adapter not loaded: %1").arg(_error);
    }
    
    size_t camCount;
    IBeamCamList camList;
    auto res = getCameraList(&camList, &camCount);
    if (res) {
        qWarning() << "Unable to get camera list" << errorText(res);
        return QString("Unable to get camera list: %1").arg(errorText(res));
    }
    
    qDebug() << LOG_ID << "Cameras found:" << camCount;

    for (int i = 0; i < camCount; i++) {
        IBeamCamInfo *cam = camList + i;
        QString displayName = cam->flags & IBEAM_CAM_FLAG_NAME_WCHAR
            ? QString::fromWCharArray((const wchar_t*)(cam->displayName))
            : QString::fromLatin1(cam->displayName);
        qDebug() << LOG_ID << "Camera Id:" << cam->id << "| Name:" << displayName << cam->displayName;
        _cameras << CameraItem {
            .cameraId = cam->id,
            .customId = makeCustomId(cam->id),
            .displayName = displayName,
        };
    }
    
    return {};
}
