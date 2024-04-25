#include "IdsComfort.h"

#include "cameras/CameraTypes.h"

#include <ids_peak_comfort_c/ids_peak_comfort_c.h>

#include <QDebug>

#define LOG_ID "IdsComfort:"

QString getPeakError(peak_status status)
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

IdsComfort* IdsComfort::init()
{
    auto res = peak_Library_Init();
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
    auto res = peak_Library_Exit();
    if (PEAK_ERROR(res))
        qWarning() << LOG_ID << "Unable to deinit library" << getPeakError(res);
    else qDebug() << LOG_ID << "Library deinited";
}

QVector<CameraItem> IdsComfort::getCameras()
{
    size_t camCount;
    auto res = peak_CameraList_Update(&camCount);
    if (PEAK_ERROR(res)) {
        qWarning() << LOG_ID << "Unable to update camera list" << getPeakError(res);
        return {};
    }
    else qDebug() << LOG_ID << "Camera list updated";

    QVector<peak_camera_descriptor> cams(camCount);
    res = peak_CameraList_Get(cams.data(), &camCount);
    if (PEAK_ERROR(res)) {
        qWarning() << LOG_ID << "Unable to get camera list" << getPeakError(res);
        return {};
    }

    QVector<CameraItem> result;
    for (const auto &cam : cams) {
        qDebug() << LOG_ID << cam.cameraID << cam.cameraType << cam.modelName << cam.serialNumber;
        result << CameraItem {
            .id = cam.cameraID,
            .name = QString::fromLatin1(cam.modelName)
        };
    }
    return result;
}
